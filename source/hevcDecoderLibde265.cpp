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

#include "hevcDecoderLibde265.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include "typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define HEVCDECODERLIBDE265_DEBUG_OUTPUT 0
#if HEVCDECODERLIBDE265_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if HEVCDECODERLIBDE265_DEBUG_OUTPUT == 1
#define DEBUG_LIBDE265 if(!isCachingDecoder) qDebug
#elif HEVCDECODERLIBDE265_DEBUG_OUTPUT == 2
#define DEBUG_LIBDE265 if(isCachingDecoder) qDebug
#elif HEVCDECODERLIBDE265_DEBUG_OUTPUT == 3
#define DEBUG_LIBDE265 if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_LIBDE265(fmt,...) ((void)0)
#endif

hevcDecoderLibde265_Functions::hevcDecoderLibde265_Functions() { memset(this, 0, sizeof(*this)); }

hevcDecoderLibde265::hevcDecoderLibde265(int signalID, bool cachingDecoder) :
  decoderBase(cachingDecoder)
{
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("libde265File", "").toString());
  settings.endGroup();

  decError = DE265_OK;
  decoder = nullptr;

  // Set the signal to decode (if supported)
  if (signalID >= 0 && signalID <= 3)
    decodeSignal = signalID;
  else
    decodeSignal = 0;
  
  // Create a new fileSource
  annexBFile.reset(new fileSourceHEVCAnnexBFile);

  // Allocate a decoder
  if (!decoderError)
    allocateNewDecoder();
}

hevcDecoderLibde265::hevcDecoderLibde265() :
  decoderBase(false)
{
  decError = DE265_OK;
  decoder = nullptr;
}

hevcDecoderLibde265::~hevcDecoderLibde265()
{
  if (decoder != nullptr)
    // Free the decoder
    de265_free_decoder(decoder);
}

bool hevcDecoderLibde265::openFile(QString fileName, decoderBase *otherDecoder)
{ 
  // Open the file, decode the first frame and return if this was successfull.
  if (otherDecoder)
    parsingError = !annexBFile->openFile(fileName, false, otherDecoder->getFileSource());
  else
    parsingError = !annexBFile->openFile(fileName);
  
  if (!parsingError)
  {
    // Once the annexB file is opened, the frame size and the YUV format is known.
    fileSourceHEVCAnnexBFile *hevcFile = dynamic_cast<fileSourceHEVCAnnexBFile*>(annexBFile.data());
    frameSize = hevcFile->getSequenceSizeSamples();
    nrBitsC0 = hevcFile->getSequenceBitDepth(Luma);
    pixelFormat = hevcFile->getSequenceSubsampling();
  }

  return !parsingError && !decoderError;
}

QStringList hevcDecoderLibde265::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList names = 
    is_Q_OS_MAC ?
    QStringList() << "libde265-internals.dylib" << "libde265.dylib" :
    QStringList() << "libde265-internals" << "libde265";

  return names;
}

void hevcDecoderLibde265::resolveLibraryFunctionPointers()
{
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
  DEBUG_LIBDE265("hevcDecoderLibde265::loadDecoderLibrary - decoding functions found");

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
  DEBUG_LIBDE265("hevcDecoderLibde265::loadDecoderLibrary - statistics internals found");
  
  // Get pointers to the functions for retrieving prediction/residual signals
  if (!resolveInternals(de265_internals_get_image_plane, "de265_internals_get_image_plane")) return;
  if (!resolveInternals(de265_internals_set_parameter_bool, "de265_internals_set_parameter_bool")) return;
  // The prediction and residual signal can be obtained
  nrSignalsSupported = 3;
  DEBUG_LIBDE265("hevcDecoderLibde265::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T hevcDecoderLibde265::resolve(T &fun, const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

template <typename T> T hevcDecoderLibde265::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void hevcDecoderLibde265::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_LIBDE265("hevcDecoderLibde265::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Create new decoder object
  decoder = de265_new_decoder();

  // Set some decoder parameters
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_SUPPRESS_FAULTY_PICTURES, false);

  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_SAO, false);

  // Set retrieval of the right component
  if (nrSignalsSupported > 0)
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

QByteArray hevcDecoderLibde265::loadYUVFrameData(int frameIdx)
{
  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    return currentOutputBuffer;
  }

  DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Start request %d", frameIdx);

  // We have to decode the requested frame.
  bool seeked = false;
  QList<QByteArray> parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and start decoding from there.
    int seekFrameIdx = annexBFile->getClosestSeekableFrameNumber(frameIdx);

    DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Seek to %d", seekFrameIdx);
    parameterSets = annexBFile->seekToFrameNumber(seekFrameIdx);
    currentOutputBufferFrameIndex = seekFrameIdx - 1;
    seeked = true;
  }
  else if (frameIdx > currentOutputBufferFrameIndex+2)
  {
    // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
    // Check if there is a random access point closer to the requested frame than the position that we are at right now.
    int seekFrameIdx = annexBFile->getClosestSeekableFrameNumber(frameIdx);
    if (seekFrameIdx > currentOutputBufferFrameIndex)
    {
      // Yes we can (and should) seek ahead in the file
      DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Seek to %d", seekFrameIdx);
      parameterSets = annexBFile->seekToFrameNumber(seekFrameIdx);
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
    for (QByteArray ps : parameterSets)
      err = de265_push_data(decoder, ps.data(), ps.size(), 0, nullptr);
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
      while (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && !annexBFile->atEnd())
      {
        // The decoder needs more data. Get it from the file.
        QByteArray chunk = annexBFile->getRemainingBuffer_Update();

        // Push the data to the decoder
        if (chunk.size() > 0)
        {
          err = de265_push_data(decoder, chunk.data(), chunk.size(), 0, nullptr);
          DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData push data %d bytes - err %s", chunk.size(), de265_get_error_text(err));
          if (err != DE265_OK && err != DE265_ERROR_WAITING_FOR_INPUT_DATA)
          {
            // An error occurred
            if (decError != err)
              decError = err;
            DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Error %s", de265_get_error_text(err));
            return QByteArray();
          }
        }

        if (annexBFile->atEnd())
          // The file ended.
          err = de265_flush_data(decoder);
      }

      if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && annexBFile->atEnd())
      {
        // The decoder wants more data but there is no more file.
        // We found the end of the sequence. Get the remaining frames from the decoder until
        // more is 0.
        DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Waiting for input bit file at end.");
      }
      else if (err != DE265_OK)
      {
        // Something went wrong
        more = 0;
        DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Error %s", de265_get_error_text(err));
        break;
      }

      const de265_image* img = de265_get_next_picture(decoder);
      if (img)
      {
        // We have received an output image
        currentOutputBufferFrameIndex++;
        DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Picture decoded %d", currentOutputBufferFrameIndex);

        // Check if the chroma format and the frame size matches the already set values (these were read from the annex B file).
        de265_chroma fmt = de265_get_chroma_format(img);
        if ((fmt == de265_chroma_mono && pixelFormat != YUV_400) ||
            (fmt == de265_chroma_420 && pixelFormat != YUV_420) ||
            (fmt == de265_chroma_422 && pixelFormat != YUV_422) ||
            (fmt == de265_chroma_444 && pixelFormat != YUV_444))
          DEBUG_LIBDE265("hevcNextGenDecoderJEM::loadYUVFrameData recieved frame has different chroma format. Set: %d Pic: %d", pixelFormat, fmt);
        int bits = de265_get_bits_per_pixel(img, 0);
        if (bits != nrBitsC0)
          DEBUG_LIBDE265("hevcNextGenDecoderJEM::loadYUVFrameData recieved frame has different bit depth. Set: %d Pic: %d", nrBitsC0, bits);
        QSize picSize = QSize(de265_get_image_width(img, 0), de265_get_image_height(img, 0));
        if (picSize != frameSize)
          DEBUG_LIBDE265("hevcNextGenDecoderJEM::loadYUVFrameData recieved frame has different size. Set: %dx%d Pic: %dx%d", frameSize.width(), frameSize.height(), picSize.width(), picSize.height());

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
          DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData decoded the requested frame %d", currentOutputBufferFrameIndex);
            
          return currentOutputBuffer;
        }
      }
    }

    if (err != DE265_OK)
    {
      // The encoding loop ended because of an error
      if (decError != err)
        decError = err;

      DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData Error %s", de265_get_error_text(err));
      return QByteArray();
    }
    if (more == 0)
    {
      // The loop ended because there is nothing more to decode but no error occurred.
      // We are at the end of the sequence.

      // This should not happen because before decoding, we check if the frame to decode is in the list of nal units that will be decoded.
      DEBUG_LIBDE265("hevcDecoderLibde265::loadYUVFrameData more == 0");

      return QByteArray();
    }
  }
  
  return QByteArray();
}

#if SSE_CONVERSION
void hevcDecoderLibde265::copyImgToByteArray(const de265_image *src, byteArrayAligned &dst)
#else
void hevcDecoderLibde265::copyImgToByteArray(const de265_image *src, QByteArray &dst)
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

  DEBUG_LIBDE265("hevcDecoderLibde265::copyImgToByteArray nrBytes %d", nrBytes);

  // Is the output big enough?
  if (dst.capacity() < nrBytes)
    dst.resize(nrBytes);

  // We can now copy from src to dst
  char* dst_c = dst.data();
  for (int c = 0; c < nrPlanes; c++)
  {
    const uint8_t* img_c = nullptr;
    if (decodeSignal == 0 || nrSignalsSupported == 1)
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

void hevcDecoderLibde265::cacheStatistics(const de265_image *img)
{
  if (!wrapperInternalsSupported())
    return;

  // Clear the local statistics cache
  curPOCStats.clear();

  /// --- CTB internals/statistics
  int widthInCTB, heightInCTB, log2CTBSize;
  de265_internals_get_CTB_Info_Layout(img, &widthInCTB, &heightInCTB, &log2CTBSize);
  int ctb_size = 1 << log2CTBSize;  // width and height of each CTB

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

        // Walk into the TU tree
        int tuIdx = (cbPosY / tuInfo_unit_size) * widthInTUInfoUnits + (cbPosX / tuInfo_unit_size);
        cacheStatistics_TUTree_recursive(tuInfo.data(), widthInTUInfoUnits, tuInfo_unit_size, iPOC, tuIdx, cbSizePix / tuInfo_unit_size, 0, predMode == 0, intraDirY.data(), intraDirC.data(), intraDir_infoUnit_size, widthInIntraDirUnits);
      }
    }
  }
}

void hevcDecoderLibde265::getPBSubPosition(int partMode, int cbSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const
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
* \param isIntra: is the CU using intra prediction?
*/
void hevcDecoderLibde265::cacheStatistics_TUTree_recursive(uint8_t *const tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth, bool isIntra, uint8_t *const intraDirY, uint8_t *const intraDirC, int intraDir_infoUnit_size, int widthInIntraDirUnits)
{
  // Check if the TU is further split.
  if (tuInfo[tuIdx] & (1 << trDepth))
  {
    // The transform is split further
    int yOffset = (tuWidth_units / 2) * tuInfoWidth;
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx                              , tuWidth_units / 2, trDepth+1, isIntra, intraDirY, intraDirC, intraDir_infoUnit_size, widthInIntraDirUnits);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx           + tuWidth_units / 2, tuWidth_units / 2, trDepth+1, isIntra, intraDirY, intraDirC, intraDir_infoUnit_size, widthInIntraDirUnits);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset                    , tuWidth_units / 2, trDepth+1, isIntra, intraDirY, intraDirC, intraDir_infoUnit_size, widthInIntraDirUnits);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset + tuWidth_units / 2, tuWidth_units / 2, trDepth+1, isIntra, intraDirY, intraDirC, intraDir_infoUnit_size, widthInIntraDirUnits);
  }
  else
  {
    // The transform is not split any further. Add the TU depth to the statistics (ID 11)
    int tuWidth = tuWidth_units * tuUnitSizePix;
    int posX = tuIdx % tuInfoWidth * tuUnitSizePix;
    int posY = tuIdx / tuInfoWidth * tuUnitSizePix;
    curPOCStats[11].addBlockValue(posX, posY, tuWidth, tuWidth, trDepth);

    if (isIntra)
    {
      // Display the intra prediction mode (as it is executed) per transform unit
  
      // Conversion from intra prediction mode to vector.
      // Coordinates are in x,y with the axes going right and down.
      static const int vectorTable[35][2] = 
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

      // Get index for this xy position in the intraDir array
      int intraDirIdx = (posY / intraDir_infoUnit_size) * widthInIntraDirUnits + (posX / intraDir_infoUnit_size);

      // Set Intra prediction direction Luma (ID 9)
      int intraDirLuma = intraDirY[intraDirIdx];
      if (intraDirLuma <= 34)
      {
        curPOCStats[9].addBlockValue(posX, posY, tuWidth, tuWidth, intraDirLuma);

        if (intraDirLuma >= 2)
        {
          // Set Intra prediction direction Luma (ID 9) as vector
          int vecX = (float)vectorTable[intraDirLuma][0] * tuWidth / 4;
          int vecY = (float)vectorTable[intraDirLuma][1] * tuWidth / 4;
          curPOCStats[9].addBlockVector(posX, posY, tuWidth, tuWidth, vecX, vecY);
        }
      }

      // Set Intra prediction direction Chroma (ID 10)
      int intraDirChroma = intraDirC[intraDirIdx];
      if (intraDirChroma <= 34)
      {
        curPOCStats[10].addBlockValue(posX, posY, tuWidth, tuWidth, intraDirChroma);

        if (intraDirChroma >= 2)
        {
          // Set Intra prediction direction Chroma (ID 10) as vector
          int vecX = (float)vectorTable[intraDirChroma][0] * tuWidth / 4;
          int vecY = (float)vectorTable[intraDirChroma][1] * tuWidth / 4;
          curPOCStats[10].addBlockVector(posX, posY, tuWidth, tuWidth, vecX, vecY);
        }
      }
    }
  }
}

statisticsData hevcDecoderLibde265::getStatisticsData(int frameIdx, int typeIdx)
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

bool hevcDecoderLibde265::reloadItemSource()
{
  if (decoderError)
    // Nothing is working, so there is nothing to reset.
    return false;

  // Reset the hevcDecoderLibde265 variables/buffers.
  decError = DE265_OK;
  statsCacheCurPOC = -1;
  currentOutputBufferFrameIndex = -1;

  // Re-open the input file. This will reload the bitstream as if it was completely unknown.
  QString fileName = annexBFile->absoluteFilePath();
  parsingError = annexBFile->openFile(fileName);
  return parsingError;
}

void hevcDecoderLibde265::fillStatisticList(statisticHandler &statSource) const
{
  StatisticsType sliceIdx(0, "Slice Index", 0, QColor(0, 0, 0), 10, QColor(255,0,0));
  sliceIdx.description = "The slice index reported per CTU";
  statSource.addStatType(sliceIdx);

  StatisticsType partSize(1, "Part Size", "jet", 0, 7);
  partSize.description = "The partition size of each CU into PUs";
  partSize.valMap.insert(0, "PART_2Nx2N");
  partSize.valMap.insert(1, "PART_2NxN");
  partSize.valMap.insert(2, "PART_Nx2N");
  partSize.valMap.insert(3, "PART_NxN");
  partSize.valMap.insert(4, "PART_2NxnU");
  partSize.valMap.insert(5, "PART_2NxnD");
  partSize.valMap.insert(6, "PART_nLx2N");
  partSize.valMap.insert(7, "PART_nRx2N");
  statSource.addStatType(partSize);

  StatisticsType predMode(2, "Pred Mode", "jet", 0, 2);
  predMode.description = "The internal libde265 prediction mode (intra/inter/skip) per CU";
  predMode.valMap.insert(0, "INTRA");
  predMode.valMap.insert(1, "INTER");
  predMode.valMap.insert(2, "SKIP");
  statSource.addStatType(predMode);

  StatisticsType pcmFlag(3, "PCM flag", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  pcmFlag.description = "The PCM flag per CU";
  statSource.addStatType(pcmFlag);

  StatisticsType transQuantBypass(4, "Transquant Bypass Flag", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  transQuantBypass.description = "The transquant bypass flag per CU";
  statSource.addStatType(transQuantBypass);

  StatisticsType refIdx0(5, "Ref POC 0", "col3_bblg", -16, 16);
  refIdx0.description = "The reference POC in LIST 0 relative to the current POC per PU";
  statSource.addStatType(refIdx0);

  StatisticsType refIdx1(6, "Ref POC 1", "col3_bblg", -16, 16);
  refIdx1.description = "The reference POC in LIST 1 relative to the current POC per PU";
  statSource.addStatType(refIdx1);

  StatisticsType motionVec0(7, "Motion Vector 0", 4);
  motionVec0.description = "The motion vector in LIST 0 per PU";
  statSource.addStatType(motionVec0);

  StatisticsType motionVec1(8, "Motion Vector 1", 4);
  motionVec1.description = "The motion vector in LIST 1 per PU";
  statSource.addStatType(motionVec1);

  StatisticsType intraDirY(9, "Intra Dir Luma", "jet", 0, 34);
  intraDirY.description = "The intra mode for the luma component per TU (intra prediction is performed on a TU level)";
  intraDirY.hasVectorData = true;
  intraDirY.renderVectorData = true;
  intraDirY.vectorScale = 32;
  // Don't draw the vector values for the intra dir. They don't have actual meaning.
  intraDirY.renderVectorDataValues = false;
  intraDirY.valMap.insert(0, "INTRA_PLANAR");
  intraDirY.valMap.insert(1, "INTRA_DC");
  intraDirY.valMap.insert(2, "INTRA_ANGULAR_2");
  intraDirY.valMap.insert(3, "INTRA_ANGULAR_3");
  intraDirY.valMap.insert(4, "INTRA_ANGULAR_4");
  intraDirY.valMap.insert(5, "INTRA_ANGULAR_5");
  intraDirY.valMap.insert(6, "INTRA_ANGULAR_6");
  intraDirY.valMap.insert(7, "INTRA_ANGULAR_7");
  intraDirY.valMap.insert(8, "INTRA_ANGULAR_8");
  intraDirY.valMap.insert(9, "INTRA_ANGULAR_9");
  intraDirY.valMap.insert(10, "INTRA_ANGULAR_10");
  intraDirY.valMap.insert(11, "INTRA_ANGULAR_11");
  intraDirY.valMap.insert(12, "INTRA_ANGULAR_12");
  intraDirY.valMap.insert(13, "INTRA_ANGULAR_13");
  intraDirY.valMap.insert(14, "INTRA_ANGULAR_14");
  intraDirY.valMap.insert(15, "INTRA_ANGULAR_15");
  intraDirY.valMap.insert(16, "INTRA_ANGULAR_16");
  intraDirY.valMap.insert(17, "INTRA_ANGULAR_17");
  intraDirY.valMap.insert(18, "INTRA_ANGULAR_18");
  intraDirY.valMap.insert(19, "INTRA_ANGULAR_19");
  intraDirY.valMap.insert(20, "INTRA_ANGULAR_20");
  intraDirY.valMap.insert(21, "INTRA_ANGULAR_21");
  intraDirY.valMap.insert(22, "INTRA_ANGULAR_22");
  intraDirY.valMap.insert(23, "INTRA_ANGULAR_23");
  intraDirY.valMap.insert(24, "INTRA_ANGULAR_24");
  intraDirY.valMap.insert(25, "INTRA_ANGULAR_25");
  intraDirY.valMap.insert(26, "INTRA_ANGULAR_26");
  intraDirY.valMap.insert(27, "INTRA_ANGULAR_27");
  intraDirY.valMap.insert(28, "INTRA_ANGULAR_28");
  intraDirY.valMap.insert(29, "INTRA_ANGULAR_29");
  intraDirY.valMap.insert(30, "INTRA_ANGULAR_30");
  intraDirY.valMap.insert(31, "INTRA_ANGULAR_31");
  intraDirY.valMap.insert(32, "INTRA_ANGULAR_32");
  intraDirY.valMap.insert(33, "INTRA_ANGULAR_33");
  intraDirY.valMap.insert(34, "INTRA_ANGULAR_34");
  statSource.addStatType(intraDirY);

  StatisticsType intraDirC(10, "Intra Dir Chroma", "jet", 0, 34);
  intraDirC.description = "The intra mode for the chroma component per TU (intra prediction is performed on a TU level)";
  intraDirC.hasVectorData = true;
  intraDirC.renderVectorData = true;
  intraDirC.renderVectorDataValues = false;
  intraDirC.vectorScale = 32;
  intraDirC.valMap.insert(0, "INTRA_PLANAR");
  intraDirC.valMap.insert(1, "INTRA_DC");
  intraDirC.valMap.insert(2, "INTRA_ANGULAR_2");
  intraDirC.valMap.insert(3, "INTRA_ANGULAR_3");
  intraDirC.valMap.insert(4, "INTRA_ANGULAR_4");
  intraDirC.valMap.insert(5, "INTRA_ANGULAR_5");
  intraDirC.valMap.insert(6, "INTRA_ANGULAR_6");
  intraDirC.valMap.insert(7, "INTRA_ANGULAR_7");
  intraDirC.valMap.insert(8, "INTRA_ANGULAR_8");
  intraDirC.valMap.insert(9, "INTRA_ANGULAR_9");
  intraDirC.valMap.insert(10, "INTRA_ANGULAR_10");
  intraDirC.valMap.insert(11, "INTRA_ANGULAR_11");
  intraDirC.valMap.insert(12, "INTRA_ANGULAR_12");
  intraDirC.valMap.insert(13, "INTRA_ANGULAR_13");
  intraDirC.valMap.insert(14, "INTRA_ANGULAR_14");
  intraDirC.valMap.insert(15, "INTRA_ANGULAR_15");
  intraDirC.valMap.insert(16, "INTRA_ANGULAR_16");
  intraDirC.valMap.insert(17, "INTRA_ANGULAR_17");
  intraDirC.valMap.insert(18, "INTRA_ANGULAR_18");
  intraDirC.valMap.insert(19, "INTRA_ANGULAR_19");
  intraDirC.valMap.insert(20, "INTRA_ANGULAR_20");
  intraDirC.valMap.insert(21, "INTRA_ANGULAR_21");
  intraDirC.valMap.insert(22, "INTRA_ANGULAR_22");
  intraDirC.valMap.insert(23, "INTRA_ANGULAR_23");
  intraDirC.valMap.insert(24, "INTRA_ANGULAR_24");
  intraDirC.valMap.insert(25, "INTRA_ANGULAR_25");
  intraDirC.valMap.insert(26, "INTRA_ANGULAR_26");
  intraDirC.valMap.insert(27, "INTRA_ANGULAR_27");
  intraDirC.valMap.insert(28, "INTRA_ANGULAR_28");
  intraDirC.valMap.insert(29, "INTRA_ANGULAR_29");
  intraDirC.valMap.insert(30, "INTRA_ANGULAR_30");
  intraDirC.valMap.insert(31, "INTRA_ANGULAR_31");
  intraDirC.valMap.insert(32, "INTRA_ANGULAR_32");
  intraDirC.valMap.insert(33, "INTRA_ANGULAR_33");
  intraDirC.valMap.insert(34, "INTRA_ANGULAR_34");
  statSource.addStatType(intraDirC);

  StatisticsType transformDepth(11, "Transform Depth", 0, QColor(0, 0, 0), 3, QColor(0,255,0));
  transformDepth.description = "The transform depth within the transform tree per TU";
  statSource.addStatType(transformDepth);
}

bool hevcDecoderLibde265::checkLibraryFile(QString libFilePath, QString &error)
{
  hevcDecoderLibde265 testDecoder;

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
  return !testDecoder.decoderError;
}