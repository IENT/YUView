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

#ifndef FILESOURCEFFMPEGFILE_H
#define FILESOURCEFFMPEGFILE_H

#include "fileSource.h"
#include "FFMpegLibrariesHandling.h"
#include "videoHandlerYUV.h"

/* This class can use the ffmpeg libraries (libavcodec) to read from any packetized file.
*/
class fileSourceFFmpegFile : public QObject
{
  Q_OBJECT

public:
  fileSourceFFmpegFile();
  fileSourceFFmpegFile(const QString &filePath) : fileSourceFFmpegFile() { openFile(filePath); }
  ~fileSourceFFmpegFile();

  // Load the ffmpeg libraries and try to open the file. The fileSource will install a watcher for the file.
  // Return false if anything goes wrong.
  bool openFile(const QString &filePath, QWidget *mainWindow=nullptr, fileSourceFFmpegFile *other=nullptr, bool parseFile=true);
  
  // Is the file at the end?
  // TODO: How do we do this?
  bool atEnd() const { return endOfFile; }

  // Return the name, filename and full path of every library loaded
  QStringList getLibraryPaths() const { return ff.getLibPaths(); }

  // Get properties of the bitstream
  double getFramerate() const { return frameRate; }
  QSize getSequenceSizeSamples() const { return frameSize; }
  RawFormat getRawFormat() const { return rawFormat; }
  YUV_Internals::yuvPixelFormat getPixelFormatYUV() const { return pixelFormat_yuv; }
  RGB_Internals::rgbPixelFormat getPixelFormatRGB() const { return pixelFormat_rgb; }

  /* Get data from the file source. You can either retrive full AVPackets or single units 
   * from the bitstream using these functions. The important thing is to not mix calls to these functions.
   */
  // Get the next NAL unit (everything excluding the start code) or the next packet.
  QByteArray getNextUnit(bool getLastDataAgain=false, uint64_t *pts=nullptr);
  // Return the next packet (unless getLastPackage is set in which case we return the current packet)
  AVPacketWrapper getNextPacket(bool getLastPackage=false, bool videoPacket=true);
  // Return the raw extradata/metadata (in avformat format containing the parameter sets)
  
  QByteArray getExtradata();
  QStringPairList getMetadata();
  // Return a list containing the raw data of all parameter set NAL units
  QList<QByteArray> getParameterSets();

  // File watching
  void updateFileWatchSetting();
  bool isFileChanged() { bool b = fileChanged; fileChanged = false; return b; }

  bool seekToDTS(int64_t dts);
  bool seekFileToBeginning();
  int64_t getMaxTS();

  // Get information on the video stream
  int getNumberFrames() const { return nrFrames; }
  AVCodecIDWrapper getVideoStreamCodecID() { return ff.getCodecIDWrapper(video_stream.getCodecID()); }
  AVCodecParametersWrapper getVideoCodecPar() { return video_stream.get_codecpar(); }

  // Get more general information about the streams
  unsigned int getNumberOfStreams() { return fmt_ctx ? fmt_ctx.get_nb_streams() : 0; }
  int getVideoStreamIndex() { return video_stream.get_index(); }
  QList<QStringPairList> getFileInfoForAllStreams();

  // Look through the keyframes and find the closest one before (or equal)
  // the given frameIdx where we can start decoding
  int getClosestSeekableDTSBefore(int frameIdx, int &seekToFrameIdx) const;

  QStringList getFFmpegLoadingLog() const { return ff.getLog(); }
  
private slots:
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

protected:

  FFmpegVersionHandler ff;          //< Access to the libraries independent of their version
  AVFormatContextWrapper fmt_ctx;
  void openFileAndFindVideoStream(QString fileName);
  bool goToNextPacket(bool videoPacketsOnly=false);
  AVPacketWrapper pkt;              //< A place for the curren (frame) input buffer
  bool endOfFile {false};           //< Are we at the end of file (draining mode)?
  // Seek the stream to the given pts value, flush the decoder and load the first packet so
  // that we are ready to start decoding from this pts.
  int64_t duration {-1};   //< duration / AV_TIME_BASE is the duration in seconds
  AVRational timeBase {0, 0};
  AVStreamWrapper video_stream;
  double frameRate {-1};
  QSize frameSize;
  
  RawFormat rawFormat {raw_Invalid};
  YUV_Internals::yuvPixelFormat pixelFormat_yuv;
  RGB_Internals::rgbPixelFormat pixelFormat_rgb;
  YUV_Internals::ColorConversion colorConversionType {YUV_Internals::ColorConversion::BT709_LimitedRange};

  // Watch the opened file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged {false};

  QString   fullFilePath;
  QFileInfo fileInfo;
  bool      isFileOpened {false};

  // In order to translate from frames to PTS, we need to count the frames and keep a list of
  // the PTS values of keyframes that we can start decoding at.
  // If a mainWindow pointer is given, open a progress dialog. Return true on success. False if the process was canceled.
  bool scanBitstream(QWidget *mainWindow);
  int nrFrames {0};

  // Private struct for navigation. We index frames by frame number and FFMpeg uses the pts.
  // This connects both values.
  struct pictureIdx
  {
    pictureIdx(int64_t frame, int64_t dts) : frame(frame), dts(dts) {}
    int64_t frame;
    int64_t dts;
  };

  packetDataFormat_t packetDataFormat {packetFormatUnknown};

  // The start code pattern to look for in case of a raw format
  QByteArray startCode;

  // These are filled after opening a file (after scanBitstream was called)
  QList<pictureIdx> keyFrameList;  //< A list of pairs (frameNr, PTS) that we can seek to.
  //pictureIdx getClosestSeekableFrameNumberBeforeBefore(int frameIdx);

  // For parsing NAL units from the compressed data:
  QByteArray currentPacketData;
  int posInFile {-1};
  bool loadNextPacket;
  int posInData;
  // We will keep the last buffer in case the reader wants to get it again
  QByteArray lastReturnArray;
};

#endif // FILESOURCEFFMPEGFILE_H
