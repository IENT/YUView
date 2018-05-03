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

#include "playlistItemCompressedVideo.h"

#include <QThread>
#include <QInputDialog>

#include "decoderFFmpeg.h"
#include "decoderHM.h"
#include "decoderLibde265.h"
#include "parserAnnexBAVC.h"
#include "parserAnnexBHEVC.h"
#include "videoHandlerYUV.h"
#include "videoHandlerRGB.h"
#include "mainwindow.h"

#define HEVC_DEBUG_OUTPUT 0
#if HEVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HEVC qDebug
#else
#define DEBUG_HEVC(fmt,...) ((void)0)
#endif

// When decoding, it can make sense to seek forward to another random access point.
// However, for this we have to clear the decoder, seek the file and restart decoding. Internally,
// the decoder might already have decoded the frame anyways so it makes no sense to seek but to just
// keep on decoding normally (linearly). If the requested fram number is only is in the future
// by lower than this threshold, we will not seek.
#define FORWARD_SEEK_THRESHOLD 5

// Initialize the static names list of the decoder engines
QStringList playlistItemCompressedVideo::inputFormatNames = QStringList() << "annexBHEVC" << "annexBAVC" << "FFMpeg";
QStringList playlistItemCompressedVideo::decoderEngineNames = QStringList() << "libDe265" << "HM" << "FFMpeg";

playlistItemCompressedVideo::playlistItemCompressedVideo(const QString &compressedFilePath, int displayComponent, inputFormat input, decoderEngine decoder)
  : playlistItemWithVideo(compressedFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  // TODO: should this change with the type of video?
  setIcon(0, convertIcon(":img_videoHEVC.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Nothing is currently being loaded
  isFrameLoading = false;
  isFrameLoadingDoubleBuffer = false;

  // An compressed file can be cached if nothing goes wrong
  cachingEnabled = true;

  currentFrameIdx[0] = -1;
  currentFrameIdx[1] = -1;
  repushDataFFmpeg = false;
  decodingOfFrameNotPossible = false;

  // Open the input file and get some properties (size, bit depth, subsampling) from the file
  if (input == inputInvalid)
  {
    QString ext = QFileInfo(compressedFilePath).suffix();
    if (ext == "hevc" || ext == "h265" || ext == "265")
      // We will try to open this as a raw annexB file
      inputFormatType = inputAnnexBHEVC;
    else if (ext == "avc" || ext == "h264" || ext == "264")
      inputFormatType = inputAnnexBAVC;
    else
      inputFormatType = inputLibavformat;
  }
  else
    inputFormatType = input;

  isinputFormatTypeAnnexB = (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC);

  // While opening the file, also determine which decoders we can use
  ffmpegCodec = AV_CODEC_ID_NONE;
  QList<decoderEngine> possibleDecoders;

  QSize frameSize;
  YUV_Internals::yuvPixelFormat format_yuv;
  RGB_Internals::rgbPixelFormat format_rgb;
  if (isinputFormatTypeAnnexB)
  {
    // Open file
    inputFileAnnexBLoading.reset(new fileSourceAnnexBFile(compressedFilePath));
    if (cachingEnabled)
      inputFileAnnexBCaching.reset(new fileSourceAnnexBFile(compressedFilePath));
    // inputFormatType a parser
    if (input == inputAnnexBHEVC)
    {
      inputFileAnnexBParser.reset(new parserAnnexBHEVC());
      ffmpegCodec = AV_CODEC_ID_HEVC;
      possibleDecoders.append(decoderEngineLibde265);
      possibleDecoders.append(decoderEngineHM);
      possibleDecoders.append(decoderEngineFFMpeg);
    }
    else if (inputFormatType == inputAnnexBAVC)
    {
      inputFileAnnexBParser.reset(new parserAnnexBAVC());
      ffmpegCodec = AV_CODEC_ID_H264;
      possibleDecoders.append(decoderEngineFFMpeg);
    }
    // Parse the loading file
    parseAnnexBFile(inputFileAnnexBLoading, inputFileAnnexBParser);
    inputFileAnnexBParser->sortPOCList();
    // Get the frame size and the pixel format
    frameSize = inputFileAnnexBParser->getSequenceSizeSamples();
    // Raw annexB files will always provide YUV data
    format_yuv = inputFileAnnexBParser->getPixelFormat();
    rawFormat = raw_YUV;
  }
  else
  {
    // Try ffmpeg to open the file
    inputFileFFmpegLoading.reset(new fileSourceFFmpegFile());
    if (!inputFileFFmpegLoading->openFile(compressedFilePath))
    {
      fileState = error;
      return;
    }
    // Is this file RGB or YUV?
    rawFormat = inputFileFFmpegLoading->getRawFormat();
    if (rawFormat == raw_YUV)
      format_yuv = inputFileFFmpegLoading->getPixelFormatYUV();
    else if (rawFormat == raw_RGB)
      format_rgb = inputFileFFmpegLoading->getPixelFormatRGB();
    frameSize = inputFileFFmpegLoading->getSequenceSizeSamples();
    ffmpegCodec = inputFileFFmpegLoading->getCodec();
    possibleDecoders.append(decoderEngineFFMpeg);
    if (ffmpegCodec == AV_CODEC_ID_HEVC)
    {
      possibleDecoders.append(decoderEngineLibde265);
      possibleDecoders.append(decoderEngineHM);
    }

    if (cachingEnabled)
    {
      // Open the file again for caching
      inputFileFFmpegCaching.reset(new fileSourceFFmpegFile());
      if (!inputFileFFmpegCaching->openFile(compressedFilePath, inputFileFFmpegLoading.data()))
      {
        fileState = error;
        return;
      }
    }
  }

  // Check/set properties
  if (!frameSize.isValid() || rawFormat == raw_Invalid || (rawFormat == raw_YUV && !format_yuv.isValid()) || (rawFormat == raw_RGB && !format_rgb.isValid()))
  {
    fileState = error;
    return;
  }

  // Allocate the videoHander (RGB or YUV)
  if (rawFormat == raw_YUV)
  {
    video.reset(new videoHandlerYUV());
    videoHandlerYUV *yuvVideo = getYUVVideo();
    yuvVideo->setFrameSize(frameSize);
    yuvVideo->setYUVPixelFormat(format_yuv);
  }
  else
  {
    video.reset(new videoHandlerRGB());
    videoHandlerRGB *rgbVideo = getRGBVideo();
    rgbVideo->setFrameSize(frameSize);
    rgbVideo->setRGBPixelFormat(format_rgb);
  }

  // Connect the basic signals from the video
  playlistItemWithVideo::connectVideo();
  statSource.setFrameSize(frameSize);
  // So far, we can parse the stream
  fileState = onlyParsing;

  if (decoder == decoderEngineInvalid)
  {
    if (possibleDecoders.length() > 1)
    {
      // There is more than one decoder we may use. Ask the user which one to use.
      bool ok;
      QString label = "<html><head/><body><p>There are multiple decoders that we can use in order to decode the bitstream file:</p>";
      if (possibleDecoders.contains(decoderEngineLibde265))
        label += "<p><b>libde265:</b> A very fast and open source HEVC decoder. The internals version even supports display of the prediction and residual signal.</p>";
      if (possibleDecoders.contains(decoderEngineHM))
        label += "<p><b>libHM:</b> The library version of the HEVC reference test model software (HM). Slower than libde265.</p>";
      if (possibleDecoders.contains(decoderEngineFFMpeg))
        label += "<p><b>FFmpeg:</b> The ffmpeg libraries offer fast decoding of almost any format. Please note that we do not ship the FFmpeg libraries.</p>";
      label += "</body></html>";
      QWidget *mainWindow = MainWindow::getMainWindow();
      QStringList possibleDecoderNames;
      for (decoderEngine e : possibleDecoders)
        possibleDecoderNames.append(decoderEngineNames[e]);
      QString item = QInputDialog::getItem(mainWindow, "Select a decoder engine", label, possibleDecoderNames, 0, false, &ok);
      if (ok && !item.isEmpty())
      {
        int idx = decoderEngineNames.indexOf(item);
        if (idx >= 0 && idx < decoderEngineNum)
          decoderEngineType = (decoderEngine)idx;
      }
    }
    else
      decoderEngineType = possibleDecoders[0];
  }
  else
    decoderEngineType = decoder;

  // Allocate the decoders
  if (decoderEngineType == decoderEngineLibde265)
  {
    loadingDecoder.reset(new decoderLibde265(displayComponent));
    cachingDecoder.reset(new decoderLibde265(displayComponent, true));
    fileState = noError;
  }
  else if (decoderEngineType == decoderEngineHM)
  {
    // TODO:
    /*loadingDecoder.reset(new hevcDecoderHM(displayComponent));
    cachingDecoder.reset(new hevcDecoderHM(displayComponent, true));*/
  }
  else if (decoderEngineType == decoderEngineFFMpeg)
  {
    if (isinputFormatTypeAnnexB)
    {
      loadingDecoder.reset(new decoderFFmpeg(ffmpegCodec, frameSize));
      if (cachingEnabled)
        cachingDecoder.reset(new decoderFFmpeg(ffmpegCodec, frameSize));
    }
    else
    {
      loadingDecoder.reset(new decoderFFmpeg(inputFileFFmpegLoading->getVideoCodecPar()));
      if (cachingEnabled)
        cachingDecoder.reset(new decoderFFmpeg(inputFileFFmpegCaching->getVideoCodecPar()));
    }
    fileState = noError;
  }
  else
    return;

  if (rawFormat == raw_YUV)
  {
    videoHandlerYUV *yuvVideo = getYUVVideo();
    yuvVideo->showPixelValuesAsDiff = loadingDecoder->isSignalDifference(loadingDecoder->getDecodeSignal());
  }

  // Fill the list of statistics that we can provide
  fillStatisticList();

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();
  if (startEndFrame.second == -1)
    // No frames to decode
    return;

  // Seek both decoders to the start of the bitstream (this will also push the parameter sets / extradata to the decoder)
  seekToPosition(0, 0, false);
  loadRawData(0, false);
  if (cachingEnabled)
  {
    seekToPosition(0, 0, true);
    loadRawData(0, true);
  }

  // Connect signals for requesting data and statistics
  connect(video.data(), &videoHandler::signalRequestRawData, this, &playlistItemCompressedVideo::loadRawData, Qt::DirectConnection);
  connect(video.data(), &videoHandler::signalUpdateFrameLimits, this, &playlistItemCompressedVideo::slotUpdateFrameLimits);
  connect(&statSource, &statisticHandler::updateItem, this, &playlistItemCompressedVideo::updateStatSource);
  connect(&statSource, &statisticHandler::requestStatisticsLoading, this, &playlistItemCompressedVideo::loadStatisticToCache, Qt::DirectConnection);
}

void playlistItemCompressedVideo::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the HEVC file. We save both in the playlist.
  QUrl fileURL(plItemNameOrFileName);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(plItemNameOrFileName);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemCompressedVideo");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the HEVC file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);
  d.appendProperiteChild("displayComponent", QString::number(loadingDecoder->getDecodeSignal()));

  QString readerEngine = inputFormatNames.at(inputFormatType);
  d.appendProperiteChild("inputFormat", readerEngine);
  QString decoderTypeName = decoderEngineNames.at(decoderEngineType);
  d.appendProperiteChild("decoder", decoderTypeName);
  
  root.appendChild(d);
}

playlistItemCompressedVideo *playlistItemCompressedVideo::newPlaylistItemCompressedVideo(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemRawCodedVideo
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  int displaySignal = root.findChildValue("displayComponent").toInt();

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  inputFormat input = inputAnnexBHEVC;
  QString inputName = root.findChildValue("inputFormat");
  int idx = inputFormatNames.indexOf(inputName);
  if (idx >= 0 && idx < input_NUM)
    input = inputFormat(idx);

  decoderEngine decoder = decoderEngineLibde265;
  QString decoderName = root.findChildValue("decoder");
  idx = decoderEngineNames.indexOf(decoderName);
  if (idx >= 0 && idx < decoderEngineNum)
    decoder = decoderEngine(idx);

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemCompressedVideo *newFile = new playlistItemCompressedVideo(filePath, displaySignal, input, decoder);

  // Load the propertied of the playlistItemIndexed
  playlistItem::loadPropertiesFromPlaylist(root, newFile);

  return newFile;
}

infoData playlistItemCompressedVideo::getInfo() const
{
  infoData info("HEVC File Info");

  // At first append the file information part (path, date created, file size...)
  // info.items.append(loadingDecoder->getFileInfoList());

  if (fileState == onlyParsing)
  {
    //info.items.append(infoItem("Num POCs", QString::number(loadingDecoder->getNumberPOCs()), "The number of pictures in the stream."));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
    info.items.append(infoItem("Reader", inputFormatNames.at(inputFormatType)));
  }
  else if (fileState == noError)
  {
    QSize videoSize = video->getFrameSize();
    info.items.append(infoItem("Reader", inputFormatNames.at(inputFormatType)));
    info.items.append(infoItem("Decoder", loadingDecoder->getDecoderName()));
    info.items.append(infoItem("Decoder", loadingDecoder->getCodecName()));
    info.items.append(infoItem("library path", loadingDecoder->getLibraryPath(), "The path to the loaded libde265 library"));
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Num POCs", QString::number(startEndFrame.second), "The number of pictures in the stream."));
    info.items.append(infoItem("Statistics", loadingDecoder->statisticsSupported() ? "Yes" : "No", "Is the decoder able to provide internals (statistics)?"));
    info.items.append(infoItem("Stat Parsing", loadingDecoder->statisticsEnabled() ? "Yes" : "No", "Are the statistics of the sequence currently extracted from the stream?"));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
  }

  return info;
}

void playlistItemCompressedVideo::infoListButtonPressed(int buttonID)
{
  Q_UNUSED(buttonID);

  // The button "Show NAL units" was pressed.
  // Parse the file using the apropriate parser ...
  QScopedPointer<parserAnnexB> parserA;
  QScopedPointer<parserAVFormat> parserB;
  if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC)
  {
    // Just open and parse the file again
    QScopedPointer<fileSourceAnnexBFile> annexBFile(new fileSourceAnnexBFile(plItemNameOrFileName));
    // Create a parser
    if (inputFormatType == inputAnnexBHEVC)
      parserA.reset(new parserAnnexBHEVC());
    else if (inputFormatType == inputAnnexBAVC)
      parserA.reset(new parserAnnexBAVC());
    
    // Parse the file
    parserA->enableModel();
    parseAnnexBFile(annexBFile, parserA);
  }
  else // inputLibavformat
  {
    // Just open and parse the file again
    QScopedPointer<fileSourceFFmpegFile> ffmpegFile(new fileSourceFFmpegFile(plItemNameOrFileName));
    AVCodecID codec = inputFileFFmpegLoading->getCodec();
    parserB.reset(new parserAVFormat(codec));
    parserB->enableModel();
    parseFFMpegFile(ffmpegFile, parserB);
  }
  
  // ... then, create a dialog with a QTreeView and show the NAL unit list.
  QDialog newDialog;
  QTreeView *view = new QTreeView();
  if (parserA)
    view->setModel(parserA->getNALUnitModel());
  else
    view->setModel(parserB->getNALUnitModel());
  QVBoxLayout *verticalLayout = new QVBoxLayout(&newDialog);
  verticalLayout->addWidget(view);
  newDialog.resize(QSize(1000, 900));
  view->setColumnWidth(0, 400);
  view->setColumnWidth(1, 50);
  newDialog.exec();
}

itemLoadingState playlistItemCompressedVideo::needsLoading(int frameIdx, bool loadRawData)
{
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  auto videoState = video->needsLoading(frameIdxInternal, loadRawData);
  if (videoState == LoadingNeeded || statSource.needsLoading(frameIdxInternal) == LoadingNeeded)
    return LoadingNeeded;
  return videoState;
}

void playlistItemCompressedVideo::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  if (decodingOfFrameNotPossible)
  {
    infoText = "Decoding of the frame not possible:\n";
    infoText += "The frame could not be decoded. Possibly, the bitstream is corrupt or was cut at an invalid position.";
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (loadingDecoder.isNull())
  {
    infoText = "No decoder allocated.\n";
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }

  else if (loadingDecoder->errorInDecoder())
  {
    // There was an error in the deocder. 
    infoText = "There was an error when loading the decoder: \n";
    infoText += loadingDecoder->decoderErrorString();
    infoText += "\n";
    if (decoderEngineType == decoderEngineHM)
    {
      infoText += "We do not currently ship the HM decoder libraries.\n";
      infoText += "You can find download links in Help->Downloads";
    }
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (fileState == noError && frameIdxInternal >= startEndFrame.first && frameIdxInternal <= startEndFrame.second)
  {
    video->drawFrame(painter, frameIdxInternal, zoomFactor, drawRawData);
    statSource.paintStatistics(painter, frameIdxInternal, zoomFactor);
  }
}

void playlistItemCompressedVideo::loadRawData(int frameIdxInternal, bool caching)
{
  if (caching && !cachingEnabled)
    return;
  if (!caching && loadingDecoder->errorInDecoder())
    return;
  if (caching && cachingDecoder->errorInDecoder())
    return;
  
  DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData %d %s", frameIdxInternal, caching ? "caching" : "");

  if (frameIdxInternal > startEndFrame.second || frameIdxInternal < 0)
  {
    DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData Invalid frame index");
    return;
  }

  // Get the right decoder
  decoderBase *dec = caching ? cachingDecoder.data() : loadingDecoder.data();
  int curFrameIdx = caching ? currentFrameIdx[1] : currentFrameIdx[0];

  // Should we seek?
  bool seek = (frameIdxInternal < curFrameIdx);
  int seekToFrame = -1;
  int seekToPTS = -1;
  if (frameIdxInternal < curFrameIdx || frameIdxInternal > curFrameIdx + FORWARD_SEEK_THRESHOLD)
  {
    if (isinputFormatTypeAnnexB)
      seekToFrame = inputFileAnnexBParser->getClosestSeekableFrameNumberBefore(frameIdxInternal);
    else
    {
      if (caching)
        seekToPTS = inputFileFFmpegCaching->getClosestSeekableDTSBefore(frameIdxInternal, seekToFrame);
      else
        seekToPTS = inputFileFFmpegLoading->getClosestSeekableDTSBefore(frameIdxInternal, seekToFrame);
    }

    if (seekToFrame > curFrameIdx + FORWARD_SEEK_THRESHOLD)
      // We will seek forward
      seek = true;
  }
  
  if (seek)
  {
    DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData seeking to frame %d PTS %d", seekToFrame, seekToPTS);
    seekToPosition(seekToFrame, seekToPTS, caching);
  }

  // Decode until we get the right frame from the deocder
  bool rightFrame = caching ? currentFrameIdx[1] == frameIdxInternal : currentFrameIdx[0] == frameIdxInternal;
  while (!rightFrame)
  {
    while (dec->needsMoreData())
    {
      DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData decoder needs more data");
      if (!isinputFormatTypeAnnexB && decoderEngineType == decoderEngineFFMpeg)
      {
        // We are using FFmpeg to read the file and decode. In this scenario, we can read AVPackets
        // from the FFmpeg file and pass them to the FFmpeg decoder directly.
        AVPacketWrapper pkt = caching ? inputFileFFmpegCaching->getNextPacket(repushDataFFmpeg) : inputFileFFmpegLoading->getNextPacket(repushDataFFmpeg);
        repushDataFFmpeg = false;
        if (pkt)
          DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData retrived packet PTS %d", pkt.get_pts());
        else
          DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData retrived empty packet");
        decoderFFmpeg *ffmpegDec;
        if (caching)
          ffmpegDec = dynamic_cast<decoderFFmpeg*>(cachingDecoder.data());
        else
          ffmpegDec = dynamic_cast<decoderFFmpeg*>(loadingDecoder.data());
        if (!ffmpegDec->pushAVPacket(pkt))
        {
          if (!ffmpegDec->decodeFrames())
            // The decoder did not switch to decoding frame mode. Error.
            return;
          repushDataFFmpeg = true;
        }
      }
      else
      {
        // The 
        QByteArray data;
        // Push more data to the decoder
      
        if (isinputFormatTypeAnnexB)
          data = caching ? inputFileAnnexBCaching->getNextNALUnit() : inputFileAnnexBLoading->getNextNALUnit();
        else
          data = caching ? inputFileFFmpegCaching->getNextNALUnit() : inputFileFFmpegLoading->getNextNALUnit();
        DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData retrived nal unit from file - size %d", data.size());
        dec->pushData(data);
      }
    }

    if (dec->decodeFrames())
    {
      if (dec->decodeNextFrame())
      {
        if (caching)
          currentFrameIdx[1]++;
        else
          currentFrameIdx[0]++;

        DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData decoded frame %d", caching ? currentFrameIdx[1] : currentFrameIdx[0]);
        rightFrame = caching ? currentFrameIdx[1] == frameIdxInternal : currentFrameIdx[0] == frameIdxInternal;
        if (rightFrame)
        {
          video->rawData = dec->getRawFrameData();
          video->rawData_frameIdx = frameIdxInternal;
        }
      }
    }

    if (!dec->needsMoreData() && !dec->decodeFrames())
    {
      // The specified frame (which is thoretically in the bitstream) can not be decoded.
      // Maybe the bitstream was cut at a position that it was not supposed to be cut at.
      decodingOfFrameNotPossible = true;
      if (caching)
        currentFrameIdx[1] = frameIdxInternal;
      else
        currentFrameIdx[0] = frameIdxInternal;
      // Just set the frame number of the buffer to the current frame so that it will trigger a
      // reload when the frame number changes.
      video->rawData_frameIdx = frameIdxInternal;
      return;
    }
  }
}

void playlistItemCompressedVideo::seekToPosition(int seekToFrame, int seekToPTS, bool caching)
{
  // Do the seek
  decoderBase *dec = caching ? cachingDecoder.data() : loadingDecoder.data();
  dec->resetDecoder();
  repushDataFFmpeg = false;
  decodingOfFrameNotPossible = false;

  // Retrieval of the raw metadata is only required if the the reader or the decoder is not ffmpeg
  const bool bothFFmpeg = (!isinputFormatTypeAnnexB && decoderEngineType == decoderEngineFFMpeg);
  
  QByteArrayList parametersets;
  if (isinputFormatTypeAnnexB)
  {
    uint64_t filePos;
    if (!bothFFmpeg)
      parametersets = inputFileAnnexBParser->getSeekFrameParamerSets(seekToFrame, filePos);
    if (caching)
      inputFileAnnexBCaching->seek(filePos);
    else
      inputFileAnnexBLoading->seek(filePos);
  }
  else
  {
    if (!bothFFmpeg)
      parametersets = caching ? inputFileFFmpegCaching->getParameterSets() : inputFileFFmpegLoading->getParameterSets();
    if (caching)
      inputFileFFmpegCaching->seekToPTS(seekToPTS);
    else
      inputFileFFmpegLoading->seekToPTS(seekToPTS);
  }

  // In case of using ffmpeg for reading and decoding, we don't need to push the parameter sets (the
  // extradata) to the decoder explicitly when seeking.
  if (!bothFFmpeg)
    // Push the parameter sets to the decoder
    for (QByteArray d : parametersets)
      dec->pushData(d);
  if (caching)
    currentFrameIdx[1] = seekToFrame - 1;
  else
    currentFrameIdx[0] = seekToFrame - 1;
}

void playlistItemCompressedVideo::createPropertiesWidget()
{
  // Absolutely always only call this once
  Q_ASSERT_X(!propertiesWidget, "playlistItemCompressedVideo::createPropertiesWidget", "Always create the properties only once!");

  // Create a new widget and populate it with controls
  propertiesWidget.reset(new QWidget);
  ui.setupUi(propertiesWidget.data());

  QFrame *lineOne = new QFrame;
  lineOne->setObjectName(QStringLiteral("line"));
  lineOne->setFrameShape(QFrame::HLine);
  lineOne->setFrameShadow(QFrame::Sunken);
  QFrame *lineTwo = new QFrame;
  lineTwo->setObjectName(QStringLiteral("line"));
  lineTwo->setFrameShape(QFrame::HLine);
  lineTwo->setFrameShadow(QFrame::Sunken);

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  ui.verticalLayout->insertLayout(0, createPlaylistItemControls());
  ui.verticalLayout->insertWidget(1, lineOne);
  ui.verticalLayout->insertLayout(2, video->createVideoHandlerControls(true));
  ui.verticalLayout->insertWidget(5, lineTwo);
  ui.verticalLayout->insertLayout(6, statSource.createStatisticsHandlerControls(), 1);

  // Set the components that we can display
  if (loadingDecoder)
  {
    ui.comboBoxDisplaySignal->addItems(loadingDecoder->getSignalNames());
    ui.comboBoxDisplaySignal->setCurrentIndex(loadingDecoder->getDecodeSignal());
  }

  // Connect signals/slots
  connect(ui.comboBoxDisplaySignal, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &playlistItemCompressedVideo::displaySignalComboBoxChanged);
}

void playlistItemCompressedVideo::fillStatisticList()
{
  if (!loadingDecoder || !loadingDecoder->statisticsSupported())
    return;

  loadingDecoder->fillStatisticList(statSource);
}

void playlistItemCompressedVideo::loadStatisticToCache(int frameIdx, int typeIdx)
{
  DEBUG_HEVC("playlistItemCompressedVideo::loadStatisticToCache Request statistics type %d for frame %d", typeIdx, frameIdx);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  if (!loadingDecoder->statisticsSupported())
    return;
  if (!loadingDecoder->statisticsEnabled())
  {
    // We have to enable collecting of statistics in the decoder. By default (for speed reasons) this is off.
    // Enabeling works like this: Enable collection, reset the decoder and decode the current frame again.
    // Statisitcs are always retrieved for the loading decoder.
    loadingDecoder->enableStatisticsRetrieval();

    // Reload the current frame (force a seek and decode operation)
    int frameToLoad = currentFrameIdx[0];
    currentFrameIdx[0] = INT_MAX;
    loadRawData(frameToLoad, false);

    // The statistics should now be loaded
  }
  else if (frameIdxInternal != currentFrameIdx[0])
    // If the requested frame is not currently decoded, decode it.
    // This can happen if the picture was gotten from the cache.
    loadRawData(frameIdxInternal, false);

  statSource.statsCache[typeIdx] = loadingDecoder->getStatisticsData(typeIdx);
}

indexRange playlistItemCompressedVideo::getStartEndFrameLimits() const
{
  if (fileState != error)
  {
    if (isinputFormatTypeAnnexB)
      return indexRange(0, inputFileAnnexBParser->getNumberPOCs() - 1);
    else
      // TODO: 
      return indexRange(0, inputFileFFmpegLoading->getNumberFrames() - 1);
  }
  
  return indexRange(0, 0);
}

ValuePairListSets playlistItemCompressedVideo::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  newSet.append("YUV", video->getPixelValues(pixelPos, frameIdxInternal));
  if (loadingDecoder->statisticsSupported() && loadingDecoder->statisticsEnabled())
    newSet.append("Stats", statSource.getValuesAt(pixelPos));

  return newSet;
}

void playlistItemCompressedVideo::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  QStringList ext;
  ext << "hevc" << "h265" << "265" << "avc" << "h264" << "264" << "avi" << "avr" << "cdxl" << "xl" << "dv" << "dif" << "flm" << "flv" << "flv" << "h261" << "h26l" << "cgi" << "ivr" << "lvf"
      << "m4v" << "mkv" << "mk3d" << "mka" << "mks" << "mjpg" << "mjpeg" << "mpo" << "j2k" << "mov" << "mp4" << "m4a" << "3gp" << "3g2" << "mj2" << "mvi" << "mxg" << "v" << "ogg" 
      << "mjpg" << "viv" << "xmv" << "ts" << "mxf";
  QString filtersString = "FFMpeg files (";
  for (QString e : ext)
    filtersString.append(QString("*.%1").arg(e));
  filtersString.append(")");

  allExtensions.append(ext);
  filters.append(filtersString);
}

void playlistItemCompressedVideo::reloadItemSource()
{
  // TODO: The caching decoder must also be reloaded
  //       All items in the cache are also now invalid

  //loadingDecoder->reloadItemSource();
  // Reset the decoder somehow

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  // Reset the videoHandlerYUV source. With the next draw event, the videoHandlerYUV will request to decode the frame again.
  video->invalidateAllBuffers();

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadRawData(0, false);
}

void playlistItemCompressedVideo::cacheFrame(int frameIdx, bool testMode)
{
  if (!cachingEnabled)
    return;

  // Cache a certain frame. This is always called in a separate thread.
  cachingMutex.lock();
  video->cacheFrame(getFrameIdxInternal(frameIdx), testMode);
  cachingMutex.unlock();
}

void playlistItemCompressedVideo::loadFrame(int frameIdx, bool playing, bool loadRawdata, bool emitSignals)
{
  // The current thread must never be the main thread but one of the interactive threads.
  Q_ASSERT(QThread::currentThread() != QApplication::instance()->thread());
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  auto stateYUV = video->needsLoading(frameIdxInternal, loadRawdata);
  auto stateStat = statSource.needsLoading(frameIdxInternal);

  if (stateYUV == LoadingNeeded || stateStat == LoadingNeeded)
  {
    isFrameLoading = true;
    if (stateYUV == LoadingNeeded)
    {
      // Load the requested current frame
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading frame %d %s", frameIdxInternal, playing ? "(playing)" : "");
      video->loadFrame(frameIdxInternal);
    }
    if (stateStat == LoadingNeeded)
    {
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading statistics %d %s", frameIdxInternal, playing ? "(playing)" : "");
      statSource.loadStatistics(frameIdxInternal);
    }

    isFrameLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }

  if (playing && (stateYUV == LoadingNeeded || stateYUV == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdxInternal + 1;
    if (nextFrameIdx <= startEndFrame.second)
    {
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading frame into double buffer %d %s", nextFrameIdx, playing ? "(playing)" : "");
      isFrameLoadingDoubleBuffer = true;
      video->loadFrame(nextFrameIdx, true);
      isFrameLoadingDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

void playlistItemCompressedVideo::displaySignalComboBoxChanged(int idx)
{
  if (loadingDecoder && idx != loadingDecoder->getDecodeSignal())
  {
    loadingDecoder->setDecodeSignal(idx);
    cachingDecoder->setDecodeSignal(idx);

    // A different display signal was chosen. Invalidate the cache and signal that we will need a redraw.
    videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
    yuvVideo->showPixelValuesAsDiff = loadingDecoder->isSignalDifference(idx);
    yuvVideo->invalidateAllBuffers();
    emit signalItemChanged(true, RECACHE_CLEAR);
  }
}

void playlistItemCompressedVideo::parseAnnexBFile(QScopedPointer<fileSourceAnnexBFile> &file, QScopedPointer<parserAnnexB> &parser)
{
  DEBUG_HEVC("playlistItemCompressedVideo::parseAnnexBFile");

  // Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidget *mainWindow = MainWindow::getMainWindow();
  // Create the dialog
  int64_t maxPos;
  if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC)
    maxPos = file->getFileSize();
  else
    // TODO: What do we do for ffmpeg files?
    maxPos = 1000;
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // Just push all NAL units from the annexBFile into the annexBParser
  QByteArray nalData;
  int nalID = 0;
  uint64_t filePos;
  while (!file->atEnd())
  {
    try
    {
      nalData = file->getNextNALUnit(&filePos);

      parser->parseAndAddNALUnit(nalID, nalData, nullptr, filePos);

      // Update the progress dialog
      if (progress.wasCanceled())
        return;

      int64_t pos;
      if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC)
        pos = file->pos();
      else
        // TODO: 
        pos = 55;

      int newPercentValue = pos * 100 / maxPos;
      if (newPercentValue != curPercentValue)
      {
        progress.setValue(newPercentValue);
        curPercentValue = newPercentValue;
      }
    
      // Next NAL
      nalID++;
    }
    catch (...)
    {
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_HEVC(":parseAndAddNALUnit Exception thrown parsing NAL %d", nalID);
      nalID++;
    }
  }
  
  // We are done.
  file->seek(0);
  progress.close();
}

void playlistItemCompressedVideo::parseFFMpegFile(QScopedPointer<fileSourceFFmpegFile> &file, QScopedPointer<parserAVFormat> &parser)
{
  // Seek to the beginning of the stream.
  file->seekToPTS(0);

  // Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidget *mainWindow = MainWindow::getMainWindow();
  // Create the dialog
  int64_t maxPTS = file->getMaxPTS();
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // First get the extradata and push it to the parser
  QByteArray extradata = file->getExtradata();
  parser->parseExtradata(extradata);

  // Parse the metadata
  QStringPairList metadata = file->getMetadata();
  parser->parseMetadata(metadata);

  // Now iterate over all packets and send them to the parser
  AVPacketWrapper packet = file->getNextPacket();
  
  int packetID = 0;
  while (!file->atEnd())
  {
    int newPercentValue = packet.get_pts() * 100 / maxPTS;
    if (newPercentValue != curPercentValue)
    {
      progress.setValue(newPercentValue);
      curPercentValue = newPercentValue;
    }

    try
    {
      parser->parseAVPacket(packetID, packet);
    }
    catch (...)
    {
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_HEVC("parseAVPacket Exception thrown parsing NAL %d", packetID);
    }
    packetID++;
    packet = file->getNextPacket();
  }
    
  // Seek back to the beginning of the stream.
  file->seekToPTS(0);
  progress.close();
}