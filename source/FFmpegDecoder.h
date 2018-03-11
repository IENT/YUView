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

#ifndef FFMPEGDECODER_H
#define FFMPEGDECODER_H

#include "fileInfoWidget.h"
#include "statisticsExtensions.h"
#include "videoHandlerYUV.h"
#include "FFMpegDecoderLibHandling.h"
#include "fileSourceAVCAnnexBFile.h"
#include <QLibrary>
#include <QFileSystemWatcher>

using namespace YUV_Internals;

// This class wraps the ffmpeg library in a demand-load fashion.
class FFmpegDecoder : public QObject
{
  Q_OBJECT

public:
  FFmpegDecoder();
  ~FFmpegDecoder();

  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  // If a second decoder is provided, the bistream will not be scanned again (scanBitstream), but
  // the values will be copied from the given decoder.
  bool openFile(QString fileName, FFmpegDecoder *otherDec=nullptr);

  // Get the pixel format and frame size. This is valid after openFile was called.
  yuvPixelFormat getYUVPixelFormat();
  QSize getFrameSize() const { return frameSize; }

  // Get some infos on the file (like date changed, file size, etc...)
  QList<infoItem> getFileInfoList() const;

  // How many frames are in the file?
  int getNumberPOCs() const { return nrFrames; }
  double getFrameRate() const { return frameRate; }
  ColorConversion getColorConversionType() const { return colorConversionType; }

  // Get the error string (if openFile returend false)
  QString decoderErrorString() const;
  bool errorInDecoder() const { return decodingError != ffmpeg_noError; }
  bool errorLoadingLibraries() const { return decodingError == ffmpeg_errorLoadingLibrary; }
  bool errorOpeningFile() const { return decodingError == ffmpeg_errorOpeningFile; }

  // Get info about the decoder (path, library versions...)
  QList<infoItem> getDecoderInfo() const;

  // Load the raw YUV data for the given frame
  QByteArray loadYUVFrameData(int frameIdx);

  // Was the file changed by some other application?
  bool isFileChanged() { bool b = fileChanged; fileChanged = false; return b; }
  // Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
  void updateFileWatchSetting();
  // Reload the input file
  bool reloadItemSource();

  // Get the statistics values for the given frame (decode if necessary)
  statisticsData getStatisticsData(int frameIdx, int typeIdx);

  // Check if the given libraries can be used to open ffmpeg
  static bool checkLibraryFiles(QString avCodecLib, QString avFormatLib, QString avUtilLib, QString swResampleLib, QString &error);

  // Annex B files
  bool canShowNALInfo() const { return canShowNALUnits; }

private slots:
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

private:

  // Try to load the ffmpeg libraries. Different paths wil be tried. 
  // If this fails, decoderError is set and errorString provides a error description.
  void loadFFmpegLibraries();
  
  // If the libraries were opened successfully, this function will attempt to get all the needed function pointers.
  // If this fails, decoderError will be set.
  void bindFunctionsFromLibraries();

  // Scan the entire stream. Get the number of frames that we can decode and the key frames
  // that we can seek to.
  bool scanBitstream();

  // The decoderLibraries can be accessed through this class independent of the FFmpeg version.
  FFmpegVersionHandler ff;

  // Error handling
  enum decodingErrorEnum
  {
    ffmpeg_noError,
    ffmpeg_errorOpeningFile,      //< The library is ok, but there was an error loading the file
    ffmpeg_errorLoadingLibrary,   //< The library could not be loaded
    ffmpeg_errorDecoding          //< There was an error while decoding
  };
  decodingErrorEnum decodingError;
  QString errorString;
  bool setOpeningError(const QString &reason) { decodingError = ffmpeg_errorOpeningFile; errorString = reason; return false; }
  void setDecodingError(const QString &reason)  { decodingError = ffmpeg_errorDecoding; errorString = reason; }

  // The pixel format. This is valid after openFile was called (and succeeded).
  enum AVPixelFormat pixelFormat;
  QSize frameSize;
  double frameRate;
  ColorConversion colorConversionType;

  bool decodeOneFrame();

  // The input file context
  AVFormatContextWrapper fmt_ctx;
  AVStreamWrapper video_stream;
  AVCodecWrapper videoCodec;        //< The video decoder codec
  AVCodecContextWrapper decCtx;     //< The decoder context
  AVFrameWrapper frame;             //< The frame that we use for decoding
  AVPacketWrapper pkt;              //< A place for the curren (frame) input buffer
  bool endOfFile;                   //< Are we at the end of file (draining mode)?

  // The information on the file which was opened with openFile
  QString   fullFilePath;
  QFileInfo fileInfo;

  // Private struct for navigation. We index frames by frame number and FFMpeg uses the pts.
  // This connects both values.
  struct pictureIdx
  {
    pictureIdx(qint64 frame, qint64 pts) : frame(frame), pts(pts) {}
    qint64 frame;
    qint64 pts;
  };

  // These are filled after opening a file (after scanBitstream was called)
  int nrFrames;                               //< How many frames are in the sequence?
  QList<pictureIdx> keyFrameList;  //< A list of pairs (frameNr, PTS) that we can seek to.
  pictureIdx getClosestSeekableFrameNumberBefore(int frameIdx);

  // Seek the stream to the given pts value, flush the decoder and load the first packet so
  // that we are ready to start decoding from this pts.
  bool seekToPTS(qint64 pts);

  // Watch the opened file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  // The buffer and the index that was requested in the last call to getOneFrame
  int currentOutputBufferFrameIndex;
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(const de265_image *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyFrameToOutputBuffer(); // Copy the raw data from the frame to the currentOutputBuffer
#endif

  fileSourceAVCAnnexBFile annexBFile;
  bool canShowNALUnits;

  // Caching
  QHash<int, statisticsData> curFrameStats;  // cache of the statistics for the current POC [statsTypeID]
  int statsCacheCurFrameIdx;                 // the POC of the statistics that are in the curPOCStats
  // Copy the motion information (if present) from the frame to a loca buffer
  void copyFrameMotionInformation();

  // Get information about the current format
  void getFormatInfo();
};

#endif // FFMPEGDECODER_H
