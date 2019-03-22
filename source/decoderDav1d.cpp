/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include <cassert>
#include <cstring>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include "typedef.h"

// Debug the decoder (0:off 1:interactive deocder only 2:caching decoder only 3:both)
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

decoderDav1d::dav1dFrameInfo::dav1dFrameInfo(QSize frameSize, Dav1dFrameType frameType) : frameSize(frameSize), frameType(frameType)
{
  const int aligned_w = (frameSize.width() + 127) & ~127;
  const int aligned_h = (frameSize.height() + 127) & ~127;
  frameSizeAligned = QSize(aligned_w, aligned_h);

  sizeInBlocks = QSize(frameSize.width() / 4, frameSize.height() / 4);
  sizeInBlocksAligned = QSize(aligned_w / 4, aligned_h / 4);

  const int bw = ((frameSize.width() + 7) >> 3) << 1;
  b4_stride = (bw + 31) & ~31;
}

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
  loadDecoderLibrary(settings.value("libDav1dFile", "").toString());
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

  dav1d_close(&decoder);
  if (decoder != nullptr)
    DEBUG_DAV1D("Error closing the decoder. The close function should set the decoder pointer to NULL");
  decoder = nullptr;

  allocateNewDecoder();
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
  // Get/check function pointers
  if (!resolve(dav1d_version, "dav1d_version")) return;
  if (!resolve(dav1d_default_settings, "dav1d_default_settings")) return;
  if (!resolve(dav1d_open, "dav1d_open")) return;
  if (!resolve(dav1d_parse_sequence_header, "dav1d_parse_sequence_header")) return;
  if (!resolve(dav1d_send_data, "dav1d_send_data")) return;
  if (!resolve(dav1d_get_picture, "dav1d_get_picture")) return;
  if (!resolve(dav1d_close, "dav1d_close")) return;
  if (!resolve(dav1d_flush, "dav1d_flush")) return;

  if (!resolve(dav1d_data_create, "dav1d_data_create")) return;

  DEBUG_DAV1D("decoderDav1d::resolveLibraryFunctionPointers - decoding functions found");

  // This means that 
  if (!resolve(dav1d_default_analyzer_settings, "dav1d_default_analyzer_flags", true)) return;
  if (!resolve(dav1d_set_analyzer_settings, "dav1d_set_analyzer_flags", true)) return;
  
  DEBUG_DAV1D("decoderDav1d::resolveLibraryFunctionPointers - analyzer functions found");
  internalsSupported = true;
  nrSignals = 3;  // We can also get prediction and reconstruction before filtering
  curPicture.setInternalsSupported();
}

template <typename T> T decoderDav1d::resolve(T &fun, const char *symbol, bool optional)
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

void decoderDav1d::allocateNewDecoder()
{
  if (decoder != nullptr)
  {
    DEBUG_DAV1D("decoderDav1d::allocateNewDecoder Error a decoder was already allocated");
    return;
  }
  if (decoderState == decoderError)
    return;

  DEBUG_DAV1D("decoderDav1d::allocateNewDecoder - decodeSignal %d", decodeSignal);

  dav1d_default_settings(&settings);

  // Create new decoder object
  int err = dav1d_open(&decoder, &settings);
  if (err != 0)
  {
    decoderState = decoderError;
    setError("Error opening new decoder (dav1d_open)");
    return;
  }

  if (internalsSupported)
  {
    // Apply the analizer settings
    dav1d_default_analyzer_settings(&analyzerSettings);
    if (nrSignals > 0)
    {
      if (decodeSignal == 1)
      {
        analyzerSettings.export_prediction = 1;
        DEBUG_DAV1D("decoderDav1d::allocateNewDecoder - Activated export of prediction");
      }
      else if (decodeSignal == 2)
      {
        analyzerSettings.export_prefilter = 1;
        DEBUG_DAV1D("decoderDav1d::allocateNewDecoder - Activated export of reconstruction pre-filtering");
      }
    }
    if (retrieveStatistics)
    {
      analyzerSettings.export_blkdata = 1;
      DEBUG_DAV1D("decoderDav1d::allocateNewDecoder - Activated export of block data");
    }
    dav1d_set_analyzer_settings(decoder, &analyzerSettings);
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
  if (decoder == nullptr)
    return false;

  curPicture.clear();

  int res = dav1d_get_picture(decoder, curPicture.getPicture());
  if (res >= 0)
  { 
    // We did get a picture
    // Get the resolution / yuv format from the frame
    QSize s = curPicture.getFrameSize();
    if (!s.isValid())
      DEBUG_DAV1D("decoderDav1d::decodeFrame got invalid frame size");
    auto subsampling = curPicture.getSubsampling();
    if (subsampling == YUV_NUM_SUBSAMPLINGS)
      DEBUG_DAV1D("decoderDav1d::decodeFrame got invalid subsampling");
    int bitDepth = curPicture.getBitDepth();
    if (bitDepth < 8 || bitDepth > 16)
      DEBUG_DAV1D("decoderDav1d::decodeFrame got invalid bit depth");

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
    DEBUG_DAV1D("decoderDav1d::decodeFrame Picture decoded - switching to retrieve frame mode");

    decoderState = decoderRetrieveFrames;
    currentOutputBuffer.clear();
    return true;
  }
  else if (res != -EAGAIN)
      return setErrorB("Error retrieving frame from decoder.");

  if (decoderState != decoderNeedsMoreData)
    DEBUG_DAV1D("decoderDav1d::decodeFrame No frame available - switching back to data push mode");
  decoderState = decoderNeedsMoreData;
  return false;
}

QByteArray decoderDav1d::getRawFrameData()
{
  QSize s = curPicture.getFrameSize();
  if (s.width() <= 0 || s.height() <= 0)
  {
    DEBUG_DAV1D("decoderDav1d::getRawFrameData: Current picture has invalid size.");
    return QByteArray();
  }
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_DAV1D("decoderDav1d::getRawFrameData: Wrong decoder state.");
    return QByteArray();
  }

  if (currentOutputBuffer.isEmpty())
  {
    // Put image data into buffer
    copyImgToByteArray(curPicture, currentOutputBuffer);
    DEBUG_DAV1D("decoderDav1d::getRawFrameData copied frame to buffer");

    if (retrieveStatistics)
      // Get the statistics from the image and put them into the statistics cache
      cacheStatistics(curPicture);
  }

  return currentOutputBuffer;
}

bool decoderDav1d::pushData(QByteArray &data) 
{
  if (decoderState != decoderNeedsMoreData)
  {
    DEBUG_DAV1D("decoderDav1d::pushData: Wrong decoder state.");
    return false;
  }
  if (flushing)
  {
    DEBUG_DAV1D("decoderDav1d::pushData: Do not push data when flushing!");
    return false;
  }

  if (!sequenceHeaderPushed)
  {
    // The first packet which is pushed to the decoder should be a sequence header.
    // Otherwise, the decoder can not decode the data.
    if (data.size() == 0)
    {
      DEBUG_DAV1D("decoderDav1d::pushData Error: Sequence header not pushed yet and the data is empty");
      return setErrorB("Error: Sequence header not pushed yet and the data is empty.");
    }

    Dav1dSequenceHeader seq;
    int err = dav1d_parse_sequence_header(&seq, (const uint8_t*)data.data(), data.size());
    if (err == 0)
    {
      sequenceHeaderPushed = true;

      QSize s = QSize(seq.max_width, seq.max_height);
      if (!s.isValid())
        DEBUG_DAV1D("decoderDav1d::pushData got invalid frame size");
      auto subsampling = convertFromInternalSubsampling(seq.layout);
      if (subsampling == YUV_NUM_SUBSAMPLINGS)
        DEBUG_DAV1D("decoderDav1d::pushData got invalid subsampling");
      int bitDepth = (seq.hbd == 0 ? 8 : (seq.hbd == 1 ? 10 : (seq.hbd == 2 ? 12 : -1)));
      if (bitDepth < 8 || bitDepth > 16)
        DEBUG_DAV1D("decoderDav1d::pushData got invalid bit depth");
      subBlockSize = (seq.sb128 >= 1) ? 128 : 64;

      frameSize = s;
      formatYUV = yuvPixelFormat(subsampling, bitDepth);
    }
    else
    {
      DEBUG_DAV1D("decoderDav1d::pushData Error: No sequence header revieved yet and parsing of this packet as slice header failed. Ignoring packet.");
      return true;
    }
  }

  if (data.size() == 0)
  {
    // The input file is at the end. Switch to flushing mode.
    DEBUG_DAV1D("decoderDav1d::pushData input ended - flushing");
    flushing = true;
  }
  else
  {
    // Since dav1d consumes the data (takes ownership), we need to copy it to a new buffer from dav1d
    Dav1dData *dav1dData = new Dav1dData;
    uint8_t *rawDataPointer = dav1d_data_create(dav1dData, data.size());
    memcpy(rawDataPointer, data.data(), data.size());
    
    int err = dav1d_send_data(decoder, dav1dData);
    if (err == -EAGAIN)
    {
      // The data was not consumed and must be pushed again after retrieving some frames
      delete dav1dData;
      DEBUG_DAV1D("decoderDav1d::pushData need to re-push data");
      return false;
    }
    else if (err != 0)
    {
      delete dav1dData;
      DEBUG_DAV1D("decoderDav1d::pushData error pushing data");
      return setErrorB("Error pushing data to the decoder.");
    }
    DEBUG_DAV1D("decoderDav1d::pushData successfully pushed %d bytes", data.size());
  }

  // Check for an available frame
  if (decodeFrame())
    decodedFrameWaiting = true;

  return true;
}

#if SSE_CONVERSION
void decoderDav1d::copyImgToByteArray(const Dav1dPictureWrapper &src, byteArrayAligned &dst)
#else
void decoderDav1d::copyImgToByteArray(const Dav1dPictureWrapper &src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  int nrPlanes = (src.getSubsampling() == YUV_400) ? 1 : 3;
  
  // At first get how many bytes we are going to write
  const int nrBytesPerSample = (src.getBitDepth() > 8) ? 2 : 1;
  const QSize framSize = src.getFrameSize();
  int nrBytes = frameSize.width() * frameSize.height() * nrBytesPerSample;
  YUVSubsamplingType layout = src.getSubsampling();
  if (layout == YUV_420)
    nrBytes += (frameSize.width() / 2) * (frameSize.height() / 2) * 2 * nrBytesPerSample;
  else if (layout == YUV_422)
    nrBytes += (frameSize.width() / 2) * frameSize.height() * 2 * nrBytesPerSample;
  else if (layout == YUV_444)
    nrBytes += frameSize.width() * frameSize.height() * 2 * nrBytesPerSample;

  DEBUG_DAV1D("decoderDav1d::copyImgToByteArray nrBytes %d", nrBytes);

  // Is the output big enough?
  if (dst.capacity() < nrBytes)
   dst.resize(nrBytes);

  uint8_t *dst_c = (uint8_t*)dst.data();

  // We can now copy from src to dst
  for (int c = 0; c < nrPlanes; c++)
  {
    int width = framSize.width();
    int height = framSize.height();
    if (c != 0)
    {
      if (layout == YUV_420 || layout == YUV_422)
        width /= 2;
      if (layout == YUV_420)
        height /= 2;
    }
    const size_t widthInBytes = width * nrBytesPerSample;

    uint8_t* img_c;
    if (decodeSignal == 0)
      img_c = curPicture.getData(c);
    else if (decodeSignal == 1)
      img_c = curPicture.getDataPrediction(c);
    else if (decodeSignal == 2)
      img_c = curPicture.getDataReconstructionPreFiltering(c);

    if (img_c == nullptr)
      return;

    const int stride = (c == 0) ? curPicture.getStride(0) : curPicture.getStride(1);
    for (int y = 0; y < height; y++)
    {
      memcpy(dst_c, img_c, widthInBytes);
      img_c += stride;
      dst_c += widthInBytes;
    }
  }
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

QString decoderDav1d::getDecoderName() const
{
  if (decoder)
  {
    QString ver = QString(dav1d_version());
    return "Dav1d deoder Version " + ver;
  }
  return "Dav1d decoder";
}

QStringList decoderDav1d::getLibraryNames()
{
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib.
  // On windows and linux ommitting the extension works
  if (is_Q_OS_MAC)
    return QStringList() << "libdav1d-internals.dylib" << "libdav1d.dylib";
  if (is_Q_OS_WIN)
    return QStringList() << "dav1d-internals" << "dav1d";
  if (is_Q_OS_LINUX)
    return QStringList() << "libdav1d-internals" << "libdav1d";
}

YUVSubsamplingType decoderDav1d::convertFromInternalSubsampling(Dav1dPixelLayout layout)
{
  if (layout == DAV1D_PIXEL_LAYOUT_I400)
    return YUV_400;
  else if (layout == DAV1D_PIXEL_LAYOUT_I420)
    return YUV_420;
  else if (layout == DAV1D_PIXEL_LAYOUT_I422)
    return YUV_422;
  else if (layout == DAV1D_PIXEL_LAYOUT_I444)
    return YUV_444;

  // Invalid
  return YUV_NUM_SUBSAMPLINGS;
}

void decoderDav1d::fillStatisticList(statisticHandler &statSource) const
{
  StatisticsType predMode(0, "Pred Mode", "jet", 0, 1);
  predMode.description = "The prediction mode (intra/inter) per block";
  predMode.valMap.insert(0, "INTRA");
  predMode.valMap.insert(1, "INTER");
  statSource.addStatType(predMode);

  // LastActiveSegId indicates the real maximum. But that can also vary per frame.
  // 255 is the maximum maximum.
  StatisticsType segmentID(1, "Segment ID", "jet", 0, 255);
  segmentID.description = "Specifies which segment is associated with the current intra block being decoded";
  statSource.addStatType(segmentID);

  StatisticsType skip(2, "skip", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  skip.description = "Equal to 0 indicates that there may be some transform coefficients for this block. 1 Indicates there are none.";
  statSource.addStatType(skip);

  StatisticsType skip_mode(3, "skip_mode", 0, QColor(0, 0, 0), 1, QColor(0,255,0));
  skip_mode.description = "Equal to 1 indicates that signaling of most of the mode info is skipped";
  statSource.addStatType(skip_mode);

  // Intra specific values

  StatisticsType intraPredModeLuma(4, "intra pred mode (Y)", "jet", 0, 13);
  intraPredModeLuma.description = "Intra prediction mode Luma (Y)";
  intraPredModeLuma.valMap.insert(0, "DC_PRED");
  intraPredModeLuma.valMap.insert(1, "VERT_PRED");
  intraPredModeLuma.valMap.insert(2, "HOR_PRED");
  intraPredModeLuma.valMap.insert(3, "DIAG_DOWN_LEFT_PRED");
  intraPredModeLuma.valMap.insert(4, "DIAG_DOWN_RIGHT_PRED");
  intraPredModeLuma.valMap.insert(5, "VERT_RIGHT_PRED");
  intraPredModeLuma.valMap.insert(6, "HOR_DOWN_PRED");
  intraPredModeLuma.valMap.insert(7, "HOR_UP_PRED");
  intraPredModeLuma.valMap.insert(8, "VERT_LEFT_PRED");
  intraPredModeLuma.valMap.insert(9, "SMOOTH_PRED");
  intraPredModeLuma.valMap.insert(10, "SMOOTH_V_PRED");
  intraPredModeLuma.valMap.insert(11, "SMOOTH_H_PRED");
  intraPredModeLuma.valMap.insert(12, "PAETH_PRED");
  intraPredModeLuma.valMap.insert(13, "CFL_PRED");
  statSource.addStatType(intraPredModeLuma);

  StatisticsType intraPredModeChroma(5, "intra pred mode (UV)", "jet", 0, 12);
  intraPredModeChroma.description = "Intra prediction mode Chroma (UV)";
  intraPredModeChroma.valMap.insert(0, "DC_PRED");
  intraPredModeChroma.valMap.insert(1, "VERT_PRED");
  intraPredModeChroma.valMap.insert(2, "HOR_PRED");
  intraPredModeChroma.valMap.insert(3, "DIAG_DOWN_LEFT_PRED");
  intraPredModeChroma.valMap.insert(4, "DIAG_DOWN_RIGHT_PRED");
  intraPredModeChroma.valMap.insert(5, "VERT_RIGHT_PRED");
  intraPredModeChroma.valMap.insert(6, "HOR_DOWN_PRED");
  intraPredModeChroma.valMap.insert(7, "HOR_UP_PRED");
  intraPredModeChroma.valMap.insert(8, "VERT_LEFT_PRED");
  intraPredModeChroma.valMap.insert(9, "SMOOTH_PRED");
  intraPredModeChroma.valMap.insert(10, "SMOOTH_V_PRED");
  intraPredModeChroma.valMap.insert(11, "SMOOTH_H_PRED");
  intraPredModeChroma.valMap.insert(12, "PAETH_PRED");
  statSource.addStatType(intraPredModeChroma);

  StatisticsType paletteSizeLuma(6, "palette size (Y)", 0, QColor(0, 0, 0), 255, QColor(0,0,255));
  statSource.addStatType(paletteSizeLuma);

  StatisticsType paletteSizeChroma(7, "palette size (U)", 0, QColor(0, 0, 0), 255, QColor(0,0,255));
  statSource.addStatType(paletteSizeChroma);

  StatisticsType intraAngleDeltaLuma(8, "intra angle delta (Y)", "col3_bblg", -3, 4);
  intraAngleDeltaLuma.description = "Offset to be applied to the intra prediction angle specified by the prediction mode";
  statSource.addStatType(intraAngleDeltaLuma);

  StatisticsType intraAngleDeltaChroma(9, "intra angle delta (UV)", "col3_bblg", -3, 4);
  intraAngleDeltaChroma.description = "Offset to be applied to the intra prediction angle specified by the prediction mode";
  statSource.addStatType(intraAngleDeltaChroma);

  StatisticsType intraDirLuma(10, "Intra direction luma", 4);
  intraDirLuma.description = "Intra prediction direction luma";
  statSource.addStatType(intraDirLuma);

  StatisticsType intraDirChroma(11, "Intra direction chroma", 4);
  intraDirChroma.description = "Intra prediction direction chroma";
  statSource.addStatType(intraDirChroma);
    
  StatisticsType chromaFromLumaAlphaU(12, "chroma from luma alpha (U)", "col3_bblg", -128, 128);
  chromaFromLumaAlphaU.description = "CflAlphaU: contains the signed value of the alpha component for the U component";
  statSource.addStatType(chromaFromLumaAlphaU);

  StatisticsType chromaFromLumaAlphaV(13, "chroma from luma alpha (V)", "col3_bblg", -128, 128);
  chromaFromLumaAlphaV.description = "CflAlphaU: contains the signed value of the alpha component for the U component";
  statSource.addStatType(chromaFromLumaAlphaV);

  // Inter specific values

  StatisticsType refFrames0(14, "ref frame index 0", "jet", 0, 7);
  statSource.addStatType(refFrames0);

  StatisticsType refFrames1(15, "ref frame index 1", "jet", 0, 7);
  statSource.addStatType(refFrames1);

  StatisticsType compoundPredType(16, "compound prediction type", "jet", 0, 4);
  compoundPredType.valMap.insert(0, "COMP_INTER_NONE");
  compoundPredType.valMap.insert(1, "COMP_INTER_WEIGHTED_AVG");
  compoundPredType.valMap.insert(2, "COMP_INTER_AVG");
  compoundPredType.valMap.insert(3, "COMP_INTER_SEG");
  compoundPredType.valMap.insert(4, "COMP_INTER_WEDGE");
  statSource.addStatType(compoundPredType);

  StatisticsType wedgeIndex(17, "wedge index", "jet", 0, 16);
  statSource.addStatType(wedgeIndex);

  StatisticsType maskSign(18, "mask sign", 0, QColor(0, 0, 0), 1, QColor(0,255,255));
  statSource.addStatType(maskSign);
  
  StatisticsType interMode(19, "inter mode", "jet", 0, 7);
  interMode.valMap.insert(0, "NEARESTMV_NEARESTMV");
  interMode.valMap.insert(1, "NEARMV_NEARMV");
  interMode.valMap.insert(2, "NEARESTMV_NEWMV");
  interMode.valMap.insert(3, "NEWMV_NEARESTMV");
  interMode.valMap.insert(4, "NEARMV_NEWMV");
  interMode.valMap.insert(5, "NEWMV_NEARMV");
  interMode.valMap.insert(6, "GLOBALMV_GLOBALMV");
  interMode.valMap.insert(7, "NEWMV_NEWMV");
  statSource.addStatType(interMode);

  StatisticsType drlIndex(20, "dynamic reference list index", 0, QColor(0, 0, 0), 16, QColor(0,255,255));
  statSource.addStatType(drlIndex);

  StatisticsType interintraType(21, "inter-intra type", "jet", 0, 2);
  interintraType.valMap.insert(0, "INTER_INTRA_NONE");
  interintraType.valMap.insert(1, "INTER_INTRA_BLEND");
  interintraType.valMap.insert(2, "INTER_INTRA_WEDGE");
  statSource.addStatType(interintraType);

  StatisticsType interintraMode(22, "inter-intra mode", "jet", 0, 4);
  interintraMode.valMap.insert(0, "II_DC_PRED");
  interintraMode.valMap.insert(1, "II_VERT_PRED");
  interintraMode.valMap.insert(2, "II_HOR_PRED");
  interintraMode.valMap.insert(3, "II_SMOOTH_PRED");
  interintraMode.valMap.insert(4, "N_INTER_INTRA_PRED_MODES");
  statSource.addStatType(interintraMode);

  StatisticsType motionMode(23, "motion mode", "jet", 0, 2);
  motionMode.valMap.insert(0, "MM_TRANSLATION");
  motionMode.valMap.insert(1, "MM_OBMC");
  motionMode.valMap.insert(2, "MM_WARP");
  statSource.addStatType(motionMode);

  StatisticsType motionVec0(24, "Motion Vector 0", 4);
  motionVec0.description = "The motion vector for component 0";
  statSource.addStatType(motionVec0);
  
  StatisticsType motionVec1(25, "Motion Vector 1", 4);
  motionVec1.description = "The motion vector for component 1";
  statSource.addStatType(motionVec1);

  StatisticsType transformDepth(26, "Transform Size", "jet", 0, 19);
  transformDepth.description = "The transform size";
  transformDepth.valMap.insert(0, "TX_4X4");
  transformDepth.valMap.insert(1, "TX_8X8");
  transformDepth.valMap.insert(2, "TX_16X16");
  transformDepth.valMap.insert(3, "TX_32X32");
  transformDepth.valMap.insert(4, "TX_64X64");
  transformDepth.valMap.insert(5, "RTX_4X8");
  transformDepth.valMap.insert(6, "RTX_8X4");
  transformDepth.valMap.insert(7, "RTX_8X16");
  transformDepth.valMap.insert(8, "RTX_16X8");
  transformDepth.valMap.insert(9, "RTX_16X32");
  transformDepth.valMap.insert(10, "RTX_32X16");
  transformDepth.valMap.insert(11, "RTX_32X64");
  transformDepth.valMap.insert(12, "RTX_64X32");
  transformDepth.valMap.insert(13, "RTX_4X16");
  transformDepth.valMap.insert(14, "RTX_16X4");
  transformDepth.valMap.insert(15, "RTX_8X32");
  transformDepth.valMap.insert(16, "RTX_32X8");
  transformDepth.valMap.insert(17, "RTX_16X64");
  transformDepth.valMap.insert(18, "RTX_64X16");
  statSource.addStatType(transformDepth);
}

void decoderDav1d::cacheStatistics(const Dav1dPictureWrapper &img)
{
  if (!internalsSupported)
    return;

  DEBUG_DAV1D("decoderDav1d::cacheStatistics");

  // Clear the local statistics cache
  curPOCStats.clear();

  Av1Block *blockData = img.getBlockData();
  Dav1dFrameHeader *frameHeader = img.getFrameHeader();
  if (frameHeader == nullptr)
    return;

  dav1dFrameInfo frameInfo(img.getFrameSize(), frameHeader->frame_type);
  frameInfo.frameSize = img.getFrameSize();

  const int sb_step = subBlockSize >> 2;

  for (int y = 0; y < frameInfo.frameSizeAligned.height(); y += sb_step)
    for (int x = 0; x < frameInfo.frameSizeAligned.width(); x += sb_step)
      parseBlockRecursive(blockData, x, y, BL_128X128, frameInfo);
}

void decoderDav1d::parseBlockRecursive(Av1Block *blockData, int x, int y, BlockLevel level, dav1dFrameInfo &frameInfo)
{
  if (y >= frameInfo.sizeInBlocks.height())
    return;

  Av1Block b = blockData[y * frameInfo.b4_stride + x];
  const BlockLevel blockLevel = (BlockLevel)b.bl;

  //assert(b.bl >= 0 && b.bl <= 4);
  if (b.bl > 4)
    return;

  const int blockWidth4 = 1 << (5 - level);

  if (blockLevel > level)
  {
    // Recurse
    const BlockLevel nextLevel = (BlockLevel)(level + 1);
    const int subw = blockWidth4 / 2;
    parseBlockRecursive(blockData, x       , y       , nextLevel, frameInfo);
    parseBlockRecursive(blockData, x + subw, y       , nextLevel, frameInfo);
    parseBlockRecursive(blockData, x       , y + subw, nextLevel, frameInfo);
    parseBlockRecursive(blockData, x + subw, y + subw, nextLevel, frameInfo);
  }
  else
  {
    // Parse the current partition. How is it split into blocks?
    const BlockPartition blockPartition = (BlockPartition)b.bp;

    const int o = blockWidth4 / 2;
    const int oq = blockWidth4 / 2;
    const int bs = blockWidth4;
    switch (blockPartition)
    {
    case PARTITION_NONE:
      parseBlockPartition(blockData, x, y, bs, bs, frameInfo);
      break;
    case PARTITION_H:
      parseBlockPartition(blockData, x, y    , bs, bs / 2, frameInfo);
      parseBlockPartition(blockData, x, y + o, bs, bs / 2, frameInfo);
      break;
    case PARTITION_V:
      parseBlockPartition(blockData, x    , y, bs / 2, bs, frameInfo);
      parseBlockPartition(blockData, x + o, y, bs / 2, bs, frameInfo);
      break;
    case PARTITION_T_TOP_SPLIT: // PARTITION_HORZ_A
      parseBlockPartition(blockData, x    , y    , bs / 2, bs / 2, frameInfo);
      parseBlockPartition(blockData, x + o, y    , bs / 2, bs / 2, frameInfo);
      parseBlockPartition(blockData, x    , y + o, bs    , bs / 2, frameInfo);
      break;
    case PARTITION_T_BOTTOM_SPLIT: // PARTITION_HORZ_B
      parseBlockPartition(blockData, x    , y    , bs    , bs / 2, frameInfo);
      parseBlockPartition(blockData, x    , y + o, bs / 2, bs / 2, frameInfo);
      parseBlockPartition(blockData, x + o, y + o, bs / 2, bs / 2, frameInfo);
      break;
    case PARTITION_T_LEFT_SPLIT: // PARTITION_VERT_A
      parseBlockPartition(blockData, x    , y    , bs / 2, bs / 2, frameInfo);
      parseBlockPartition(blockData, x    , y + o, bs / 2, bs / 2, frameInfo);
      parseBlockPartition(blockData, x + o, y    , bs / 2, bs    , frameInfo);
      break;
    case PARTITION_T_RIGHT_SPLIT: // PARTITION_VERT_B
      parseBlockPartition(blockData, x    , y    , bs / 2, bs    , frameInfo);
      parseBlockPartition(blockData, x    , y + o, bs / 2, bs / 2, frameInfo);
      parseBlockPartition(blockData, x + o, y + o, bs / 2, bs / 2, frameInfo);
      break;
    case PARTITION_H4: // PARTITION_HORZ_4
      parseBlockPartition(blockData, x, y         , bs, bs / 4, frameInfo);
      parseBlockPartition(blockData, x, y + oq    , bs, bs / 4, frameInfo);
      parseBlockPartition(blockData, x, y + oq * 2, bs, bs / 4, frameInfo);
      parseBlockPartition(blockData, x, y + oq * 3, bs, bs / 4, frameInfo);
      break;
    case PARTITION_V4: // PARTITION_VER_4
      parseBlockPartition(blockData, x         , y, bs / 4, bs, frameInfo);
      parseBlockPartition(blockData, x + oq    , y, bs / 4, bs, frameInfo);
      parseBlockPartition(blockData, x + oq * 2, y, bs / 4, bs, frameInfo);
      parseBlockPartition(blockData, x + oq * 3, y, bs / 4, bs, frameInfo);
      break;
    case PARTITION_SPLIT:
      if (blockLevel == BL_8X8)
      {
        // 4 square 4x4 blocks. This is allowed.
        assert(blockWidth4 == 2);
        parseBlockPartition(blockData, x    , y    , 1, 1, frameInfo);
        parseBlockPartition(blockData, x + 1, y    , 1, 1, frameInfo);
        parseBlockPartition(blockData, x    , y + 1, 1, 1, frameInfo);
        parseBlockPartition(blockData, x + 1, y + 1, 1, 1, frameInfo);
      }
      else
      {
        // This should not happen here
        return;
      }
    }
  }
}

void decoderDav1d::parseBlockPartition(Av1Block *blockData, int x, int y, int blockWidth4, int blockHeight4, dav1dFrameInfo &frameInfo)
{
  if (y >= frameInfo.sizeInBlocks.height() || x >= frameInfo.sizeInBlocks.width())
    return;

  Av1Block b = blockData[y * frameInfo.b4_stride + x];

  const int cbPosX = x * 4;
  const int cbPosY = y * 4;
  const int cbWidth = blockWidth4 * 4;
  const int cbHeight = blockHeight4 * 4;

  // Set prediction mode (ID 0)
  const bool isIntra = (b.intra != 0);
  const int predMode = isIntra ? 0 : 1;
  curPOCStats[0].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, predMode);

  bool FrameIsIntra = (frameInfo.frameType == DAV1D_FRAME_TYPE_KEY || frameInfo.frameType == DAV1D_FRAME_TYPE_INTRA);
  if (FrameIsIntra)
  {
    // Set the segment ID (ID 1)
    curPOCStats[1].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.seg_id);
  }

  // Set the skip "flag" (ID 2)
  curPOCStats[2].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.skip);

  // Set the skip_mode (ID 3)
  curPOCStats[3].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.skip_mode);

  if (isIntra)
  {
    // Set the intra pred mode luma/chrmoa (ID 4, 5)
    curPOCStats[4].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.y_mode);
    curPOCStats[5].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.uv_mode);

    // Set the palette size Y/UV (ID 6, 7)
    curPOCStats[6].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.pal_sz[0]);
    curPOCStats[7].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.pal_sz[1]);

    // Set the intra angle delta luma/chroma (ID 8, 9)
    curPOCStats[8].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.y_angle);
    curPOCStats[9].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.uv_angle);

    // Calculate and set the intra prediction direction luma/chroma (ID 10, 11)
    for (int yc=0; yc<2; yc++)
    {
      int angleDelta = (yc == 0) ? b.y_angle : b.uv_angle;
      IntraPredMode predMode = (yc == 0) ? (IntraPredMode)b.y_mode : (IntraPredMode)b.uv_mode;
      QIntPair vec = calculateIntraPredDirection(predMode, angleDelta);
      if (vec.first == 0 && vec.second == 0)
        continue;
      
      int blockScale = std::min(blockWidth4, blockHeight4);
      int vecX = (float)vec.first * blockScale / 4;
      int vecY = (float)vec.second * blockScale / 4;
      
      curPOCStats[10 + yc].addBlockVector(cbPosX, cbPosY, cbWidth, cbHeight, vecX, vecY);
    }

    if (b.y_mode == CFL_PRED)
    {
      // Set the chroma from luma alpha U/V (ID 12, 13)
      curPOCStats[12].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.cfl_alpha[0]);
      curPOCStats[13].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.cfl_alpha[1]);
    }
  }
  else // inter
  {
    CompInterType compoundType = (CompInterType)b.comp_type;
    bool isCompound = (compoundType != COMP_INTER_NONE);

    // Set the reference frame indices 0/1 (ID 14, 15)
    curPOCStats[14].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.ref[0]);
    if (isCompound)
      curPOCStats[15].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.ref[1]);

    // Set the compound prediction type (ID 16)
    curPOCStats[16].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.comp_type);

    // Set the wedge index (ID 17)
    if (b.comp_type == COMP_INTER_WEDGE || b.interintra_type == INTER_INTRA_WEDGE)
      curPOCStats[17].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.wedge_idx);

    // Set the mask sign (ID 18)
    if (isCompound) // TODO: This might not be correct
      curPOCStats[18].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.mask_sign);

    // Set the inter mode (ID 19)
    curPOCStats[19].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.inter_mode);

    // Set the dynamic reference list index (ID 20)
    if (isCompound) // TODO: This might not be correct
      curPOCStats[20].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.drl_idx);

    if (isCompound)
    {
      // Set inter intra type (ID 21)
      curPOCStats[21].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.interintra_type);
      // Set inter intra mode (ID 22)
      curPOCStats[22].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.interintra_mode);
    }

    // Set motion mode (ID 23)
    curPOCStats[23].addBlockValue(cbPosX, cbPosY, cbWidth, cbHeight, b.motion_mode);

    // Set motion vector 0/1 (ID 24, 25)
    curPOCStats[24].addBlockVector(cbPosX, cbPosY, cbWidth, cbHeight, b.mv[0].x, b.mv[0].y);
    if (isCompound)
      curPOCStats[25].addBlockVector(cbPosX, cbPosY, cbWidth, cbHeight, b.mv[1].x, b.mv[1].y);
  }

  const TxfmSize tx_val = TxfmSize(isIntra ? b.tx : b.max_ytx);
  static const int TxfmSizeWidthTable[] = {4, 8, 16, 32, 64, 4, 8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64};
  static const int TxfmSizeHeightTable[] = { 4, 8, 16, 32, 64, 8, 4, 16, 8, 32, 16, 64, 32, 16, 4, 32, 8, 64, 16};
  const int tx_w = TxfmSizeWidthTable[tx_val];
  const int tx_h = TxfmSizeHeightTable[tx_val];
  assert(tx_w <= cbWidth && tx_h <= cbHeight);

  for (int x = 0; x < cbWidth; x += tx_w)
  {
    for (int y = 0; y < cbHeight; y += tx_h)
    {
      // Set the transform size (ID 26)
      const int x_abs = cbPosX + x;
      const int y_abs = cbPosY + y;
      if (x_abs < frameInfo.frameSize.width() && y_abs < frameInfo.frameSize.height())
        curPOCStats[26].addBlockValue(x_abs, y_abs, tx_w, tx_h, (int)tx_val);
    }
  }
}

QIntPair decoderDav1d::calculateIntraPredDirection(IntraPredMode predMode, int angleDelta)
{
  if (predMode == DC_PRED || predMode > VERT_LEFT_PRED)
    return QIntPair(0, 0);

  // angleDelta should be between -4 and 3
  const int modeIndex = predMode - VERT_PRED;
  assert(modeIndex >= 0 && modeIndex < 8);
  const int deltaIndex = angleDelta + 4;
  assert(deltaIndex >= 0 && deltaIndex < 8);

  /* The python code which was used to create the vectorTable
  import math

  def getVector(m, s):
    a = m + 3 * s
    x = -math.cos(math.radians(a))
    y = -math.sin(math.radians(a))
    return round(x*32), round(y*32)	
  
  modes = [90, 180, 45, 135, 113, 157, 203, 67]
  sub_angles = [-4, -3, -2, -1, 0, 1, 2, 3]
  for m in modes:
    out = ""
    for s in sub_angles:
      x, y = getVector(m, s)
      out += "{{{0}, {1}}}, ".format(x, y)
    print(out)
  */

  static const int vectorTable[8][8][2] =
  {
    {{-7, 31}, {-5, 32}, {-3, 32}, {-2, 32}, {0, 32}, {2, 32}, {3, 32}, {5, 32}},
    {{31, 7}, {32, 5}, {32, 3}, {32, 2}, {32, 0}, {32, -2}, {32, -3}, {32, -5}},
    {{-27, 17}, {-26, 19}, {-25, 20}, {-24, 21}, {-23, 23}, {-21, 24}, {-20, 25}, {-19, 26}},
    {{17, 27}, {19, 26}, {20, 25}, {21, 24}, {23, 23}, {24, 21}, {25, 20}, {26, 19}},
    {{6, 31}, {8, 31}, {9, 31}, {11, 30}, {13, 29}, {14, 29}, {16, 28}, {17, 27}},
    {{26, 18}, {27, 17}, {28, 16}, {29, 14}, {29, 13}, {30, 11}, {31, 9}, {31, 8}},
    {{31, -6}, {31, -8}, {31, -9}, {30, -11}, {29, -13}, {29, -14}, {28, -16}, {27, -17}},
    {{-18, 26}, {-17, 27}, {-16, 28}, {-14, 29}, {-13, 29}, {-11, 30}, {-9, 31}, {-8, 31}}
   };

  QIntPair vec(vectorTable[modeIndex][deltaIndex][0], vectorTable[modeIndex][deltaIndex][1]);
  return vec;
}
