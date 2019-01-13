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

  // 
  if (!resolve(dav1d_default_analyzer_settings, "dav1d_default_analyzer_settings")) return;
  if (!resolve(dav1d_set_analyzer_settings, "dav1d_set_analyzer_settings")) return;
  
  DEBUG_DAV1D("decoderDav1d::resolveLibraryFunctionPointers - analizer functions found");
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

  dav1d_default_analyzer_settings(&analyzerSettings);

  if (nrSignals > 0)
  {
    if (decodeSignal == 1)
      analyzerSettings.export_prediction = 1;
    else if (decodeSignal == 2)
      analyzerSettings.export_prefilter = 1;
  }

  dav1d_set_analyzer_settings(decoder, &analyzerSettings);

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

void decoderDav1d::cacheStatistics(const Dav1dPictureWrapper &img)
{
  if (!internalsSupported)
    return;

  DEBUG_DAV1D("decoderDav1d::cacheStatistics");

  // Clear the local statistics cache
  curPOCStats.clear();

  // TODO
  // ...
}

/// ------------------------- Dav1dPictureWrapper -----------------------

decoderDav1d::Dav1dPictureWrapper::Dav1dPictureWrapper()
{
  curPicture = reinterpret_cast<Dav1dPicture*>(&curPicture_original);
}

void decoderDav1d::Dav1dPictureWrapper::setInternalsSupported()
{
  internalsSupported = true;
  curPicture = reinterpret_cast<Dav1dPicture*>(&curPicture_analizer);
}

void decoderDav1d::Dav1dPictureWrapper::clear()
{
  if (internalsSupported)
    memset(&curPicture_analizer, 0, sizeof(curPicture_analizer));
  else
    memset(&curPicture_original, 0, sizeof(curPicture_original));
}

QSize decoderDav1d::Dav1dPictureWrapper::getFrameSize() const
{
  if (internalsSupported)
    return QSize(curPicture_analizer.p.w, curPicture_analizer.p.h);
  else
    return QSize(curPicture_original.p.w, curPicture_original.p.h);
}

YUVSubsamplingType decoderDav1d::Dav1dPictureWrapper::getSubsampling() const
{
  if (internalsSupported)
    return decoderDav1d::convertFromInternalSubsampling(curPicture_analizer.p.layout);
  else
    return decoderDav1d::convertFromInternalSubsampling(curPicture_original.p.layout);
}

int decoderDav1d::Dav1dPictureWrapper::getBitDepth() const
{
  if (internalsSupported)
    return curPicture_analizer.p.bpc;
  else
    return curPicture_original.p.bpc;
}

uint8_t *decoderDav1d::Dav1dPictureWrapper::getData(int component) const
{
  if (internalsSupported)
    return (uint8_t*)curPicture_analizer.data[component];
  else
    return (uint8_t*)curPicture_original.data[component];
}

uint8_t *decoderDav1d::Dav1dPictureWrapper::getDataPrediction(int component) const
{
  if (!internalsSupported)
    return nullptr;
  return (uint8_t*)curPicture_analizer.pred[component];
}

uint8_t *decoderDav1d::Dav1dPictureWrapper::getDataReconstructionPreFiltering(int component) const
{
  if (!internalsSupported)
    return nullptr;
  return (uint8_t*)curPicture_analizer.pre_lpf[component];
}

ptrdiff_t decoderDav1d::Dav1dPictureWrapper::getStride(int component) const
{
  if (internalsSupported)
    return curPicture_analizer.stride[component];
  else
    return curPicture_original.stride[component];
}
