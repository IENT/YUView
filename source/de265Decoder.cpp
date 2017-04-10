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

#include "de265Decoder.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include "typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define LIBDE265DECODER_DEBUG_OUTPUT 0
#if LIBDE265DECODER_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if LIBDE265DECODER_DEBUG_OUTPUT == 1
#define DEBUG_LIBDE265 if(!isCachingDecoder) qDebug
#elif LIBDE265DECODER_DEBUG_OUTPUT == 2
#define DEBUG_LIBDE265 if(isCachingDecoder) qDebug
#elif LIBDE265DECODER_DEBUG_OUTPUT == 3
#define DEBUG_LIBDE265 if (isCachingDecoder) qDebug("c:") else qDebug("i:") qDebug
#endif
#else
#define DEBUG_LIBDE265(fmt,...) ((void)0)
#endif

// Conversion from intra prediction mode to vector.
// Coordinates are in x,y with the axes going right and down.
#define VECTOR_SCALING 0.25
const int de265Decoder::vectorTable[35][2] = 
{
  {0,0}, {0,0},
  {32, -32},
  {32, -26}, {32, -21}, {32, -17}, { 32, -13}, { 32,  -9}, { 32, -5}, { 32, -2},
  {32,   0},
  {32,   2}, {32,   5}, {32,   9}, { 32,  13}, { 32,  17}, { 32, 21}, { 32, 26},
  {32,  32},
  {26,  32}, {21,  32}, {17,  32}, { 13,  32}, {  9,  32}, {  5, 32}, {  2, 32},
  {0,   32},
  {-2,  32}, {-5,  32}, {-9,  32}, {-13,  32}, {-17,  32}, {-21, 32}, {-26, 32},
  {-32, 32} 
};

de265Functions::de265Functions() { memset(this, 0, sizeof(*this)); }

de265Decoder::de265Decoder(int signalID, bool cachingDecoder) :
  decoderError(false),
  parsingError(false),
  internalsSupported(false),
  predAndResiSignalsSupported(false)
{
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  loadDecoderLibrary();

  decError = DE265_OK;
  decoder = nullptr;
  retrieveStatistics = false;
  statsCacheCurPOC = -1;
  isCachingDecoder = cachingDecoder;

  // Set the signal to decode (if supported)
  if (predAndResiSignalsSupported && signalID >= 0 && signalID <= 3)
    decodeSignal = signalID;
  else
    decodeSignal = 0;

  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;

  if (decoderError)
    // There was an internal error while loading/initializing the decoder. Abort.
    return;
  
  // Allocate a decoder
  allocateNewDecoder();
}

bool de265Decoder::openFile(QString fileName, de265Decoder *otherDecoder)
{ 
  // Open the file, decode the first frame and return if this was successfull.
  if (otherDecoder)
    parsingError = !annexBFile.openFile(fileName, false, &otherDecoder->annexBFile);
  else
    parsingError = !annexBFile.openFile(fileName);
  if (!decoderError)
    decoderError &= (!loadYUVFrameData(0).isEmpty());
  return !parsingError && !decoderError;
}

void de265Decoder::setError(const QString &reason)
{
  decoderError = true;
  errorString = reason;
}

QFunctionPointer de265Decoder::resolve(const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr) setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T de265Decoder::resolve(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolve(symbol));
}

template <typename T> T de265Decoder::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void de265Decoder::loadDecoderLibrary()
{
  // Try to load the libde265 library from the current working directory
  // Unfortunately relative paths like this do not work: (at least on windows)
  // library.setFileName(".\\libde265");

  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList const libNames =
        is_Q_OS_MAC ?
           QStringList() << "libde265-internals.dylib" << "libde265.dylib" :
           QStringList() << "libde265-internals" << "libde265";

  QStringList const libPaths = QStringList()
      << QDir::currentPath() + "/%1"
      << QDir::currentPath() + "/libde265/%1"
      << QCoreApplication::applicationDirPath() + "/%1"
      << QCoreApplication::applicationDirPath() + "/libde265/%1"
      << "%1"; // Try the system directories.

  bool libLoaded = false;
  for (auto &libName : libNames)
  {
    for (auto &libPath : libPaths)
    {
      library.setFileName(libPath.arg(libName));
      libraryPath = libPath.arg(libName);
      libLoaded = library.load();
      if (libLoaded)
        break;
    }
    if (libLoaded)
      break;
  }

  if (!libLoaded)
  {
    libraryPath.clear();
    return setError(library.errorString());
  }

  DEBUG_LIBDE265("de265Decoder::loadDecoderLibrary - %s", libraryPath);

  // Get/check function pointers
  if (!resolve(de265_new_decoder, "de265_new_decoder")) return;
  if (!resolve(de265_set_parameter_bool, "de265_set_parameter_bool")) return;
  if (!resolve(de265_set_parameter_int, "de265_set_parameter_int")) return;
  if (!resolve(de265_disable_logging, "de265_disable_logging")) return;
  if (!resolve(de265_set_verbosity, "de265_set_verbosity")) return;
  if (!resolve(de265_start_worker_threads, "de265_start_worker_threads")) return;
  if (!resolve(de265_set_limit_TID, "de265_set_limit_TID")) return;
  if (!resolve(de265_get_error_text, "de265_get_error_text")) return;
  if (!resolve(de265_get_chroma_format, "de265_get_chroma_format")) return;
  if (!resolve(de265_get_image_width, "de265_get_image_width")) return;
  if (!resolve(de265_get_image_height, "de265_get_image_height")) return;
  if (!resolve(de265_get_image_plane, "de265_get_image_plane")) return;
  if (!resolve(de265_get_bits_per_pixel,"de265_get_bits_per_pixel")) return;
  if (!resolve(de265_decode, "de265_decode")) return;
  if (!resolve(de265_push_data, "de265_push_data")) return;
  if (!resolve(de265_flush_data, "de265_flush_data")) return;
  if (!resolve(de265_get_next_picture, "de265_get_next_picture")) return;
  if (!resolve(de265_free_decoder, "de265_free_decoder")) return;
  DEBUG_LIBDE265("de265Decoder::loadDecoderLibrary - decoding functions found");

  // Get pointers to the internals/statistics functions (if present)
  // If not, disable the statistics extraction. Normal decoding of the video will still work.

  if (!resolveInternals(de265_internals_get_CTB_Info_Layout, "de265_internals_get_CTB_Info_Layout")) return;
  if (!resolveInternals(de265_internals_get_CTB_sliceIdx, "de265_internals_get_CTB_sliceIdx")) return;
  if (!resolveInternals(de265_internals_get_CB_Info_Layout, "de265_internals_get_CB_Info_Layout")) return;
  if (!resolveInternals(de265_internals_get_CB_info, "de265_internals_get_CB_info")) return;
  if (!resolveInternals(de265_internals_get_PB_Info_layout, "de265_internals_get_PB_Info_layout")) return;
  if (!resolveInternals(de265_internals_get_PB_info, "de265_internals_get_PB_info")) return;
  if (!resolveInternals(de265_internals_get_IntraDir_Info_layout, "de265_internals_get_IntraDir_Info_layout")) return;
  if (!resolveInternals(de265_internals_get_intraDir_info, "de265_internals_get_intraDir_info")) return;
  if (!resolveInternals(de265_internals_get_TUInfo_Info_layout, "de265_internals_get_TUInfo_Info_layout")) return;
  if (!resolveInternals(de265_internals_get_TUInfo_info, "de265_internals_get_TUInfo_info")) return;
  // All interbals functions were successfully retrieved
  internalsSupported = true;
  DEBUG_LIBDE265("de265Decoder::loadDecoderLibrary - statistics internals found");
  
  // Get pointers to the functions for retrieving prediction/residual signals
  if (!resolveInternals(de265_internals_get_image_plane, "de265_internals_get_image_plane")) return;
  if (!resolveInternals(de265_internals_set_parameter_bool, "de265_internals_set_parameter_bool")) return;
  // The prediction and residual signal can be obtained
  predAndResiSignalsSupported = true;
  DEBUG_LIBDE265("de265Decoder::loadDecoderLibrary - prediction/residual internals found");
}

void de265Decoder::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_LIBDE265("de265Decoder::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Create new decoder object
  decoder = de265_new_decoder();

  // Set some decoder parameters
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_SUPPRESS_FAULTY_PICTURES, false);

  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_SAO, false);

  // Set retrieval of the right component
  if (predAndResiSignalsSupported)
  {
    if (decodeSignal == 1)
      de265_internals_set_parameter_bool(decoder, DE265_INTERNALS_DECODER_PARAM_SAVE_PREDICTION, true);
    else if (decodeSignal == 2)
      de265_internals_set_parameter_bool(decoder, DE265_INTERNALS_DECODER_PARAM_SAVE_RESIDUAL, true);
    else if (decodeSignal == 3)
      de265_internals_set_parameter_bool(decoder, DE265_INTERNALS_DECODER_PARAM_SAVE_TR_COEFF, true);
  }

  // You could disable SSE acceleration ... not really recommended
  //de265_set_parameter_int(decoder, DE265_DECODER_PARAM_ACCELERATION_CODE, de265_acceleration_SCALAR);

  de265_disable_logging();

  // Verbosity level (0...3(highest))
  de265_set_verbosity(0);

  // Set the number of decoder threads. Libde265 can use wavefronts to utilize these.
  decError = de265_start_worker_threads(decoder, 8);

  // The highest temporal ID to decode. Set this to very high (all) by default.
  de265_set_limit_TID(decoder, 100);
}

void de265Decoder::setDecodeSignal(int signalID)
{
  if (!predAndResiSignalsSupported)
    return;

  DEBUG_LIBDE265("de265Decoder::setDecodeSignal old %d new %d", decodeSignal, signalID);

  if (signalID != decodeSignal)
  {
    // A different signal was selected
    decodeSignal = signalID;

    // We will have to decode the current frame again to get the internals/statistics
    // This can be done like this:
    currentOutputBufferFrameIndex = -1;
    // Now the next call to loadYUVFrameData will load the frame again...
  }
}

QByteArray de265Decoder::loadYUVFrameData(int frameIdx)
{
  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    return currentOutputBuffer;
  }

  DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Start request %d", frameIdx);

  // We have to decode the requested frame.
  bool seeked = false;
  QByteArray parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and start decoding from there.
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);

    DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Seek to %d", seekFrameIdx);
    parameterSets = annexBFile.seekToFrameNumber(seekFrameIdx);
    currentOutputBufferFrameIndex = seekFrameIdx - 1;
    seeked = true;
  }
  else if (frameIdx > currentOutputBufferFrameIndex+2)
  {
    // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
    // Check if there is a random access point closer to the requested frame than the position that we are at right now.
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);
    if (seekFrameIdx > currentOutputBufferFrameIndex)
    {
      // Yes we can (and should) seek ahead in the file
      DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Seek to %d", seekFrameIdx);
      parameterSets = annexBFile.seekToFrameNumber(seekFrameIdx);
      currentOutputBufferFrameIndex = seekFrameIdx - 1;
      seeked = true;
    }
  }

  if (seeked)
  {
    // Reset the decoder and feed the parameter sets to it.
    // Then start normal decoding

    if (parameterSets.size() == 0)
      return QByteArray();

    // Delete decoder
    de265_error err = de265_free_decoder(decoder);
    if (err != DE265_OK)
    {
      // Freeing the decoder failed.
      if (decError != err)
        decError = err;
      return QByteArray();
    }

    decoder = nullptr;

    // Create new decoder
    allocateNewDecoder();

    // Feed the parameter sets
    err = de265_push_data(decoder, parameterSets.data(), parameterSets.size(), 0, nullptr);
  }

  // Perform the decoding right now blocking the main thread.
  // Decode frames until we receive the one we are looking for.
  de265_error err;
  while (true)
  {
    int more = 1;
    while (more)
    {
      more = 0;

      err = de265_decode(decoder, &more);
      while (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && !annexBFile.atEnd())
      {
        // The decoder needs more data. Get it from the file.
        QByteArray chunk = annexBFile.getRemainingBuffer_Update();

        // Push the data to the decoder
        if (chunk.size() > 0)
        {
          err = de265_push_data(decoder, chunk.data(), chunk.size(), 0, nullptr);
          DEBUG_LIBDE265("de265Decoder::loadYUVFrameData push data %d bytes - err %s", chunk.size(), de265_get_error_text(err));
          if (err != DE265_OK && err != DE265_ERROR_WAITING_FOR_INPUT_DATA)
          {
            // An error occurred
            if (decError != err)
              decError = err;
            DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Error %s", de265_get_error_text(err));
            return QByteArray();
          }
        }

        if (annexBFile.atEnd())
          // The file ended.
          err = de265_flush_data(decoder);
      }

      if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && annexBFile.atEnd())
      {
        // The decoder wants more data but there is no more file.
        // We found the end of the sequence. Get the remaining frames from the decoder until
        // more is 0.
        DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Waiting for input bit file at end.");
      }
      else if (err != DE265_OK)
      {
        // Something went wrong
        more = 0;
        DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Error %s", de265_get_error_text(err));
        break;
      }

      const de265_image* img = de265_get_next_picture(decoder);
      if (img)
      {
        // We have received an output image
        currentOutputBufferFrameIndex++;
        DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Picture decoded %d", currentOutputBufferFrameIndex);

        // First update the chroma format and frame size
        pixelFormat = de265_get_chroma_format(img);
        nrBitsC0 = de265_get_bits_per_pixel(img, 0);
        frameSize = QSize(de265_get_image_width(img, 0), de265_get_image_height(img, 0));

        if (currentOutputBufferFrameIndex == frameIdx)
        {
          // This is the frame that we want to decode
            
          // Put image data into buffer
          copyImgToByteArray(img, currentOutputBuffer);

          if (retrieveStatistics)
          {
            // Get the statistics from the image and put them into the statistics cache
            cacheStatistics(img);

            // The cache now contains the statistics for iPOC
            statsCacheCurPOC = currentOutputBufferFrameIndex;
          }

          // Picture decoded
          DEBUG_LIBDE265("de265Decoder::loadYUVFrameData decoded the requested frame %d", currentOutputBufferFrameIndex);
            
          return currentOutputBuffer;
        }
      }
    }

    if (err != DE265_OK)
    {
      // The encoding loop ended because of an error
      if (decError != err)
        decError = err;

      DEBUG_LIBDE265("de265Decoder::loadYUVFrameData Error %s", de265_get_error_text(err));
      return QByteArray();
    }
    if (more == 0)
    {
      // The loop ended because there is nothing more to decode but no error occurred.
      // We are at the end of the sequence.

      // This should not happen because before decoding, we check if the frame to decode is in the list of nal units that will be decoded.
      DEBUG_LIBDE265("de265Decoder::loadYUVFrameData more == 0");

      return QByteArray();
    }
  }
  
  return QByteArray();
}

#if SSE_CONVERSION
void de265Decoder::copyImgToByteArray(const de265_image *src, byteArrayAligned &dst)
#else
void de265Decoder::copyImgToByteArray(const de265_image *src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  de265_chroma cMode = de265_get_chroma_format(src);
  int nrPlanes = (cMode == de265_chroma_mono) ? 1 : 3;

  // At first get how many bytes we are going to write
  int nrBytes = 0;
  int stride;
  for (int c = 0; c < nrPlanes; c++)
  {
    int width = de265_get_image_width(src, c);
    int height = de265_get_image_height(src, c);
    int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;

    nrBytes += width * height * nrBytesPerSample;
  }

  DEBUG_LIBDE265("de265Decoder::copyImgToByteArray nrBytes %d", nrBytes);

  // Is the output big enough?
  if (dst.capacity() < nrBytes)
    dst.resize(nrBytes);

  // We can now copy from src to dst
  char* dst_c = dst.data();
  for (int c = 0; c < nrPlanes; c++)
  {
    const uint8_t* img_c = nullptr;
    if (decodeSignal == 0 || !predAndResiSignalsSupported)
      img_c = de265_get_image_plane(src, c, &stride);
    else if (decodeSignal == 1)
      img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_PREDICTION, c, &stride);
    else if (decodeSignal == 2)
      img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_RESIDUAL, c, &stride);
    else if (decodeSignal == 3)
      img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_TR_COEFF, c, &stride);
    
    if (img_c == nullptr)
      return;

    int width = de265_get_image_width(src, c);
    int height = de265_get_image_height(src, c);
    int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;
    size_t size = width * nrBytesPerSample;

    for (int y = 0; y < height; y++)
    {
      memcpy(dst_c, img_c, size);
      img_c += stride;
      dst_c += size;
    }
  }
}

/* Convert the de265_chroma format to a YUVCPixelFormatType and return it.
*/
yuvPixelFormat de265Decoder::getYUVPixelFormat()
{
  if (pixelFormat == de265_chroma_mono)
    return yuvPixelFormat(YUV_400, nrBitsC0);
  else if (pixelFormat == de265_chroma_420)
    return yuvPixelFormat(YUV_420, nrBitsC0);
  else if (pixelFormat == de265_chroma_422)
    return yuvPixelFormat(YUV_422, nrBitsC0);
  else if (pixelFormat == de265_chroma_444)
    return yuvPixelFormat(YUV_444, nrBitsC0);
  return yuvPixelFormat();
}

void de265Decoder::cacheStatistics(const de265_image *img)
{
  if (!wrapperInternalsSupported())
    return;

  // Clear the local statistics cache
  curPOCStats.clear();

  /// --- CTB internals/statistics
  int widthInCTB, heightInCTB, log2CTBSize;
  de265_internals_get_CTB_Info_Layout(img, &widthInCTB, &heightInCTB, &log2CTBSize);
  int ctb_size = 1 << log2CTBSize;	// width and height of each CTB

                                    // Save Slice index
  {
    QScopedArrayPointer<uint16_t> tmpArr(new uint16_t[ widthInCTB * heightInCTB ]);
    de265_internals_get_CTB_sliceIdx(img, tmpArr.data());
    for (int y = 0; y < heightInCTB; y++)
      for (int x = 0; x < widthInCTB; x++)
      {
        uint16_t val = tmpArr[ y * widthInCTB + x ];
        curPOCStats[0].addBlockValue(x*ctb_size, y*ctb_size, ctb_size, ctb_size, (int)val);
      }
  }

  /// --- CB internals/statistics (part Size, prediction mode, PCM flag, CU trans_quant_bypass_flag)

  const int iPOC = currentOutputBufferFrameIndex;

  // Get CB info array layout from image
  int widthInCB, heightInCB, log2CBInfoUnitSize;
  de265_internals_get_CB_Info_Layout(img, &widthInCB, &heightInCB, &log2CBInfoUnitSize);
  int cb_infoUnit_size = 1 << log2CBInfoUnitSize;
  // Get CB info from image
  QScopedArrayPointer<uint16_t> cbInfoArr(new uint16_t[widthInCB * heightInCB]);
  de265_internals_get_CB_info(img, cbInfoArr.data());

  // Get PB array layout from image
  int widthInPB, heightInPB, log2PBInfoUnitSize;
  de265_internals_get_PB_Info_layout(img, &widthInPB, &heightInPB, &log2PBInfoUnitSize);
  int pb_infoUnit_size = 1 << log2PBInfoUnitSize;

  // Get PB info from image
  QScopedArrayPointer<int16_t> refPOC0(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> refPOC1(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec0_x(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec0_y(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec1_x(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec1_y(new int16_t[widthInPB*heightInPB]);
  de265_internals_get_PB_info(img, refPOC0.data(), refPOC1.data(), vec0_x.data(), vec0_y.data(), vec1_x.data(), vec1_y.data());

  // Get intra prediction mode (intra direction) layout from image
  int widthInIntraDirUnits, heightInIntraDirUnits, log2IntraDirUnitsSize;
  de265_internals_get_IntraDir_Info_layout(img, &widthInIntraDirUnits, &heightInIntraDirUnits, &log2IntraDirUnitsSize);
  int intraDir_infoUnit_size = 1 << log2IntraDirUnitsSize;

  // Get intra prediction mode (intra direction) from image
  QScopedArrayPointer<uint8_t> intraDirY(new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits]);
  QScopedArrayPointer<uint8_t> intraDirC(new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits]);
  de265_internals_get_intraDir_info(img, intraDirY.data(), intraDirC.data());

  // Get TU info array layout
  int widthInTUInfoUnits, heightInTUInfoUnits, log2TUInfoUnitSize;
  de265_internals_get_TUInfo_Info_layout(img, &widthInTUInfoUnits, &heightInTUInfoUnits, &log2TUInfoUnitSize);
  int tuInfo_unit_size = 1 << log2TUInfoUnitSize;

  // Get TU info
  QScopedArrayPointer<uint8_t> tuInfo(new uint8_t[widthInTUInfoUnits*heightInTUInfoUnits]);
  de265_internals_get_TUInfo_info(img, tuInfo.data());

  for (int y = 0; y < heightInCB; y++)
  {
    for (int x = 0; x < widthInCB; x++)
    {
      uint16_t val = cbInfoArr[ y * widthInCB + x ];

      uint8_t log2_cbSize = (val & 7);	 // Extract lowest 3 bits;

      if (log2_cbSize > 0) {
        // We are in the top left position of a CB.

        // Get values of this CB
        uint8_t cbSizePix = 1 << log2_cbSize;  // Size (w,h) in pixels
        int cbPosX = x * cb_infoUnit_size;	   // Position of this CB in pixels
        int cbPosY = y * cb_infoUnit_size;
        uint8_t partMode = ((val >> 3) & 7);   // Extract next 3 bits (part size);
        uint8_t predMode = ((val >> 6) & 3);   // Extract next 2 bits (prediction mode);
        bool    pcmFlag  = (val & 256);		   // Next bit (PCM flag)
        bool    tqBypass = (val & 512);        // Next bit (TransQuant bypass flag)

                                               // Set part mode (ID 1)
        curPOCStats[1].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, partMode);

        // Set prediction mode (ID 2)
        curPOCStats[2].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, predMode);

        // Set PCM flag (ID 3)
        curPOCStats[3].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, pcmFlag);

        // Set transQuant bypass flag (ID 4)
        curPOCStats[4].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, tqBypass);

        if (predMode != 0)
        {
          // For each of the prediction blocks set some info

          int numPB = (partMode == 0) ? 1 : (partMode == 3) ? 4 : 2;
          for (int i=0; i<numPB; i++)
          {
            // Get pb position/size
            int pbSubX, pbSubY, pbW, pbH;
            getPBSubPosition(partMode, cbSizePix, i, &pbSubX, &pbSubY, &pbW, &pbH);
            int pbX = cbPosX + pbSubX;
            int pbY = cbPosY + pbSubY;

            // Get index for this xy position in pb_info array
            int pbIdx = (pbY / pb_infoUnit_size) * widthInPB + (pbX / pb_infoUnit_size);

            // Add ref index 0 (ID 5)
            int16_t ref0 = refPOC0[pbIdx];
            if (ref0 != -1)
              curPOCStats[5].addBlockValue(pbX, pbY, pbW, pbH, ref0-iPOC);

            // Add ref index 1 (ID 6)
            int16_t ref1 = refPOC1[pbIdx];
            if (ref1 != -1)
              curPOCStats[6].addBlockValue(pbX, pbY, pbW, pbH, ref1-iPOC);

            // Add motion vector 0 (ID 7)
            if (ref0 != -1)
              curPOCStats[7].addBlockVector(pbX, pbY, pbW, pbH, vec0_x[pbIdx], vec0_y[pbIdx]);

            // Add motion vector 1 (ID 8)
            if (ref1 != -1)
              curPOCStats[8].addBlockVector(pbX, pbY, pbW, pbH, vec1_x[pbIdx], vec1_y[pbIdx]);
          }
        }
        else if (predMode == 0)
        {
          // Get index for this xy position in the intraDir array
          int intraDirIdx = (cbPosY / intraDir_infoUnit_size) * widthInIntraDirUnits + (cbPosX / intraDir_infoUnit_size);

          // Set Intra prediction direction Luma (ID 9)
          int intraDirLuma = intraDirY[intraDirIdx];
          if (intraDirLuma <= 34)
          {
            curPOCStats[9].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, intraDirLuma);

            if (intraDirLuma >= 2)
            {
              // Set Intra prediction direction Luma (ID 9) as vector
              int vecX = (float)vectorTable[intraDirLuma][0] * cbSizePix / 4;
              int vecY = (float)vectorTable[intraDirLuma][1] * cbSizePix / 4;
              curPOCStats[9].addBlockVector(cbPosX, cbPosY, cbSizePix, cbSizePix, vecX, vecY);
            }
          }

          // Set Intra prediction direction Chroma (ID 10)
          int intraDirChroma = intraDirC[intraDirIdx];
          if (intraDirChroma <= 34)
          {
            curPOCStats[10].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, intraDirChroma);

            if (intraDirChroma >= 2)
            {
              // Set Intra prediction direction Chroma (ID 10) as vector
              int vecX = (float)vectorTable[intraDirChroma][0] * cbSizePix / 4;
              int vecY = (float)vectorTable[intraDirChroma][1] * cbSizePix / 4;
              curPOCStats[10].addBlockVector(cbPosX, cbPosY, cbSizePix, cbSizePix, vecX, vecY);
            }
          }
        }

        // Walk into the TU tree
        int tuIdx = (cbPosY / tuInfo_unit_size) * widthInTUInfoUnits + (cbPosX / tuInfo_unit_size);
        cacheStatistics_TUTree_recursive(tuInfo.data(), widthInTUInfoUnits, tuInfo_unit_size, iPOC, tuIdx, cbSizePix / tuInfo_unit_size, 0);
      }
    }
  }
}

void de265Decoder::getPBSubPosition(int partMode, int cbSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const
{
  // Get the position/width/height of the PB
  if (partMode == 0) // PART_2Nx2N
  {
    *pbW = cbSizePix;
    *pbH = cbSizePix;
    *pbX = 0;
    *pbY = 0;
  }
  else if (partMode == 1) // PART_2NxN
  {
    *pbW = cbSizePix;
    *pbH = cbSizePix / 2;
    *pbX = 0;
    *pbY = (pbIdx == 0) ? 0 : cbSizePix / 2;
  }
  else if (partMode == 2) // PART_Nx2N
  {
    *pbW = cbSizePix / 2;
    *pbH = cbSizePix;
    *pbX = (pbIdx == 0) ? 0 : cbSizePix / 2;
    *pbY = 0;
  }
  else if (partMode == 3) // PART_NxN
  {
    *pbW = cbSizePix / 2;
    *pbH = cbSizePix / 2;
    *pbX = (pbIdx == 0 || pbIdx == 2) ? 0 : cbSizePix / 2;
    *pbY = (pbIdx == 0 || pbIdx == 1) ? 0 : cbSizePix / 2;
  }
  else if (partMode == 4) // PART_2NxnU
  {
    *pbW = cbSizePix;
    *pbH = (pbIdx == 0) ? cbSizePix / 4 : cbSizePix / 4 * 3;
    *pbX = 0;
    *pbY = (pbIdx == 0) ? 0 : cbSizePix / 4;
  }
  else if (partMode == 5) // PART_2NxnD
  {
    *pbW = cbSizePix;
    *pbH = (pbIdx == 0) ? cbSizePix / 4 * 3 : cbSizePix / 4;
    *pbX = 0;
    *pbY = (pbIdx == 0) ? 0 : cbSizePix / 4 * 3;
  }
  else if (partMode == 6) // PART_nLx2N
  {
    *pbW = (pbIdx == 0) ? cbSizePix / 4 : cbSizePix / 4 * 3;
    *pbH = cbSizePix;
    *pbX = (pbIdx == 0) ? 0 : cbSizePix / 4;
    *pbY = 0;
  }
  else if (partMode == 7) // PART_nRx2N
  {
    *pbW = (pbIdx == 0) ? cbSizePix / 4 * 3 : cbSizePix / 4;
    *pbH = cbSizePix;
    *pbX = (pbIdx == 0) ? 0 : cbSizePix / 4 * 3;
    *pbY = 0;
  }
}

/* Walk into the TU tree and set the tree depth as a statistic value if the TU is not further split
* \param tuInfo: The tuInfo array
* \param tuInfoWidth: The number of TU units per line in the tuInfo array
* \param tuUnitSizePix: The size of one TU unit in pixels
* \param iPOC: The current POC
* \param tuIdx: The top left index of the currently handled TU in tuInfo
* \param tuWidth_units: The WIdth of the TU in units
* \param trDepth: The current transform tree depth
*/
void de265Decoder::cacheStatistics_TUTree_recursive(uint8_t *const tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth)
{
  // Check if the TU is further split.
  if (tuInfo[tuIdx] & (1 << trDepth))
  {
    // The transform is split further
    int yOffset = (tuWidth_units / 2) * tuInfoWidth;
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx                              , tuWidth_units / 2, trDepth+1);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx           + tuWidth_units / 2, tuWidth_units / 2, trDepth+1);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset                    , tuWidth_units / 2, trDepth+1);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset + tuWidth_units / 2, tuWidth_units / 2, trDepth+1);
  }
  else
  {
    // The transform is not split any further. Add the TU depth to the statistics (ID 11)
    int tuWidth = tuWidth_units * tuUnitSizePix;
    int posX_units = tuIdx % tuInfoWidth;
    int posY_units = tuIdx / tuInfoWidth;
    curPOCStats[11].addBlockValue(posX_units * tuUnitSizePix, posY_units * tuUnitSizePix, tuWidth, tuWidth, trDepth);
  }
}

statisticsData de265Decoder::getStatisticsData(int frameIdx, int typeIdx)
{
  if (!retrieveStatistics)
  {
    retrieveStatistics = true;
  }

  if (frameIdx != statsCacheCurPOC)
  {
    if (currentOutputBufferFrameIndex == frameIdx)
      // We will have to decode the current frame again to get the internals/statistics
      // This can be done like this:
      currentOutputBufferFrameIndex++;

    loadYUVFrameData(frameIdx);
  }

  return curPOCStats[typeIdx];
}

bool de265Decoder::reloadItemSource()
{
  if (decoderError)
    // Nothing is working, so there is nothing to reset.
    return false;

  // Reset the de265Decoder variables/buffers.
  decError = DE265_OK;
  statsCacheCurPOC = -1;
  currentOutputBufferFrameIndex = -1;

  // Re-open the input file. This will reload the bitstream as if it was completely unknown.
  QString fileName = annexBFile.absoluteFilePath();
  parsingError = annexBFile.openFile(fileName);
  return parsingError;
}