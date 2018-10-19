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

#include "vvcDecoderVTM.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include "typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define vvcDecoderVTM_DEBUG_OUTPUT 0
#if vvcDecoderVTM_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if vvcDecoderVTM_DEBUG_OUTPUT == 1
#define DEBUG_DECJEM if(!isCachingDecoder) qDebug
#elif vvcDecoderVTM_DEBUG_OUTPUT == 2
#define DEBUG_DECJEM if(isCachingDecoder) qDebug
#elif vvcDecoderVTM_DEBUG_OUTPUT == 3
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

vvcDecoderVTM_Functions::vvcDecoderVTM_Functions() { memset(this, 0, sizeof(*this)); }

vvcDecoderVTM::vvcDecoderVTM(int signalID, bool cachingDecoder) : decoderBase(cachingDecoder)
{
  // We don't support other signals than the reconstruction (yet?)
  Q_UNUSED(signalID);

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("VVCDecoderLib", "").toString());
  settings.endGroup();

  decoder = nullptr;
  currentHMPic = nullptr;
  stateReadingFrames = false;
  currentOutputBufferFrameIndex = -1;

  // Allocate a decoder
  if (!decoderError)
    allocateNewDecoder();

  // Create a new fileSource
  annexBFile.reset(new fileSourceVVCAnnexBFile);
}

vvcDecoderVTM::vvcDecoderVTM() : decoderBase(false)
{
  decoder = nullptr;
}

vvcDecoderVTM::~vvcDecoderVTM()
{
  freeDecoder();
}

bool vvcDecoderVTM::openFile(QString fileName, decoderBase *otherDecoder)
{ 
  vvcDecoderVTM *otherVVCDecoder = dynamic_cast<vvcDecoderVTM*>(otherDecoder);
  // Open the file, decode the first frame and return if this was successfull.
  if (otherVVCDecoder)
    parsingError = !annexBFile->openFile(fileName, false, otherVVCDecoder->getFileSource());
  else
  {
    // Connect the signal from the file source "signalGetNALUnitInfo", parse the bitstream and disconnect the signal again.
    fileSourceVVCAnnexBFile *file = dynamic_cast<fileSourceVVCAnnexBFile*>(annexBFile.data());
    QMetaObject::Connection c = connect(file, &fileSourceVVCAnnexBFile::signalGetNALUnitInfo, this, &vvcDecoderVTM::slotGetNALUnitInfo);
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

QStringList vvcDecoderVTM::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  QStringList names = 
    is_Q_OS_MAC ?
    QStringList() << "VVCDecoderLib.dylib" :
    QStringList() << "VVCDecoderLib";

  return names;
}

void vvcDecoderVTM::resolveLibraryFunctionPointers()
{
  // Get/check function pointers
  if (!resolve(libVVCDec_get_version,             "libVVCDec_get_version")) return;
  if (!resolve(libVVCDec_new_decoder,             "libVVCDec_new_decoder")) return;
  if (!resolve(libVVCDec_free_decoder,            "libVVCDec_free_decoder")) return;
  if (!resolve(libVVCDec_set_SEI_Check,           "libVVCDec_set_SEI_Check")) return;
  if (!resolve(libVVCDec_set_max_temporal_layer,  "libVVCDec_set_max_temporal_layer")) return;
  if (!resolve(libVVCDec_push_nal_unit,           "libVVCDec_push_nal_unit")) return;
  if (!resolve(libVVCDec_get_nal_unit_info,       "libVVCDec_get_nal_unit_info")) return;

  if (!resolve(libVVCDec_get_picture,             "libVVCDec_get_picture")) return;
  if (!resolve(libVVCDEC_get_POC,                 "libVVCDEC_get_POC")) return;
  if (!resolve(libVVCDEC_get_picture_width,       "libVVCDEC_get_picture_width")) return;
  if (!resolve(libVVCDEC_get_picture_height,      "libVVCDEC_get_picture_height")) return;
  if (!resolve(libVVCDEC_get_picture_stride,      "libVVCDEC_get_picture_stride")) return;
  if (!resolve(libVVCDEC_get_image_plane,         "libVVCDEC_get_image_plane")) return;
  if (!resolve(libVVCDEC_get_chroma_format,       "libVVCDEC_get_chroma_format")) return;
  if (!resolve(libVVCDEC_get_internal_bit_depth,  "libVVCDEC_get_internal_bit_depth")) return;
  
  if (!resolve(libVVCDEC_get_internal_type_number, "libVVCDEC_get_internal_type_number")) return;
  if (!resolve(libVVCDEC_get_internal_type_name, "libVVCDEC_get_internal_type_name")) return;
  if (!resolve(libVVCDEC_get_internal_type, "libVVCDEC_get_internal_type")) return;
  if (!resolve(libVVCDEC_get_internal_type_max, "libVVCDEC_get_internal_type_max")) return;
  if (!resolve(libVVCDEC_get_internal_type_vector_scaling, "libVVCDEC_get_internal_type_vector_scaling")) return;
  if (!resolve(libVVCDEC_get_internal_type_description, "libVVCDEC_get_internal_type_description")) return;
  if (!resolve(libVVCDEC_get_internal_info, "libVVCDEC_get_internal_info")) return;
  if (!resolve(libVVCDEC_clear_internal_info, "libVVCDEC_clear_internal_info")) return;
  
  // All internals functions were successfully retrieved
  internalsSupported = true;

  return;
  
  // TODO: could we somehow get the prediction/residual signal?
  // I don't think this is possible without changes to the reference decoder.
  DEBUG_DECJEM("vvcDecoderVTM::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T vvcDecoderVTM::resolve(T &fun, const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    setError(QStringLiteral("Error loading the JEM library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

template <typename T> T vvcDecoderVTM::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void vvcDecoderVTM::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_DECJEM("vvcDecoderVTM::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Set some decoder parameters
  libVVCDec_set_SEI_Check(decoder, true);
  libVVCDec_set_max_temporal_layer(decoder, -1);

  // Create new decoder object
  decoder = libVVCDec_new_decoder();
}

void vvcDecoderVTM::freeDecoder()
{
  if (decoder == nullptr)
    // Nothing to free
    return;

  DEBUG_DECJEM("vvcDecoderVTM::freeDecoder");

  // Delete decoder
  libVVCDec_error err = libVVCDec_free_decoder(decoder);
  if (err != libVVCDEC_OK)
    // Freeing the decoder failed.
    decError = err;

  decoder = nullptr;
  currentHMPic = nullptr;
  stateReadingFrames = false;
  currentOutputBufferFrameIndex = -1;
  lastNALUnit.clear();
}

void vvcDecoderVTM::slotGetNALUnitInfo(QByteArray nalBytes)
{
  if (!decoder)
    return;

  //    err = libVVCDec_push_nal_unit(decoder, (uint8_t*)ps.data(), ps.size(), false, bNewPicture, checkOutputPictures);
  int poc, picWidth, picHeight, bitDepthLuma, bitDepthChroma;
  bool isRAP, isParameterSet;
  libVVCDec_ChromaFormat chromaFormat;
  libVVCDec_get_nal_unit_info(decoder, (uint8_t*)nalBytes.data(), nalBytes.size(), false, poc, isRAP, isParameterSet, picWidth, picHeight, bitDepthLuma, bitDepthChroma, chromaFormat);

  // 
  if (!frameSize.isValid() && picWidth >= 0 && picHeight >= 0)
    frameSize = QSize(picWidth, picHeight);
  if (pixelFormat == YUV_NUM_SUBSAMPLINGS && chromaFormat != libVVCDEC_CHROMA_UNKNOWN)
  {
    if (chromaFormat == libVVCDEC_CHROMA_400)
      pixelFormat = YUV_400;
    else if (chromaFormat == libVVCDEC_CHROMA_420)
      pixelFormat = YUV_420;
    else if (chromaFormat == libVVCDEC_CHROMA_422)
      pixelFormat = YUV_422;
    else if (chromaFormat == libVVCDEC_CHROMA_444)
      pixelFormat = YUV_444;
  }
  if (nrBitsC0 == -1 && bitDepthLuma >= 0)
    nrBitsC0 = bitDepthLuma;

  fileSourceVVCAnnexBFile *file = dynamic_cast<fileSourceVVCAnnexBFile*>(annexBFile.data());
  file->setNALUnitInfo(poc, isRAP, isParameterSet);
}

QByteArray vvcDecoderVTM::loadYUVFrameData(int frameIdx)
{
  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    return currentOutputBuffer;
  }

  DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData Start request %d", frameIdx);

  // We have to decode the requested frame.
  bool seeked = false;
  QList<QByteArray> parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and start decoding from there.
    int seekFrameIdx = annexBFile->getClosestSeekableFrameNumber(frameIdx);

    DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
      DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData Seek to %d", seekFrameIdx);
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
      libVVCDec_error err = libVVCDec_push_nal_unit(decoder, (uint8_t*)ps.data(), ps.size(), false, bNewPicture, checkOutputPictures);
      DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData pushed parameter NAL length %d%s%s - err %d", ps.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "", err);
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
      bool bNewPicture = false;
      bool checkOutputPictures = false;
      // The picture pointer will be invalid when we push the next NAL unit to the decoder.
      currentHMPic = nullptr;

      if (!lastNALUnit.isEmpty())
      {
        libVVCDec_push_nal_unit(decoder, lastNALUnit, lastNALUnit.length(), endOfFile, bNewPicture, checkOutputPictures);
        DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData pushed last NAL length %d%s%s", lastNALUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
        // bNewPicture should now be false
        //assert(!bNewPicture);
        lastNALUnit.clear();
      }
      else
      {
        // Get the next NAL unit
        QByteArray nalUnit = annexBFile->getNextNALUnit();
        assert(nalUnit.length() > 0);
        endOfFile = annexBFile->atEnd();
        bool endOfFile = annexBFile->atEnd();
        libVVCDec_push_nal_unit(decoder, nalUnit, nalUnit.length(), endOfFile, bNewPicture, checkOutputPictures);
        DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData pushed next NAL length %d%s%s", nalUnit.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");
        
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
      libVVCDec_picture *pic = libVVCDec_get_picture(decoder);
      while (pic != nullptr)
      {
        // We recieved a picture
        currentOutputBufferFrameIndex++;
        currentHMPic = pic;

        // Check if the chroma format and the frame size matches the already set values (these were read from the annex B file).
        libVVCDec_ChromaFormat fmt = libVVCDEC_get_chroma_format(pic);
        if ((fmt == libVVCDEC_CHROMA_400 && pixelFormat != YUV_400) ||
            (fmt == libVVCDEC_CHROMA_420 && pixelFormat != YUV_420) ||
            (fmt == libVVCDEC_CHROMA_422 && pixelFormat != YUV_422) ||
            (fmt == libVVCDEC_CHROMA_444 && pixelFormat != YUV_444))
          DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData recieved frame has different chroma format. Set: %d Pic: %d", pixelFormat, fmt);
        int bits = libVVCDEC_get_internal_bit_depth(pic, libVVCDEC_LUMA);
        if (bits != nrBitsC0)
          DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData recieved frame has different bit depth. Set: %d Pic: %d", nrBitsC0, bits);
        QSize picSize = QSize(libVVCDEC_get_picture_width(pic, libVVCDEC_LUMA), libVVCDEC_get_picture_height(pic, libVVCDEC_LUMA));
        if (picSize != frameSize)
          DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData recieved frame has different size. Set: %dx%d Pic: %dx%d", frameSize.width(), frameSize.height(), picSize.width(), picSize.height());
        
        if (currentOutputBufferFrameIndex == frameIdx)
        {
          // This is the frame that we want to decode

          // Put image data into buffer
          copyImgToByteArray(pic, currentOutputBuffer);

          // Picture decoded
          DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData decoded the requested frame %d - POC %d", currentOutputBufferFrameIndex, libVVCDEC_get_POC(pic));

          if (retrieveStatistics)
          {
            // Get the statistics from the image and put them into the statistics cache
            cacheStatistics(pic);

            // The cache now contains the statistics for iPOC
            statsCacheCurPOC = currentOutputBufferFrameIndex;
          }

          return currentOutputBuffer;
        }
        else
        {
          DEBUG_DECJEM("vvcDecoderVTM::loadYUVFrameData decoded the unrequested frame %d - POC %d", currentOutputBufferFrameIndex, libVVCDEC_get_POC(pic));
        }

        // Try to get another picture
        pic = libVVCDec_get_picture(decoder);
      }
    }
    
    // If we are currently reading frames (stateReadingFrames true), this code is reached if no more frame could
    // be recieved from the decoder. Switch back to NAL pushing mode only if we are not at the end of the stream.
    if (stateReadingFrames && (!endOfFile || !lastNALUnit.isEmpty()))
      stateReadingFrames = false;
  }
  
  return QByteArray();
}

void vvcDecoderVTM::copyImgToByteArray(libVVCDec_picture *src, QByteArray &dst)
{
  // How many image planes are there?
  int nrPlanes = (pixelFormat == YUV_400) ? 1 : 3;

  // Is the output going to be 8 or 16 bit?
  bool outputTwoByte = false;
  for (int c = 0; c < nrPlanes; c++)
  {
    libVVCDec_ColorComponent component = (c == 0) ? libVVCDEC_LUMA : (c == 1) ? libVVCDEC_CHROMA_U : libVVCDEC_CHROMA_V;
    if (libVVCDEC_get_internal_bit_depth(src, component) > 8)
      outputTwoByte = true;
  }

  // How many samples are in each component?
  int outSizeY = libVVCDEC_get_picture_width(src, libVVCDEC_LUMA) * libVVCDEC_get_picture_height(src, libVVCDEC_LUMA);
  int outSizeCb = (nrPlanes == 1) ? 0 : (libVVCDEC_get_picture_width(src, libVVCDEC_CHROMA_U) * libVVCDEC_get_picture_height(src, libVVCDEC_CHROMA_U));
  int outSizeCr = (nrPlanes == 1) ? 0 : (libVVCDEC_get_picture_width(src, libVVCDEC_CHROMA_V) * libVVCDEC_get_picture_height(src, libVVCDEC_CHROMA_V));
  // How many bytes do we need in the output buffer?
  int nrBytesOutput = (outSizeY + outSizeCb + outSizeCr) * (outputTwoByte ? 2 : 1);
  DEBUG_DECJEM("vvcDecoderVTM::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < nrBytesOutput)
    dst.resize(nrBytesOutput);

  // The source (from HM) is always short (16bit). The destination is a QByteArray so
  // we have to cast it right.
  for (int c = 0; c < nrPlanes; c++)
  {
    libVVCDec_ColorComponent component = (c == 0) ? libVVCDEC_LUMA : (c == 1) ? libVVCDEC_CHROMA_U : libVVCDEC_CHROMA_V;

    const short* img_c = libVVCDEC_get_image_plane(src, component);
    int stride = libVVCDEC_get_picture_stride(src, component);
    
    if (img_c == nullptr)
      return;

    int width = libVVCDEC_get_picture_width(src, component);
    int height = libVVCDEC_get_picture_height(src, component);

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

bool vvcDecoderVTM::reloadItemSource()
{
  if (decoderError)
    // Nothing is working, so there is nothing to reset.
    return false;

  // Reset the vvcDecoderVTM variables/buffers.
  decError = libVVCDEC_OK;
  statsCacheCurPOC = -1;
  currentOutputBufferFrameIndex = -1;

  // Re-open the input file. This will reload the bitstream as if it was completely unknown.
  QString fileName = annexBFile->absoluteFilePath();
  parsingError = annexBFile->openFile(fileName);
  return parsingError;
}

void vvcDecoderVTM::cacheStatistics(libVVCDec_picture *img)
{
  if (!wrapperInternalsSupported())
    return;

  DEBUG_DECJEM("vvcDecoderVTM::cacheStatistics POC %d", libVVCDEC_get_POC(img));

  // Clear the local statistics cache
  curPOCStats.clear();

  // Get all the statistics
  // TODO: Could we only retrieve the statistics that are active/displayed?
  unsigned int nrTypes = libVVCDEC_get_internal_type_number();
  for (unsigned int t = 0; t <= nrTypes; t++)
  {
    bool callAgain;
    do
    {
      // Get a pointer to the data values and how many values in this array are valid.
      unsigned int nrValues;
      libVVCDec_BlockValue *stats = libVVCDEC_get_internal_info(decoder, img, t, nrValues, callAgain);

      libVVCDec_InternalsType statType = libVVCDEC_get_internal_type(t);
      if (stats != nullptr && nrValues > 0)
      {
        for (unsigned int i = 0; i < nrValues; i++)
        {
          libVVCDec_BlockValue b = stats[i];
          if (statType == libVVCDEC_TYPE_VECTOR)
            curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, b.value, b.value2);
          else
            curPOCStats[t].addBlockValue(b.x, b.y, b.w, b.h, b.value);
        }
      }
    } while (callAgain); // Continue until there are no more values to cache
  }
}

void vvcDecoderVTM::fillStatisticList(statisticHandler &statSource) const
{
  // Ask the decoder how many internals types there are
  unsigned int nrTypes = libVVCDEC_get_internal_type_number();

  for (unsigned int i = 0; i < nrTypes; i++)
  {
    QString name = libVVCDEC_get_internal_type_name(i);
    QString description = libVVCDEC_get_internal_type_description(i);
    libVVCDec_InternalsType statType = libVVCDEC_get_internal_type(i);
    int max = 0;
    if (statType == libVVCDEC_TYPE_RANGE || statType == libVVCDEC_TYPE_RANGE_ZEROCENTER || statType == libVVCDEC_TYPE_INTRA_DIR)
    {
      unsigned int uMax = libVVCDEC_get_internal_type_max(i);
      max = (uMax > INT_MAX) ? INT_MAX : uMax;
    }

    if (statType == libVVCDEC_TYPE_FLAG)
    {
      StatisticsType flag(i, name, "jet", 0, 1);
      flag.description = description;
      statSource.addStatType(flag);
    }
    else if (statType == libVVCDEC_TYPE_RANGE)
    {
      StatisticsType range(i, name, "jet", 0, max);
      range.description = description;
      statSource.addStatType(range);
    }
    else if (statType == libVVCDEC_TYPE_RANGE_ZEROCENTER)
    {
      StatisticsType rangeZero(i, name, "col3_bblg", -max, max);
      rangeZero.description = description;
      statSource.addStatType(rangeZero);
    }
    else if (statType == libVVCDEC_TYPE_VECTOR)
    {
      unsigned int scale = libVVCDEC_get_internal_type_vector_scaling(i);
      StatisticsType vec(i, name, scale);
      vec.description = description;
      statSource.addStatType(vec);
    }
    else if (statType == libVVCDEC_TYPE_INTRA_DIR)
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

statisticsData vvcDecoderVTM::getStatisticsData(int frameIdx, int typeIdx)
{
  DEBUG_DECJEM("vvcDecoderVTM::getStatisticsData %s", retrieveStatistics ? "" : "staistics retrievel avtivated");
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


QString vvcDecoderVTM::getDecoderName() const
{
  return (decoderError) ? "VVC" : libVVCDec_get_version();
}

bool vvcDecoderVTM::checkLibraryFile(QString libFilePath, QString &error)
{
  vvcDecoderVTM testDecoder;

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