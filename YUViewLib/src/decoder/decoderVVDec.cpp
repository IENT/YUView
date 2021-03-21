/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "decoderVVDec.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <cstring>

#include "common/typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define decoderVVDec_DEBUG_OUTPUT 0
#if decoderVVDec_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if decoderVVDec_DEBUG_OUTPUT == 1
#define DEBUG_vvdec                                                                                \
  if (!isCachingDecoder)                                                                           \
  qDebug
#elif decoderVVDec_DEBUG_OUTPUT == 2
#define DEBUG_vvdec                                                                                \
  if (isCachingDecoder)                                                                            \
  qDebug
#elif decoderVVDec_DEBUG_OUTPUT == 3
#define DEBUG_vvdec                                                                                \
  if (isCachingDecoder)                                                                            \
    qDebug("c:");                                                                                  \
  else                                                                                             \
    qDebug("i:");                                                                                  \
  qDebug
#endif
#else
#define DEBUG_vvdec(fmt, ...) ((void)0)
#endif

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of
// the pointer will only be accessed through that pointer (and pointers copied from it).
#if __STDC__ != 1
#define restrict __restrict /* use implementation __ format */
#else
#ifndef __STDC_VERSION__
#define restrict __restrict /* use implementation __ format */
#else
#if __STDC_VERSION__ < 199901L
#define restrict __restrict /* use implementation __ format */
#else
#/* all ok */
#endif
#endif
#endif

namespace decoder
{

namespace
{

const auto MAX_CODED_PICTURE_SIZE = 800000;

void loggingCallback(void *ptr, int level, const char *msg, va_list list)
{
  (void)ptr;
  (void)level;
  (void)msg;
  (void)list;
#if decoderVVDec_DEBUG_OUTPUT && !NDEBUG
  char buf[200];
  vsnprintf( buf, 200, msg, list );
  qDebug() << "decoderVVDec::decoderVVDec vvdeclog(" << level << "): " << buf;
#endif
}

YUV_Internals::Subsampling convertFromInternalSubsampling(vvdecColorFormat fmt)
{
  if (fmt == VVDEC_CF_YUV400_PLANAR)
    return YUV_Internals::Subsampling::YUV_400;
  if (fmt == VVDEC_CF_YUV420_PLANAR)
    return YUV_Internals::Subsampling::YUV_420;
  if (fmt == VVDEC_CF_YUV422_PLANAR)
    return YUV_Internals::Subsampling::YUV_422;
  if (fmt == VVDEC_CF_YUV444_PLANAR)
    return YUV_Internals::Subsampling::YUV_444;

  return YUV_Internals::Subsampling::UNKNOWN;
}

Size calculateChromaSize(Size lumaSize, vvdecColorFormat fmt)
{
  if (fmt == VVDEC_CF_YUV400_PLANAR)
    return {};
  if (fmt == VVDEC_CF_YUV420_PLANAR)
    return {lumaSize.width / 2, lumaSize.height / 2};
  if (fmt == VVDEC_CF_YUV422_PLANAR)
    return {lumaSize.width / 2, lumaSize.height};
  if (fmt == VVDEC_CF_YUV444_PLANAR)
    return lumaSize;

  return {};
}

} // namespace

decoderVVDec::decoderVVDec(int signalID, bool cachingDecoder) : decoderBaseSingleLib(cachingDecoder)
{
  // For now we don't support different signals (like prediction, residual)
  (void)signalID;

  this->rawFormat = YUView::raw_YUV;

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  this->loadDecoderLibrary(settings.value("libVVDecFile", "").toString());
  settings.endGroup();

  if (this->decoderState != DecoderState::Error)
    this->allocateNewDecoder();
}

decoderVVDec::~decoderVVDec()
{
  if (this->decoder != nullptr)
  {
    auto ret = this->lib.vvdec_decoder_close(this->decoder);
    if (ret != VVDEC_OK)
      DEBUG_vvdec("decoderVVDec::~decoderVVDec - error freeing decoder");
    this->decoder = nullptr;
  }
  if (this->accessUnit != nullptr)
  {
    this->lib.vvdec_accessUnit_free(this->accessUnit);
    this->accessUnit = nullptr;
  }
}

QStringList decoderVVDec::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open the .so file first.
  // Since this has been compiled for linux it will fail and not even try to open the .dylib.
  // On windows and linux ommitting the extension works
  QStringList names;
  if (is_Q_OS_LINUX)
    names << "libvvdecLib";
  if (is_Q_OS_MAC)
    names << "libvvdecLib.dylib";
  if (is_Q_OS_WIN)
    names << "vvdecLib";

  return names;
}

void decoderVVDec::resolveLibraryFunctionPointers()
{
  if (!resolve(this->lib.vvdec_get_version, "vvdec_get_version"))
    return;

  if (!resolve(this->lib.vvdec_accessUnit_alloc, "vvdec_accessUnit_alloc"))
    return;
  if (!resolve(this->lib.vvdec_accessUnit_free, "vvdec_accessUnit_free"))
    return;
  if (!resolve(this->lib.vvdec_accessUnit_alloc_payload, "vvdec_accessUnit_alloc_payload"))
    return;
  if (!resolve(this->lib.vvdec_accessUnit_free_payload, "vvdec_accessUnit_free_payload"))
    return;
  if (!resolve(this->lib.vvdec_accessUnit_default, "vvdec_accessUnit_default"))
    return;

  if (!resolve(this->lib.vvdec_params_default, "vvdec_params_default"))
    return;
  if (!resolve(this->lib.vvdec_params_alloc, "vvdec_params_alloc"))
    return;
  if (!resolve(this->lib.vvdec_params_alloc, "vvdec_params_alloc"))
    return;
  if (!resolve(this->lib.vvdec_params_free, "vvdec_params_free"))
    return;

  if (!resolve(this->lib.vvdec_decoder_open, "vvdec_decoder_open"))
    return;
  if (!resolve(this->lib.vvdec_decoder_close, "vvdec_decoder_close"))
    return;

  if (!resolve(this->lib.vvdec_set_logging_callback, "vvdec_set_logging_callback"))
    return;
  if (!resolve(this->lib.vvdec_decode, "vvdec_decode"))
    return;
  if (!resolve(this->lib.vvdec_flush, "vvdec_flush"))
    return;
  if (!resolve(this->lib.vvdec_frame_unref, "vvdec_frame_unref"))
    return;

  if (!resolve(this->lib.vvdec_get_hash_error_count, "vvdec_get_hash_error_count"))
    return;
  if (!resolve(this->lib.vvdec_get_dec_information, "vvdec_get_dec_information"))
    return;
  if (!resolve(this->lib.vvdec_get_last_error, "vvdec_get_last_error"))
    return;
  if (!resolve(this->lib.vvdec_get_last_additional_error, "vvdec_get_last_additional_error"))
    return;
  if (!resolve(this->lib.vvdec_get_error_msg, "vvdec_get_error_msg"))
    return;
}

template <typename T> T decoderVVDec::resolve(T &fun, const char *symbol, bool optional)
{
  auto ptr = this->library.resolve(symbol);
  if (!ptr)
  {
    if (!optional)
      this->setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.")
                         .arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

void decoderVVDec::resetDecoder()
{
  if (this->decoder != nullptr)
    if (this->lib.vvdec_decoder_close(decoder) != VVDEC_OK)
      return setError("Reset: Freeing the decoder failded.");

  this->decoder = nullptr;

  this->allocateNewDecoder();
}

void decoderVVDec::allocateNewDecoder()
{
  if (this->decoder != nullptr)
    return;

  DEBUG_vvdec("decoderVVDec::allocateNewDecoder - decodeSignal %d", decodeSignal);

  vvdecParams params;
  this->lib.vvdec_params_default(&params);

  params.logLevel = VVDEC_INFO;

  this->decoder = this->lib.vvdec_decoder_open(&params);
  if (this->decoder == nullptr)
  {
    this->setError("Error allocating deocder");
    return;
  }

  this->flushing = false;
  this->currentOutputBuffer.clear();
  this->decoderState = DecoderState::NeedsMoreData;

  auto ret = this->lib.vvdec_set_logging_callback(this->decoder, loggingCallback);
  if (ret != VVDEC_OK)
    DEBUG_vvdec("decoderVVDec::allocateNewDecoder - error setting logging callback");

  if (this->accessUnit == nullptr)
  {
    this->accessUnit = this->lib.vvdec_accessUnit_alloc();
    if (this->accessUnit == nullptr)
    {
      this->setError("Error allocating access unit");
      return;
    }
    this->lib.vvdec_accessUnit_alloc_payload(this->accessUnit, MAX_CODED_PICTURE_SIZE);
    if (this->accessUnit->payload == nullptr)
      this->setError("Error allocating AU payload buffer");
  }
}

bool decoderVVDec::decodeNextFrame()
{
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_vvdec("decoderVVDec::decodeNextFrame: Wrong decoder state.");
    return false;
  }

  if (this->flushing)
  {
    // This is our way of moving the decoder to the next picture when flushing.
    auto ret = this->lib.vvdec_flush(this->decoder, &this->currentFrame);
    if (ret == VVDEC_EOF)
    {
      DEBUG_vvdec("decoderVVDec::pushData: Flushing returned EOF");
      this->decoderState = DecoderState::EndOfBitstream;
      return false;
    }
    else if (ret != VVDEC_OK)
      return setErrorB("Error sendling flush to decoder");
    DEBUG_vvdec("decoderVVDec::pushData: Flushing to next pixture. %s",
                this->currentFrame != nullptr ? " frameAvailable" : "");

    if (this->currentFrame == nullptr)
    {
      DEBUG_vvdec("decoderVVDec::decodeNextFrame No more frames to flush. EOF.");
      this->decoderState = DecoderState::EndOfBitstream;
      return false;
    }

    this->currentOutputBuffer.clear();
    DEBUG_vvdec("decoderVVDec::decodeNextFrame Flushing - Invalidate buffer");
  }
  else
  {
    if (this->currentFrameReadyForRetrieval)
    {
      DEBUG_vvdec("decoderVVDec::decodeNextFrame Switch back to NeedsMoreData");
      this->decoderState                  = DecoderState::NeedsMoreData;
      this->currentFrameReadyForRetrieval = false;
      return false;
    }
    else
    {
      DEBUG_vvdec("decoderVVDec::decodeNextFrame Current frame ready to read out");
      this->currentFrameReadyForRetrieval = true;
      return true;
    }
  }

  return this->getNextFrameFromDecoder();
}

bool decoderVVDec::getNextFrameFromDecoder()
{
  DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder");

  if (this->currentFrame == nullptr)
    return false;

  // Check the validity of the picture
  const auto lumaSize = Size({this->currentFrame->width, this->currentFrame->height});
  const auto nrPlanes = (this->currentFrame->colorFormat == VVDEC_CF_YUV400_PLANAR) ? 1u : 3u;

  if (lumaSize.width == 0 || lumaSize.height == 0)
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got invalid size");
  auto subsampling = convertFromInternalSubsampling(this->currentFrame->colorFormat);
  if (subsampling == YUV_Internals::Subsampling::UNKNOWN)
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got invalid chroma format");
  auto bitDepth = this->currentFrame->bitDepth;
  if (bitDepth < 8 || bitDepth > 16)
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got invalid bit depth");
  if (nrPlanes != this->currentFrame->numPlanes)
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got non expected number of planes");

  for (unsigned i = 1; i < this->currentFrame->numPlanes; i++)
  {
    auto expectedSize = calculateChromaSize(lumaSize, this->currentFrame->colorFormat);
  }

  if (!this->frameSize && !this->formatYUV.isValid())
  {
    // Set the values
    this->frameSize = lumaSize;
    this->formatYUV = YUV_Internals::yuvPixelFormat(subsampling, bitDepth);
  }
  else
  {
    // Check the values against the previously set values
    if (this->frameSize != lumaSize)
      return setErrorB("Received a frame of different size");
    if (this->formatYUV.subsampling != subsampling)
      return setErrorB("Received a frame with different subsampling");
    if (unsigned(this->formatYUV.bitsPerSample) != bitDepth)
      return setErrorB("Received a frame with different bit depth");
  }

  DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got a valid frame");
  return true;
}

bool decoderVVDec::pushData(QByteArray &data)
{
  if (decoderState != DecoderState::NeedsMoreData)
  {
    DEBUG_vvdec("decoderVVDec::pushData: Wrong decoder state.");
    return false;
  }
  if (this->flushing)
  {
    DEBUG_vvdec("decoderVVDec::pushData: Don't push more data when flushing.");
    return false;
  }

  bool endOfFile = (data.length() == 0);
  if (endOfFile)
  {
    auto ret = this->lib.vvdec_flush(this->decoder, &this->currentFrame);
    if (ret != VVDEC_OK)
      return setErrorB("Error sendling flush to decoder");
    DEBUG_vvdec("decoderVVDec::pushData: Received empty packet. Sending EOF. %s",
                this->currentFrame != nullptr ? " frameAvailable" : "");
  }
  else
  {
    if (data.size() > this->accessUnit->payloadSize)
      return setErrorB("Access unit too big to push");

    std::memcpy(this->accessUnit->payload, data.constData(), data.size());
    this->accessUnit->payloadUsedSize = data.size();

    auto ret = this->lib.vvdec_decode(this->decoder, this->accessUnit, &this->currentFrame);
    if (ret == VVDEC_EOF)
      endOfFile = true;
    else if (ret != VVDEC_TRY_AGAIN && ret != VVDEC_OK)
    {
      auto cErr    = this->lib.vvdec_get_last_error(this->decoder);
      auto cErrAdd = this->lib.vvdec_get_last_additional_error(this->decoder);
      return setErrorB(QString("Error pushing data to decoder length %1 - %2 - %3")
                           .arg(data.length())
                           .arg(cErr)
                           .arg(cErrAdd));
    }

    DEBUG_vvdec("decoderVVDec::pushData pushed NAL length %d%s",
                data.length(),
                this->currentFrame != nullptr ? " frameAvailable" : "");
  }

  if (this->getNextFrameFromDecoder())
  {
    this->decoderState = DecoderState::RetrieveFrames;
    this->currentOutputBuffer.clear();
  }

  if (endOfFile)
    this->flushing = true;

  return true;
}

QByteArray decoderVVDec::getRawFrameData()
{
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_vvdec("decoderVVDec::getRawFrameData: Wrong decoder state.");
    return {};
  }

  if (this->currentOutputBuffer.isEmpty())
  {
    // Put image data into buffer
    copyImgToByteArray(this->currentOutputBuffer);
    DEBUG_vvdec("decoderVVDec::getRawFrameData copied frame to buffer");
  }

  return currentOutputBuffer;
}

void decoderVVDec::copyImgToByteArray(QByteArray &dst)
{
  auto fmt = this->currentFrame->colorFormat;
  if (fmt == VVDEC_CF_INVALID)
  {
    DEBUG_vvdec("decoderVVDec::copyImgToByteArray picture format is unknown");
    return;
  }
  const auto nrPlanes      = this->currentFrame->numPlanes;
  const auto bytesPerSample = this->currentFrame->bitDepth > 8 ? 2 : 1;
  const auto lumaSize      = Size({this->currentFrame->width, this->currentFrame->height});
  const auto chromaSize    = calculateChromaSize(lumaSize, fmt);

  auto outSizeLumaBytes   = lumaSize.width * lumaSize.height * bytesPerSample;
  auto outSizeChromaBytes = chromaSize.width * chromaSize.height * bytesPerSample;
  // How many bytes do we need in the output buffer?
  auto nrBytesOutput = (outSizeLumaBytes + outSizeChromaBytes * 2);
  DEBUG_vvdec("decoderVVDec::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < int(nrBytesOutput))
    dst.resize(int(nrBytesOutput));

  for (unsigned c = 0; c < nrPlanes; c++)
  {
    auto &component = this->currentFrame->planes[c];
    auto cIdx      = (c == 0 ? 0 : 1);

    auto widthBytes = component.width * bytesPerSample;
    auto plane = component.ptr;

    if (component.ptr == nullptr)
    {
      DEBUG_vvdec("decoderVVDec::copyImgToByteArray unable to get plane for component %d", c);
      return;
    }

    unsigned char *restrict d = (unsigned char *)dst.data();
    if (c > 0)
      d += outSizeLumaBytes;
    if (c == 2)
      d += outSizeChromaBytes;

    for (unsigned y = 0; y < component.height; y++)
    {
      std::memcpy(d, plane, widthBytes);
      plane += component.stride;
      d += widthBytes;
    }
  }
}

QString decoderVVDec::getDecoderName() const
{
  return (decoderState == DecoderState::Error) ? "vvdec" : this->lib.vvdec_get_version();
}

bool decoderVVDec::checkLibraryFile(QString libFilePath, QString &error)
{
  decoderVVDec testDecoder;

  // Try to load the library file
  testDecoder.library.setFileName(libFilePath);
  if (!testDecoder.library.load())
  {
    error = "Error opening QLibrary.";
    return false;
  }

  // Now let's see if we can retrive all the function pointers that we will need.
  // If this works, we can be fairly certain that this is a valid library.
  testDecoder.resolveLibraryFunctionPointers();
  error = testDecoder.decoderErrorString();
  return testDecoder.state() != DecoderState::Error;
}

}