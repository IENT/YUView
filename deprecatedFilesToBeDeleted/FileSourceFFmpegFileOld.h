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

#pragma once

#include "FileSource.h"

#include <LibFFmpeg/src/lib/AVCodec/wrappers/AVPacketWrapper.h>

/* This class can use the ffmpeg libraries (libavcodec) to read from any packetized file.
 */
class FileSourceFFmpegFile : public QObject
{
  Q_OBJECT

public:
  FileSourceFFmpegFile();
  ~FileSourceFFmpegFile();

  [[nodiscard]] bool loadLibrariesAndOpenFile(const QString &       filePath,
                                              QWidget *             mainWindow = nullptr,
                                              FileSourceFFmpegFile *other      = nullptr,
                                              bool                  parseFile  = true);

  [[nodiscard]] bool        atEnd() const;
  [[nodiscard]] QStringList getPathsOfLoadedLibraries() const;

  [[nodiscard]] double                     getFramerate() const;
  [[nodiscard]] Size                       getSequenceSizeSamples() const;
  [[nodiscard]] video::RawFormat           getRawFormat() const;
  [[nodiscard]] video::yuv::PixelFormatYUV getPixelFormatYUV() const;
  [[nodiscard]] video::rgb::PixelFormatRGB getPixelFormatRGB() const;
  [[nodiscard]] int64_t                    getMaxTS() const;

  libffmpeg::avcodec::AVPacketWrapper getNextPacket();

  [[nodiscard]] ByteVector    getExtradata();
  [[nodiscard]] StringPairVec getMetadata();
  [[nodiscard]] ByteVector    getLhvCData();

  void               updateFileWatchSetting();
  [[nodiscard]] bool isFileChanged() const;

  [[nodiscard]] bool seekToDTS(int64_t dts);
  [[nodiscard]] bool seekFileToBeginning();

  [[nodiscard]] indexRange               getDecodableFrameLimits() const;
  [[nodiscard]] FFmpeg::AVCodecIDWrapper getVideoStreamCodecID();
  FFmpeg::AVCodecParametersWrapper       getVideoCodecPar() { return video_stream.getCodecpar(); }

  // Get more general information about the streams
  unsigned int getNumberOfStreams() { return this->formatCtx ? this->formatCtx.getNbStreams() : 0; }
  int          getVideoStreamIndex() { return video_stream.getIndex(); }
  QList<QStringPairList>    getFileInfoForAllStreams();
  QList<FFmpeg::AVRational> getTimeBaseAllStreams();
  StringVec                 getShortStreamDescriptionAllStreams();

  // Look through the keyframes and find the closest one before (or equal)
  // the given frameIdx where we can start decoding
  // Return: POC and frame index
  std::pair<int64_t, size_t> getClosestSeekableFrameBefore(int frameIdx) const;

  QStringList getFFmpegLoadingLog() const { return ff.getLog(); }

private slots:
  void fileSystemWatcherFileChanged(const QString &) { fileChanged = true; }

protected:
  FFmpeg::FFmpegVersionHandler   ff; //< Access to the libraries independent of their version
  FFmpeg::AVFormatContextWrapper formatCtx;
  void                           openFileAndFindVideoStream(QString fileName);
  bool                           goToNextPacket(bool videoPacketsOnly = false);
  FFmpeg::AVPacketWrapper        currentPacket;    //< A place for the curren (frame) input buffer
  bool                           endOfFile{false}; //< Are we at the end of file (draining mode)?

  QString fileName;

  // Seek the stream to the given pts value, flush the decoder and load the first packet so
  // that we are ready to start decoding from this pts.
  int64_t                 duration{-1}; //< duration / AV_TIME_BASE is the duration in seconds
  FFmpeg::AVRational      timeBase{0, 0};
  FFmpeg::AVStreamWrapper video_stream;
  double                  frameRate{-1};
  Size                    frameSize;

  struct streamIndices_t
  {
    int        video{-1};
    QList<int> audio;

    struct subtitle_t
    {
      QList<int> dvb;
      QList<int> eia608;
      QList<int> other;
    };
    subtitle_t subtitle;
  };
  streamIndices_t streamIndices;

  video::RawFormat            rawFormat{video::RawFormat::Invalid};
  video::yuv::PixelFormatYUV  pixelFormat_yuv;
  video::rgb::PixelFormatRGB  pixelFormat_rgb;
  video::yuv::ColorConversion colorConversionType{video::yuv::ColorConversion::BT709_LimitedRange};

  // Watch the opened file for modifications
  QFileSystemWatcher fileWatcher;
  bool               fileChanged{false};

  QString   fullFilePath{};
  QFileInfo fileInfo;
  bool      isFileOpened{false};

  // In order to translate from frames to PTS, we need to count the frames and keep a list of
  // the PTS values of keyframes that we can start decoding at.
  // If a mainWindow pointer is given, open a progress dialog. Return true on success. False if the
  // process was canceled.
  bool   scanBitstream(QWidget *mainWindow);
  size_t nrFrames{0};

  // Private struct for navigation. We index frames by frame number and FFMpeg uses the pts.
  // This connects both values.
  struct pictureIdx
  {
    pictureIdx(size_t frame, int64_t dts) : frame(frame), dts(dts) {}
    size_t  frame;
    int64_t dts;
  };

  FFmpeg::PacketDataFormat packetDataFormat{FFmpeg::PacketDataFormat::Unknown};

  // These are filled after opening a file (after scanBitstream was called)
  QList<pictureIdx> keyFrameList; //< A list of pairs (frameNr, PTS) that we can seek to.
  // pictureIdx getClosestSeekableFrameNumberBeforeBefore(int frameIdx);

  // For parsing NAL units from the compressed data:
  QByteArray currentPacketData;
  int        posInFile{-1};
  bool       loadNextPacket;
  int        posInData;
  // We will keep the last buffer in case the reader wants to get it again
  QByteArray lastReturnArray;
};
