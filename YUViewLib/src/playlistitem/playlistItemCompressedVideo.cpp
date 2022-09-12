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

#include <common/Functions.h>
#include <common/FunctionsGui.h>
#include <common/YUViewDomElement.h>
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
#include <ui/Mainwindow.h>
#include <ui_playlistItemCompressedFile_logDialog.h>
#include <video/videoHandlerRGB.h>
#include <video/videoHandlerYUV.h>

using namespace functions;
using namespace decoder;

#define COMPRESSED_VIDEO_DEBUG_OUTPUT 1
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
  this->setIcon(0, functionsGui::convertIcon(":img_videoHEVC.png"));
  this->setFlags(flags() | Qt::ItemIsDropEnabled);

  this->prop.isFileSource          = true;
  this->prop.propertiesWidgetTitle = "Compressed File Properties";

  // An compressed file can be cached if nothing goes wrong
  this->cachingEnabled = true;

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
  video::yuv::PixelFormatYUV formatYuv;
  video::rgb::PixelFormatRGB formatRgb;
  auto                       mainWindow = MainWindow::getMainWindow();
  Codec                      codec      = Codec::Other;
  if (isInputFormatTypeAnnexB(this->inputFormat))
  {
    // Open file
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Open annexB file");
    this->inputFileAnnexBLoading = std::make_unique<FileSourceAnnexBFile>(compressedFilePath);
    if (this->cachingEnabled)
      this->inputFileAnnexBCaching = std::make_unique<FileSourceAnnexBFile>(compressedFilePath);
    // inputFormatType a parser
    if (this->inputFormat == InputFormat::AnnexBHEVC)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Type is HEVC");
      this->inputFileAnnexBParser = std::make_unique<parser::AnnexBHEVC>();
      this->ffmpegCodec.setTypeHEVC();
      codec = Codec::HEVC;
    }
    else if (this->inputFormat == InputFormat::AnnexBVVC)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Type is VVC");
      this->inputFileAnnexBParser = std::make_unique<parser::AnnexBVVC>();
      codec                       = Codec::VVC;
    }
    else if (this->inputFormat == InputFormat::AnnexBAVC)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Type is AVC");
      this->inputFileAnnexBParser = std::make_unique<parser::AnnexBAVC>();
      this->ffmpegCodec.setTypeAVC();
      codec = Codec::Other;
    }

    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo Start parsing of file");
    this->inputFileAnnexBParser->parseAnnexBFile(this->inputFileAnnexBLoading, mainWindow);

    // Get the frame size and the pixel format
    frameSize = this->inputFileAnnexBParser->getSequenceSizeSamples();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Frame size "
                     << frameSize.width << "x" << frameSize.height);
    formatYuv = this->inputFileAnnexBParser->getPixelFormat();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo YUV format "
                     << formatYuv.getName().c_str());
    this->rawFormat      = video::RawFormat::YUV;
    this->prop.frameRate = this->inputFileAnnexBParser->getFramerate();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo framerate "
                     << this->prop.frameRate);
    this->prop.startEndRange = indexRange(0, int(this->inputFileAnnexBParser->getNumberPOCs() - 1));
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo startEndRange (0,"
                     << this->inputFileAnnexBParser->getNumberPOCs() << ")");
    this->prop.sampleAspectRatio = this->inputFileAnnexBParser->getSampleAspectRatio();
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo sample aspect ratio ("
        << this->prop.sampleAspectRatio.num << "," << this->prop.sampleAspectRatio.den << ")");
  }
  else
  {
    // Try ffmpeg to open the file
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo Open file using ffmpeg");
    this->inputFileFFmpegLoading = std::make_unique<FileSourceFFmpegFile>();
    if (!this->inputFileFFmpegLoading->openFile(compressedFilePath, mainWindow))
    {
      this->setError("Error opening file using libavcodec.");
      return;
    }
    // Is this file RGB or YUV?
    this->rawFormat = this->inputFileFFmpegLoading->getRawFormat();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Raw format "
                     << (this->rawFormat == video::RawFormat::YUV   ? "YUV"
                         : this->rawFormat == video::RawFormat::RGB ? "RGB"
                                                                    : "Unknown"));
    if (this->rawFormat == video::RawFormat::YUV)
      formatYuv = this->inputFileFFmpegLoading->getPixelFormatYUV();
    else if (this->rawFormat == video::RawFormat::RGB)
      formatRgb = this->inputFileFFmpegLoading->getPixelFormatRGB();
    else
    {
      this->setError("Unknown raw format.");
      return;
    }
    frameSize = this->inputFileFFmpegLoading->getSequenceSizeSamples();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo Frame size "
                     << frameSize.width << "x" << frameSize.height);
    this->prop.frameRate = this->inputFileFFmpegLoading->getFramerate();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo framerate "
                     << this->prop.frameRate);
    this->prop.startEndRange = this->inputFileFFmpegLoading->getDecodableFrameLimits();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo startEndRange ("
                     << this->prop.startEndRange.first << "x" << this->prop.startEndRange.second
                     << ")");
    this->ffmpegCodec = inputFileFFmpegLoading->getVideoStreamCodecID();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::playlistItemCompressedVideo ffmpeg codec "
                     << this->ffmpegCodec.getCodecName());
    this->prop.sampleAspectRatio =
        this->inputFileFFmpegLoading->getVideoCodecPar().getSampleAspectRatio();
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::playlistItemCompressedVideo sample aspect ratio ("
        << this->prop.sampleAspectRatio.num << "x" << this->prop.sampleAspectRatio.den << ")");
    if (!this->ffmpegCodec.isNone())
      codec = Codec::Other;
    if (this->ffmpegCodec.isHEVC())
      codec = Codec::HEVC;
    if (this->ffmpegCodec.isAV1())
      codec = Codec::AV1;

    if (this->cachingEnabled)
    {
      // Open the file again for caching
      this->inputFileFFmpegCaching = std::make_unique<FileSourceFFmpegFile>();
      if (!this->inputFileFFmpegCaching->openFile(
              compressedFilePath, mainWindow, this->inputFileFFmpegLoading.get()))
      {
        this->setError("Error opening file a second time using libavcodec for caching.");
        return;
      }
    }
  }

  // Check/set properties
  if (!frameSize.isValid())
  {
    this->setError("Error opening file: Unable to obtain frame size from file.");
    return;
  }
  if (this->rawFormat == video::RawFormat::Invalid ||
      (this->rawFormat == video::RawFormat::YUV && !formatYuv.isValid()) ||
      (this->rawFormat == video::RawFormat::RGB && !formatRgb.isValid()))
  {
    this->setError("Error opening file: Unable to obtain a valid pixel format from file.");
    return;
  }

  // Allocate the videoHander (RGB or YUV)
  if (this->rawFormat == video::RawFormat::YUV)
  {
    this->video   = std::make_unique<video::yuv::videoHandlerYUV>();
    auto yuvVideo = this->getYUVVideo();
    yuvVideo->setFrameSize(frameSize);
    yuvVideo->setPixelFormatYUV(formatYuv);
  }
  else
  {
    this->video   = std::make_unique<video::rgb::videoHandlerRGB>();
    auto rgbVideo = this->getRGBVideo();
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
    if (this->possibleDecoders.size() == 1)
      this->decoderEngine = this->possibleDecoders[0];
    else if (this->possibleDecoders.size() > 1)
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
  if (!this->allocateDecoder(displayComponent))
    return;

  if (this->rawFormat == video::RawFormat::YUV)
  {
    auto yuvVideo = this->getYUVVideo();
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
  this->seekToPosition(0, 0, false);
  if (this->cachingEnabled)
    this->seekToPosition(0, 0, true);

  // Connect signals for requesting data and statistics
  this->connect(video.get(),
                &video::videoHandler::signalRequestRawData,
                this,
                &playlistItemCompressedVideo::loadRawData,
                Qt::DirectConnection);
  this->connect(&this->statisticsUIHandler,
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
  auto relativePath = playlistDir.relativeFilePath(filename);

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
  if (this->inputFileFFmpegLoading)
  {
    auto l = this->inputFileFFmpegLoading->getLibraryPaths();
    if (l.length() % 3 == 0)
    {
      for (int i = 0; i < l.length() / 3; i++)
        info.items.append(InfoItem(l[i * 3], l[i * 3 + 1], l[i * 3 + 2]));
    }
  }
  if (!this->unresolvableError)
  {
    auto videoSize = video->getFrameSize();
    info.items.append(InfoItem("Resolution",
                               QString("%1x%2").arg(videoSize.width).arg(videoSize.height),
                               "The video resolution in pixel (width x height)"));
    auto nrFrames =
        (this->properties().startEndRange.second - this->properties().startEndRange.first) + 1;
    info.items.append(
        InfoItem("Num POCs", QString::number(nrFrames), "The number of pictures in the stream."));
    if (this->decodingEnabled)
    {
      auto l = loadingDecoder->getLibraryPaths();
      if (l.length() % 3 == 0)
      {
        for (int i = 0; i < l.length() / 3; i++)
          info.items.append(InfoItem(l[i * 3], l[i * 3 + 1], l[i * 3 + 2]));
      }
      info.items.append(InfoItem("Decoder", this->loadingDecoder->getDecoderName()));
      info.items.append(InfoItem("Decoder", this->loadingDecoder->getCodecName()));
      info.items.append(InfoItem("Statistics",
                                 this->loadingDecoder->statisticsSupported() ? "Yes" : "No",
                                 "Is the decoder able to provide internals (statistics)?"));
      info.items.append(
          InfoItem("Stat Parsing",
                   this->loadingDecoder->statisticsEnabled() ? "Yes" : "No",
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
    if (this->inputFileFFmpegLoading)
    {
      auto    logLoading = this->inputFileFFmpegLoading->getFFmpegLoadingLog();
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
  if (this->unresolvableError || !this->decodingEnabled)
    return ItemLoadingState::LoadingNotNeeded;

  auto videoState = this->video->needsLoading(frameIdx, loadRawData);
  if (videoState == ItemLoadingState::LoadingNeeded && this->decodingNotPossibleAfter >= 0 &&
      frameIdx >= this->decodingNotPossibleAfter && frameIdx >= this->currentFrameIdx[0])
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
  const auto range = this->properties().startEndRange;
  if (this->decodingNotPossibleAfter >= 0 && frameIdx >= this->decodingNotPossibleAfter)
  {
    this->infoText = "Decoding of the frame not possible:\n";
    this->infoText +=
        "The frame could not be decoded. Possibly, the bitstream is corrupt or was cut at "
        "an invalid position.";
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (this->unresolvableError || !this->decodingEnabled)
  {
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (!this->loadingDecoder)
  {
    this->infoText = "No decoder allocated.\n";
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  }
  else if (frameIdx >= range.first && frameIdx <= range.second)
  {
    this->video->drawFrame(painter, frameIdx, zoomFactor, drawRawData);
    stats::paintStatisticsData(painter, this->statisticsData, frameIdx, zoomFactor);
  }
}

void playlistItemCompressedVideo::loadRawData(int frameIdx, bool caching)
{
  if (caching && !this->cachingEnabled)
    return;
  if (!caching && this->loadingDecoder->state() == decoder::DecoderState::Error)
  {
    if (frameIdx < this->currentFrameIdx[0])
    {
      // There was an error in the loading decoder but we will seek backwards so maybe this will
      // work again
    }
    else
      return;
  }
  if (caching && this->cachingDecoder->state() == decoder::DecoderState::Error)
    return;

  DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData " << frameIdx
                                                               << (caching ? " caching" : ""));

  if (frameIdx > this->properties().startEndRange.second || frameIdx < 0)
  {
    DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData Invalid frame index");
    return;
  }

  // Get the right decoder
  const auto dec         = caching ? this->cachingDecoder.get() : this->loadingDecoder.get();
  const auto curFrameIdx = caching ? this->currentFrameIdx[1] : this->currentFrameIdx[0];

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
      auto seekInfo = this->inputFileAnnexBParser->getClosestSeekPoint(unsigned(frameIdx), curIdx);
      if (seekInfo.frameDistanceInCodingOrder > FORWARD_SEEK_THRESHOLD)
        seek = true;
      seekToFrame = seekInfo.frameIndex;
    }
    else
    {
      if (caching)
        std::tie(seekToDTS, seekToFrame) =
            this->inputFileFFmpegCaching->getClosestSeekableFrameBefore(frameIdx);
      else
        std::tie(seekToDTS, seekToFrame) =
            this->inputFileFFmpegLoading->getClosestSeekableFrameBefore(frameIdx);

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
                       << this->readAnnexBFrameCounterCodingOrder);
      this->seekToPosition(this->readAnnexBFrameCounterCodingOrder, seekToDTS, caching);
    }
  }

  // Decode until we get the right frame from the decoder
  bool rightFrame =
      caching ? this->currentFrameIdx[1] == frameIdx : this->currentFrameIdx[0] == frameIdx;
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
        auto pkt   = caching ? this->inputFileFFmpegCaching->getNextPacket(repushData)
                             : this->inputFileFFmpegLoading->getNextPacket(repushData);
        repushData = false;
        if (pkt)
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData retrived packet PTS "
                           << pkt.getPTS());
        else
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData retrived empty packet");
        auto ffmpegDec =
            (caching ? dynamic_cast<decoder::decoderFFmpeg *>(this->cachingDecoder.get())
                     : dynamic_cast<decoder::decoderFFmpeg *>(this->loadingDecoder.get()));
        if (!ffmpegDec->pushAVPacket(pkt))
        {
          if (ffmpegDec->state() != decoder::DecoderState::RetrieveFrames)
            // The decoder did not switch to decoding frame mode. Error.
            return;
          this->repushData = true;
        }
      }
      else if (isInputFormatTypeAnnexB(this->inputFormat) &&
               this->decoderEngine == DecoderEngine::FFMpeg)
      {
        // We are reading from a raw annexB file and use ffmpeg for decoding
        QByteArray data;
        if (this->readAnnexBFrameCounterCodingOrder >= 0 &&
            unsigned(this->readAnnexBFrameCounterCodingOrder) >=
                this->inputFileAnnexBParser->getNumberPOCs())
        {
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData EOF");
        }
        else
        {
          // Get the data of the next frame (which might be multiple NAL units)
          auto frameStartEndFilePos = this->inputFileAnnexBParser->getFrameStartEndPos(
              this->readAnnexBFrameCounterCodingOrder);
          Q_ASSERT_X(frameStartEndFilePos,
                     "playlistItemCompressedVideo::loadRawData",
                     "frameStartEndFilePos could not be retrieved. This should always work for a "
                     "raw AnnexB file.");

          data = caching ? this->inputFileAnnexBCaching->getFrameData(*frameStartEndFilePos)
                         : this->inputFileAnnexBLoading->getFrameData(*frameStartEndFilePos);
          DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData retrived frame data from file "
                           "- AnnexBCnt "
                           << this->readAnnexBFrameCounterCodingOrder << " startEnd "
                           << frameStartEndFilePos->first << "-" << frameStartEndFilePos->second
                           << " - size " << data.size());
        }

        if (!dec->pushData(data))
        {
          if (dec->state() != decoder::DecoderState::RetrieveFrames)
          {
            DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData The decoder did not switch "
                             "to decoding frame mode. Error.");
            this->decodingNotPossibleAfter = frameIdx;
            break;
          }
          // Pushing the data failed because the ffmpeg decoder wants us to read frames first.
          // Don't increase readAnnexBFrameCounterCodingOrder so that we will push the same data
          // again.
        }
        else
          this->readAnnexBFrameCounterCodingOrder++;
      }
      else if (isInputFormatTypeAnnexB(this->inputFormat) &&
               this->decoderEngine != DecoderEngine::FFMpeg)
      {
        auto data = caching ? this->inputFileAnnexBCaching->getNextNALUnit(repushData)
                            : this->inputFileAnnexBLoading->getNextNALUnit(repushData);
        DEBUG_COMPRESSED(
            "playlistItemCompressedVideo::loadRawData retrived nal unit from file - size "
            << data.size());
        this->repushData = !dec->pushData(data);
      }
      else if (isInputFormatTypeFFmpeg(this->inputFormat) &&
               this->decoderEngine != DecoderEngine::FFMpeg)
      {
        // Get the next unit (NAL or OBU) form ffmepg and push it to the decoder
        auto data = caching ? this->inputFileFFmpegCaching->getNextUnit(repushData)
                            : this->inputFileFFmpegLoading->getNextUnit(repushData);
        DEBUG_COMPRESSED(
            "playlistItemCompressedVideo::loadRawData retrived nal unit from file - size "
            << data.size());
        this->repushData = !dec->pushData(data);
      }
      else
        assert(false);
    }

    if (dec->state() == decoder::DecoderState::RetrieveFrames)
    {
      if (dec->decodeNextFrame())
      {
        if (caching)
          this->currentFrameIdx[1]++;
        else
          this->currentFrameIdx[0]++;

        DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData decoded frame "
                         << (caching ? this->currentFrameIdx[1] : this->currentFrameIdx[0]));
        rightFrame =
            caching ? this->currentFrameIdx[1] == frameIdx : this->currentFrameIdx[0] == frameIdx;
        if (rightFrame)
        {
          if (dec->statisticsEnabled())
            this->statisticsData.setFrameIndex(frameIdx);
          this->video->rawData            = dec->getRawFrameData();
          this->video->rawData_frameIndex = frameIdx;
        }
      }
    }

    if (dec->state() != decoder::DecoderState::NeedsMoreData &&
        dec->state() != decoder::DecoderState::RetrieveFrames)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadRawData decoder neither needs more data "
                       "nor can decode frames");
      this->decodingNotPossibleAfter = frameIdx;
      break;
    }
  }

  if (this->decodingNotPossibleAfter >= 0 && frameIdx >= this->decodingNotPossibleAfter)
  {
    // The specified frame (which is thoretically in the bitstream) can not be decoded.
    // Maybe the bitstream was cut at a position that it was not supposed to be cut at.
    if (caching)
      this->currentFrameIdx[1] = frameIdx;
    else
      this->currentFrameIdx[0] = frameIdx;
    // Just set the frame number of the buffer to the current frame so that it will trigger a
    // reload when the frame number changes.
    this->video->rawData_frameIndex = frameIdx;
  }
  else if (this->loadingDecoder->state() == decoder::DecoderState::Error)
  {
    this->infoText = "There was an error in the decoder: \n";
    this->infoText += this->loadingDecoder->decoderErrorString();
    this->infoText += "\n";

    this->decodingEnabled = false;
  }
}

void playlistItemCompressedVideo::seekToPosition(int seekToFrame, int64_t seekToDTS, bool caching)
{
  // Do the seek
  auto dec = caching ? this->cachingDecoder.get() : this->loadingDecoder.get();
  dec->resetDecoder();
  this->repushData               = false;
  this->decodingNotPossibleAfter = -1;

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
      auto seekData = this->inputFileAnnexBParser->getSeekData(seekToFrame);
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
      this->inputFileAnnexBCaching->seek(filePos);
    else
      this->inputFileAnnexBLoading->seek(filePos);
  }
  else
  {
    if (!bothFFmpeg)
      parametersets = caching ? this->inputFileFFmpegCaching->getParameterSets()
                              : this->inputFileFFmpegLoading->getParameterSets();
    DEBUG_COMPRESSED("playlistItemCompressedVideo::seekToPosition seeking ffmpeg file to pts "
                     << seekToDTS);
    if (caching)
      this->inputFileFFmpegCaching->seekToDTS(seekToDTS);
    else
      this->inputFileFFmpegLoading->seekToDTS(seekToDTS);
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
    this->currentFrameIdx[1] = seekToFrame - 1;
  else
    this->currentFrameIdx[0] = seekToFrame - 1;
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
  this->propertiesWidget.reset(new QWidget);
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
  this->connect(ui.comboBoxDisplaySignal,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &playlistItemCompressedVideo::displaySignalComboBoxChanged);
  this->connect(ui.comboBoxDecoder,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &playlistItemCompressedVideo::decoderComboxBoxChanged);
}

bool playlistItemCompressedVideo::allocateDecoder(int displayComponent)
{
  // Reset (existing) decoders
  this->loadingDecoder.reset();
  this->cachingDecoder.reset();

  if (this->decoderEngine == DecoderEngine::Libde265)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive libde265 decoder");
    this->loadingDecoder = std::make_unique<decoder::decoderLibde265>(displayComponent);
    if (this->cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder Initializing caching libde265 decoder");
      this->cachingDecoder = std::make_unique<decoder::decoderLibde265>(displayComponent, true);
    }
  }
  else if (this->decoderEngine == DecoderEngine::HM)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive HM decoder");
    this->loadingDecoder = std::make_unique<decoder::decoderHM>(displayComponent);
    if (this->cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive HM decoder");
      this->cachingDecoder = std::make_unique<decoder::decoderHM>(displayComponent, true);
    }
  }
  else if (this->decoderEngine == DecoderEngine::VTM)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive VTM decoder");
    this->loadingDecoder = std::make_unique<decoder::decoderVTM>(displayComponent);
    if (this->cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive VTM decoder");
      this->cachingDecoder = std::make_unique<decoder::decoderVTM>(displayComponent, true);
    }
  }
  else if (this->decoderEngine == DecoderEngine::VVDec)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive VVDec decoder");
    this->loadingDecoder = std::make_unique<decoder::decoderVVDec>(displayComponent);
    if (this->cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive VVDec decoder");
      this->cachingDecoder = std::make_unique<decoder::decoderVVDec>(displayComponent, true);
    }
  }
  else if (this->decoderEngine == DecoderEngine::Dav1d)
  {
    DEBUG_COMPRESSED(
        "playlistItemCompressedVideo::allocateDecoder Initializing interactive dav1d decoder");
    this->loadingDecoder = std::make_unique<decoder::decoderDav1d>(displayComponent);
    if (this->cachingEnabled)
    {
      DEBUG_COMPRESSED(
          "playlistItemCompressedVideo::allocateDecoder caching interactive dav1d decoder");
      this->cachingDecoder = std::make_unique<decoder::decoderDav1d>(displayComponent, true);
    }
  }
  else if (this->decoderEngine == DecoderEngine::FFMpeg)
  {
    if (isInputFormatTypeAnnexB(this->inputFormat))
    {
      auto frameSize    = this->inputFileAnnexBParser->getSequenceSizeSamples();
      auto extradata    = this->inputFileAnnexBParser->getExtradata();
      auto fmt          = this->inputFileAnnexBParser->getPixelFormat();
      auto profileLevel = this->inputFileAnnexBParser->getProfileLevel();
      auto ratio        = this->inputFileAnnexBParser->getSampleAspectRatio();

      DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing interactive "
                       "ffmpeg decoder from raw anexB stream. frameSize "
                       << frameSize.width << "x" << frameSize.height << " extradata length "
                       << extradata.length() << " PixelFormatYUV "
                       << QString::fromStdString(fmt.getName()) << " profile/level "
                       << profileLevel.first << "/" << profileLevel.second << ", aspect raio "
                       << ratio.num << "/" << ratio.den);
      this->loadingDecoder = std::make_unique<decoder::decoderFFmpeg>(
          ffmpegCodec, frameSize, extradata, fmt, profileLevel, ratio);
      if (this->cachingEnabled)
      {
        DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing caching ffmpeg "
                         "decoder from raw anexB stream. Same settings.");
        this->cachingDecoder = std::make_unique<decoder::decoderFFmpeg>(
            ffmpegCodec, frameSize, extradata, fmt, profileLevel, ratio, true);
      }
    }
    else
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing interactive "
                       "ffmpeg decoder using ffmpeg as parser");
      this->loadingDecoder = std::make_unique<decoder::decoderFFmpeg>(
          this->inputFileFFmpegLoading->getVideoCodecPar());
      if (this->cachingEnabled)
      {
        DEBUG_COMPRESSED("playlistItemCompressedVideo::allocateDecoder Initializing caching ffmpeg "
                         "decoder using ffmpeg as parser");
        this->cachingDecoder = std::make_unique<decoder::decoderFFmpeg>(
            this->inputFileFFmpegCaching->getVideoCodecPar());
      }
    }
  }
  else
  {
    this->infoText        = "No valid decoder was selected.";
    this->decodingEnabled = false;
    return false;
  }

  this->decodingEnabled = this->loadingDecoder->state() != decoder::DecoderState::Error;
  if (!decodingEnabled)
  {
    this->infoText = "There was an error allocating the new decoder: \n";
    this->infoText += loadingDecoder->decoderErrorString();
    this->infoText += "\n";
    return false;
  }

  return true;
}

void playlistItemCompressedVideo::fillStatisticList()
{
  if (!this->loadingDecoder || !this->loadingDecoder->statisticsSupported())
    return;

  this->loadingDecoder->fillStatisticList(this->statisticsData);
}

void playlistItemCompressedVideo::loadStatistics(int frameIdx)
{
  DEBUG_COMPRESSED("playlistItemCompressedVideo::loadStatisticToCache Request statistics for frame "
                   << frameIdx);

  if (!this->loadingDecoder->statisticsSupported())
    return;
  if (!this->loadingDecoder->statisticsEnabled())
  {
    // We have to enable collecting of statistics in the decoder. By default (for speed reasons)
    // this is off. Enabeling works like this: Enable collection, reset the decoder and decode the
    // current frame again. Statisitcs are always retrieved for the loading decoder.
    this->loadingDecoder->enableStatisticsRetrieval(&this->statisticsData);
    DEBUG_COMPRESSED("playlistItemCompressedVideo::loadStatistics Enable loading of stats frame "
                     << frameIdx);

    // Reload the current frame (force a seek and decode operation)
    int frameToLoad          = this->currentFrameIdx[0];
    this->currentFrameIdx[0] = -1;
    this->loadRawData(frameToLoad, false);

    // The statistics should now be loaded
  }
  else if (frameIdx != this->currentFrameIdx[0])
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

  newSet.append("YUV", this->video->getPixelValues(pixelPos, frameIdx));
  if (this->loadingDecoder->statisticsSupported() && this->loadingDecoder->statisticsEnabled())
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
  this->video->invalidateAllBuffers();

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadRawData(0, false);
}

void playlistItemCompressedVideo::cacheFrame(int frameIdx, bool testMode)
{
  if (!this->cachingEnabled)
    return;

  // Cache a certain frame. This is always called in a separate thread.
  this->cachingMutex.lock();
  this->video->cacheFrame(frameIdx, testMode);
  this->cachingMutex.unlock();
}

void playlistItemCompressedVideo::loadFrame(int  frameIdx,
                                            bool playing,
                                            bool loadRawdata,
                                            bool emitSignals)
{
  // The current thread must never be the main thread but one of the interactive threads.
  Q_ASSERT(QThread::currentThread() != QApplication::instance()->thread());

  auto stateYUV  = this->video->needsLoading(frameIdx, loadRawdata);
  auto stateStat = this->statisticsData.needsLoading(frameIdx);

  if (stateYUV == ItemLoadingState::LoadingNeeded || stateStat == ItemLoadingState::LoadingNeeded)
  {
    this->isFrameLoading = true;
    if (stateYUV == ItemLoadingState::LoadingNeeded)
    {
      // Load the requested current frame
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadFrame loading frame "
                       << frameIdx << (playing ? " (playing)" : ""));
      this->video->loadFrame(frameIdx);
    }
    if (stateStat == ItemLoadingState::LoadingNeeded)
    {
      DEBUG_COMPRESSED("playlistItemCompressedVideo::loadFrame loading statistics "
                       << frameIdx << (playing ? " (playing)" : ""));
      this->loadStatistics(frameIdx);
    }

    this->isFrameLoading = false;
    if (emitSignals)
      emit SignalItemChanged(true, RECACHE_NONE);
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
      this->isFrameLoadingDoubleBuffer = true;
      this->video->loadFrame(nextFrameIdx, true);
      this->isFrameLoadingDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

void playlistItemCompressedVideo::displaySignalComboBoxChanged(int idx)
{
  if (this->loadingDecoder && idx != this->loadingDecoder->getDecodeSignal())
  {
    bool resetDecoder = false;
    this->loadingDecoder->setDecodeSignal(idx, resetDecoder);
    this->cachingDecoder->setDecodeSignal(idx, resetDecoder);

    if (resetDecoder)
    {
      this->loadingDecoder->resetDecoder();
      this->cachingDecoder->resetDecoder();

      // Reset the decoded frame indices so that decoding of the current frame is triggered
      this->currentFrameIdx[0] = -1;
      this->currentFrameIdx[1] = -1;
    }

    // A different display signal was chosen. Invalidate the cache and signal that we will need a
    // redraw.
    auto yuvVideo = dynamic_cast<video::yuv::videoHandlerYUV *>(this->video.get());
    yuvVideo->showPixelValuesAsDiff = this->loadingDecoder->isSignalDifference(idx);
    yuvVideo->invalidateAllBuffers();

    emit SignalItemChanged(true, RECACHE_CLEAR);
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
    auto yuvVideo = dynamic_cast<video::yuv::videoHandlerYUV *>(this->video.get());
    if (this->loadingDecoder)
      yuvVideo->showPixelValuesAsDiff = this->loadingDecoder->isSignalDifference(idx);
    yuvVideo->invalidateAllBuffers();

    // Reset the decoded frame indices so that decoding of the current frame is triggered
    this->currentFrameIdx[0] = -1;
    this->currentFrameIdx[1] = -1;

    this->decodingNotPossibleAfter = -1;

    // Update the list of display signals
    if (this->loadingDecoder)
    {
      QSignalBlocker block(ui.comboBoxDisplaySignal);
      ui.comboBoxDisplaySignal->clear();
      ui.comboBoxDisplaySignal->addItems(this->loadingDecoder->getSignalNames());
      ui.comboBoxDisplaySignal->setCurrentIndex(this->loadingDecoder->getDecodeSignal());
    }

    // Update the statistics list with what the new decoder can provide
    this->statisticsUIHandler.clearStatTypes();
    this->statisticsData.setFrameSize(this->video->getFrameSize());
    this->fillStatisticList();
    this->statisticsUIHandler.updateStatisticsHandlerControls();

    emit SignalItemChanged(true, RECACHE_CLEAR);
  }
}
