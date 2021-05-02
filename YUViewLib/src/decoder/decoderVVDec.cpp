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
#define DEBUG_vvdec                                                                               \
  if (!isCachingDecoder)                                                                           \
  qDebug
#elif decoderVVDec_DEBUG_OUTPUT == 2
#define DEBUG_vvdec                                                                               \
  if (isCachingDecoder)                                                                            \
  qDebug
#elif decoderVVDec_DEBUG_OUTPUT == 3
#define DEBUG_vvdec                                                                               \
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

namespace
{

const std::vector<libvvdec_ColorComponent>
    colorComponentMap({LIBvvdec_LUMA, LIBvvdec_CHROMA_U, LIBvvdec_CHROMA_V});

void loggingCallback(void *ptr, int level, const char *msg)
{
  (void)ptr;
  (void)level;
  (void)msg;
#if decoderVVDec_DEBUG_OUTPUT && !NDEBUG
  qDebug() << "decoderVVDec::decoderVVDec vvdeclog(" << level << "): " << msg;
#endif
}

} // namespace

decoderVVDec::decoderVVDec(int signalID, bool cachingDecoder)
    : decoderBaseSingleLib(cachingDecoder)
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
    this->lib.libvvdec_free_decoder(this->decoder);
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
  // Get/check function pointers
  if (!resolve(this->lib.libvvdec_get_version, "libvvdec_get_version"))
    return;
  if (!resolve(this->lib.libvvdec_new_decoder, "libvvdec_new_decoder"))
    return;
  if (!resolve(this->lib.libvvdec_set_logging_callback, "libvvdec_set_logging_callback"))
    return;
  if (!resolve(this->lib.libvvdec_free_decoder, "libvvdec_free_decoder"))
    return;
  if (!resolve(this->lib.libvvdec_push_nal_unit, "libvvdec_push_nal_unit"))
    return;

  if (!resolve(this->lib.libvvdec_get_picture_POC, "libvvdec_get_picture_POC"))
    return;
  if (!resolve(this->lib.libvvdec_get_picture_width, "libvvdec_get_picture_width"))
    return;
  if (!resolve(this->lib.libvvdec_get_picture_height, "libvvdec_get_picture_height"))
    return;
  if (!resolve(this->lib.libvvdec_get_picture_stride, "libvvdec_get_picture_stride"))
    return;
  if (!resolve(this->lib.libvvdec_get_picture_plane, "libvvdec_get_picture_plane"))
    return;
  if (!resolve(this->lib.libvvdec_get_picture_chroma_format,
               "libvvdec_get_picture_chroma_format"))
    return;
  if (!resolve(this->lib.libvvdec_get_picture_bit_depth, "libvvdec_get_picture_bit_depth"))
    return;
}

template <typename T> T decoderVVDec::resolve(T &fun, const char *symbol, bool optional)
{
  auto ptr = this->library.resolve(symbol);
  if (!ptr)
  {
    if (!optional)
      setError(QStringLiteral("Error loading the libvvdec library: Can't find function %1.")
                   .arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

void decoderVVDec::resetDecoder()
{
  if (this->decoder != nullptr)
    if (this->lib.libvvdec_free_decoder(decoder) != LIBvvdec_OK)
      return setError("Reset: Freeing the decoder failded.");

  this->decoder = nullptr;

  this->allocateNewDecoder();
}

void decoderVVDec::allocateNewDecoder()
{
  if (this->decoder != nullptr)
    return;

  DEBUG_vvdec("decoderVVDec::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Create new decoder object
  this->decoder  = this->lib.libvvdec_new_decoder();
  this->flushing = false;
  this->currentOutputBuffer.clear();
  this->decoderState = DecoderState::NeedsMoreData;

  this->lib.libvvdec_set_logging_callback(
      this->decoder, &loggingCallback, this, LIBvvdec_LOGLEVEL_INFO);
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
    bool checkOutputPictures = false;
    auto err = this->lib.libvvdec_push_nal_unit(this->decoder, nullptr, 0, checkOutputPictures);
    if (err == LIBvvdec_ERROR)
    {
      DEBUG_vvdec("decoderVVDec::decodeNextFrame Error getting next flushed frame from decoder.");
      return false;
    }
    if (!checkOutputPictures)
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

  // Check the validity of the picture
  auto picSize = Size(this->lib.libvvdec_get_picture_width(this->decoder, LIBvvdec_LUMA),
                       this->lib.libvvdec_get_picture_height(this->decoder, LIBvvdec_LUMA));
  if (picSize.isValid())
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got invalid size");
  auto subsampling =
      convertFromInternalSubsampling(this->lib.libvvdec_get_picture_chroma_format(this->decoder));
  if (subsampling == YUV_Internals::Subsampling::UNKNOWN)
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got invalid chroma format");
  auto bitDepth = this->lib.libvvdec_get_picture_bit_depth(this->decoder, LIBvvdec_LUMA);
  if (bitDepth < 8 || bitDepth > 16)
    DEBUG_vvdec("decoderVVDec::getNextFrameFromDecoder got invalid bit depth");

  if (!this->frameSize.isValid() && !this->formatYUV.isValid())
  {
    // Set the values
    this->frameSize = picSize;
    this->formatYUV = YUV_Internals::YUVPixelFormat(subsampling, bitDepth);
  }
  else
  {
    // Check the values against the previously set values
    if (this->frameSize != picSize)
      return setErrorB("Received a frame of different size");
    if (this->formatYUV.getSubsampling() != subsampling)
      return setErrorB("Received a frame with different subsampling");
    if (unsigned(this->formatYUV.getBitsPerSample()) != bitDepth)
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

  bool endOfFile           = (data.length() == 0);
  int  err                 = 0;
  bool checkOutputPictures = false;
  if (endOfFile)
  {
    DEBUG_vvdec("decoderVVDec::pushData: Received empty packet. Sending EOF.");
    err = this->lib.libvvdec_push_nal_unit(this->decoder, nullptr, 0, checkOutputPictures);
  }
  else
  {
    err = this->lib.libvvdec_push_nal_unit(
        this->decoder, (const unsigned char *)data.data(), data.length(), checkOutputPictures);
    DEBUG_vvdec("decoderVVDec::pushData pushed NAL length %d%s",
                 data.length(),
                 checkOutputPictures ? " checkOutputPictures" : "");
  }

  if (err != LIBvvdec_OK)
  {
    DEBUG_vvdec("decoderVVDec::pushData Error pushing data");
    return setErrorB(QString("Error pushing data to decoder (libvvdec_push_nal_unit) length %1")
                         .arg(data.length()));
  }

  if (checkOutputPictures && this->getNextFrameFromDecoder())
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
  auto fmt = this->lib.libvvdec_get_picture_chroma_format(this->decoder);
  if (fmt == LIBvvdec_CHROMA_UNKNOWN)
  {
    DEBUG_vvdec("decoderVVDec::copyImgToByteArray picture format is unknown");
    return;
  }
  auto nrPlanes = (fmt == LIBvvdec_CHROMA_400) ? 1u : 3u;

  bool outputTwoByte =
      (this->lib.libvvdec_get_picture_bit_depth(this->decoder, LIBvvdec_LUMA) > 8);
  if (nrPlanes > 1)
  {
    auto bitDepthU = this->lib.libvvdec_get_picture_bit_depth(this->decoder, LIBvvdec_CHROMA_U);
    auto bitDepthV = this->lib.libvvdec_get_picture_bit_depth(this->decoder, LIBvvdec_CHROMA_V);
    if ((outputTwoByte != (bitDepthU > 8)) || (outputTwoByte != (bitDepthV > 8)))
    {
      DEBUG_vvdec("decoderVVDec::copyImgToByteArray different bit depth in YUV components. This "
                   "is not supported.");
      return;
    }
  }

  // How many samples are in each component?
  const uint32_t width[2] = {
      this->lib.libvvdec_get_picture_width(this->decoder, LIBvvdec_LUMA),
      this->lib.libvvdec_get_picture_width(this->decoder, LIBvvdec_CHROMA_U)};
  const uint32_t height[2] = {
      this->lib.libvvdec_get_picture_height(this->decoder, LIBvvdec_LUMA),
      this->lib.libvvdec_get_picture_height(this->decoder, LIBvvdec_CHROMA_U)};

  if (this->lib.libvvdec_get_picture_width(this->decoder, LIBvvdec_CHROMA_V) != width[1] ||
      this->lib.libvvdec_get_picture_height(this->decoder, LIBvvdec_CHROMA_V) != height[1])
  {
    DEBUG_vvdec("decoderVVDec::copyImgToByteArray chroma components have different size");
    return;
  }

  auto outSizeLumaBytes   = width[0] * height[0] * (outputTwoByte ? 2 : 1);
  auto outSizeChromaBytes = (nrPlanes == 1) ? 0u : (width[1] * height[1] * (outputTwoByte ? 2 : 1));
  // How many bytes do we need in the output buffer?
  auto nrBytesOutput = (outSizeLumaBytes + outSizeChromaBytes * 2);
  DEBUG_vvdec("decoderVVDec::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < int(nrBytesOutput))
    dst.resize(nrBytesOutput);

  for (unsigned c = 0; c < nrPlanes; c++)
  {
    auto component = colorComponentMap[c];
    auto cIdx      = (c == 0 ? 0 : 1);

    auto plane      = this->lib.libvvdec_get_picture_plane(this->decoder, component);
    auto stride     = this->lib.libvvdec_get_picture_stride(this->decoder, component);
    auto widthBytes = width[cIdx] * (outputTwoByte ? 2 : 1);

    if (plane == nullptr)
    {
      DEBUG_vvdec("decoderVVDec::copyImgToByteArray unable to get plane for component %d", c);
      return;
    }

    unsigned char *restrict d = (unsigned char *)dst.data();
    if (c > 0)
      d += outSizeLumaBytes;
    if (c == 2)
      d += outSizeChromaBytes;

    for (unsigned y = 0; y < height[cIdx]; y++)
    {
      std::memcpy(d, plane, widthBytes);
      plane += stride;
      d += widthBytes;
    }
  }
}

QString decoderVVDec::getDecoderName() const
{
  return (decoderState == DecoderState::Error) ? "vvdec" : this->lib.libvvdec_get_version();
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
  return !testDecoder.errorInDecoder();
}

YUV_Internals::Subsampling decoderVVDec::convertFromInternalSubsampling(libvvdec_ChromaFormat fmt)
{
  if (fmt == LIBvvdec_CHROMA_400)
    return YUV_Internals::Subsampling::YUV_400;
  if (fmt == LIBvvdec_CHROMA_420)
    return YUV_Internals::Subsampling::YUV_420;
  if (fmt == LIBvvdec_CHROMA_422)
    return YUV_Internals::Subsampling::YUV_422;
  if (fmt == LIBvvdec_CHROMA_444)
    return YUV_Internals::Subsampling::YUV_444;

  return YUV_Internals::Subsampling::UNKNOWN;
}
