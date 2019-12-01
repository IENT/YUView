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

#include "decoderVTM.h"

#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include "common/typedef.h"

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define DECODERVTM_DEBUG_OUTPUT 0
#if DECODERVTM_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if DECODERVTM_DEBUG_OUTPUT == 1
#define DEBUG_DECVTM if(!isCachingDecoder) qDebug
#elif DECODERVTM_DEBUG_OUTPUT == 2
#define DEBUG_DECVTM if(isCachingDecoder) qDebug
#elif DECODERVTM_DEBUG_OUTPUT == 3
#define DEBUG_DECVTM if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_DECVTM(fmt,...) ((void)0)
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

decoderVTM_Functions::decoderVTM_Functions()
{ 
  memset(this, 0, sizeof(*this)); 
}

decoderVTM::decoderVTM(int signalID, bool cachingDecoder) :
  decoderBaseSingleLib(cachingDecoder)
{
  // For now we don't support different signals (like prediction, residual)
  Q_UNUSED(signalID);

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("libVTMFile", "").toString());
  settings.endGroup();

  if (decoderState != decoderError)
    allocateNewDecoder();
}

decoderVTM::~decoderVTM()
{
  if (decoder != nullptr)
    libVTMDec_free_decoder(decoder);
}

QStringList decoderVTM::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open the .so file first. 
  // Since this has been compiled for linux it will fail and not even try to open the .dylib.
  // On windows and linux ommitting the extension works
  QStringList names = 
    is_Q_OS_MAC ?
    QStringList() << "libVTMDecoder.dylib" :
    QStringList() << "libVTMDecoder";

  return names;
}

void decoderVTM::resolveLibraryFunctionPointers()
{
  // Get/check function pointers
  if (!resolve(libVTMDec_get_version, "libVTMDec_get_version")) return;
  if (!resolve(libVTMDec_new_decoder, "libVTMDec_new_decoder")) return;
  if (!resolve(libVTMDec_free_decoder, "libVTMDec_free_decoder")) return;
  if (!resolve(libVTMDec_set_SEI_Check, "libVTMDec_set_SEI_Check")) return;
  if (!resolve(libVTMDec_set_max_temporal_layer, "libVTMDec_set_max_temporal_layer")) return;
  if (!resolve(libVTMDec_push_nal_unit, "libVTMDec_push_nal_unit")) return;

  if (!resolve(libVTMDec_get_picture, "libVTMDec_get_picture")) return;
  if (!resolve(libVTMDec_get_POC, "libVTMDec_get_POC")) return;
  if (!resolve(libVTMDec_get_picture_width, "libVTMDec_get_picture_width")) return;
  if (!resolve(libVTMDec_get_picture_height, "libVTMDec_get_picture_height")) return;
  if (!resolve(libVTMDec_get_picture_stride, "libVTMDec_get_picture_stride")) return;
  if (!resolve(libVTMDec_get_image_plane, "libVTMDec_get_image_plane")) return;
  if (!resolve(libVTMDec_get_chroma_format, "libVTMDec_get_chroma_format")) return;
  if (!resolve(libVTMDec_get_internal_bit_depth, "libVTMDec_get_internal_bit_depth")) return;
  
  /*if (!resolve(libVTMDec_get_internal_type_number, "libVTMDec_get_internal_type_number")) return;
  if (!resolve(libVTMDec_get_internal_type_name, "libVTMDec_get_internal_type_name")) return;*/
  /*if (!resolve(libVTMDec_get_internal_type, "libVTMDec_get_internal_type")) return;
  if (!resolve(libVTMDec_get_internal_type_max, "libVTMDec_get_internal_type_max")) return;
  if (!resolve(libVTMDec_get_internal_type_vector_scaling, "libVTMDec_get_internal_type_vector_scaling")) return;
  if (!resolve(libVTMDec_get_internal_type_description, "libVTMDec_get_internal_type_description")) return;
  if (!resolve(libVTMDec_get_internal_info, "libVTMDec_get_internal_info")) return;
  if (!resolve(libVTMDec_clear_internal_info, "libVTMDec_clear_internal_info")) return;*/
  
  // All interbals functions were successfully retrieved
  internalsSupported = true;

  return;
  
  // TODO: could we somehow get the prediction/residual signal?
  // I don't think this is possible without changes to the reference decoder.
  DEBUG_DECVTM("decoderVTM::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T decoderVTM::resolve(T &fun, const char *symbol, bool optional)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr)
  {
    if (!optional)
      setError(QStringLiteral("Error loading the VTM decoder library: Can't find function %1.").arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

void decoderVTM::resetDecoder()
{
  // Delete decoder
  if (decoder != nullptr)
    if (libVTMDec_free_decoder(decoder) != LIBVTMDEC_OK)
      return setError("Reset: Freeing the decoder failded.");

  decoder = nullptr;
  decodedFrameWaiting = false;

  // Create new decoder
  allocateNewDecoder();
}

void decoderVTM::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  DEBUG_DECVTM("decoderVTM::allocateNewDecoder - decodeSignal %d", decodeSignal);

  // Create new decoder object
  decoder = libVTMDec_new_decoder();

  // Set some decoder parameters
  libVTMDec_set_SEI_Check(decoder, true);
  libVTMDec_set_max_temporal_layer(decoder, -1);
}

bool decoderVTM::decodeNextFrame()
{
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_DECVTM("decoderVTM::decodeNextFrame: Wrong decoder state.");
    return false;
  }

  if (currentVTMPic == nullptr)
  {
    DEBUG_DECVTM("decoderVTM::decodeNextFrame: No frame available.");
    return false;
  }

  if (decodedFrameWaiting)
  {
    decodedFrameWaiting = false;
    return true;
  }

  return getNextFrameFromDecoder();
}

bool decoderVTM::getNextFrameFromDecoder()
{
  DEBUG_DECVTM("decoderVTM::getNextFrameFromDecoder");

  currentVTMPic = libVTMDec_get_picture(decoder);
  if (currentVTMPic == nullptr)
  {
    decoderState = decoderNeedsMoreData;
    return false;
  }

  // Check the validity of the picture
  QSize picSize = QSize(libVTMDec_get_picture_width(currentVTMPic, LIBVTMDEC_LUMA), libVTMDec_get_picture_height(currentVTMPic, LIBVTMDEC_LUMA));
  if (!picSize.isValid())
    DEBUG_DECVTM("decoderVTM::getNextFrameFromDecoder got invalid size");
  auto subsampling = convertFromInternalSubsampling(libVTMDec_get_chroma_format(currentVTMPic));
  if (subsampling == YUV_NUM_SUBSAMPLINGS)
    DEBUG_DECVTM("decoderVTM::getNextFrameFromDecoder got invalid chroma format");
  int bitDepth = libVTMDec_get_internal_bit_depth(currentVTMPic, LIBVTMDEC_LUMA);
  if (bitDepth < 8 || bitDepth > 16)
    DEBUG_DECVTM("decoderVTM::getNextFrameFromDecoder got invalid bit depth");
  int poc = libVTMDec_get_POC(currentVTMPic);

  if (!frameSize.isValid() && !formatYUV.isValid())
  {
    // Set the values
    frameSize = picSize;
    formatYUV = yuvPixelFormat(subsampling, bitDepth);
  }
  else
  {
    // Check the values against the previously set values
    if (frameSize != picSize)
      return setErrorB("Recieved a frame of different size");
    if (formatYUV.subsampling != subsampling)
      return setErrorB("Recieved a frame with different subsampling");
    if (formatYUV.bitsPerSample != bitDepth)
      return setErrorB("Recieved a frame with different bit depth");
  }
  
  DEBUG_DECVTM("decoderVTM::getNextFrameFromDecoder got a valid frame wit POC %d", poc);
  currentOutputBuffer.clear();
  return true;
}

bool decoderVTM::pushData(QByteArray &data)
{
  if (decoderState != decoderNeedsMoreData)
  {
    DEBUG_DECVTM("decoderVTM::pushData: Wrong decoder state.");
    return false;
  }

  bool endOfFile = (data.length() == 0);
  if (endOfFile)
    DEBUG_DECVTM("decoderVTM::pushData: Recieved empty packet. Setting EOF.");

  // Push the data of the NAL unit. The function libVTMDec_push_nal_unit can handle data 
  // with a start code and without.
  bool checkOutputPictures = false;
  bool bNewPicture = false;
  libVTMDec_error err = libVTMDec_push_nal_unit(decoder, data, data.length(), endOfFile, bNewPicture, checkOutputPictures);
  if (err != LIBVTMDEC_OK)
    return setErrorB(QString("Error pushing data to decoder (libVTMDec_push_nal_unit) length %1").arg(data.length()));
  DEBUG_DECVTM("decoderVTM::pushData pushed NAL length %d%s%s", data.length(), bNewPicture ? " bNewPicture" : "", checkOutputPictures ? " checkOutputPictures" : "");

  if (checkOutputPictures && getNextFrameFromDecoder())
  {
    decodedFrameWaiting = true;
    decoderState = decoderRetrieveFrames;
    currentOutputBuffer.clear();
  }

  // If bNewPicture is true, the decoder noticed that a new picture starts with this 
  // NAL unit and decoded what it already has (in the original decoder, the bitstream will
  // be rewound). The decoder expects us to push the data again.
  return !bNewPicture;
}

QByteArray decoderVTM::getRawFrameData()
{
  if (currentVTMPic == nullptr)
    return QByteArray();
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_DECVTM("decoderVTM::getRawFrameData: Wrong decoder state.");
    return QByteArray();
  }

  if (currentOutputBuffer.isEmpty())
  {
    // Put image data into buffer
    copyImgToByteArray(currentVTMPic, currentOutputBuffer);
    DEBUG_DECVTM("decoderVTM::getRawFrameData copied frame to buffer");

    if (retrieveStatistics)
      // Get the statistics from the image and put them into the statistics cache
      cacheStatistics(currentVTMPic);
  }

  return currentOutputBuffer;
}

#if SSE_CONVERSION
void decoderVTM::copyImgToByteArray(libVTMDec_picture *src, byteArrayAligned &dst)
#else
void decoderVTM::copyImgToByteArray(libVTMDec_picture *src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  libVTMDec_ChromaFormat fmt = libVTMDec_get_chroma_format(src);
  int nrPlanes = (fmt == LIBVTMDEC_CHROMA_400) ? 1 : 3;

  // VTM always uses 16 bit as the return array
  bool outputTwoByte = (libVTMDec_get_internal_bit_depth(src, LIBVTMDEC_LUMA) > 8);

  // How many samples are in each component?
  int outSizeY = libVTMDec_get_picture_width(src, LIBVTMDEC_LUMA) * libVTMDec_get_picture_height(src, LIBVTMDEC_LUMA);
  int outSizeCb = (nrPlanes == 1) ? 0 : (libVTMDec_get_picture_width(src, LIBVTMDEC_CHROMA_U) * libVTMDec_get_picture_height(src, LIBVTMDEC_CHROMA_U));
  int outSizeCr = (nrPlanes == 1) ? 0 : (libVTMDec_get_picture_width(src, LIBVTMDEC_CHROMA_V) * libVTMDec_get_picture_height(src, LIBVTMDEC_CHROMA_V));
  // How many bytes do we need in the output buffer?
  int nrBytesOutput = (outSizeY + outSizeCb + outSizeCr) * (outputTwoByte ? 2 : 1);
  DEBUG_DECVTM("decoderVTM::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < nrBytesOutput)
    dst.resize(nrBytesOutput);

  // The source (from VTM) is always short (16bit). The destination is a QByteArray so
  // we have to cast it right.
  for (int c = 0; c < nrPlanes; c++)
  {
    libVTMDec_ColorComponent component = (c == 0) ? LIBVTMDEC_LUMA : (c == 1) ? LIBVTMDEC_CHROMA_U : LIBVTMDEC_CHROMA_V;

    const short* img_c = libVTMDec_get_image_plane(src, component);
    int stride = libVTMDec_get_picture_stride(src, component);
    
    if (img_c == nullptr)
      return;

    int width = libVTMDec_get_picture_width(src, component);
    int height = libVTMDec_get_picture_height(src, component);

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
      // Output is one byte per pixel but VTM internally always saves everything in two bytes per pixel
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

void decoderVTM::cacheStatistics(libVTMDec_picture *img)
{
  if (!internalsSupported)
    return;

  DEBUG_DECVTM("decoderVTM::cacheStatistics POC %d", libVTMDec_get_POC(img));

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

  //// Get all the statistics
  //// TODO: Could we only retrieve the statistics that are active/displayed?
  //unsigned int nrTypes = libVTMDec_get_internal_type_number();
  //for (unsigned int t = 0; t <= nrTypes; t++)
  //{
  //  bool callAgain;
  //  do
  //  {
  //    // Get a pointer to the data values and how many values in this array are valid.
  //    unsigned int nrValues;
  //    libVTMDec_BlockValue *stats = libVTMDec_get_internal_info(decoder, img, t, nrValues, callAgain);

  //    libVTMDec_InternalsType statType = libVTMDec_get_internal_type(t);
  //    if (stats != nullptr && nrValues > 0)
  //    {
  //      for (unsigned int i = 0; i < nrValues; i++)
  //      {
  //        libVTMDec_BlockValue b = stats[i];

  //        if (statType == libVTMDec_TYPE_VECTOR)
  //          curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, b.value, b.value2);
  //        else
  //          curPOCStats[t].addBlockValue(b.x, b.y, b.w, b.h, b.value);
  //        if (statType == libVTMDec_TYPE_INTRA_DIR)
  //        {
  //          // Also add the vecotr to draw
  //          if (b.value >= 0 && b.value < 35)
  //          {
  //            int vecX = (float)vectorTable[b.value][0] * b.w / 4;
  //            int vecY = (float)vectorTable[b.value][1] * b.w / 4;
  //            curPOCStats[t].addBlockVector(b.x, b.y, b.w, b.h, vecX, vecY);
  //          }
  //        }
  //      }
  //    }
  //  } while (callAgain); // Continue until the library returns that there is no more to retrive
  //}
}

void decoderVTM::fillStatisticList(statisticHandler &statSource) const
{
  // Ask the decoder how many internals types there are
  // unsigned int nrTypes = libVTMDec_get_internal_type_number();

  //for (unsigned int i = 0; i < nrTypes; i++)
  //{
  //  QString name = libVTMDec_get_internal_type_name(i);
  //  QString description = libVTMDec_get_internal_type_description(i);
  //  libVTMDec_InternalsType statType = libVTMDec_get_internal_type(i);
  //  int max = 0;
  //  if (statType == libVTMDec_TYPE_RANGE || statType == libVTMDec_TYPE_RANGE_ZEROCENTER)
  //  {
  //    unsigned int uMax = libVTMDec_get_internal_type_max(i);
  //    max = (uMax > INT_MAX) ? INT_MAX : uMax;
  //  }

  //  if (statType == libVTMDec_TYPE_FLAG)
  //  {
  //    StatisticsType flag(i, name, "jet", 0, 1);
  //    flag.description = description;
  //    statSource.addStatType(flag);
  //  }
  //  else if (statType == libVTMDec_TYPE_RANGE)
  //  {
  //    StatisticsType range(i, name, "jet", 0, max);
  //    range.description = description;
  //    statSource.addStatType(range);
  //  }
  //  else if (statType == libVTMDec_TYPE_RANGE_ZEROCENTER)
  //  {
  //    StatisticsType rangeZero(i, name, "col3_bblg", -max, max);
  //    rangeZero.description = description;
  //    statSource.addStatType(rangeZero);
  //  }
  //  else if (statType == libVTMDec_TYPE_VECTOR)
  //  {
  //    unsigned int scale = libVTMDec_get_internal_type_vector_scaling(i);
  //    StatisticsType vec(i, name, scale);
  //    vec.description = description;
  //    statSource.addStatType(vec);
  //  }
  //  else if (statType == libVTMDec_TYPE_INTRA_DIR)
  //  {
  //    StatisticsType intraDir(i, name, "jet", 0, 34);
  //    intraDir.description = description;
  //    intraDir.hasVectorData = true;
  //    intraDir.renderVectorData = true;
  //    intraDir.vectorScale = 32;
  //    // Don't draw the vector values for the intra dir. They don't have actual meaning.
  //    intraDir.renderVectorDataValues = false;
  //    intraDir.valMap.insert(0, "INTRA_PLANAR");
  //    intraDir.valMap.insert(1, "INTRA_DC");
  //    intraDir.valMap.insert(2, "INTRA_ANGULAR_2");
  //    intraDir.valMap.insert(3, "INTRA_ANGULAR_3");
  //    intraDir.valMap.insert(4, "INTRA_ANGULAR_4");
  //    intraDir.valMap.insert(5, "INTRA_ANGULAR_5");
  //    intraDir.valMap.insert(6, "INTRA_ANGULAR_6");
  //    intraDir.valMap.insert(7, "INTRA_ANGULAR_7");
  //    intraDir.valMap.insert(8, "INTRA_ANGULAR_8");
  //    intraDir.valMap.insert(9, "INTRA_ANGULAR_9");
  //    intraDir.valMap.insert(10, "INTRA_ANGULAR_10");
  //    intraDir.valMap.insert(11, "INTRA_ANGULAR_11");
  //    intraDir.valMap.insert(12, "INTRA_ANGULAR_12");
  //    intraDir.valMap.insert(13, "INTRA_ANGULAR_13");
  //    intraDir.valMap.insert(14, "INTRA_ANGULAR_14");
  //    intraDir.valMap.insert(15, "INTRA_ANGULAR_15");
  //    intraDir.valMap.insert(16, "INTRA_ANGULAR_16");
  //    intraDir.valMap.insert(17, "INTRA_ANGULAR_17");
  //    intraDir.valMap.insert(18, "INTRA_ANGULAR_18");
  //    intraDir.valMap.insert(19, "INTRA_ANGULAR_19");
  //    intraDir.valMap.insert(20, "INTRA_ANGULAR_20");
  //    intraDir.valMap.insert(21, "INTRA_ANGULAR_21");
  //    intraDir.valMap.insert(22, "INTRA_ANGULAR_22");
  //    intraDir.valMap.insert(23, "INTRA_ANGULAR_23");
  //    intraDir.valMap.insert(24, "INTRA_ANGULAR_24");
  //    intraDir.valMap.insert(25, "INTRA_ANGULAR_25");
  //    intraDir.valMap.insert(26, "INTRA_ANGULAR_26");
  //    intraDir.valMap.insert(27, "INTRA_ANGULAR_27");
  //    intraDir.valMap.insert(28, "INTRA_ANGULAR_28");
  //    intraDir.valMap.insert(29, "INTRA_ANGULAR_29");
  //    intraDir.valMap.insert(30, "INTRA_ANGULAR_30");
  //    intraDir.valMap.insert(31, "INTRA_ANGULAR_31");
  //    intraDir.valMap.insert(32, "INTRA_ANGULAR_32");
  //    intraDir.valMap.insert(33, "INTRA_ANGULAR_33");
  //    intraDir.valMap.insert(34, "INTRA_ANGULAR_34");
  //    statSource.addStatType(intraDir);
  //  }
  //}
}

QString decoderVTM::getDecoderName() const
{
  return (decoderState == decoderError) ? "VTM" : libVTMDec_get_version();
}

bool decoderVTM::checkLibraryFile(QString libFilePath, QString &error)
{
  decoderVTM testDecoder;

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

YUVSubsamplingType decoderVTM::convertFromInternalSubsampling(libVTMDec_ChromaFormat fmt)
{
  if (fmt == LIBVTMDEC_CHROMA_400)
    return YUV_400;
  if (fmt == LIBVTMDEC_CHROMA_420)
    return YUV_420;
  if (fmt == LIBVTMDEC_CHROMA_422)
    return YUV_422;
  if (fmt == LIBVTMDEC_CHROMA_444)
    return YUV_444;

  // Invalid
  return YUV_NUM_SUBSAMPLINGS;
}
