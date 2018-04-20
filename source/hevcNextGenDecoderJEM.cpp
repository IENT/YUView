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

#include "hevcNextGenDecoderJEM.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include "typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define HEVCNEXTGENDECODERJEM_DEBUG_OUTPUT 0
#if HEVCNEXTGENDECODERJEM_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if HEVCNEXTGENDECODERJEM_DEBUG_OUTPUT == 1
#define DEBUG_DECJEM if(!isCachingDecoder) qDebug
#elif HEVCNEXTGENDECODERJEM_DEBUG_OUTPUT == 2
#define DEBUG_DECJEM if(isCachingDecoder) qDebug
#elif HEVCNEXTGENDECODERJEM_DEBUG_OUTPUT == 3
#define DEBUG_DECJEM if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_DECJEM(fmt,...) ((void)0)
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

hevcNextGenDecoderJEM_Functions::hevcNextGenDecoderJEM_Functions() { memset(this, 0, sizeof(*this)); }

hevcNextGenDecoderJEM::hevcNextGenDecoderJEM(int signalID, bool cachingDecoder) : decoderBase(cachingDecoder)
{
  // We don't support other signals than the reconstruction (yet?)
  Q_UNUSED(signalID);

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("libJEMFile", "").toString());
  settings.endGroup();

  decoder = nullptr;
  currentHMPic = nullptr;
  stateReadingFrames = false;
  currentOutputBufferFrameIndex = -1;

  // Allocate a decoder
  if (!decoderError)
    allocateNewDecoder();

  // Create a new fileSource
  annexBFile.reset(new fileSourceJEMAnnexBFile);
}

hevcNextGenDecoderJEM::hevcNextGenDecoderJEM() : decoderBase(false)
{
  decoder = nullptr;
}

hevcNextGenDecoderJEM::~hevcNextGenDecoderJEM()
{
  freeDecoder();
}

bool hevcNextGenDecoderJEM::openFile(QString fileName, decoderBase *otherDecoder)
{ 
  hevcNextGenDecoderJEM *otherJEMDecoder = dynamic_cast<hevcNextGenDecoderJEM*>(otherDecoder);
  // Open the file, decode the first frame and return if this was successfull.
  if (otherJEMDecoder)
    parsingError = !annexBFile->openFile(fileName, false, otherJEMDecoder->getFileSource());
  else
  {
    // Connect the signal from the file source "signalGetNALUnitInfo", parse the bitstream and disconnect the signal again.
    fileSourceJEMAnnexBFile *jemFile = dynamic_cast<fileSourceJEMAnnexBFile*>(annexBFile.data());
    QMetaObject::Connection c = connect(jemFile, &fileSourceJEMAnnexBFile::signalGetNALUnitInfo, this, &hevcNextGenDecoderJEM::slotGetNALUnitInfo);
    parsingError = !annexBFile->openFile(fileName);
    disconnect(c);
  }

  // After parsing the bitstream using the callback function "slotGetNALUnitInfo" and before actually decoding,
  // we must destroy the existing decoder and create a new one.
  // Delete decoder
  freeDecoder();
  // Create new decoder
  allocateNewDecoder();
  
  return !parsingError && !decoderError;
}

QStringList hevcNextGenDecoderJEM::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList names = 
    is_Q_OS_MAC ?
    QStringList() << "libJEMDecoder.dylib" :
    QStringList() << "libJEMDecoder";

  return names;
}

void hevcNextGenDecoderJEM::resolveLibraryFunctionPointers()
{
  // Get/check function pointers
  if (!resolve(libJEMDec_get_version, "libJEMDec_get_version")) return;
  if (!resolve(libJEMDec_new_decoder, "libJEMDec_new_decoder")) return;
  if (!resolve(libJEMDec_free_decoder, "libJEMDec_free_decoder")) return;
  if (!resolve(libJEMDec_set_SEI_Check, "libJEMDec_set_SEI_Check")) return;
  if (!resolve(libJEMDec_set_max_temporal_layer, "libJEMDec_set_max_temporal_layer")) return;
  if (!resolve(libJEMDec_push_nal_unit, "libJEMDec_push_nal_unit")) return;
  if (!resolve(libJEMDec_get_nal_unit_info, "libJEMDec_get_nal_unit_info")) return;

  if (!resolve(libJEMDec_get_picture, "libJEMDec_get_picture")) return;
  if (!resolve(libJEMDEC_get_POC, "libJEMDEC_get_POC")) return;
  if (!resolve(libJEMDEC_get_picture_width, "libJEMDEC_get_picture_width")) return;
  if (!resolve(libJEMDEC_get_picture_height, "libJEMDEC_get_picture_height")) return;
  if (!resolve(libJEMDEC_get_picture_stride, "libJEMDEC_get_picture_stride")) return;
  if (!resolve(libJEMDEC_get_image_plane, "libJEMDEC_get_image_plane")) return;
  if (!resolve(libJEMDEC_get_chroma_format, "libJEMDEC_get_chroma_format")) return;
  if (!resolve(libJEMDEC_get_internal_bit_depth, "libJEMDEC_get_internal_bit_depth")) return;
  
  if (!resolve(libJEMDEC_get_internal_type_number, "libJEMDEC_get_internal_type_number")) return;
  if (!resolve(libJEMDEC_get_internal_type_name, "libJEMDEC_get_internal_type_name")) return;
  if (!resolve(libJEMDEC_get_internal_type, "libJEMDEC_get_internal_type")) return;
  if (!resolve(libJEMDEC_get_internal_type_max, "libJEMDEC_get_internal_type_max")) return;
  if (!resolve(libJEMDEC_get_internal_type_vector_scaling, "libJEMDEC_get_internal_type_vector_scaling")) return;
  if (!resolve(libJEMDEC_get_internal_type_description, "libJEMDEC_get_internal_type_description")) return;
  if (!resolve(libJEMDEC_get_internal_info, "libJEMDEC_get_internal_info")) return;
  if (!resolve(libJEMDEC_clear_internal_info, "libJEMDEC_clear_internal_info")) return;
  
  // All interbals functions were successfully retrieved
  internalsSupported = true;

  return;
  
  // TODO: could we somehow get the prediction/residual signal?
  // I don't think this is possible without changes to the reference decoder.
  DEBUG_DECJEM("hevcNextGenDecoderJEM::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T hevcNextGenDecoderJEM::resolve(T &fun, const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    setError(QStringLiteral("Error loading the JEM library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

template <typename T> T hevcNextGenDecoderJEM::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void hevcNextGenDecoderJEM::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_DECJEM("hevcNextGenDecoderJEM::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Set some decoder parameters
  libJEMDec_set_SEI_Check(decoder, true);
  libJEMDec_set_max_temporal_layer(decoder, -1);

  // Create new decoder object
  decoder = libJEMDec_new_decoder();
}

void hevcNextGenDecoderJEM::freeDecoder()
{
  if (decoder == nullptr)
    // Nothing to free
    return;

  DEBUG_DECJEM("hevcNextGenDecoderJEM::freeDecoder");

  // Delete decoder
  libJEMDec_error err = libJEMDec_free_decoder(decoder);
  if (err != LIBJEMDEC_OK)
    // Freeing the decoder failed.
    decError = err;

  decoder = nullptr;
  currentHMPic = nullptr;
  stateReadingFrames = false;
  currentOutputBufferFrameIndex = -1;
  lastNALUnit.clear();
}

void hevcNextGenDecoderJEM::slotGetNALUnitInfo(QByteArray nalBytes)
{
  if (!decoder)
    return;

  //    err = libJEMDec_push_nal_unit(decoder, (uint8_t*)ps.data(), ps.size(), false, bNewPicture, checkOutputPictures);
  int poc, picWidth, picHeight, bitDepthLuma, bitDepthChroma;
  bool isRAP, isParameterSet;
  libJEMDec_ChromaFormat chromaFormat;
  libJEMDec_get_nal_unit_info(decoder, (uint8_t*)nalBytes.data(), nalBytes.size(), false, poc, isRAP, isParameterSet, picWidth, picHeight, bitDepthLuma, bitDepthChroma, chromaFormat);

  // 
  if (!frameSize.isValid() && picWidth >= 0 && picHeight >= 0)
    frameSize = QSize(picWidth, picHeight);
  if (pixelFormat == YUV_NUM_SUBSAMPLINGS && chromaFormat != LIBJEMDEC_CHROMA_UNKNOWN)
  {
    if (chromaFormat == LIBJEMDEC_CHROMA_400)
      pixelFormat = YUV_400;
    else if (chromaFormat == LIBJEMDEC_CHROMA_420)
      pixelFormat = YUV_420;
    else if (chromaFormat == LIBJEMDEC_CHROMA_422)
      pixelFormat = YUV_422;
    else if (chromaFormat == LIBJEMDEC_CHROMA_444)
      pixelFormat = YUV_444;
  }
  if (nrBitsC0 == -1 && bitDepthLuma >= 0)
    nrBitsC0 = bitDepthLuma;

  fileSourceJEMAnnexBFile *jemFile = dynamic_cast<fileSourceJEMAnnexBFile*>(annexBFile.data());
  jemFile->setNALUnitInfo(poc, isRAP, isParameterSet);
}

QByteArray hevcNextGenDecoderJEM::loadYUVFrameData(int frameIdx)
{
  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    return currentOutputBuffer;
  }

  DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData Start request %d", frameIdx);

  // We have to decode the requested frame.
  bool seeked = false;
  QList<QByteArray> parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and start decoding from there.
    int seekFrameIdx = annexBFile->getClosestSeekableFrameNumber(frameIdx);

    DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
      DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData Seek to %d", seekFrameIdx);
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

    // Delete decoder and re-create
    freeDecoder();
    allocateNewDecoder();

    // Feed the parameter sets to the decoder
    bool bNewPicture;
    bool checkOutputPictures;
    for (QByteArray ps : parameterSets)
    {
      libJEMDec_error err = libJEMDec_push_nal_unit(decoder, (uint8_t*)ps.data(), ps.size(), false, bNewPicture, checkOutputPictures);
      DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData pushed parameter NAL length %d%s%s - err %d", ps.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "", err);
      // If debugging is off, err is not used.
      Q_UNUSED(err);
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
        libJEMDec_push_nal_unit(decoder, lastNALUnit, lastNALUnit.length(), endOfFile, bNewPicture, checkOutputPictures);
        DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData pushed last NAL length %d%s%s", lastNALUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
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
        libJEMDec_push_nal_unit(decoder, nalUnit, nalUnit.length(), endOfFile, bNewPicture, checkOutputPictures);
        DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData pushed next NAL length %d%s%s", nalUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
        
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
      libJEMDec_picture *pic = libJEMDec_get_picture(decoder);
      while (pic != nullptr)
      {
        // We recieved a picture
        currentOutputBufferFrameIndex++;
        currentHMPic = pic;

        // Check if the chroma format and the frame size matches the already set values (these were read from the annex B file).
        libJEMDec_ChromaFormat fmt = libJEMDEC_get_chroma_format(pic);
        if ((fmt == LIBJEMDEC_CHROMA_400 && pixelFormat != YUV_400) ||
            (fmt == LIBJEMDEC_CHROMA_420 && pixelFormat != YUV_420) ||
            (fmt == LIBJEMDEC_CHROMA_422 && pixelFormat != YUV_422) ||
            (fmt == LIBJEMDEC_CHROMA_444 && pixelFormat != YUV_444))
          DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData recieved frame has different chroma format. Set: %d Pic: %d", pixelFormat, fmt);
        int bits = libJEMDEC_get_internal_bit_depth(pic, LIBJEMDEC_LUMA);
        if (bits != nrBitsC0)
          DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData recieved frame has different bit depth. Set: %d Pic: %d", nrBitsC0, bits);
        QSize picSize = QSize(libJEMDEC_get_picture_width(pic, LIBJEMDEC_LUMA), libJEMDEC_get_picture_height(pic, LIBJEMDEC_LUMA));
        if (picSize != frameSize)
          DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData recieved frame has different size. Set: %dx%d Pic: %dx%d", frameSize.width(), frameSize.height(), picSize.width(), picSize.height());
        
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
          DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData decoded the requested frame %d - POC %d", currentOutputBufferFrameIndex, libJEMDEC_get_POC(pic));

          return currentOutputBuffer;
        }
        else
        {
          DEBUG_DECJEM("hevcNextGenDecoderJEM::loadYUVFrameData decoded the unrequested frame %d - POC %d", currentOutputBufferFrameIndex, libJEMDEC_get_POC(pic));
        }

        // Try to get another picture
        pic = libJEMDec_get_picture(decoder);
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
void hevcNextGenDecoderJEM::copyImgToByteArray(libJEMDec_picture *src, byteArrayAligned &dst)
#else
void hevcNextGenDecoderJEM::copyImgToByteArray(libJEMDec_picture *src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  int nrPlanes = (pixelFormat == YUV_400) ? 1 : 3;

  // Is the output going to be 8 or 16 bit?
  bool outputTwoByte = false;
  for (int c = 0; c < nrPlanes; c++)
  {
    libJEMDec_ColorComponent component = (c == 0) ? LIBJEMDEC_LUMA : (c == 1) ? LIBJEMDEC_CHROMA_U : LIBJEMDEC_CHROMA_V;
    if (libJEMDEC_get_internal_bit_depth(src, component) > 8)
      outputTwoByte = true;
  }

  // How many samples are in each component?
  int outSizeY = libJEMDEC_get_picture_width(src, LIBJEMDEC_LUMA) * libJEMDEC_get_picture_height(src, LIBJEMDEC_LUMA);
  int outSizeCb = (nrPlanes == 1) ? 0 : (libJEMDEC_get_picture_width(src, LIBJEMDEC_CHROMA_U) * libJEMDEC_get_picture_height(src, LIBJEMDEC_CHROMA_U));
  int outSizeCr = (nrPlanes == 1) ? 0 : (libJEMDEC_get_picture_width(src, LIBJEMDEC_CHROMA_V) * libJEMDEC_get_picture_height(src, LIBJEMDEC_CHROMA_V));
  // How many bytes do we need in the output buffer?
  int nrBytesOutput = (outSizeY + outSizeCb + outSizeCr) * (outputTwoByte ? 2 : 1);
  DEBUG_DECJEM("hevcNextGenDecoderJEM::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < nrBytesOutput)
    dst.resize(nrBytesOutput);

  // The source (from HM) is always short (16bit). The destination is a QByteArray so
  // we have to cast it right.
  for (int c = 0; c < nrPlanes; c++)
  {
    libJEMDec_ColorComponent component = (c == 0) ? LIBJEMDEC_LUMA : (c == 1) ? LIBJEMDEC_CHROMA_U : LIBJEMDEC_CHROMA_V;

    const short* img_c = libJEMDEC_get_image_plane(src, component);
    int stride = libJEMDEC_get_picture_stride(src, component);
    
    if (img_c == nullptr)
      return;

    int width = libJEMDEC_get_picture_width(src, component);
    int height = libJEMDEC_get_picture_height(src, component);

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

void hevcNextGenDecoderJEM::cacheStatistics(libJEMDec_picture *img)
{
  if (!wrapperInternalsSupported())
    return;

  DEBUG_DECJEM("hevcNextGenDecoderJEM::cacheStatistics POC %d", libJEMDEC_get_POC(img));

  // Clear the local statistics cache
  curPOCStats.clear();

  // Get all the statistics
  // TODO: Could we only retrieve the statistics that are active/displayed?
  unsigned int nrTypes = libJEMDEC_get_internal_type_number();
  for (unsigned int t = 0; t <= nrTypes; t++)
  {
    bool callAgain;
    do
    {
      // Get a pointer to the data values and how many values in this array are valid.
      unsigned int nrValues;
      libJEMDec_BlockValue *stats = libJEMDEC_get_internal_info(decoder, img, t, nrValues, callAgain);

      libJEMDec_InternalsType statType = libJEMDEC_get_internal_type(t);
      if (stats != nullptr && nrValues > 0)
      {
        for (unsigned int i = 0; i < nrValues; i++)
        {
          libJEMDec_BlockValue b = stats[i];

          if (statType == LIBJEMDEC_TYPE_VECTOR)
            curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, b.value, b.value2);
          else
            curPOCStats[t].addBlockValue(b.x, b.y, b.w, b.h, b.value);
          if (statType == LIBJEMDEC_TYPE_INTRA_DIR)
          {
            // Also add the vecotr to draw
            // TODO: There are more intra modes now. 
            /*if (b.value >= 0 && b.value < 35)
            {
              int vecX = (float)vectorTable[b.value][0] * b.w / 4;
              int vecY = (float)vectorTable[b.value][1] * b.w / 4;
              curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, vecX, vecY);
            }*/
          }
        }
      }
    } while (callAgain); // Continue until the 
  }
}

statisticsData hevcNextGenDecoderJEM::getStatisticsData(int frameIdx, int typeIdx)
{
  DEBUG_DECJEM("hevcNextGenDecoderJEM::getStatisticsData %s", retrieveStatistics ? "" : "staistics retrievel avtivated");
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

bool hevcNextGenDecoderJEM::reloadItemSource()
{
  if (decoderError)
    // Nothing is working, so there is nothing to reset.
    return false;

  // Reset the hevcNextGenDecoderJEM variables/buffers.
  decError = LIBJEMDEC_OK;
  statsCacheCurPOC = -1;
  currentOutputBufferFrameIndex = -1;

  // Re-open the input file. This will reload the bitstream as if it was completely unknown.
  QString fileName = annexBFile->absoluteFilePath();
  parsingError = annexBFile->openFile(fileName);
  return parsingError;
}

void hevcNextGenDecoderJEM::fillStatisticList(statisticHandler &statSource) const
{
  // Ask the decoder how many internals types there are
  unsigned int nrTypes = libJEMDEC_get_internal_type_number();

  for (unsigned int i = 0; i < nrTypes; i++)
  {
    QString name = libJEMDEC_get_internal_type_name(i);
    QString description = libJEMDEC_get_internal_type_description(i);
    libJEMDec_InternalsType statType = libJEMDEC_get_internal_type(i);
    int max = 0;
    if (statType == LIBJEMDEC_TYPE_RANGE || statType == LIBJEMDEC_TYPE_RANGE_ZEROCENTER || statType == LIBJEMDEC_TYPE_INTRA_DIR)
    {
      unsigned int uMax = libJEMDEC_get_internal_type_max(i);
      max = (uMax > INT_MAX) ? INT_MAX : uMax;
    }

    if (statType == LIBJEMDEC_TYPE_FLAG)
    {
      StatisticsType flag(i, name, "jet", 0, 1);
      flag.description = description;
      statSource.addStatType(flag);
    }
    else if (statType == LIBJEMDEC_TYPE_RANGE)
    {
      StatisticsType range(i, name, "jet", 0, max);
      range.description = description;
      statSource.addStatType(range);
    }
    else if (statType == LIBJEMDEC_TYPE_RANGE_ZEROCENTER)
    {
      StatisticsType rangeZero(i, name, "col3_bblg", -max, max);
      rangeZero.description = description;
      statSource.addStatType(rangeZero);
    }
    else if (statType == LIBJEMDEC_TYPE_VECTOR)
    {
      unsigned int scale = libJEMDEC_get_internal_type_vector_scaling(i);
      StatisticsType vec(i, name, scale);
      vec.description = description;
      statSource.addStatType(vec);
    }
    else if (statType == LIBJEMDEC_TYPE_INTRA_DIR)
    {
      
      StatisticsType intraDir(i, name, "jet", 0, max);
      intraDir.description = description;
      intraDir.hasVectorData = true;
      intraDir.renderVectorData = true;
      intraDir.vectorScale = 32;
      // Don't draw the vector values for the intra dir. They don't have actual meaning.
      intraDir.renderVectorDataValues = false;
      statSource.addStatType(intraDir);
    }
  }
}

QString hevcNextGenDecoderJEM::getDecoderName() const
{
  return (decoderError) ? "JEM" : libJEMDec_get_version();
}

bool hevcNextGenDecoderJEM::checkLibraryFile(QString libFilePath, QString &error)
{
  hevcNextGenDecoderJEM testDecoder;

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