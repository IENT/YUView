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

#include "decoderHM.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <cstring>

#include <common/Functions.h>
#include <common/Typedef.h>

namespace decoder
{

// Debug the decoder ( 0:off 1:interactive decoder only 2:caching decoder only 3:both)
#define DECODERHM_DEBUG_OUTPUT 0
#if DECODERHM_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if DECODERHM_DEBUG_OUTPUT == 1
#define DEBUG_DECHM                                                                                \
  if (!isCachingDecoder)                                                                           \
  qDebug
#elif DECODERHM_DEBUG_OUTPUT == 2
#define DEBUG_DECHM                                                                                \
  if (isCachingDecoder)                                                                            \
  qDebug
#elif DECODERHM_DEBUG_OUTPUT == 3
#define DEBUG_DECHM                                                                                \
  if (isCachingDecoder)                                                                            \
    qDebug("c:");                                                                                  \
  else                                                                                             \
    qDebug("i:");                                                                                  \
  qDebug
#endif
#else
#define DEBUG_DECHM(fmt, ...) ((void)0)
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

using Subsampling = video::yuv::Subsampling;

namespace
{

Subsampling convertFromInternalSubsampling(libHMDec_ChromaFormat fmt)
{
  if (fmt == LIBHMDEC_CHROMA_400)
    return Subsampling::YUV_400;
  if (fmt == LIBHMDEC_CHROMA_420)
    return Subsampling::YUV_420;
  if (fmt == LIBHMDEC_CHROMA_422)
    return Subsampling::YUV_422;

  return Subsampling::UNKNOWN;
}

} // namespace

decoderHM::decoderHM(int, bool cachingDecoder) : decoderBaseSingleLib(cachingDecoder)
{
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  QSettings settings;
  settings.beginGroup("Decoders");
  loadDecoderLibrary(settings.value("libHMFile", "").toString());
  settings.endGroup();

  if (decoderState != DecoderState::Error)
    allocateNewDecoder();
}

decoderHM::~decoderHM()
{
  if (decoder != nullptr)
    this->lib.libHMDec_free_decoder(decoder);
}

QStringList decoderHM::getLibraryNames() const
{
  // If the file name is not set explicitly, QLibrary will try to open the .so file first.
  // Since this has been compiled for linux it will fail and not even try to open the .dylib.
  // On windows and linux ommitting the extension works
  auto names =
      is_Q_OS_MAC ? QStringList() << "libHMDecoder.dylib" : QStringList() << "libHMDecoder";

  return names;
}

void decoderHM::resolveLibraryFunctionPointers()
{
  // Get/check function pointers
  if (!this->resolve(this->lib.libHMDec_get_version, "libHMDec_get_version"))
    return;
  if (!this->resolve(this->lib.libHMDec_new_decoder, "libHMDec_new_decoder"))
    return;
  if (!this->resolve(this->lib.libHMDec_free_decoder, "libHMDec_free_decoder"))
    return;
  if (!this->resolve(this->lib.libHMDec_set_SEI_Check, "libHMDec_set_SEI_Check"))
    return;
  if (!this->resolve(this->lib.libHMDec_set_max_temporal_layer, "libHMDec_set_max_temporal_layer"))
    return;
  if (!this->resolve(this->lib.libHMDec_push_nal_unit, "libHMDec_push_nal_unit"))
    return;

  if (!this->resolve(this->lib.libHMDec_get_picture, "libHMDec_get_picture"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_POC, "libHMDEC_get_POC"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_picture_width, "libHMDEC_get_picture_width"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_picture_height, "libHMDEC_get_picture_height"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_picture_stride, "libHMDEC_get_picture_stride"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_image_plane, "libHMDEC_get_image_plane"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_chroma_format, "libHMDEC_get_chroma_format"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_bit_depth, "libHMDEC_get_internal_bit_depth"))
    return;

  if (!this->resolve(this->lib.libHMDEC_get_internal_type_number,
                     "libHMDEC_get_internal_type_number"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_type_name, "libHMDEC_get_internal_type_name"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_type, "libHMDEC_get_internal_type"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_type_max, "libHMDEC_get_internal_type_max"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_type_vector_scaling,
                     "libHMDEC_get_internal_type_vector_scaling"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_type_description,
                     "libHMDEC_get_internal_type_description"))
    return;
  if (!this->resolve(this->lib.libHMDEC_get_internal_info, "libHMDEC_get_internal_info"))
    return;
  if (!this->resolve(this->lib.libHMDEC_clear_internal_info, "libHMDEC_clear_internal_info"))
    return;

  this->statisticsSupported = true;

  return;

  // TODO: could we somehow get the prediction/residual signal?
  // I don't think this is possible without changes to the reference decoder.
  DEBUG_DECHM("decoderHM::loadDecoderLibrary - prediction/residual internals found");
}

template <typename T> T decoderHM::resolve(T &fun, const char *symbol, bool optional)
{
  auto ptr = this->library.resolve(symbol);
  if (!ptr)
  {
    if (!optional)
      setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.")
                   .arg(symbol));
    return nullptr;
  }

  return fun = reinterpret_cast<T>(ptr);
}

void decoderHM::resetDecoder()
{
  if (this->decoder != nullptr)
    if (this->lib.libHMDec_free_decoder(this->decoder) != LIBHMDEC_OK)
      return setError("Reset: Freeing the decoder failed.");

  decoderBase::resetDecoder();
  this->decoder             = nullptr;
  this->decodedFrameWaiting = false;

  this->allocateNewDecoder();
}

void decoderHM::allocateNewDecoder()
{
  if (this->decoder != nullptr)
    return;

  DEBUG_DECHM("decoderHM::allocateNewDecoder - decodeSignal %d", this->decodeSignal);

  // Create new decoder object
  this->decoder = this->lib.libHMDec_new_decoder();

  // Set some decoder parameters
  this->lib.libHMDec_set_SEI_Check(this->decoder, true);
  this->lib.libHMDec_set_max_temporal_layer(this->decoder, -1);

  decoderBase::resetDecoder();
  this->decodedFrameWaiting = false;
}

bool decoderHM::decodeNextFrame()
{
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_DECHM("decoderHM::decodeNextFrame: Wrong decoder state.");
    return false;
  }

  if (this->currentHMPic == nullptr)
  {
    DEBUG_DECHM("decoderHM::decodeNextFrame: No frame available.");
    return false;
  }

  if (this->decodedFrameWaiting)
  {
    this->decodedFrameWaiting = false;
    return true;
  }

  return this->getNextFrameFromDecoder();
}

bool decoderHM::getNextFrameFromDecoder()
{
  DEBUG_DECHM("decoderHM::getNextFrameFromDecoder");

  this->currentHMPic = this->lib.libHMDec_get_picture(decoder);
  if (this->currentHMPic == nullptr)
  {
    decoderState = DecoderState::NeedsMoreData;
    return false;
  }

  // Check the validity of the picture
  auto picSize = Size(this->lib.libHMDEC_get_picture_width(this->currentHMPic, LIBHMDEC_LUMA),
                      this->lib.libHMDEC_get_picture_height(this->currentHMPic, LIBHMDEC_LUMA));
  if (!picSize.isValid())
    DEBUG_DECHM("decoderHM::getNextFrameFromDecoder got invalid size");
  auto subsampling =
      convertFromInternalSubsampling(this->lib.libHMDEC_get_chroma_format(this->currentHMPic));
  if (subsampling == video::yuv::Subsampling::UNKNOWN)
    DEBUG_DECHM("decoderHM::getNextFrameFromDecoder got invalid chroma format");
  auto bitDepth = functions::clipToUnsigned(
      this->lib.libHMDEC_get_internal_bit_depth(this->currentHMPic, LIBHMDEC_LUMA));
  if (bitDepth < 8 || bitDepth > 16)
    DEBUG_DECHM("decoderHM::getNextFrameFromDecoder got invalid bit depth");

  if (!this->frameSize.isValid() && !this->formatYUV.isValid())
  {
    // Set the values
    this->frameSize = picSize;
    this->formatYUV = video::yuv::PixelFormatYUV(subsampling, bitDepth);
  }
  else
  {
    // Check the values against the previously set values
    if (this->frameSize != picSize)
      return setErrorB("Received a frame of different size");
    if (this->formatYUV.getSubsampling() != subsampling)
      return setErrorB("Received a frame with different subsampling");
    if (this->formatYUV.getBitsPerSample() != bitDepth)
      return setErrorB("Received a frame with different bit depth");
  }

  DEBUG_DECHM("decoderHM::getNextFrameFromDecoder got a valid frame");
  return true;
}

bool decoderHM::pushData(QByteArray &data)
{
  if (this->decoderState != DecoderState::NeedsMoreData)
  {
    DEBUG_DECHM("decoderHM::pushData: Wrong decoder state.");
    return false;
  }

  bool endOfFile = (data.length() == 0);
  if (endOfFile)
    DEBUG_DECHM("decoderFFmpeg::pushData: Received empty packet. Setting EOF.");

  // Push the data of the NAL unit. The function libHMDec_push_nal_unit can handle data
  // with a start code and without.
  bool checkOutputPictures = false;
  bool bNewPicture         = false;
  auto err                 = this->lib.libHMDec_push_nal_unit(
      decoder, data, data.length(), endOfFile, bNewPicture, checkOutputPictures);
  if (err != LIBHMDEC_OK)
    return setErrorB(QString("Error pushing data to decoder (libHMDec_push_nal_unit) length %1")
                         .arg(data.length()));
  DEBUG_DECHM("decoderHM::pushData pushed NAL length %d%s%s",
              data.length(),
              bNewPicture ? " bNewPicture" : "",
              checkOutputPictures ? " checkOutputPictures" : "");

  if (checkOutputPictures && this->getNextFrameFromDecoder())
  {
    this->decodedFrameWaiting = true;
    this->decoderState        = DecoderState::RetrieveFrames;
    this->currentOutputBuffer.clear();
  }

  // If bNewPicture is true, the decoder noticed that a new picture starts with this
  // NAL unit and decoded what it already has (in the original decoder, the bitstream will
  // be rewound). The decoder expects us to push the data again.
  return !bNewPicture;
}

stats::StatisticsTypes decoderHM::getStatisticsTypes() const
{
  using namespace stats::color;

  stats::StatisticsTypes types;

  auto nrTypes = this->lib.libHMDEC_get_internal_type_number();

  for (auto i = 0u; i < nrTypes; i++)
  {
    auto name        = QString(this->lib.libHMDEC_get_internal_type_name(i));
    auto description = QString(this->lib.libHMDEC_get_internal_type_description(i));
    auto statType    = this->lib.libHMDEC_get_internal_type(i);
    int  max         = 0;
    if (statType == LIBHMDEC_TYPE_RANGE || statType == LIBHMDEC_TYPE_RANGE_ZEROCENTER)
    {
      auto uMax = this->lib.libHMDEC_get_internal_type_max(i);
      max       = (uMax > INT_MAX) ? INT_MAX : uMax;
    }

    if (statType == LIBHMDEC_TYPE_FLAG)
    {
      stats::StatisticsType flag(i, name, ColorMapper({0, 1}, PredefinedType::Jet));
      flag.description = description;
      types.push_back(flag);
    }
    else if (statType == LIBHMDEC_TYPE_RANGE)
    {
      stats::StatisticsType range(i, name, ColorMapper({0, max}, PredefinedType::Jet));
      range.description = description;
      types.push_back(range);
    }
    else if (statType == LIBHMDEC_TYPE_RANGE_ZEROCENTER)
    {
      stats::StatisticsType rangeZero(i, name, ColorMapper({-max, max}, PredefinedType::Col3_bblg));
      rangeZero.description = description;
      types.push_back(rangeZero);
    }
    else if (statType == LIBHMDEC_TYPE_VECTOR)
    {
      auto                  scale = this->lib.libHMDEC_get_internal_type_vector_scaling(i);
      stats::StatisticsType vec(i, name, scale);
      vec.description = description;
      types.push_back(vec);
    }
    else if (statType == LIBHMDEC_TYPE_INTRA_DIR)
    {
      stats::StatisticsType intraDir(i, name, ColorMapper({0, 34}, PredefinedType::Jet));
      intraDir.description      = description;
      intraDir.hasVectorData    = true;
      intraDir.renderVectorData = true;
      intraDir.vectorScale      = 32;
      // Don't draw the vector values for the intra dir. They don't have actual meaning.
      intraDir.renderVectorDataValues = false;
      intraDir.setMappingValues(
          {"INTRA_PLANAR",     "INTRA_DC",         "INTRA_ANGULAR_2",  "INTRA_ANGULAR_3",
           "INTRA_ANGULAR_4",  "INTRA_ANGULAR_5",  "INTRA_ANGULAR_6",  "INTRA_ANGULAR_7",
           "INTRA_ANGULAR_8",  "INTRA_ANGULAR_9",  "INTRA_ANGULAR_10", "INTRA_ANGULAR_11",
           "INTRA_ANGULAR_12", "INTRA_ANGULAR_13", "INTRA_ANGULAR_14", "INTRA_ANGULAR_15",
           "INTRA_ANGULAR_16", "INTRA_ANGULAR_17", "INTRA_ANGULAR_18", "INTRA_ANGULAR_19",
           "INTRA_ANGULAR_20", "INTRA_ANGULAR_21", "INTRA_ANGULAR_22", "INTRA_ANGULAR_23",
           "INTRA_ANGULAR_24", "INTRA_ANGULAR_25", "INTRA_ANGULAR_26", "INTRA_ANGULAR_27",
           "INTRA_ANGULAR_28", "INTRA_ANGULAR_29", "INTRA_ANGULAR_30", "INTRA_ANGULAR_31",
           "INTRA_ANGULAR_32", "INTRA_ANGULAR_33", "INTRA_ANGULAR_34"});
      types.push_back(intraDir);
    }
  }

  return types;
}

QByteArray decoderHM::getRawFrameData()
{
  if (this->currentHMPic == nullptr)
    return QByteArray();
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_DECHM("decoderHM::getRawFrameData: Wrong decoder state.");
    return QByteArray();
  }

  if (this->currentOutputBuffer.isEmpty())
  {
    // Put image data into buffer
    this->copyImgToByteArray(this->currentHMPic, this->currentOutputBuffer);
    DEBUG_DECHM("decoderHM::getRawFrameData copied frame to buffer");
  }

  return this->currentOutputBuffer;
}

stats::DataPerTypeMap decoderHM::getFrameStatisticsData()
{
  if (this->currentHMPic == nullptr)
    return {};
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_DECHM("decoderHM::getFrameStatisticsData: Wrong decoder state.");
    return {};
  }
  if (!this->statisticsEnabled)
    return {};

  DEBUG_DECHM("decoderHM::getFrameStatisticsData POC %d",
              this->lib.libHMDEC_get_POC(this->currentHMPic));

  stats::DataPerTypeMap data;

  // Conversion from intra prediction mode to vector.
  // Coordinates are in x,y with the axes going right and down.
  static const int vectorTable[35][2] = {
      {0, 0},   {0, 0},   {32, -32}, {32, -26}, {32, -21}, {32, -17}, {32, -13}, {32, -9}, {32, -5},
      {32, -2}, {32, 0},  {32, 2},   {32, 5},   {32, 9},   {32, 13},  {32, 17},  {32, 21}, {32, 26},
      {32, 32}, {26, 32}, {21, 32},  {17, 32},  {13, 32},  {9, 32},   {5, 32},   {2, 32},  {0, 32},
      {-2, 32}, {-5, 32}, {-9, 32},  {-13, 32}, {-17, 32}, {-21, 32}, {-26, 32}, {-32, 32}};

  // Get all the statistics
  // TODO: Could we only retrieve the statistics that are active/displayed?
  auto nrTypes = this->lib.libHMDEC_get_internal_type_number();
  for (unsigned t = 0; t <= nrTypes; t++)
  {
    bool callAgain{};
    do
    {
      // Get a pointer to the data values and how many values in this array are valid.
      unsigned int nrValues{};
      auto         stats =
          this->lib.libHMDEC_get_internal_info(decoder, this->currentHMPic, t, nrValues, callAgain);

      auto statType = this->lib.libHMDEC_get_internal_type(t);
      if (stats != nullptr && nrValues > 0)
      {
        for (unsigned i = 0; i < nrValues; i++)
        {
          auto         b = stats[i];
          stats::Block block(b.x, b.y, b.w, b.h);

          if (statType == LIBHMDEC_TYPE_VECTOR)
            data[t].vectorData.push_back(
                stats::BlockWithVector(block, stats::Vector(b.value, b.value2)));
          else
            data[t].valueData.push_back(stats::BlockWithValue(block, b.value));
          if (statType == LIBHMDEC_TYPE_INTRA_DIR)
          {
            // Also add the vecotr to draw
            if (b.value >= 0 && b.value < 35)
            {
              int vecX = (float)vectorTable[b.value][0] * b.w / 4;
              int vecY = (float)vectorTable[b.value][1] * b.w / 4;
              data[t].vectorData.push_back(
                  stats::BlockWithVector(block, stats::Vector(vecX, vecY)));
            }
          }
        }
      }
    } while (callAgain); // Continue until the library returns that there is no more to retrive
  }

  return data;
}

#if SSE_CONVERSION
void decoderHM::copyImgToByteArray(libHMDec_picture *src, byteArrayAligned &dst)
#else
void decoderHM::copyImgToByteArray(libHMDec_picture *src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  auto fmt      = this->lib.libHMDEC_get_chroma_format(src);
  int  nrPlanes = (fmt == LIBHMDEC_CHROMA_400) ? 1 : 3;

  // Is the output going to be 8 or 16 bit?
  bool outputTwoByte = false;
  for (int c = 0; c < nrPlanes; c++)
  {
    auto component = (c == 0) ? LIBHMDEC_LUMA : (c == 1) ? LIBHMDEC_CHROMA_U : LIBHMDEC_CHROMA_V;
    if (this->lib.libHMDEC_get_internal_bit_depth(src, component) > 8)
      outputTwoByte = true;
  }

  // How many samples are in each component?
  int outSizeY = this->lib.libHMDEC_get_picture_width(src, LIBHMDEC_LUMA) *
                 this->lib.libHMDEC_get_picture_height(src, LIBHMDEC_LUMA);
  int outSizeCb = (nrPlanes == 1) ? 0
                                  : (this->lib.libHMDEC_get_picture_width(src, LIBHMDEC_CHROMA_U) *
                                     this->lib.libHMDEC_get_picture_height(src, LIBHMDEC_CHROMA_U));
  int outSizeCr = (nrPlanes == 1) ? 0
                                  : (this->lib.libHMDEC_get_picture_width(src, LIBHMDEC_CHROMA_V) *
                                     this->lib.libHMDEC_get_picture_height(src, LIBHMDEC_CHROMA_V));
  // How many bytes do we need in the output buffer?
  int nrBytesOutput = (outSizeY + outSizeCb + outSizeCr) * (outputTwoByte ? 2 : 1);
  DEBUG_DECHM("decoderHM::copyImgToByteArray nrBytesOutput %d", nrBytesOutput);

  // Is the output big enough?
  if (dst.capacity() < nrBytesOutput)
    dst.resize(nrBytesOutput);

  // The source (from HM) is always short (16bit). The destination is a QByteArray so
  // we have to cast it right.
  for (int c = 0; c < nrPlanes; c++)
  {
    auto component = (c == 0) ? LIBHMDEC_LUMA : (c == 1) ? LIBHMDEC_CHROMA_U : LIBHMDEC_CHROMA_V;

    auto       img_c  = this->lib.libHMDEC_get_image_plane(src, component);
    const auto stride = this->lib.libHMDEC_get_picture_stride(src, component);

    if (img_c == nullptr)
      return;

    auto width  = this->lib.libHMDEC_get_picture_width(src, component);
    auto height = this->lib.libHMDEC_get_picture_height(src, component);

    if (outputTwoByte)
    {
      unsigned short *restrict d = (unsigned short *)dst.data();
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
      unsigned char *restrict d = (unsigned char *)dst.data();
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

QString decoderHM::getDecoderName() const
{
  return (decoderState == DecoderState::Error) ? "HM" : this->lib.libHMDec_get_version();
}

bool decoderHM::checkLibraryFile(QString libFilePath, QString &error)
{
  decoderHM testDecoder;

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
  return testDecoder.state() != DecoderState::Error;
}

} // namespace decoder