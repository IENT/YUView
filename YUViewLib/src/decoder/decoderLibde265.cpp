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

#include "decoderLibde265.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include "common/typedef.h"

using namespace YUView;

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define DECODERLIBD265_DEBUG_OUTPUT 0
#if DECODERLIBD265_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if DECODERLIBD265_DEBUG_OUTPUT == 1
#define DEBUG_LIBDE265 if(!isCachingDecoder) qDebug
#elif DECODERLIBD265_DEBUG_OUTPUT == 2
#define DEBUG_LIBDE265 if(isCachingDecoder) qDebug
#elif DECODERLIBD265_DEBUG_OUTPUT == 3
#define DEBUG_LIBDE265 if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_LIBDE265(fmt,...) ((void)0)
#endif

decoderLibde265_Functions::decoderLibde265_Functions() 
{ 
  memset(this, 0, sizeof(*this)); 
}

decoderLibde265::decoderLibde265(int signalID, bool cachingDecoder) :
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

decoderLibde265::~decoderLibde265()
{
  if (decoder != nullptr)
    // Free the decoder
    de265_free_decoder(decoder);
}

void decoderLibde265::resetDecoder()
{
  if (!decoder)
    return;

  // Delete decoder
  de265_error err = de265_free_decoder(decoder);
  if (err != DE265_OK)
    return setError("Reset: Freeing the decoder failded.");
  
  decoder = nullptr;
  decodedFrameWaiting = false;
  
  // Create new decoder
  allocateNewDecoder();
}

void decoderLibde265::setDecodeSignal(int signalID, bool &decoderResetNeeded)
{
  decoderResetNeeded = false;
  if (signalID == decodeSignal)
    return;
  if (signalID >= 0 && signalID < nrSignalsSupported())
    decodeSignal = signalID;
  decoderResetNeeded = true;
}

void decoderLibde265::resolveLibraryFunctionPointers()
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
  if (!resolve(de265_push_NAL, "de265_push_NAL")) return;
  if (!resolve(de265_flush_data, "de265_flush_data")) return;
  if (!resolve(de265_get_next_picture, "de265_get_next_picture")) return;
  if (!resolve(de265_free_decoder, "de265_free_decoder")) return;
  DEBUG_LIBDE265("decoderLibde265::resolveLibraryFunctionPointers - decoding functions found");

  // Get pointers to the internals/statistics functions (if present)
  // If not, disable the statistics extraction. Normal decoding of the video will still work.

  if (!resolve(de265_internals_get_CTB_Info_Layout, "de265_internals_get_CTB_Info_Layout", true)) return;
  if (!resolve(de265_internals_get_CTB_sliceIdx, "de265_internals_get_CTB_sliceIdx", true)) return;
  if (!resolve(de265_internals_get_CB_Info_Layout, "de265_internals_get_CB_Info_Layout", true)) return;
  if (!resolve(de265_internals_get_CB_info, "de265_internals_get_CB_info", true)) return;
  if (!resolve(de265_internals_get_PB_Info_layout, "de265_internals_get_PB_Info_layout", true)) return;
  if (!resolve(de265_internals_get_PB_info, "de265_internals_get_PB_info", true)) return;
  if (!resolve(de265_internals_get_IntraDir_Info_layout, "de265_internals_get_IntraDir_Info_layout", true)) return;
  if (!resolve(de265_internals_get_intraDir_info, "de265_internals_get_intraDir_info", true)) return;
  if (!resolve(de265_internals_get_TUInfo_Info_layout, "de265_internals_get_TUInfo_Info_layout", true)) return;
  if (!resolve(de265_internals_get_TUInfo_info, "de265_internals_get_TUInfo_info", true)) return;
  // All interbals functions were successfully retrieved
  internalsSupported = true;
  DEBUG_LIBDE265("decoderLibde265::resolveLibraryFunctionPointers - statistics internals found");
  
  // Get pointers to the functions for retrieving prediction/residual signals
  if (!resolve(de265_internals_get_image_plane, "de265_internals_get_image_plane", true)) return;
  if (!resolve(de265_internals_set_parameter_bool, "de265_internals_set_parameter_bool", true)) return;
  // The prediction and residual signal can be obtained
  nrSignals = 4;
  DEBUG_LIBDE265("decoderLibde265::resolveLibraryFunctionPointers - prediction/residual internals found");
}

template <typename T> T decoderLibde265::resolve(T &fun, const char *symbol, bool optional)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    if (!optional)
      setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

void decoderLibde265::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;
  if (decoderState == decoderError)
    return;

  DEBUG_LIBDE265("decoderLibde265::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Create new decoder object
  decoder = de265_new_decoder();
  if (!decoder)
  {
    decoderState = decoderError;
    setError("Error allocating decoder (de265_new_decoder)");
    return;
  }

  // Set some decoder parameters
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_SUPPRESS_FAULTY_PICTURES, false);

  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_SAO, false);

  // Set retrieval of the right component
  if (nrSignals > 0)
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
  // The highest temporal ID to decode. Set this to very high (all) by default.
  de265_set_limit_TID(decoder, 100);

  // Set the number of decoder threads. Libde265 can use wavefronts to utilize these.
  // TODO: We should add a setting for this maybe?
  de265_error err = de265_start_worker_threads(decoder, 8);
  if (err != DE265_OK)
    return setError("Error starting libde265 worker threads (de265_start_worker_threads)");

  // The decoder is ready to receive data
  decoderBase::resetDecoder();
  currentOutputBuffer.clear();
  decodedFrameWaiting = false;
  flushing = false;
}

bool decoderLibde265::decodeNextFrame()
{
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_LIBDE265("decoderLibde265::decodeNextFrame: Wrong decoder state.");
    return false;
  }
  if (decodedFrameWaiting)
  {
    decodedFrameWaiting = false;
    return true;
  }
  
  return decodeFrame();
}

bool decoderLibde265::decodeFrame()
{
  int more = 1;
  curImage = nullptr;
  while (more && curImage == nullptr)
  {
    more = 0;
    de265_error err = de265_decode(decoder, &more);

    if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA)
    {
      decoderState = decoderNeedsMoreData;
      return false;
    }
    else if (err != DE265_OK)
      return setErrorB("Error decoding (de265_decode)");

    curImage = de265_get_next_picture(decoder);
  }

  if (more == 0 && curImage == nullptr)
  {
    // Decoding ended
    decoderState = decoderEndOfBitstream;
    return false;
  }

  if (curImage != nullptr)
  {
    // Get the resolution / yuv format from the frame
    QSize s = QSize(de265_get_image_width(curImage, 0), de265_get_image_height(curImage, 0));
    if (!s.isValid())
      DEBUG_LIBDE265("decoderLibde265::decodeFrame got invalid frame size");
    auto subsampling = convertFromInternalSubsampling(de265_get_chroma_format(curImage));
    if (subsampling == YUV_NUM_SUBSAMPLINGS)
      DEBUG_LIBDE265("decoderLibde265::decodeFrame got invalid subsampling");
    int bitDepth = de265_get_bits_per_pixel(curImage, 0);
    if (bitDepth < 8 || bitDepth > 16)
      DEBUG_LIBDE265("decoderLibde265::decodeFrame got invalid bit depth");

    if (!frameSize.isValid() && !formatYUV.isValid())
    {
      // Set the values
      frameSize = s;
      formatYUV = yuvPixelFormat(subsampling, bitDepth);
    }
    else
    {
      // Check the values against the previously set values
      if (frameSize != s)
        return setErrorB("Recieved a frame of different size");
      if (formatYUV.subsampling != subsampling)
        return setErrorB("Recieved a frame with different subsampling");
      if (formatYUV.bitsPerSample != bitDepth)
        return setErrorB("Recieved a frame with different bit depth");
    }
    DEBUG_LIBDE265("decoderLibde265::decodeFrame Picture decoded");

    decoderState = decoderRetrieveFrames;
    currentOutputBuffer.clear();
    return true;
  }
  return false;
}

QByteArray decoderLibde265::getRawFrameData()
{
  if (curImage == nullptr)
    return QByteArray();
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_LIBDE265("decoderLibde265::getRawFrameData: Wrong decoder state.");
    return QByteArray();
  }

  if (currentOutputBuffer.isEmpty())
  {
    // Put image data into buffer
    copyImgToByteArray(curImage, currentOutputBuffer);
    DEBUG_LIBDE265("decoderLibde265::getRawFrameData copied frame to buffer");
    
    if (retrieveStatistics)
      // Get the statistics from the image and put them into the statistics cache
      cacheStatistics(curImage);
  }

  return currentOutputBuffer;
}

bool decoderLibde265::pushData(QByteArray &data) 
{
  if (decoderState != decoderNeedsMoreData)
  {
    DEBUG_LIBDE265("decoderLibde265::pushData: Wrong decoder state.");
    return false;
  }
  if (flushing)
  {
    DEBUG_LIBDE265("decoderLibde265::pushData: Do not push data when flushing!");
    return false;
  }
  
  // Push the data to the decoder
  if (data.size() > 0)
  {
    // de265_push_NAL expects the NAL data without the start code
    int offset = 0;
    if (data.at(0) == (char)0 && data.at(1) == (char)0)
    {
      if (data.at(2) == (char)1)
        offset = 3;
      if (data.at(2) == (char)0 && data.at(3) == (char)1)
        offset = 4;
    }
    // de265_push_NAL will return either DE265_OK or DE265_ERROR_OUT_OF_MEMORY
    de265_error err = de265_push_NAL(decoder, data.data() + offset, data.size() - offset, 0, nullptr);
    DEBUG_LIBDE265("decoderLibde265::pushData push data %d bytes%s%s", data.size(), err != DE265_OK ? " - err " : "", err != DE265_OK ? de265_get_error_text(err) : "");
    if (err != DE265_OK)
      return setErrorB("Error pushing data to decoder (de265_push_NAL): " + QString(de265_get_error_text(err)));
  }
  else
  {
    // The input file is at the end. Switch to flushing mode.
    DEBUG_LIBDE265("decoderLibde265::pushData input ended - flushing");
    de265_error err = de265_flush_data(decoder);
    if (err != DE265_OK)
      return setErrorB("Error switching to flushing mode.");
    flushing = true;
  }

  // Check for an available frame
  if (decodeFrame())
    decodedFrameWaiting = true;

  return true;
}

#if SSE_CONVERSION
void decoderLibde265::copyImgToByteArray(const de265_image *src, byteArrayAligned &dst)
#else
void decoderLibde265::copyImgToByteArray(const de265_image *src, QByteArray &dst)
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

  DEBUG_LIBDE265("decoderLibde265::copyImgToByteArray nrBytes %d", nrBytes);

  // Is the output big enough?
  if (dst.capacity() < nrBytes)
    dst.resize(nrBytes);

  uint8_t *dst_c = (uint8_t*)dst.data();

  // We can now copy from src to dst
  for (int c = 0; c < nrPlanes; c++)
  {
    const int width = de265_get_image_width(src, c);
    const int height = de265_get_image_height(src, c);
    const int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;
    const size_t widthInBytes = width * nrBytesPerSample;

    const uint8_t* img_c = nullptr;
    if (decodeSignal == 0)
      img_c = de265_get_image_plane(src, c, &stride);
    else if (decodeSignal == 1)
      img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_PREDICTION, c, &stride);
    else if (decodeSignal == 2)
      img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_RESIDUAL, c, &stride);
    else if (decodeSignal == 3)
      img_c = de265_internals_get_image_plane(src, DE265_INTERNALS_DECODER_PARAM_SAVE_TR_COEFF, c, &stride);
      
    if (img_c == nullptr)
      return;

    for (int y = 0; y < height; y++)
    {
      memcpy(dst_c, img_c, widthInBytes);
      img_c += stride;
      dst_c += widthInBytes;
    }
  }
}

void decoderLibde265::cacheStatistics(const de265_image *img)
{
  if (!internalsSupported)
    return;

  DEBUG_LIBDE265("decoderLibde265::cacheStatistics");

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

  // TODO: How do we get the POC in here? / Should the decoder not be able to tell us the POC?
  const int iPOC = 0;

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

void decoderLibde265::getPBSubPosition(int partMode, int cbSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const
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
void decoderLibde265::cacheStatistics_TUTree_recursive(uint8_t *const tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth, bool isIntra, uint8_t *const intraDirY, uint8_t *const intraDirC, int intraDir_infoUnit_size, int widthInIntraDirUnits)
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

void decoderLibde265::fillStatisticList(statisticHandler &statSource) const
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

bool decoderLibde265::checkLibraryFile(QString libFilePath, QString &error)
{
  decoderLibde265 testDecoder;

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

QStringList decoderLibde265::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList libNames = is_Q_OS_MAC ?
    QStringList() << "libde265-internals.dylib" << "libde265.dylib" :
    QStringList() << "libde265-internals" << "libde265";

  return libNames;
}

YUVSubsamplingType decoderLibde265::convertFromInternalSubsampling(de265_chroma fmt)
{
  if (fmt == de265_chroma_mono)
    return YUV_400;
  else if (fmt == de265_chroma_420)
    return YUV_420;
  else if (fmt == de265_chroma_422)
    return YUV_422;
  else if (fmt == de265_chroma_444)
    return YUV_444;
  
  // Invalid
  return YUV_NUM_SUBSAMPLINGS;
}
