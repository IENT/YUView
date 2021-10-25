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

#include "playlistItemCompressedVideo.h"

#include <QInputDialog>
#include <QPlainTextEdit>
#include <QThread>

#include <inttypes.h>

#include <common/YUViewDomElement.h>
#include <common/functions.h>
#include <common/functionsGui.h>
#include <decoder/decoderDav1d.h>
#include <decoder/decoderFFmpeg.h>
#include <decoder/decoderHM.h>
#include <decoder/decoderLibde265.h>
#include <decoder/decoderVTM.h>
#include <decoder/decoderVVDec.h>
#include <parser/AVC/AnnexBAVC.h>
#include <parser/HEVC/AnnexBHEVC.h>
#include <parser/VVC/AnnexBVVC.h>
#include <parser/common/SubByteReaderLogging.h>
#include <statistics/StatisticsDataPainting.h>
#include <ui/mainwindow.h>
#include <ui_playlistItemCompressedFile_logDialog.h>
#include <video/videoHandlerRGB.h>
#include <video/videoHandlerYUV.h>

using namespace functions;
using namespace decoder;

#define COMPRESSED_VIDEO_DEBUG_OUTPUT 0
#if COMPRESSED_VIDEO_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_COMPRESSED(f) qDebug() << f
#else
#define DEBUG_COMPRESSED(f) ((void)0)
#endif

namespace
{

bool isInputFormatTypeAnnexB(InputFormat format)
{
  return format == InputFormat::AnnexBHEVC || format == InputFormat::AnnexBVVC ||
         format == InputFormat::AnnexBAVC;
}

bool isInputFormatTypeFFmpeg(InputFormat format)
{
  return format == InputFormat::Libav;
}

enum class Codec
{
  AV1,
  HEVC,
  VVC,
  Other
};

} // namespace

// When decoding, it can make sense to seek forward to another random access point.
// However, for this we have to clear the decoder, seek the file and restart decoding. Internally,
// the decoder might already have decoded the frame anyways so it makes no sense to seek but to just
// keep on decoding normally (linearly). If the requested fram number is only is in the future
// by lower than this threshold, we will not seek.
#define FORWARD_SEEK_THRESHOLD 5

playlistItemCompressedVideo::playlistItemCompressedVideo(const QString &compressedFilePath,
                                                         int            displayComponent,
                                                         InputFormat    input,
                                                         DecoderEngine  decoder)
    : playlistItemWithVideo(compressedFilePath)
{
  // Set the properties of the playlistItem
  // TODO: should this change with the type of video?
  setIcon(0, functionsGui::convertIcon(":img_videoHEVC.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  this->prop.isFileSource          = true;
  this->prop.propertiesWidgetTitle = "Compressed File Properties";

  // An compressed file can be cached if nothing goes wrong
  cachingEnabled = true;

  // Open the input file and get some properties (size, bit depth, subsampling) from the file
  if (input == InputFormat::Invalid)
  {
    QString ext = QFileInfo(compressedFilePath).suffix();
    if (ext == "hevc" || ext == "h265" || ext == "265")
      this->inputFormat = InputFormat::AnnexBHEVC;
    else if (ext == "vvc" || ext == "h266" || ext == "266")
      this->inputFormat = InputFormat::AnnexBVVC;
    else if (ext == "avc" || ext == "h264" || ext == "264")
      this->inputFormat = InputFormat::AnnexBAVC;
    else
      this->inputFormat = InputFormat::Libav;
  }
  else
    this->inputFormat = input;

  // While opening the file, also determine which decoders we can use
  Size                       frameSize;
  video::yuv::YUVPixelFormat formatYuv;
  video::rgb::rgbPixelFormat formatRgb;
  auto                       mainWindow = MainWindow::getMainWindow();
  Codec                      codec      = Codec::Other;
  if (isInputFormatTypeAnnexB(this->inputFormat))
  {
    // Open file
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Open annexB file");
    inputFileAnnexBLoading.reset(new FileSourceAnnexBFile(compressedFilePath));
    if (cachingEnabled)
      inputFileAnnexBCaching.reset(new FileSourceAnnexBFile(compressedFilePath));
    // inputFormatType a parser
    if (this->inputFormat == InputFormat::AnnexBHEVC)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Type is HEVC");
      inputFileAnnexBParser.reset(new parser::AnnexBHEVC());
      ffmpegCodec.setTypeHEVC();
      codec = Codec::HEVC;
    }
    else if (this->inputFormat == InputFormat::AnnexBVVC)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Type is VVC");
      inputFileAnnexBParser.reset(new parser::AnnexBVVC());
      codec = Codec::VVC;
    }
    else if (this->inputFormat == InputFormat::AnnexBAVC)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Type is AVC");
      inputFileAnnexBParser.reset(new parser::AnnexBAVC());
      ffmpegCodec.setTypeAVC();
      codec = Codec::Other;
    }

    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo Start parsing of file");
    inputFileAnnexBParser->parseAnnexBFile(inputFileAnnexBLoading, mainWindow);

    // Get the frame size and the pixel format
    frameSize = inputFileAnnexBParser->getSequenceSizeSamples();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Frame size "
                     << frameSize.width << "x" << frameSize.height);
    formatYuv = inputFileAnnexBParser->getPixelFormat();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo YUV format "
                     << formatYuv.getName().c_str());
    this->rawFormat      = video::RawFormat::YUV;
    this->prop.frameRate = inputFileAnnexBParser->getFramerate();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo framerate "
                     << this->prop.frameRate);
    this->prop.startEndRange = indexRange(0, int(inputFileAnnexBParser->getNumberPOCs() - 1));
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo startEndRange (0,"
                     << inputFileAnnexBParser->getNumberPOCs() << ")");
    this->prop.sampleAspectRatio = inputFileAnnexBParser->getSampleAspectRatio();
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo sample aspect ratio ("
        << this->prop.sampleAspectRatio.num << "," << this->prop.sampleAspectRatio.den << ")");
  }
  else
  {
    // Try ffmpeg to open the file
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo Open file using ffmpeg");
    inputFileFFmpegLoading.reset(new FileSourceFFmpegFile());
    if (!inputFileFFmpegLoading->openFile(compressedFilePath, mainWindow))
    {
      setError("Error opening file using libavcodec.");
      return;
    }
    // Is this file RGB or YUV?
    this->rawFormat = inputFileFFmpegLoading->getRawFormat();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Raw format "
                     << (this->rawFormat == raw_YUV                 ? "YUV"
                         : this->rawFormat == video::RawFormat::RGB ? "RGB"
                                                                    : "Unknown"));
    if (this->rawFormat == video::RawFormat::YUV)
      formatYuv = inputFileFFmpegLoading->getPixelFormatYUV();
    else if (this->rawFormat == video::RawFormat::RGB)
      formatRgb = inputFileFFmpegLoading->getPixelFormatRGB();
    else
    {
      setError("Unknown raw format.");
      return;
    }
    frameSize = inputFileFFmpegLoading->getSequenceSizeSamples();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Frame size "
                     << frameSize.width << "x" << frameSize.height);
    this->prop.frameRate = inputFileFFmpegLoading->getFramerate();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo framerate "
                     << this->prop.frameRate);
    this->prop.startEndRange = inputFileFFmpegLoading->getDecodableFrameLimits();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo startEndRange ("
                     << this->prop.startEndRange.first << "x" << this->prop.startEndRange.second
                     << ")");
    ffmpegCodec = inputFileFFmpegLoading->getVideoStreamCodecID();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo ffmpeg codec "
                     << ffmpegCodec.getCodecName());
    this->prop.sampleAspectRatio =
        inputFileFFmpegLoading->getVideoCodecPar().getSampleAspectRatio();
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo sample aspect ratio ("
        << this->prop.sampleAspectRatio.num << "x" << this->prop.sampleAspectRatio.den << ")");
    if (!ffmpegCodec.isNone())
      codec = Codec::Other;
    if (ffmpegCodec.isHEVC())
      codec = Codec::HEVC;
    if (ffmpegCodec.isAV1())
      codec = Codec::AV1;

    if (cachingEnabled)
    {
      // Open the file again for caching
      inputFileFFmpegCaching.reset(new FileSourceFFmpegFile());
      if (!inputFileFFmpegCaching->openFile(
              compressedFilePath, mainWindow, inputFileFFmpegLoading.data()))
      {
        setError("Error opening file a second time using libavcodec for caching.");
        return;
      }
    }
  }

  // Check/set properties
  if (!frameSize.isValid())
  {
    setError("Error opening file: Unable to obtain frame size from file.");
    return;
  }
  if (rawFormat == video::RawFormat::Invalid ||
      (rawFormat == video::RawFormat::YUV && !formatYuv.isValid()) ||
      (rawFormat == video::RawFormat::RGB && !formatRgb.isValid()))
  {
    setError("Error opening file: Unable to obtain a valid pixel format from file.");
    return;
  }

  // Allocate the videoHander (RGB or YUV)
  if (rawFormat == video::RawFormat::YUV)
  {
    video.reset(new video::videoHandlerYUV());
    auto yuvVideo = getYUVVideo();
    yuvVideo->setFrameSize(frameSize);
    yuvVideo->setYUVPixelFormat(formatYuv);
  }
  else
  {
    video.reset(new video::videoHandlerRGB());
    auto rgbVideo = getRGBVideo();
    rgbVideo->setFrameSize(frameSize);
    rgbVideo->setRGBPixelFormat(formatRgb);
  }

  // Connect the basic signals from the video
  playlistItemWithVideo::connectVideo();
  this->statisticsData.setFrameSize(this->video->getFrameSize());

  if (codec == Codec::HEVC)
    this->possibleDecoders = DecodersHEVC;
  else if (codec == Codec::VVC)
    this->possibleDecoders = DecodersVVC;
  else if (codec == Codec::AV1)
    this->possibleDecoders = DecodersAV1;
  else
    this->possibleDecoders = {DecoderEngine::FFMpeg};

  if (decoder != DecoderEngine::Invalid && vectorContains(possibleDecoders, decoder))
  {
    this->decoderEngine = decoder;
  }
  else
  {
    if (possibleDecoders.size() == 1)
      this->decoderEngine = possibleDecoders[0];
    else if (possibleDecoders.size() > 1)
    {
      // Is a default decoder set in the settings?
      QSettings settings;
      settings.beginGroup("Decoders");
      QString defaultDecSetting;
      if (codec == Codec::HEVC)
        defaultDecSetting = "DefaultDecoderHEVC";
      if (codec == Codec::VVC)
        defaultDecSetting = "DefaultDecoderVVC";
      if (codec == Codec::AV1)
        defaultDecSetting = "DefaultDecoderAV1";

      auto defaultDecoder = DecoderEngineMapper.getValue(
          settings.value(defaultDecSetting, -1).toString().toStdString());
      if (!defaultDecSetting.isEmpty() && defaultDecoder &&
          vectorContains(possibleDecoders, *defaultDecoder))
      {
        this->decoderEngine = *defaultDecoder;
      }
      else
      {
        // Prefer libde265 over FFmpeg over others
        if (vectorContains(possibleDecoders, DecoderEngine::Libde265))
          this->decoderEngine = DecoderEngine::Libde265;
        else if (vectorContains(possibleDecoders, DecoderEngine::FFMpeg))
          this->decoderEngine = DecoderEngine::FFMpeg;
        else
          this->decoderEngine = possibleDecoders[0];
      }
    }
  }

  this->statisticsUIHandler.setStatisticsData(&this->statisticsData);

  // Allocate the decoders
  DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Initializing "
                   << QString::fromStdString(DecoderEngineMapper.getName(this->decoderEngine))
                   << " decoder");
  if (!allocateDecoder(displayComponent))
    return;

  if (rawFormat == video::RawFormat::YUV)
  {
    auto yuvVideo = getYUVVideo();
    yuvVideo->showPixelValuesAsDiff =
        loadingDecoder->isSignalDifference(loadingDecoder->getDecodeSignal());
  }

  // Fill the list of statistics that we can provide
  DEBUG_COMPRESSED(
      "playlistItemCompressedVideo::playlistItemCompressedVideo Fill the statistics list");
  this->fillStatisticList();

  // Set the frame number limits
  if (this->prop.startEndRange == indexRange({-1, -1}))
    // No frames to decode
    return;

  // Seek both decoders to the start of the bitstream (this will also push the parameter sets /
  // extradata to the decoder)
  DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Seek decoders to 0");
  seekToPosition(0, 0, false);
  if (cachingEnabled)
    seekToPosition(0, 0, true);

  // Connect signals for requesting data and statistics
  connect(video.get(),
          &video::videoHandler::signalRequestRawData,
          this,
          &playlistItemCompressedVideo::loadRawData,
          Qt::DirectConnection);
  connect(&this->statisticsUIHandler,
          &stats::StatisticUIHandler::updateItem,
          this,
          &playlistItemCompressedVideo::updateStatSource);
}

void playlistItemCompressedVideo::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  auto filename = this->properties().name;

  // Determine the relative path to the HEVC file. We save both in the playlist.
  QUrl fileURL(filename);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(filename);

  YUViewDomElement d = root.ownerDocument().createElement("playlistItemCompressedVideo");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the HEVC file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);
  d.appendProperiteChild("displayComponent",
                         QString::number(loadingDecoder ? loadingDecoder->getDecodeSignal() : -1));

  d.appendProperiteChild("inputFormat", InputFormatMapper.getName(this->inputFormat));
  d.appendProperiteChild("decoder", DecoderEngineMapper.getName(this->decoderEngine));

  if (this->video)
    this->video->savePlaylist(d);
  if (this->loadingDecoder && loadingDecoder->statisticsSupported())
  {
    auto newChild = YUViewDomElement(d.ownerDocument().createElement("StatisticsData"));
    this->statisticsData.savePlaylist(newChild);
    d.appendChild(newChild);
  }

  root.appendChild(d);
}

playlistItemCompressedVideo *
playlistItemCompressedVideo::newPlaylistItemCompressedVideo(const YUViewDomElement &root,
                                                            const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemRawCodedVideo
  auto absolutePath  = root.findChildValue("absolutePath");
  auto relativePath  = root.findChildValue("relativePath");
  int  displaySignal = root.findChildValue("displayComponent").toInt();

  // check if file with absolute path exists, otherwise check relative path
  auto filePath = FileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  auto input = InputFormat::Libav;
  if (auto readFormat =
          InputFormatMapper.getValue(root.findChildValue("inputFormat").toStdString()))
    input = *readFormat;

  auto decoder = DecoderEngine::FFMpeg;
  if (auto readDec = DecoderEngineMapper.getValue(root.findChildValue("decoder").toStdString()))
    decoder = *readDec;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  auto newFile = new playlistItemCompressedVideo(filePath, displaySignal, input, decoder);

  newFile->video->loadPlaylist(root);
  auto n = root.firstChild();
  while (!n.isNull())
  {
    if (n.isElement())
    {
      QDomElement elem = n.toElement();
      if (elem.tagName() == "StatisticsData")
        newFile->statisticsData.loadPlaylist(elem);
    }
    n = n.nextSibling();
  }
  playlistItem::loadPropertiesFromPlaylist(root, newFile);

  return newFile;
}

InfoData playlistItemCompressedVideo::getInfo() const
{
  InfoData info("HEVC File Info");

  // At first append the file information part (path, date created, file size...)
  // info.items.append(loadingDecoder->getFileInfoList());

  info.items.append(
      InfoItem("Reader", QString::fromStdString(InputFormatMapper.getName(this->inputFormat))));
  if (inputFileFFmpegLoading)
  {
    auto l = inputFileFFmpegLoading->getLibraryPaths();
    if (l.length() % 3 == 0)
    {
      for (int i = 0; i < l.length() / 3; i++)
        info.items.append(InfoItem(l[i * 3], l[i * 3 + 1], l[i * 3 + 2]));
    }
  }
  if (!unresolvableError)
  {
    auto videoSize = video->getFrameSize();
    info.items.append(InfoItem("Resolution",
                               QString("%1x%2").arg(videoSize.width).arg(videoSize.height),
                               "The video resolution in pixel (width x height)"));
    auto nrFrames =
        (this->properties().startEndRange.second - this->properties().startEndRange.first) + 1;
    info.items.append(
        InfoItem("Num POCs", QString::number(nrFrames), "The number of pictures in the stream."));
    if (decodingEnabled)
    {
      auto l = loadingDecoder->getLibraryPaths();
      if (l.length() % 3 == 0)
      {
        for (int i = 0; i < l.length() / 3; i++)
          info.items.append(InfoItem(l[i * 3], l[i * 3 + 1], l[i * 3 + 2]));
      }
      info.items.append(InfoItem("Decoder", loadingDecoder->getDecoderName()));
      info.items.append(InfoItem("Decoder", loadingDecoder->getCodecName()));
      info.items.append(InfoItem("Statistics",
                                 loadingDecoder->statisticsSupported() ? "Yes" : "No",
                                 "Is the decoder able to provide internals (statistics)?"));
      info.items.append(
          InfoItem("Stat Parsing",
                   loadingDecoder->statisticsEnabled() ? "Yes" : "No",
                   "Are the statistics of the sequence currently extracted from the stream?"));
    }
  }
  if (this->decoderEngine == DecoderEngine::FFMpeg)
    info.items.append(
        InfoItem("FFMpeg Log", "Show FFmpeg Log", "Show the log messages from FFmpeg.", true, 0));

  return info;
}

void playlistItemCompressedVideo::infoListButtonPressed(int buttonID)
{
  auto mainWindow = MainWindow::getMainWindow();
  if (buttonID == 0)
  {
    // The button "shof ffmpeg log" was pressed
    QDialog             newDialog(mainWindow);
    Ui::ffmpegLogDialog uiDialog;
    uiDialog.setupUi(&newDialog);

    // Get the ffmpeg log
    auto    logFFmpeg = decoder::decoderFFmpeg::getLogMessages();
    QString logFFmpegString;
    for (const auto &l : logFFmpeg)
      logFFmpegString.append(l);
    uiDialog.ffmpegLogEdit->setPlainText(logFFmpegString);

    // Get the loading log
    if (inputFileFFmpegLoading)
    {
      auto    logLoading = inputFileFFmpegLoading->getFFmpegLoadingLog();
      QString logLoadingString;
      for (const auto &l : logLoading)
        logLoadingString.append(l + "\n");
      uiDialog.libraryLogEdit->setPlainText(logLoadingString);
    }

    newDialog.exec();
  }
}

ItemLoadingState playlistItemCompressedVideo::needsLoading(int frameIdx, bool loadRawData)
{
  if (unresolvableError || !decodingEnabled)
    return ItemLoadingState::LoadingNotNeeded;

  auto videoState = video->needsLoading(frameIdx, loadRawData);
  if (videoState == ItemLoadingState::LoadingNeeded && decodingNotPossibleAfter >= 0 &&
      frameIdx >= decodingNotPossibleAfter && frameIdx >= currentFrameIdx[0])
    // The decoder can not decode this frame.
    return ItemLoadingState::LoadingNotNeeded;
  if (videoState == ItemLoadingState::LoadingNeeded ||
      this->statisticsData.needsLoading(frameIdx) == ItemLoadingState::LoadingNeeded)
    return ItemLoadingState::LoadingNeeded;
  return videoState;
}

void playlistItemCompressedVideo::drawItem(QPainter *painter,
                                           int       frameIdx,
                                           double    zoomFactor,
                                           bool      drawRawData)
{
  auto range = this->properties().startEndRange;
  if (decodingNotPossibleAfter >= 0 && frameIdx >= decodingNotPossibleAfter)
  {
    infoText = "Decoding of the frame not possible:\n";
    infoText += "The frame could not be decoded. Possibly, the bitstream is corrupt or was cut at "
                "an invalid position.";
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (unresolvableError || !decodingEnabled)
  {
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (loadingDecoder.isNull())
  {
    infoText = "No decoder allocated.\n";
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (frameIdx >= range.first && frameIdx <= range.second)
  {
    video->drawFrame(painter, frameIdx, zoomFactor, drawRawData);
    stats::paintStatisticsData(painter, this->statisticsData, frameIdx, zoomFactor);
  }
}

void playlistItemCompressedVideo::loadRawData(int frameIdx, bool caching)
{
  if (caching && !cachingEnabled)
    return;
  if (!caching && loadingDecoder->state() == decoder::DecoderState::Error)
  {
    if (frameIdx < currentFrameIdx[0])
    {
      // There was an error in the loading decoder but we will seek backwards so maybe this will
      // work again
    }
    else
      return;
  }
  if (caching && cachingDecoder->state() == decoder::DecoderState::Error)
    return;

  DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData " << frameIdx
                                                               << (caching ? " caching" : ""));

  if (frameIdx > this->properties().startEndRange.second || frameIdx < 0)
  {
    DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData Invalid frame index");
    return;
  }

  // Get the right decoder
  auto dec         = caching ? cachingDecoder.data() : loadingDecoder.data();
  int  curFrameIdx = caching ? currentFrameIdx[1] : currentFrameIdx[0];

  // Should we seek?
  if (curFrameIdx == -1 || frameIdx < curFrameIdx ||
      frameIdx > curFrameIdx + FORWARD_SEEK_THRESHOLD)
  {
    // Definitely seek when we have to go backwards
    bool seek = curFrameIdx == -1 || (frameIdx < curFrameIdx);

    // Get the closest possible seek position
    size_t  seekToFrame = 0;
    int64_t seekToDTS   = -1;
    if (isInputFormatTypeAnnexB(this->inputFormat))
    {
      auto curIdx   = unsigned(std::max(curFrameIdx, 0));
      auto seekInfo = inputFileAnnexBParser->getClosestSeekPoint(unsigned(frameIdx), curIdx);
      if (seekInfo.frameDistanceInCodingOrder > FORWARD_SEEK_THRESHOLD)
        seek = true;
      seekToFrame = seekInfo.frameIndex;
    }
    else
    {
      if (caching)
        std::tie(seekToDTS, seekToFrame) =
            inputFileFFmpegCaching->getClosestSeekableFrameBefore(frameIdx);
      else
        std::tie(seekToDTS, seekToFrame) =
            inputFileFFmpegLoading->getClosestSeekableFrameBefore(frameIdx);

      // The distance in the display order unfortunately does not tell us
      // too much about the number of frames that must be decoded to seek
      // so this is more of a guess.
      if (seekToFrame > unsigned(curFrameIdx) + FORWARD_SEEK_THRESHOLD)
        seek = true;
    }

    if (seek)
    {
      // Seek and update the frame counters. The seekToPosition function will update the
      // currentFrameIdx[] indices
      this->readAnnexBFrameCounterCodingOrder = int(seekToFrame);
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData seeking to frame "
                       << seekToFrame << " PTS " << seekToDTS << " AnnexBCnt "
                       << readAnnexBFrameCounterCodingOrder);
      this->seekToPosition(this->readAnnexBFrameCounterCodingOrder, seekToDTS, caching);
    }
  }

  // Decode until we get the right frame from the decoder
  bool rightFrame = caching ? currentFrameIdx[1] == frameIdx : currentFrameIdx[0] == frameIdx;
  while (!rightFrame)
  {
    while (dec->state() == decoder::DecoderState::NeedsMoreData)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData decoder needs more data");
      if (isInputFormatTypeFFmpeg(this->inputFormat) &&
          this->decoderEngine == DecoderEngine::FFMpeg)
      {
        // In this scenario, we can read and push AVPackets
        // from the FFmpeg file and pass them to the FFmpeg decoder directly.
        auto pkt   = caching ? inputFileFFmpegCaching->getNextPacket(repushData)
                             : inputFileFFmpegLoading->getNextPacket(repushData);
        repushData = false;
        if (pkt)
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData retrived packet PTS "
                           << pkt.getPTS());
        else
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData retrived empty packet");
        auto ffmpegDec = (caching ? dynamic_cast<decoder::decoderFFmpeg *>(cachingDecoder.data())
                                  : dynamic_cast<decoder::decoderFFmpeg *>(loadingDecoder.data()));
        if (!ffmpegDec->pushAVPacket(pkt))
        {
          if (ffmpegDec->state() != decoder::DecoderState::RetrieveFrames)
            // The decoder did not switch to decoding frame mode. Error.
            return;
          repushData = true;
        }
      }
      else if (isInputFormatTypeAnnexB(this->inputFormat) &&
               this->decoderEngine == DecoderEngine::FFMpeg)
      {
        // We are reading from a raw annexB file and use ffmpeg for decoding
        QByteArray data;
        if (readAnnexBFrameCounterCodingOrder >= 0 &&
            unsigned(readAnnexBFrameCounterCodingOrder) >= inputFileAnnexBParser->getNumberPOCs())
        {
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData EOF");
        }
        else
        {
          // Get the data of the next frame (which might be multiple NAL units)
          auto frameStartEndFilePos =
              inputFileAnnexBParser->getFrameStartEndPos(readAnnexBFrameCounterCodingOrder);
          Q_ASSERT_X(frameStartEndFilePos,
                     "playlistItemCompressedVideo::loadRawData",
                     "frameStartEndFilePos could not be retrieved. This should always work for a "
                     "raw AnnexB file.");

          data = caching ? inputFileAnnexBCaching->getFrameData(*frameStartEndFilePos)
                         : inputFileAnnexBLoading->getFrameData(*frameStartEndFilePos);
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData retrived frame data from file "
                           "- AnnexBCnt "
                           << readAnnexBFrameCounterCodingOrder << " startEnd "
                           << frameStartEndFilePos->first << "-" << frameStartEndFilePos->second
                           << " - size " << data.size());
        }

        if (!dec->pushData(data))
        {
          if (dec->state() != decoder::DecoderState::RetrieveFrames)
          {
            DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData The decoder did not switch "
                             "to decoding frame mode. Error.");
            decodingNotPossibleAfter = frameIdx;
            break;
          }
          // Pushing the data failed because the ffmpeg decoder wants us to read frames first.
          // Don't increase readAnnexBFrameCounterCodingOrder so that we will push the same data
          // again.
        }
        else
          readAnnexBFrameCounterCodingOrder++;
      }
      else if (isInputFormatTypeAnnexB(this->inputFormat) &&
               this->decoderEngine != DecoderEngine::FFMpeg)
      {
        auto data = caching ? inputFileAnnexBCaching->getNextNALUnit(repushData)
                            : inputFileAnnexBLoading->getNextNALUnit(repushData);
        DEBUG_COMPRESSED(
            "playlistItemCompressedVideo::loadRawData retrived nal unit from file - size "
            << data.size());
        repushData = !dec->pushData(data);
      }
      else if (isInputFormatTypeFFmpeg(this->inputFormat) &&
               this->decoderEngine != DecoderEngine::FFMpeg)
      {
        // Get the next unit (NAL or OBU) form ffmepg and push it to the decoder
        auto data = caching ? inputFileFFmpegCaching->getNextUnit(repushData)
                            : inputFileFFmpegLoading->getNextUnit(repushData);
        DEBUG_COMPRESSED(
            "playlistItemCompressedVideo::loadRawData retrived nal unit from file - size "
            << data.size());
        repushData = !dec->pushData(data);
      }
      else
        assert(false);
    }

    if (dec->state() == decoder::DecoderState::RetrieveFrames)
    {
      if (dec->decodeNextFrame())
      {
        if (caching)
          currentFrameIdx[1]++;
        else
          currentFrameIdx[0]++;

        DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData decoded frame "
                         << (caching ? currentFrameIdx[1] : currentFrameIdx[0]));
        rightFrame = caching ? currentFrameIdx[1] == frameIdx : currentFrameIdx[0] == frameIdx;
        if (rightFrame)
        {
          if (dec->statisticsEnabled())
            this->statisticsData.setFrameIndex(frameIdx);
          video->rawData            = dec->getRawFrameData();
          video->rawData_frameIndex = frameIdx;
        }
      }
    }

    if (dec->state() != decoder::DecoderState::NeedsMoreData &&
        dec->state() != decoder::DecoderState::RetrieveFrames)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData decoder neither needs more data "
                       "nor can decode frames");
      decodingNotPossibleAfter = frameIdx;
      break;
    }
  }

  if (decodingNotPossibleAfter >= 0 && frameIdx >= decodingNotPossibleAfter)
  {
    // The specified frame (which is thoretically in the bitstream) can not be decoded.
    // Maybe the bitstream was cut at a position that it was not supposed to be cut at.
    if (caching)
      currentFrameIdx[1] = frameIdx;
    else
      currentFrameIdx[0] = frameIdx;
    // Just set the frame number of the buffer to the current frame so that it will trigger a
    // reload when the frame number changes.
    video->rawData_frameIndex = frameIdx;
  }
  else if (loadingDecoder->state() == decoder::DecoderState::Error)
  {
    infoText = "There was an error in the decoder: \n";
    infoText += loadingDecoder->decoderErrorString();
    infoText += "\n";

    decodingEnabled = false;
  }
}

void playlistItemCompressedVideo::seekToPosition(int seekToFrame, int64_t seekToDTS, bool caching)
{
  // Do the seek
  auto dec = caching ? cachingDecoder.data() : loadingDecoder.data();
  dec->resetDecoder();
  repushData               = false;
  decodingNotPossibleAfter = -1;

  // Retrieval of the raw metadata is only required if the the reader or the decoder is not ffmpeg
  const bool bothFFmpeg =
      (!isInputFormatTypeAnnexB(this->inputFormat) && this->decoderEngine == DecoderEngine::FFMpeg);
  const bool decFFmpeg = (this->decoderEngine == DecoderEngine::FFMpeg);

  QByteArrayList parametersets;
  if (isInputFormatTypeAnnexB(this->inputFormat))
  {
    uint64_t filePos = 0;
    if (!bothFFmpeg)
    {
      auto seekData = inputFileAnnexBParser->getSeekData(seekToFrame);
      if (!seekData)
      {
        DEBUG_COMPRESSED("playlistItemCompressedVideo::seekToPosition Error getting seek data");
      }
      else
      {
        if (seekData->filePos)
          filePos = *seekData->filePos;
        for (const auto &paramSet : seekData->parameterSets)
          parametersets.push_back(
              parser::reader::SubByteReaderLogging::convertToQByteArray(paramSet));
      }
    }
    DEBUG_COMPRESSED("playlistItemCompressedVideo::seekToPosition seeking annexB file to filePos "
                     << filePos);
    if (caching)
      inputFileAnnexBCaching->seek(filePos);
    else
      inputFileAnnexBLoading->seek(filePos);
  }
  else
  {
    if (!bothFFmpeg)
      parametersets = caching ? inputFileFFmpegCaching->getParameterSets()
                              : inputFileFFmpegLoading->getParameterSets();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::seekToPosition seeking ffmpeg file to pts "
                     << seekToDTS);
    if (caching)
      inputFileFFmpegCaching->seekToDTS(seekToDTS);
    else
      inputFileFFmpegLoading->seekToDTS(seekToDTS);
  }

  // In case of using ffmpeg for decoding, we don't need to push the parameter sets (the
  // extradata) to the decoder explicitly when seeking.
  if (!decFFmpeg)
  {
    // Push the parameter sets to the decoder
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::seekToPosition pushing parameter sets to decoder (nr "
        << parametersets.length() << ")");
    for (QByteArray d : parametersets)
      if (!dec->pushData(d))
      {
        setDecodingError("Error when seeking in file.");
        return;
      }
  }
  if (caching)
    currentFrameIdx[1] = seekToFrame - 1;
  else
    currentFrameIdx[0] = seekToFrame - 1;
}

void playlistItemCompressedVideo::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  if (!video)
  {
    playlistItem::createPropertiesWidget();
    return;
  }

  // Create a new widget and populate it with controls
  propertiesWidget.reset(new QWidget);
  ui.setupUi(propertiesWidget.data());

  auto lineOne = new QFrame;
  lineOne->setObjectName(QStringLiteral("line"));
  lineOne->setFrameShape(QFrame::HLine);
  lineOne->setFrameShadow(QFrame::Sunken);
  auto lineTwo = new QFrame;
  lineTwo->setObjectName(QStringLiteral("line"));
  lineTwo->setFrameShape(QFrame::HLine);
  lineTwo->setFrameShadow(QFrame::Sunken);

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  ui.verticalLayout->insertLayout(0, createPlaylistItemControls());
  ui.verticalLayout->insertWidget(1, lineOne);
  ui.verticalLayout->insertLayout(2, video->createVideoHandlerControls(true));
  ui.verticalLayout->insertWidget(5, lineTwo);
  ui.verticalLayout->insertLayout(
      6, this->statisticsUIHandler.createStatisticsHandlerControls(), 1);

  // Set the components that we can display
  if (loadingDecoder)
  {
    ui.comboBoxDisplaySignal->addItems(loadingDecoder->getSignalNames());
    ui.comboBoxDisplaySignal->setCurrentIndex(loadingDecoder->getDecodeSignal());
  }
  // Add decoders we can use
  for (auto e : possibleDecoders)
    ui.comboBoxDecoder->addItem(QString::fromStdString(DecoderEngineMapper.getName(e)));
  ui.comboBoxDecoder->setCurrentIndex(indexInVec(possibleDecoders, this->decoderEngine));

  // Connect signals/slots
  connect(ui.comboBoxDisplaySignal,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &playlistItemCompressedVideo::displaySignalComboBoxChanged);
  connect(ui.comboBoxDecoder,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &playlistItemCompressedVideo::decoderComboxBoxChanged);
}

bool playlistItemCompressedVideo::allocateDecoder(int displayComponent)
{
  // Reset (existing) decoders
  loadingDecoder.reset();
  cachingDecoder.reset();

  if (this->decoderEngine == DecoderEngine::Libde265)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive libde265 decoder");
    loadingDecoder.reset(new decoder::decoderLibde265(displayComponent));
    if (cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder Initializing caching libde265 decoder");
      cachingDecoder.reset(new decoder::decoderLibde265(displayComponent, true));
    }
  }
  else if (this->decoderEngine == DecoderEngine::HM)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive HM decoder");
    loadingDecoder.reset(new decoder::decoderHM(displayComponent));
    if (cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive HM decoder");
      cachingDecoder.reset(new decoder::decoderHM(displayComponent, true));
    }
  }
  else if (this->decoderEngine == DecoderEngine::VTM)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive VTM decoder");
    loadingDecoder.reset(new decoder::decoderVTM(displayComponent));
    if (cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive VTM decoder");
      cachingDecoder.reset(new decoder::decoderVTM(displayComponent, true));
    }
  }
  else if (this->decoderEngine == DecoderEngine::VVDec)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive VVDec decoder");
    loadingDecoder.reset(new decoder::decoderVVDec(displayComponent));
    if (cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive VVDec decoder");
      cachingDecoder.reset(new decoder::decoderVVDec(displayComponent, true));
    }
  }
  else if (this->decoderEngine == DecoderEngine::Dav1d)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive dav1d decoder");
    loadingDecoder.reset(new decoder::decoderDav1d(displayComponent));
    if (cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive dav1d decoder");
      cachingDecoder.reset(new decoder::decoderDav1d(displayComponent, true));
    }
  }
  else if (this->decoderEngine == DecoderEngine::FFMpeg)
  {
    if (isInputFormatTypeAnnexB(this->inputFormat))
    {
      auto frameSize    = inputFileAnnexBParser->getSequenceSizeSamples();
      auto extradata    = inputFileAnnexBParser->getExtradata();
      auto fmt          = inputFileAnnexBParser->getPixelFormat();
      auto profileLevel = inputFileAnnexBParser->getProfileLevel();
      auto ratio        = inputFileAnnexBParser->getSampleAspectRatio();

      DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing interactive "
                       "ffmpeg decoder from raw anexB stream. frameSize "
                       << frameSize.width << "x" << frameSize.height << " extradata length "
                       << extradata.length() << " YUVPixelFormat "
                       << QString::fromStdString(fmt.getName()) << " profile/level "
                       << profileLevel.first << "/" << profileLevel.second << ", aspect raio "
                       << ratio.num << "/" << ratio.den);
      loadingDecoder.reset(
          new decoder::decoderFFmpeg(ffmpegCodec, frameSize, extradata, fmt, profileLevel, ratio));
      if (cachingEnabled)
      {
        DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing caching ffmpeg "
                         "decoder from raw anexB stream. Same settings.");
        cachingDecoder.reset(new decoder::decoderFFmpeg(
            ffmpegCodec, frameSize, extradata, fmt, profileLevel, ratio, true));
      }
    }
    else
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing interactive "
                       "ffmpeg decoder using ffmpeg as parser");
      loadingDecoder.reset(new decoder::decoderFFmpeg(inputFileFFmpegLoading->getVideoCodecPar()));
      if (cachingEnabled)
      {
        DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing caching ffmpeg "
                         "decoder using ffmpeg as parser");
        cachingDecoder.reset(
            new decoder::decoderFFmpeg(inputFileFFmpegCaching->getVideoCodecPar()));
      }
    }
  }
  else
  {
    infoText        = "No valid decoder was selected.";
    decodingEnabled = false;
    return false;
  }

  decodingEnabled = loadingDecoder->state() != decoder::DecoderState::Error;
  if (!decodingEnabled)
  {
    infoText = "There was an error allocating the new decoder: \n";
    infoText += loadingDecoder->decoderErrorString();
    infoText += "\n";
    return false;
  }

  return true;
}

void playlistItemCompressedVideo::fillStatisticList()
{
  if (!loadingDecoder || !loadingDecoder->statisticsSupported())
    return;

  loadingDecoder->fillStatisticList(this->statisticsData);
}

void playlistItemCompressedVideo::loadStatistics(int frameIdx)
{
  DEBUG_COMPRESSED("playlistItemCompressedVideo::loadStatisticToCache Request statistics for frame "
                   << frameIdx);

  if (!loadingDecoder->statisticsSupported())
    return;
  if (!loadingDecoder->statisticsEnabled())
  {
    // We have to enable collecting of statistics in the decoder. By default (for speed reasons)
    // this is off. Enabeling works like this: Enable collection, reset the decoder and decode the
    // current frame again. Statisitcs are always retrieved for the loading decoder.
    loadingDecoder->enableStatisticsRetrieval(&this->statisticsData);
    DEBUG_COMPRESSED("playlistItemCompressedVideo::loadStatistics Enable loading of stats frame "
                     << frameIdx);

    // Reload the current frame (force a seek and decode operation)
    int frameToLoad    = currentFrameIdx[0];
    currentFrameIdx[0] = -1;
    this->loadRawData(frameToLoad, false);

    // The statistics should now be loaded
  }
  else if (frameIdx != currentFrameIdx[0])
  {
    // If the requested frame is not currently decoded, decode it.
    // This can happen if the picture was gotten from the cache.
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::loadStatistics Trigger manual load of stats frame "
        << frameIdx);
    this->loadRawData(frameIdx, false);
  }
}

ValuePairListSets playlistItemCompressedVideo::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  newSet.append("YUV", video->getPixelValues(pixelPos, frameIdx));
  if (loadingDecoder->statisticsSupported() && loadingDecoder->statisticsEnabled())
    newSet.append("Stats", this->statisticsData.getValuesAt(pixelPos));

  return newSet;
}

void playlistItemCompressedVideo::getSupportedFileExtensions(QStringList &allExtensions,
                                                             QStringList &filters)
{
  QStringList ext;
  ext << "hevc"
      << "h265"
      << "265"
      << "avc"
      << "h264"
      << "264"
      << "vvc"
      << "h266"
      << "266"
      << "avi"
      << "avr"
      << "cdxl"
      << "xl"
      << "dv"
      << "dif"
      << "flm"
      << "flv"
      << "flv"
      << "h261"
      << "h26l"
      << "cgi"
      << "ivf"
      << "ivr"
      << "lvf"
      << "m4v"
      << "mkv"
      << "mk3d"
      << "mka"
      << "mks"
      << "mjpg"
      << "mjpeg"
      << "mpg"
      << "mpo"
      << "j2k"
      << "mov"
      << "mp4"
      << "m4a"
      << "3gp"
      << "3g2"
      << "mj2"
      << "mvi"
      << "mxg"
      << "v"
      << "ogg"
      << "mjpg"
      << "viv"
      << "webm"
      << "xmv"
      << "ts"
      << "mxf";
  QString filtersString = "FFmpeg files (";
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

  // loadingDecoder->reloadItemSource();
  // Reset the decoder somehow

  // Reset the videoHandlerYUV source. With the next draw event, the videoHandlerYUV will request to
  // decode the frame again.
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
  video->cacheFrame(frameIdx, testMode);
  cachingMutex.unlock();
}

void playlistItemCompressedVideo::loadFrame(int  frameIdx,
                                            bool playing,
                                            bool loadRawdata,
                                            bool emitSignals)
{
  // The current thread must never be the main thread but one of the interactive threads.
  Q_ASSERT(QThread::currentThread() != QApplication::instance()->thread());

  auto stateYUV  = video->needsLoading(frameIdx, loadRawdata);
  auto stateStat = this->statisticsData.needsLoading(frameIdx);

  if (stateYUV == ItemLoadingState::LoadingNeeded || stateStat == ItemLoadingState::LoadingNeeded)
  {
    isFrameLoading = true;
    if (stateYUV == ItemLoadingState::LoadingNeeded)
    {
      // Load the requested current frame
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadFrame loading frame "
                       << frameIdx << (playing ? " (playing)" : ""));
      video->loadFrame(frameIdx);
    }
    if (stateStat == ItemLoadingState::LoadingNeeded)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadFrame loading statistics "
                       << frameIdx << (playing ? " (playing)" : ""));
      this->loadStatistics(frameIdx);
    }

    isFrameLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }

  if (playing && (stateYUV == ItemLoadingState::LoadingNeeded ||
                  stateYUV == ItemLoadingState::LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdx + 1;
    if (nextFrameIdx <= this->properties().startEndRange.second)
    {
      DEBUG_COMPRESSED("playlistItplaylistItemCompressedVideoemRawFile::loadFrame loading frame "
                       "into double buffer "
                       << nextFrameIdx << (playing ? " (playing)" : ""));
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
    bool resetDecoder = false;
    loadingDecoder->setDecodeSignal(idx, resetDecoder);
    cachingDecoder->setDecodeSignal(idx, resetDecoder);

    if (resetDecoder)
    {
      loadingDecoder->resetDecoder();
      cachingDecoder->resetDecoder();

      // Reset the decoded frame indices so that decoding of the current frame is triggered
      currentFrameIdx[0] = -1;
      currentFrameIdx[1] = -1;
    }

    // A different display signal was chosen. Invalidate the cache and signal that we will need a
    // redraw.
    auto yuvVideo                   = dynamic_cast<video::videoHandlerYUV *>(video.get());
    yuvVideo->showPixelValuesAsDiff = loadingDecoder->isSignalDifference(idx);
    yuvVideo->invalidateAllBuffers();

    emit signalItemChanged(true, RECACHE_CLEAR);
  }
}

void playlistItemCompressedVideo::decoderComboxBoxChanged(int idx)
{
  auto e = this->possibleDecoders.at(idx);
  if (e != this->decoderEngine)
  {
    // Allocate a new decoder of the new type
    this->decoderEngine = e;
    this->allocateDecoder();

    // A different display signal was chosen. Invalidate the cache and signal that we will need a
    // redraw.
    auto yuvVideo = dynamic_cast<video::videoHandlerYUV *>(video.get());
    if (loadingDecoder)
      yuvVideo->showPixelValuesAsDiff = loadingDecoder->isSignalDifference(idx);
    yuvVideo->invalidateAllBuffers();

    // Reset the decoded frame indices so that decoding of the current frame is triggered
    currentFrameIdx[0] = -1;
    currentFrameIdx[1] = -1;

    this->decodingNotPossibleAfter = -1;

    // Update the list of display signals
    if (loadingDecoder)
    {
      QSignalBlocker block(ui.comboBoxDisplaySignal);
      ui.comboBoxDisplaySignal->clear();
      ui.comboBoxDisplaySignal->addItems(loadingDecoder->getSignalNames());
      ui.comboBoxDisplaySignal->setCurrentIndex(loadingDecoder->getDecodeSignal());
    }

    // Update the statistics list with what the new decoder can provide
    this->statisticsUIHandler.clearStatTypes();
    this->statisticsData.setFrameSize(this->video->getFrameSize());
    this->fillStatisticList();
    this->statisticsUIHandler.updateStatisticsHandlerControls();

    emit signalItemChanged(true, RECACHE_CLEAR);
  }
}
