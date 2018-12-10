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

#include "decoderDav1d.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include "typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define DECODERDAV1D_DEBUG_OUTPUT 0
#if DECODERDAV1D_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if DECODERDAV1D_DEBUG_OUTPUT == 1
#define DEBUG_DAV1D if(!isCachingDecoder) qDebug
#elif DECODERDAV1D_DEBUG_OUTPUT == 2
#define DEBUG_DAV1D if(isCachingDecoder) qDebug
#elif DECODERDAV1D_DEBUG_OUTPUT == 3
#define DEBUG_DAV1D if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_DAV1D(fmt,...) ((void)0)
#endif

decoderDav1d_Functions::decoderDav1d_Functions() 
{ 
  memset(this, 0, sizeof(*this));
}

decoderDav1d::decoderDav1d(int signalID, bool cachingDecoder) :
  decoderBaseSingleLib(cachingDecoder)
{
  currentOutputBuffer.clear();

  // Libde265 can only decoder HEVC in YUV format
  rawFormat = raw_YUV;

  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("libde265File", "").toString());
  settings.endGroup();

  bool resetDecoder;
  setDecodeSignal(signalID, resetDecoder);
  allocateNewDecoder();
}

decoderDav1d::~decoderDav1d()
{
  if (decoder != nullptr)
  {
    // Free the decoder
    dav1d_close(&decoder);
    if (decoder != nullptr)
      DEBUG_DAV1D("Error closing the decoder. The close function should set the decoder pointer to NULL");
  }
}

void decoderDav1d::resetDecoder()
{
  // Delete decoder
  if (!decoder)
    return setError("Resetting the decoder failed. No decoder allocated.");

  dav1d_flush(decoder);

  decoderBase::resetDecoder();
  currentOutputBuffer.clear();
  decodedFrameWaiting = false;
  flushing = false;
}

void decoderDav1d::setDecodeSignal(int signalID, bool &decoderResetNeeded)
{
  decoderResetNeeded = false;
  if (signalID == decodeSignal)
    return;
  if (signalID >= 0 && signalID < nrSignalsSupported())
    decodeSignal = signalID;
  decoderResetNeeded = true;
}

void decoderDav1d::resolveLibraryFunctionPointers()
{
  //// Get/check function pointers
  //if (!resolve(de265_new_decoder, "de265_new_decoder")) return;
  //if (!resolve(de265_set_parameter_bool, "de265_set_parameter_bool")) return;
  //if (!resolve(de265_set_parameter_int, "de265_set_parameter_int")) return;
  //if (!resolve(de265_disable_logging, "de265_disable_logging")) return;
  //if (!resolve(de265_set_verbosity, "de265_set_verbosity")) return;
  //if (!resolve(de265_start_worker_threads, "de265_start_worker_threads")) return;
  //if (!resolve(de265_set_limit_TID, "de265_set_limit_TID")) return;
  //if (!resolve(de265_get_error_text, "de265_get_error_text")) return;
  //if (!resolve(de265_get_chroma_format, "de265_get_chroma_format")) return;
  //if (!resolve(de265_get_image_width, "de265_get_image_width")) return;
  //if (!resolve(de265_get_image_height, "de265_get_image_height")) return;
  //if (!resolve(de265_get_image_plane, "de265_get_image_plane")) return;
  //if (!resolve(de265_get_bits_per_pixel,"de265_get_bits_per_pixel")) return;
  //if (!resolve(de265_decode, "de265_decode")) return;
  //if (!resolve(de265_push_data, "de265_push_data")) return;
  //if (!resolve(de265_push_NAL, "de265_push_NAL")) return;
  //if (!resolve(de265_flush_data, "de265_flush_data")) return;
  //if (!resolve(de265_get_next_picture, "de265_get_next_picture")) return;
  //if (!resolve(de265_free_decoder, "de265_free_decoder")) return;
  //DEBUG_DAV1D("decoderLibde265::loadDecoderLibrary - decoding functions found");

  //// Get pointers to the internals/statistics functions (if present)
  //// If not, disable the statistics extraction. Normal decoding of the video will still work.

  //if (!resolveInternals(de265_internals_get_CTB_Info_Layout, "de265_internals_get_CTB_Info_Layout")) return;
  //if (!resolveInternals(de265_internals_get_CTB_sliceIdx, "de265_internals_get_CTB_sliceIdx")) return;
  //if (!resolveInternals(de265_internals_get_CB_Info_Layout, "de265_internals_get_CB_Info_Layout")) return;
  //if (!resolveInternals(de265_internals_get_CB_info, "de265_internals_get_CB_info")) return;
  //if (!resolveInternals(de265_internals_get_PB_Info_layout, "de265_internals_get_PB_Info_layout")) return;
  //if (!resolveInternals(de265_internals_get_PB_info, "de265_internals_get_PB_info")) return;
  //if (!resolveInternals(de265_internals_get_IntraDir_Info_layout, "de265_internals_get_IntraDir_Info_layout")) return;
  //if (!resolveInternals(de265_internals_get_intraDir_info, "de265_internals_get_intraDir_info")) return;
  //if (!resolveInternals(de265_internals_get_TUInfo_Info_layout, "de265_internals_get_TUInfo_Info_layout")) return;
  //if (!resolveInternals(de265_internals_get_TUInfo_info, "de265_internals_get_TUInfo_info")) return;
  //// All interbals functions were successfully retrieved
  //DEBUG_DAV1D("decoderLibde265::loadDecoderLibrary - statistics internals found");

  //// Get pointers to the functions for retrieving prediction/residual signals
  //if (!resolveInternals(de265_internals_get_image_plane, "de265_internals_get_image_plane")) return;
  //if (!resolveInternals(de265_internals_set_parameter_bool, "de265_internals_set_parameter_bool")) return;
  //// The prediction and residual signal can be obtained
  //nrSignals = 4;
  //DEBUG_DAV1D("decoderLibde265::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T decoderDav1d::resolve(T &fun, const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

template <typename T> T decoderDav1d::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void decoderDav1d::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_DAV1D("decoderDav1d::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Create new decoder object
  int err = dav1d_open(&decoder, &settings);
  if (err != 0)
  {
    decoderState = decoderError;
    setError("Error opening new decoder (dav1d_open)");
    return;
  }

  // The decoder is ready to receive data
  decoderBase::resetDecoder();
  currentOutputBuffer.clear();
  decodedFrameWaiting = false;
  flushing = false;
}

bool decoderDav1d::decodeNextFrame()
{
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_DAV1D("decoderLibde265::decodeNextFrame: Wrong decoder state.");
    return false;
  }
  if (decodedFrameWaiting)
  {
    decodedFrameWaiting = false;
    return true;
  }

  return decodeFrame();
}

bool decoderDav1d::decodeFrame()
{
  //int more = 1;
  //curImage = nullptr;
  //while (more && curImage == nullptr)
  //{
  //  more = 0;
  //  de265_error err = de265_decode(decoder, &more);

  //  if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA)
  //  {
  //    decoderState = decoderNeedsMoreData;
  //    return false;
  //  }
  //  else if (err != DE265_OK)
  //    return setErrorB("Error decoding (de265_decode)");

  //  curImage = de265_get_next_picture(decoder);
  //}

  //if (more == 0 && curImage == nullptr)
  //{
  //  // Decoding ended
  //  decoderState = decoderEndOfBitstream;
  //  return false;
  //}

  //if (curImage != nullptr)
  //{
  //  // Get the resolution / yuv format from the frame
  //  QSize s = QSize(de265_get_image_width(curImage, 0), de265_get_image_height(curImage, 0));
  //  if (!s.isValid())
  //    DEBUG_DAV1D("decoderLibde265::decodeFrame got invalid frame size");
  //  auto subsampling = convertFromInternalSubsampling(de265_get_chroma_format(curImage));
  //  if (subsampling == YUV_NUM_SUBSAMPLINGS)
  //    DEBUG_DAV1D("decoderLibde265::decodeFrame got invalid subsampling");
  //  int bitDepth = de265_get_bits_per_pixel(curImage, 0);
  //  if (bitDepth < 8 || bitDepth > 16)
  //    DEBUG_DAV1D("decoderLibde265::decodeFrame got invalid bit depth");

  //  if (!frameSize.isValid() && !formatYUV.isValid())
  //  {
  //    // Set the values
  //    frameSize = s;
  //    formatYUV = yuvPixelFormat(subsampling, bitDepth);
  //  }
  //  else
  //  {
  //    // Check the values against the previously set values
  //    if (frameSize != s)
  //      return setErrorB("Recieved a frame of different size");
  //    if (formatYUV.subsampling != subsampling)
  //      return setErrorB("Recieved a frame with different subsampling");
  //    if (formatYUV.bitsPerSample != bitDepth)
  //      return setErrorB("Recieved a frame with different bit depth");
  //  }
  //  DEBUG_DAV1D("decoderLibde265::decodeFrame Picture decoded");

  //  decoderState = decoderRetrieveFrames;
  //  currentOutputBuffer.clear();
  //  return true;
  //}
  return false;
}

QByteArray decoderDav1d::getRawFrameData()
{
  return QByteArray();
  //if (curImage == nullptr)
  //  return QByteArray();
  //if (decoderState != decoderRetrieveFrames)
  //{
  //  DEBUG_DAV1D("decoderLibde265::getRawFrameData: Wrong decoder state.");
  //  return QByteArray();
  //}

  //if (currentOutputBuffer.isEmpty())
  //{
  //  // Put image data into buffer
  //  copyImgToByteArray(curImage, currentOutputBuffer);
  //  DEBUG_DAV1D("decoderLibde265::getRawFrameData copied frame to buffer");

  //  if (retrieveStatistics)
  //    // Get the statistics from the image and put them into the statistics cache
  //    cacheStatistics(curImage);
  //}

  //return currentOutputBuffer;
}

bool decoderDav1d::pushData(QByteArray &data) 
{
  return false;
  //if (decoderState != decoderNeedsMoreData)
  //{
  //  DEBUG_DAV1D("decoderLibde265::pushData: Wrong decoder state.");
  //  return false;
  //}
  //if (flushing)
  //{
  //  DEBUG_DAV1D("decoderLibde265::pushData: Do not push data when flushing!");
  //  return false;
  //}

  //// Push the data to the decoder
  //if (data.size() > 0)
  //{
  //  // de265_push_NAL expects the NAL data without the start code
  //  int offset = 0;
  //  if (data.at(0) == (char)0 && data.at(1) == (char)0)
  //  {
  //    if (data.at(2) == (char)1)
  //      offset = 3;
  //    if (data.at(2) == (char)0 && data.at(3) == (char)1)
  //      offset = 4;
  //  }
  //  // de265_push_NAL will return either DE265_OK or DE265_ERROR_OUT_OF_MEMORY
  //  de265_error err = de265_push_NAL(decoder, data.data() + offset, data.size() - offset, 0, nullptr);
  //  DEBUG_DAV1D("decoderLibde265::pushData push data %d bytes%s%s", data.size(), err != DE265_OK ? " - err " : "", err != DE265_OK ? de265_get_error_text(err) : "");
  //  if (err != DE265_OK)
  //    return setErrorB("Error pushing data to decoder (de265_push_NAL): " + QString(de265_get_error_text(err)));
  //}
  //else
  //{
  //  // The input file is at the end. Switch to flushing mode.
  //  DEBUG_DAV1D("decoderLibde265::pushData input ended - flushing");
  //  de265_error err = de265_flush_data(decoder);
  //  if (err != DE265_OK)
  //    return setErrorB("Error switching to flushing mode.");
  //  flushing = true;
  //}

  //// Check for an available frame
  //if (decodeFrame())
  //  decodedFrameWaiting = true;

  //return true;
}

#if SSE_CONVERSION
void decoderDav1d::copyImgToByteArray(const Dav1dPicture *src, byteArrayAligned &dst)
#else
void decoderDav1d::copyImgToByteArray(const Dav1dPicture *src, QByteArray &dst)
#endif
{
  //// How many image planes are there?
  //de265_chroma cMode = de265_get_chroma_format(src);
  //int nrPlanes = (cMode == de265_chroma_mono) ? 1 : 3;

  //// At first get how many bytes we are going to write
  //int nrBytes = 0;
  //int stride;
  //for (int c = 0; c < nrPlanes; c++)
  //{
  //  int width = de265_get_image_width(src, c);
  //  int height = de265_get_image_height(src, c);
  //  int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;

  //  nrBytes += width * height * nrBytesPerSample;
  //}

  //DEBUG_DAV1D("decoderLibde265::copyImgToByteArray nrBytes %d", nrBytes);

  //// Is the output big enough?
  //if (dst.capacity() < nrBytes)
  //  dst.resize(nrBytes);

  //uint8_t *dst_c = (uint8_t*)dst.data();

  //// We can now copy from src to dst
  //for (int c = 0; c < nrPlanes; c++)
  //{
  //  const int width = de265_get_image_width(src, c);
  //  const int height = de265_get_image_height(src, c);
  //  const int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;
  //  const size_t widthInBytes = width * nrBytesPerSample;

  //  const uint8_t* img_c = nullptr;
  //  if (decodeSignal == 0)
  //    img_c = de265_get_image_plane(src, c, &stride);
  //  else if (decodeSignal == 1)
  //    img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_PREDICTION, c, &stride);
  //  else if (decodeSignal == 2)
  //    img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_RESIDUAL, c, &stride);
  //  else if (decodeSignal == 3)
  //    img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_TR_COEFF, c, &stride);

  //  if (img_c == nullptr)
  //    return;

  //  for (int y = 0; y < height; y++)
  //  {
  //    memcpy(dst_c, img_c, widthInBytes);
  //    img_c += stride;
  //    dst_c += widthInBytes;
  //  }
  //}
}

bool decoderDav1d::checkLibraryFile(QString libFilePath, QString &error)
{
  decoderDav1d testDecoder;

  // Try to load the library file
  testDecoder.library.setFileName(libFilePath);
  if (!testDecoder.library.load())
  {
    error = "Error opening QLibrary.";
    return false;
  }

  // Now let's see if we can retrive all the function pointers that we will need.
  // If this works, we can be fairly certain that this is a valid libde265 library.
  testDecoder.resolveLibraryFunctionPointers();
  error = testDecoder.decoderErrorString();
  return !testDecoder.errorInDecoder();
}

QStringList decoderDav1d::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList libNames = is_Q_OS_MAC ?
    QStringList() << "dav1d-internals.dylib" << "dav1d.dylib" :
    QStringList() << "dav1d-internals" << "dav1d";

  return libNames;
}
