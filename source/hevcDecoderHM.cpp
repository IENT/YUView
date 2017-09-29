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
#include <QSettings>
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

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of the pointer will only be accessed through that pointer (and pointers copied from it).
#if __STDC__ != 1
#    define restrict __restrict /* use implementation __ format */
#else
#    ifndef __STDC_VERSION__
#        define restrict __restrict /* use implementation __ format */
#    else
#        if __STDC_VERSION__ < 199901L
#            define restrict __restrict /* use implementation __ format */
#        else
#            /* all ok */
#        endif
#    endif
#endif

hevcDecoderHM_Functions::hevcDecoderHM_Functions() { memset(this, 0, sizeof(*this)); }

hevcDecoderHM::hevcDecoderHM(int signalID, bool cachingDecoder) :
  decoderBase(cachingDecoder)
{
  // For now we don't support different signals (like prediction, residual)
  Q_UNUSED(signalID);

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("libHMFile", "").toString());
  settings.endGroup();

  decoder = nullptr;
  currentHMPic = nullptr;
  stateReadingFrames = false;
  currentOutputBufferFrameIndex = -1;

  // Set the signal to decode (if supported)
  decodeSignal = 0;

  // Create a new hevc annexB file source
  annexBFile.reset(new fileSourceHEVCAnnexBFile);

  // Allocate a decoder
  if (!decoderError)
    allocateNewDecoder();
}

hevcDecoderHM::hevcDecoderHM() : decoderBase(false)
{
  decoder = nullptr;
}

hevcDecoderHM::~hevcDecoderHM()
{
  if (decoder != nullptr)
    libHMDec_free_decoder(decoder);
}

bool hevcDecoderHM::openFile(QString fileName, decoderBase *otherDecoder)
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

QStringList hevcDecoderHM::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList names = 
    is_Q_OS_MAC ?
    QStringList() << "libHMDecoder.dylib" :
    QStringList() << "libHMDecoder";

  return names;
}

void hevcDecoderHM::resolveLibraryFunctionPointers()
{
  // Get/check function pointers
  if (!resolve(libHMDec_get_version, "libHMDec_get_version")) return;
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
  
  if (!resolve(libHMDEC_get_internal_type_number, "libHMDEC_get_internal_type_number")) return;
  if (!resolve(libHMDEC_get_internal_type_name, "libHMDEC_get_internal_type_name")) return;
  if (!resolve(libHMDEC_get_internal_type, "libHMDEC_get_internal_type")) return;
  if (!resolve(libHMDEC_get_internal_type_max, "libHMDEC_get_internal_type_max")) return;
  if (!resolve(libHMDEC_get_internal_type_vector_scaling, "libHMDEC_get_internal_type_vector_scaling")) return;
  if (!resolve(libHMDEC_get_internal_type_description, "libHMDEC_get_internal_type_description")) return;
  if (!resolve(libHMDEC_get_internal_info, "libHMDEC_get_internal_info")) return;
  if (!resolve(libHMDEC_clear_internal_info, "libHMDEC_clear_internal_info")) return;
  
  // All interbals functions were successfully retrieved
  internalsSupported = true;

  return;
  
  // TODO: could we somehow get the prediction/residual signal?
  // I don't think this is possible without changes to the reference decoder.
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
    int seekFrameIdx = annexBFile->getClosestSeekableFrameNumber(frameIdx);

    DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
      DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
  bool endOfFile = annexBFile->atEnd();
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
        libHMDec_push_nal_unit(decoder, lastNALUnit, lastNALUnit.length(), endOfFile, bNewPicture, checkOutputPictures);
        DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData pushed last NAL length %d%s%s", lastNALUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
        // bNewPicture should now be false
        assert(!bNewPicture);
        lastNALUnit.clear();
      }
      else
      {
        // Get the next NAL unit
        QByteArray nalUnit = annexBFile->getNextNALUnit();
        assert(nalUnit.length() > 0);
        endOfFile = annexBFile->atEnd();
        bool endOfFile = annexBFile->atEnd();
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

        // Check if the chroma format and the frame size matches the already set values (these were read from the annex B file).
        libHMDec_ChromaFormat fmt = libHMDEC_get_chroma_format(pic);
        if ((fmt == LIBHMDEC_CHROMA_400 && pixelFormat != YUV_400) ||
            (fmt == LIBHMDEC_CHROMA_420 && pixelFormat != YUV_420) ||
            (fmt == LIBHMDEC_CHROMA_422 && pixelFormat != YUV_422) ||
            (fmt == LIBHMDEC_CHROMA_444 && pixelFormat != YUV_444))
          DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData recieved frame has different chroma format. Set: %d Pic: %d", pixelFormat, fmt);
        int bits = libHMDEC_get_internal_bit_depth(pic, LIBHMDEC_LUMA);
        if (bits != nrBitsC0)
          DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData recieved frame has different bit depth. Set: %d Pic: %d", nrBitsC0, bits);
        QSize picSize = QSize(libHMDEC_get_picture_width(pic, LIBHMDEC_LUMA), libHMDEC_get_picture_height(pic, LIBHMDEC_LUMA));
        if (picSize != frameSize)
          DEBUG_DECHM("hevcDecoderHM::loadYUVFrameData recieved frame has different size. Set: %dx%d Pic: %dx%d", frameSize.width(), frameSize.height(), picSize.width(), picSize.height());
        
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
    
    // If we are currently reading frames (stateReadingFrames true), this code is reached if no more frame could
    // be recieved from the decoder. Switch back to NAL pushing mode only if we are not at the end of the stream.
    if (stateReadingFrames && (!endOfFile || !lastNALUnit.isEmpty()))
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
  int nrPlanes = (pixelFormat == YUV_400) ? 1 : 3;

  // Is the output going to be 8 or 16 bit?
  bool outputTwoByte = false;
  for (int c = 0; c < nrPlanes; c++)
  {
    libHMDec_ColorComponent component = (c == 0) ? LIBHMDEC_LUMA : (c == 1) ? LIBHMDEC_CHROMA_U : LIBHMDEC_CHROMA_V;
    if (libHMDEC_get_internal_bit_depth(src, component) > 8)
      outputTwoByte = true;
  }

  // How many samples are in each component?
  int outSizeY = libHMDEC_get_picture_width(src, LIBHMDEC_LUMA) * libHMDEC_get_picture_height(src, LIBHMDEC_LUMA);
  int outSizeCb = (nrPlanes == 1) ? 0 : (libHMDEC_get_picture_width(src, LIBHMDEC_CHROMA_U) * libHMDEC_get_picture_height(src, LIBHMDEC_CHROMA_U));
  int outSizeCr = (nrPlanes == 1) ? 0 : (libHMDEC_get_picture_width(src, LIBHMDEC_CHROMA_V) * libHMDEC_get_picture_height(src, LIBHMDEC_CHROMA_V));
  // How many bytes do we need in the output buffer?
  int nrBytesOutput = (outSizeY + outSizeCb + outSizeCr) * (outputTwoByte ? 2 : 1);
  DEBUG_DECHM("hevcDecoderHM::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < nrBytesOutput)
    dst.resize(nrBytesOutput);

  // The source (from HM) is always short (16bit). The destination is a QByteArray so
  // we have to cast it right.
  for (int c = 0; c < nrPlanes; c++)
  {
    libHMDec_ColorComponent component = (c == 0) ? LIBHMDEC_LUMA : (c == 1) ? LIBHMDEC_CHROMA_U : LIBHMDEC_CHROMA_V;

    const short* img_c = libHMDEC_get_image_plane(src, component);
    int stride = libHMDEC_get_picture_stride(src, component);
    
    if (img_c == nullptr)
      return;

    int width = libHMDEC_get_picture_width(src, component);
    int height = libHMDEC_get_picture_height(src, component);

    if (outputTwoByte)
    {
      unsigned short * restrict d = (unsigned short*)dst.data();
      if (c > 0)
        d += outSizeY;
      if (c == 2)
        d += outSizeCb;

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          d[x] = (unsigned short)img_c[x];
        }
        img_c += stride;
        d += width;
      }
    }
    else
    {
      unsigned char * restrict d = (unsigned char*)dst.data();
      if (c > 0)
        d += outSizeY;
      if (c == 2)
        d += outSizeCb;

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          d[x] = (char)img_c[x];
        }
        img_c += stride;
        d += width;
      }
    }
  }
}

void hevcDecoderHM::cacheStatistics(libHMDec_picture *img)
{
  if (!wrapperInternalsSupported())
    return;

  DEBUG_DECHM("hevcDecoderHM::cacheStatistics POC %d", libHMDEC_get_POC(img));

  // Clear the local statistics cache
  curPOCStats.clear();

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

  // Get all the statistics
  // TODO: Could we only retrieve the statistics that are active/displayed?
  unsigned int nrTypes = libHMDEC_get_internal_type_number();
  for (unsigned int t = 0; t <= nrTypes; t++)
  {
    bool callAgain;
    do
    {
      // Get a pointer to the data values and how many values in this array are valid.
      unsigned int nrValues;
      libHMDec_BlockValue *stats = libHMDEC_get_internal_info(decoder, img, t, nrValues, callAgain);

      libHMDec_InternalsType statType = libHMDEC_get_internal_type(t);
      if (stats != nullptr && nrValues > 0)
      {
        for (unsigned int i = 0; i < nrValues; i++)
        {
          libHMDec_BlockValue b = stats[i];

          if (statType == LIBHMDEC_TYPE_VECTOR)
            curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, b.value, b.value2);
          else
            curPOCStats[t].addBlockValue(b.x, b.y, b.w, b.h, b.value);
          if (statType == LIBHMDEC_TYPE_INTRA_DIR)
          {
            // Also add the vecotr to draw
            if (b.value >= 0 && b.value < 35)
            {
              int vecX = (float)vectorTable[b.value][0] * b.w / 4;
              int vecY = (float)vectorTable[b.value][1] * b.w / 4;
              curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, vecX, vecY);
            }
          }
        }
      }
    } while (callAgain); // Continue until the 
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
  QString fileName = annexBFile->absoluteFilePath();
  parsingError = annexBFile->openFile(fileName);
  return parsingError;
}

void hevcDecoderHM::fillStatisticList(statisticHandler &statSource) const
{
  // Ask the decoder how many internals types there are
  unsigned int nrTypes = libHMDEC_get_internal_type_number();

  for (unsigned int i = 0; i < nrTypes; i++)
  {
    QString name = libHMDEC_get_internal_type_name(i);
    QString description = libHMDEC_get_internal_type_description(i);
    libHMDec_InternalsType statType = libHMDEC_get_internal_type(i);
    int max = 0;
    if (statType == LIBHMDEC_TYPE_RANGE || statType == LIBHMDEC_TYPE_RANGE_ZEROCENTER)
    {
      unsigned int uMax = libHMDEC_get_internal_type_max(i);
      max = (uMax > INT_MAX) ? INT_MAX : uMax;
    }

    if (statType == LIBHMDEC_TYPE_FLAG)
    {
      StatisticsType flag(i, name, "jet", 0, 1);
      flag.description = description;
      statSource.addStatType(flag);
    }
    else if (statType == LIBHMDEC_TYPE_RANGE)
    {
      StatisticsType range(i, name, "jet", 0, max);
      range.description = description;
      statSource.addStatType(range);
    }
    else if (statType == LIBHMDEC_TYPE_RANGE_ZEROCENTER)
    {
      StatisticsType rangeZero(i, name, "col3_bblg", -max, max);
      rangeZero.description = description;
      statSource.addStatType(rangeZero);
    }
    else if (statType == LIBHMDEC_TYPE_VECTOR)
    {
      unsigned int scale = libHMDEC_get_internal_type_vector_scaling(i);
      StatisticsType vec(i, name, scale);
      vec.description = description;
      statSource.addStatType(vec);
    }
    else if (statType == LIBHMDEC_TYPE_INTRA_DIR)
    {
      StatisticsType intraDir(i, name, "jet", 0, 34);
      intraDir.description = description;
      intraDir.hasVectorData = true;
      intraDir.renderVectorData = true;
      intraDir.vectorScale = 32;
      // Don't draw the vector values for the intra dir. They don't have actual meaning.
      intraDir.renderVectorDataValues = false;
      intraDir.valMap.insert(0, "INTRA_PLANAR");
      intraDir.valMap.insert(1, "INTRA_DC");
      intraDir.valMap.insert(2, "INTRA_ANGULAR_2");
      intraDir.valMap.insert(3, "INTRA_ANGULAR_3");
      intraDir.valMap.insert(4, "INTRA_ANGULAR_4");
      intraDir.valMap.insert(5, "INTRA_ANGULAR_5");
      intraDir.valMap.insert(6, "INTRA_ANGULAR_6");
      intraDir.valMap.insert(7, "INTRA_ANGULAR_7");
      intraDir.valMap.insert(8, "INTRA_ANGULAR_8");
      intraDir.valMap.insert(9, "INTRA_ANGULAR_9");
      intraDir.valMap.insert(10, "INTRA_ANGULAR_10");
      intraDir.valMap.insert(11, "INTRA_ANGULAR_11");
      intraDir.valMap.insert(12, "INTRA_ANGULAR_12");
      intraDir.valMap.insert(13, "INTRA_ANGULAR_13");
      intraDir.valMap.insert(14, "INTRA_ANGULAR_14");
      intraDir.valMap.insert(15, "INTRA_ANGULAR_15");
      intraDir.valMap.insert(16, "INTRA_ANGULAR_16");
      intraDir.valMap.insert(17, "INTRA_ANGULAR_17");
      intraDir.valMap.insert(18, "INTRA_ANGULAR_18");
      intraDir.valMap.insert(19, "INTRA_ANGULAR_19");
      intraDir.valMap.insert(20, "INTRA_ANGULAR_20");
      intraDir.valMap.insert(21, "INTRA_ANGULAR_21");
      intraDir.valMap.insert(22, "INTRA_ANGULAR_22");
      intraDir.valMap.insert(23, "INTRA_ANGULAR_23");
      intraDir.valMap.insert(24, "INTRA_ANGULAR_24");
      intraDir.valMap.insert(25, "INTRA_ANGULAR_25");
      intraDir.valMap.insert(26, "INTRA_ANGULAR_26");
      intraDir.valMap.insert(27, "INTRA_ANGULAR_27");
      intraDir.valMap.insert(28, "INTRA_ANGULAR_28");
      intraDir.valMap.insert(29, "INTRA_ANGULAR_29");
      intraDir.valMap.insert(30, "INTRA_ANGULAR_30");
      intraDir.valMap.insert(31, "INTRA_ANGULAR_31");
      intraDir.valMap.insert(32, "INTRA_ANGULAR_32");
      intraDir.valMap.insert(33, "INTRA_ANGULAR_33");
      intraDir.valMap.insert(34, "INTRA_ANGULAR_34");
      statSource.addStatType(intraDir);
    }
  }
}

QString hevcDecoderHM::getDecoderName() const
{
  return (decoderError) ? "HM" : libHMDec_get_version();
}

bool hevcDecoderHM::checkLibraryFile(QString libFilePath, QString &error)
{
  hevcDecoderHM testDecoder;

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