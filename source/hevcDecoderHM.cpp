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

#include "hevcDecoderHM.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include "typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define HEVCDECODERHM_DEBUG_OUTPUT 0
#if HEVCDECODERHM_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if HEVCDECODERHM_DEBUG_OUTPUT == 1
#define DEBUG_DECHM if(!isCachingDecoder) qDebug
#elif HEVCDECODERHM_DEBUG_OUTPUT == 2
#define DEBUG_DECHM if(isCachingDecoder) qDebug
#elif HEVCDECODERHM_DEBUG_OUTPUT == 3
#define DEBUG_DECHM if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_DECHM(fmt,...) ((void)0)
#endif

hevcDecoderHM_Functions::hevcDecoderHM_Functions() { memset(this, 0, sizeof(*this)); }

hevcDecoderHM::hevcDecoderHM(int signalID, bool cachingDecoder) :
  hevcDecoderBase(cachingDecoder)
{
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  loadDecoderLibrary();

  decoder = nullptr;
  currentHMPic = nullptr;
  stateReadingFrames = false;

  // Set the signal to decode (if supported)
  if (predAndResiSignalsSupported && signalID >= 0 && signalID <= 3)
    decodeSignal = signalID;
  else
    decodeSignal = 0;

  // Allocate a decoder
  if (!decoderError)
    allocateNewDecoder();
}

hevcDecoderHM::~hevcDecoderHM()
{
  if (decoder != nullptr)
    libHMDec_free_decoder(decoder);
}

QStringList hevcDecoderHM::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList names = 
    is_Q_OS_MAC ?
    QStringList() << "libhevcDecoderHM.dylib" :
    QStringList() << "libhevcDecoderHM";

  return names;
}

void hevcDecoderHM::resolveLibraryFunctionPointers()
{
  // Get/check function pointers
  if (!resolve(libHMDec_new_decoder, "libHMDec_new_decoder")) return;
  if (!resolve(libHMDec_free_decoder, "libHMDec_free_decoder")) return;
  if (!resolve(libHMDec_set_SEI_Check, "libHMDec_set_SEI_Check")) return;
  if (!resolve(libHMDec_set_max_temporal_layer, "libHMDec_set_max_temporal_layer")) return;
  if (!resolve(libHMDec_push_nal_unit, "libHMDec_push_nal_unit")) return;

  if (!resolve(libHMDec_get_picture, "libHMDec_get_picture")) return;
  if (!resolve(libHMDEC_get_POC, "libHMDEC_get_POC")) return;
  if (!resolve(libHMDEC_get_picture_width, "libHMDEC_get_picture_width")) return;
  if (!resolve(libHMDEC_get_picture_height, "libHMDEC_get_picture_height")) return;
  if (!resolve(libHMDEC_get_picture_stride, "libHMDEC_get_picture_stride")) return;
  if (!resolve(libHMDEC_get_image_plane, "libHMDEC_get_image_plane")) return;
  if (!resolve(libHMDEC_get_chroma_format, "libHMDEC_get_chroma_format")) return;

  if (!resolve(libHMDEC_get_internal_bit_depth, "libHMDEC_get_internal_bit_depth")) return;
  
  // Get pointers to the internals/statistics functions (if present)
  if (!resolve(libHMDEC_get_internal_info, "libHMDEC_get_internal_info")) return;

  // All interbals functions were successfully retrieved
  internalsSupported = true;

  return;
  
  // TODO: could we somehow get the prediction/residual signal?
  // I don't think this is possible without changes to the reference decoder.
  predAndResiSignalsSupported = true;
  DEBUG_DECHM("hevcDecoderHM::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T hevcDecoderHM::resolve(T &fun, const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

template <typename T> T hevcDecoderHM::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void hevcDecoderHM::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_DECHM("hevcDecoderHM::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Set some decoder parameters
  libHMDec_set_SEI_Check(decoder, true);
  libHMDec_set_max_temporal_layer(decoder, -1);

  // Create new decoder object
  decoder = libHMDec_new_decoder();
  
  // Set retrieval of the right component
  if (predAndResiSignalsSupported)
  {
    // TODO. Or is this even possible?
  }
}

QByteArray hevcDecoderHM::loadYUVFrameData(int frameIdx)
{
  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    return currentOutputBuffer;
  }

  DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData Start request %d", frameIdx);

  // We have to decode the requested frame.
  bool seeked = false;
  QList<QByteArray> parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and start decoding from there.
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);

    DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
      DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
    libHMDec_error err = libHMDec_free_decoder(decoder);
    if (err != LIBHMDEC_OK)
    {
      // Freeing the decoder failed.
      if (decError != err)
        decError = err;
      return QByteArray();
    }

    decoder = nullptr;

    // Create new decoder
    allocateNewDecoder();

    // Feed the parameter sets to the decoder
    bool bNewPicture;
    bool checkOutputPictures;
    for (QByteArray ps : parameterSets)
    {
      err = libHMDec_push_nal_unit(decoder, (uint8_t*)ps.data(), ps.size(), false, bNewPicture, checkOutputPictures);
      DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData pushed parameter NAL length %d%s%s", ps.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
    }
  }

  // Perform the decoding right now blocking the main thread.
  // Decode frames until we receive the one we are looking for.
  while (true)
  {
    // Decoding with the HM library works like this:
    // Push a NAL unit to the decoder. If bNewPicture is set, we will have to push it to the decoder again.
    // If checkOutputPictures is set, we can see if there is one (or more) pictures that can be read.

    if (!stateReadingFrames)
    {
      bool bNewPicture;
      bool checkOutputPictures = false;
      // The picture pointer will be invalid when we push the next NAL unit to the decoder.
      currentHMPic = nullptr;

      if (!lastNALUnit.isEmpty())
      {
        libHMDec_push_nal_unit(decoder, lastNALUnit, lastNALUnit.length(), false, bNewPicture, checkOutputPictures);
        DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData pushed last NAL length %d%s%s", lastNALUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
        // bNewPicture should now be false
        assert(!bNewPicture);
        lastNALUnit.clear();
      }
      else
      {
        // Get the next NAL unit
        QByteArray nalUnit = annexBFile.getNextNALUnit();
        assert(nalUnit.length() > 0);
        bool endOfFile = annexBFile.atEnd();

        libHMDec_push_nal_unit(decoder, nalUnit, nalUnit.length(), endOfFile, bNewPicture, checkOutputPictures);
        DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData pushed next NAL length %d%s%s", nalUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
        
        if (bNewPicture)
          // Save the NAL unit
          lastNALUnit = nalUnit;
      }
      
      if (checkOutputPictures)
        stateReadingFrames = true;
    }

    if (stateReadingFrames)
    {
      // Try to read pictures
      libHMDec_picture *pic = libHMDec_get_picture(decoder);
      while (pic != nullptr)
      {
        // We recieved a picture
        currentOutputBufferFrameIndex++;
        currentHMPic = pic;

        // First update the chroma format and frame size
        pixelFormat = libHMDEC_get_chroma_format(pic);
        nrBitsC0 = libHMDEC_get_internal_bit_depth(LIBHMDEC_LUMA);
        frameSize = QSize(libHMDEC_get_picture_width(pic, LIBHMDEC_LUMA), libHMDEC_get_picture_height(pic, LIBHMDEC_LUMA));
        
        if (currentOutputBufferFrameIndex == frameIdx)
        {
          // This is the frame that we want to decode

          // Put image data into buffer
          copyImgToByteArray(pic, currentOutputBuffer);

          if (retrieveStatistics)
          {
            // Get the statistics from the image and put them into the statistics cache
            cacheStatistics(pic);

            // The cache now contains the statistics for iPOC
            statsCacheCurPOC = currentOutputBufferFrameIndex;
          }

          // Picture decoded
          DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData decoded the requested frame %d - POC %d", currentOutputBufferFrameIndex, libHMDEC_get_POC(pic));

          return currentOutputBuffer;
        }
        else
        {
          DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData decoded the unrequested frame %d - POC %d", currentOutputBufferFrameIndex, libHMDEC_get_POC(pic));
        }

        // Try to get another picture
        pic = libHMDec_get_picture(decoder);
      }
    }
    
    stateReadingFrames = false;
  }
  
  return QByteArray();
}

#if SSE_CONVERSION
void hevcDecoderHM::copyImgToByteArray(libHMDec_picture *src, byteArrayAligned &dst)
#else
void hevcDecoderHM::copyImgToByteArray(libHMDec_picture *src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  int nrPlanes = (pixelFormat == LIBHMDEC_CHROMA_400) ? 1 : 3;

  // At first get how many bytes we are going to write
  int nrBytes = 0;
  int stride;
  for (int c = 0; c < nrPlanes; c++)
  {
    libHMDec_ColorComponent component = (c == 0) ? LIBHMDEC_LUMA : (c == 1) ? LIBHMDEC_CHROMA_U : LIBHMDEC_CHROMA_V;

    int width = libHMDEC_get_picture_width(src, component);
    int height = libHMDEC_get_picture_height(src, component);
    int nrBytesPerSample = (libHMDEC_get_internal_bit_depth(LIBHMDEC_LUMA) > 8) ? 2 : 1;

    nrBytes += width * height * nrBytesPerSample;
  }

  DEBUG_DECHM("hevcDecoderHM::copyImgToByteArray nrBytes %d", nrBytes);

  // Is the output big enough?
  if (dst.capacity() < nrBytes)
    dst.resize(nrBytes);

  // We can now copy from src to dst
  char* dst_c = dst.data();
  for (int c = 0; c < nrPlanes; c++)
  {
    libHMDec_ColorComponent component = (c == 0) ? LIBHMDEC_LUMA : (c == 1) ? LIBHMDEC_CHROMA_U : LIBHMDEC_CHROMA_V;

    const short* img_c = nullptr;
    img_c = libHMDEC_get_image_plane(src, component);
    stride = libHMDEC_get_picture_stride(src, component);
    
    if (img_c == nullptr)
      return;

    int width = libHMDEC_get_picture_width(src, component);
    int height = libHMDEC_get_picture_height(src, component);
    int nrBytesPerSample = (libHMDEC_get_internal_bit_depth(LIBHMDEC_LUMA) > 8) ? 2 : 1;
    size_t size = width * nrBytesPerSample;

    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x++)
      {
        dst_c[x] = (char)img_c[x];
      }
      img_c += stride;
      dst_c += size;
    }
  }
}

/* Convert the de265_chroma format to a YUVCPixelFormatType and return it.
*/
yuvPixelFormat hevcDecoderHM::getYUVPixelFormat()
{
  if (pixelFormat == LIBHMDEC_CHROMA_400)
    return yuvPixelFormat(YUV_420, nrBitsC0);
  else if (pixelFormat == LIBHMDEC_CHROMA_420)
    return yuvPixelFormat(YUV_420, nrBitsC0);
  else if (pixelFormat == LIBHMDEC_CHROMA_422)
    return yuvPixelFormat(YUV_422, nrBitsC0);
  else if (pixelFormat == LIBHMDEC_CHROMA_444)
    return yuvPixelFormat(YUV_444, nrBitsC0);
  return yuvPixelFormat();
}

void hevcDecoderHM::cacheStatistics(libHMDec_picture *img)
{
  if (!wrapperInternalsSupported())
    return;

  DEBUG_DECHM("hevcDecoderHM::cacheStatistics POC %d", libHMDEC_get_POC(img));

  // Clear the local statistics cache
  curPOCStats.clear();

  // Get all the statistics
  // TODO: Could we only retrieve the statistics that are active/displayed?
  for (int i = 0; i <= LIBHMDEC_TU_COEFF_ENERGY_CR; i++)
  {
    libHMDec_info_type type = (libHMDec_info_type)i;
    std::vector<libHMDec_BlockValue> *stats = libHMDEC_get_internal_info(decoder, img, type);
    if (stats != nullptr)
      for (libHMDec_BlockValue b : (*stats))
      {
        curPOCStats[i].addBlockValue(b.x, b.y, b.w, b.h, b.value);
        if (i == LIBHMDEC_CU_INTRA_MODE_LUMA || i == LIBHMDEC_CU_INTRA_MODE_CHROMA)
        {
          // Also add the vecotr to draw
          if (b.value >= 0 && b.value < 35)
          {
            int vecX = (float)vectorTable[b.value][0] * b.w / 4;
            int vecY = (float)vectorTable[b.value][1] * b.w / 4;
            curPOCStats[i].addBlockVector(b.x, b.y, b.w, b.h, vecX, vecY);
          }
        }
      }
  }
}

statisticsData hevcDecoderHM::getStatisticsData(int frameIdx, int typeIdx)
{
  DEBUG_DECHM("hevcDecoderHM::getStatisticsData %s", retrieveStatistics ? "" : "staistics retrievel avtivated");
  if (!retrieveStatistics)
    retrieveStatistics = true;

  if (frameIdx != statsCacheCurPOC)
  {
    if (currentOutputBufferFrameIndex == frameIdx && currentHMPic != NULL)
    {
      // We don't have to decode everything again if we still have a valid pointer to the picture
      cacheStatistics(currentHMPic);
      // The cache now contains the statistics for iPOC
      statsCacheCurPOC = currentOutputBufferFrameIndex;

      return curPOCStats[typeIdx];
    }
    else if (currentOutputBufferFrameIndex == frameIdx)
      // We will have to decode the current frame again to get the internals/statistics
      // This can be done like this:
      currentOutputBufferFrameIndex++;

    loadYUVFrameData(frameIdx);
  }

  return curPOCStats[typeIdx];
}

bool hevcDecoderHM::reloadItemSource()
{
  if (decoderError)
    // Nothing is working, so there is nothing to reset.
    return false;

  // Reset the hevcDecoderHM variables/buffers.
  decError = LIBHMDEC_OK;
  statsCacheCurPOC = -1;
  currentOutputBufferFrameIndex = -1;

  // Re-open the input file. This will reload the bitstream as if it was completely unknown.
  QString fileName = annexBFile.absoluteFilePath();
  parsingError = annexBFile.openFile(fileName);
  return parsingError;
}

void hevcDecoderHM::fillStatisticList(statisticHandler &statSource) const
{
  StatisticsType sliceIdx(0, "Slice Index", "jet", 0, 10);
  statSource.addStatType(sliceIdx);

  StatisticsType predMode(1, "Pred Mode", "jet", 0, 1);
  predMode.valMap.insert(0, "INTER");
  predMode.valMap.insert(1, "INTRA");
  statSource.addStatType(predMode);

  StatisticsType transquantBypass(2, "Transquant Bypass", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(transquantBypass);

  StatisticsType skipFlag(3, "Skip", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(skipFlag);

  StatisticsType partMode(4, "Part Mode", "jet", 0, 7);
  partMode.valMap.insert(0, "SIZE_2Nx2N");
  partMode.valMap.insert(1, "SIZE_2NxN");
  partMode.valMap.insert(2, "SIZE_Nx2N");
  partMode.valMap.insert(3, "SIZE_NxN");
  partMode.valMap.insert(4, "SIZE_2NxnU");
  partMode.valMap.insert(5, "SIZE_2NxnD");
  partMode.valMap.insert(6, "SIZE_nLx2N");
  partMode.valMap.insert(7, "SIZE_nRx2N");

  StatisticsType intraDirY(5, "Intra Mode Luma", "jet", 0, 34);
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

  StatisticsType intraDirC(6, "Intra Mode Chroma", "jet", 0, 34);
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

  StatisticsType rootCBF(7, "Root CBF", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(rootCBF);

  StatisticsType mergeFlag(8, "Merge Flag", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(mergeFlag);

  StatisticsType mergeIndex(2, "Merge Index", "jet", 0, 6);
  statSource.addStatType(mergeIndex);

  StatisticsType uniBiPrediction(9, "Uni Bi Prediction", 0, QColor(0, 0, 255), 1, QColor(255,0,0));
  uniBiPrediction.valMap.insert(0, "Uni");
  uniBiPrediction.valMap.insert(1, "Bi");
  statSource.addStatType(uniBiPrediction);

  StatisticsType refIdx0(10, "Ref POC 0", "col3_bblg", -16, 16);
  statSource.addStatType(refIdx0);

  StatisticsType motionVec0(11, "Motion Vector 0", 4);
  statSource.addStatType(motionVec0);

  StatisticsType refIdx1(12, "Ref POC 1", "col3_bblg", -16, 16);
  statSource.addStatType(refIdx1);

  StatisticsType motionVec1(13, "Motion Vector 0", 4);
  statSource.addStatType(motionVec1);

  StatisticsType cbfY(14, "CBF Y", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(cbfY);

  StatisticsType cbfU(15, "CBF Cb", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(cbfU);

  StatisticsType cbfV(16, "CBF Cr", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(cbfV);

  StatisticsType trSkipY(17, "CBF Y", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(trSkipY);

  StatisticsType trSkipU(18, "CBF Cb", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(trSkipU);

  StatisticsType trSkipV(19, "CBF Cr", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(trSkipV);

  StatisticsType energyY(20, "Coeff Energy Y", 0, QColor(0, 0, 0), 1000, QColor(255,0,0));
  statSource.addStatType(energyY);

  StatisticsType energyU(21, "Coeff Energy Cb", 0, QColor(0, 0, 0), 1000, QColor(255,0,0));
  statSource.addStatType(energyU);

  StatisticsType energyV(22, "Coeff Energy Cr", 0, QColor(0, 0, 0), 1000, QColor(255,0,0));
  statSource.addStatType(energyV);
}

QString hevcDecoderHM::getDecoderName() const
{
  // TODO: For now only return "HM" but in the future, this should also return the version
  return "HM";
}