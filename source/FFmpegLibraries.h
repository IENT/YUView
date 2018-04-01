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
#include "FFMpegLibrariesHandling.h"
#include <QLibrary>
#include <QFileSystemWatcher>

using namespace YUV_Internals;

// This class wraps the ffmpeg library in a demand-load fashion.
// As FFMpeg, the library can then be used to read from containers, or to decode videos.
class FFmpegLibraries : public QObject
{
  Q_OBJECT

public:
  FFmpegLibraries();
  ~FFmpegLibraries();

  /* ---- Reading from a container -----
   * Scenraio 1: Read data from a container.
   * First, open a file ...
   */

  bool openFile(QString fileName);
  void closeFile() { /* TODO */ assert(false); };

  // After succesfully opening a container, these should provide some information about the bitstream
  double getFrameRate() const { return frameRate; }
  ColorConversion getColorConversionType() const { return colorConversionType; }
  yuvPixelFormat getYUVPixelFormat();
  QSize getFrameSize() const { return frameSize; }





    

  // Get the error string (if openFile returend false)
  QString decoderErrorString() const;
  bool errorInDecoder() const { return decodingError; }
  bool errorLoadingLibraries() const { return librariesLoadingError; }
  bool errorOpeningFile() const { return decodingError; }

  // Get info about the decoder (path, library versions...)
  QList<infoItem> getDecoderInfo() const;

  // Load the raw YUV data for the given frame
  QByteArray loadYUVFrameData(int frameIdx);

  // Get the statistics values for the given frame (decode if necessary)
  statisticsData getStatisticsData(int frameIdx, int typeIdx);

  // Check if the given libraries can be used to open ffmpeg
  static bool checkLibraryFiles(QString avCodecLib, QString avFormatLib, QString avUtilLib, QString swResampleLib, QString &error);
  
private:

  // Try to load the ffmpeg libraries. Different paths wil be tried. 
  // If this fails, decoderError is set and errorString provides a error description.
  bool loadFFmpegLibraries();
  bool librariesLoaded;
  bool librariesLoadingError;

  // Opening files
  bool readingFileError;
  AVFormatContextWrapper fmt_ctx;
  AVStreamWrapper video_stream;

  // ---- Decoding 
  bool decodingError;








  
  // If the libraries were opened successfully, this function will attempt to get all the needed function pointers.
  // If this fails, decoderError will be set.
  void bindFunctionsFromLibraries();

  //// Scan the entire stream. Get the number of frames that we can decode and the key frames
  //// that we can seek to.
  //bool scanBitstream();

  // The decoderLibraries can be accessed through this class independent of the FFmpeg version.
  FFmpegVersionHandler ff;

  QString errorString;
  bool setOpeningError(const QString &reason) { readingFileError = true; errorString = reason; return false; }
  void setDecodingError(const QString &reason)  { decodingError = true; errorString = reason; }

  // The pixel format. This is valid after openFile was called (and succeeded).
  enum AVPixelFormat pixelFormat;
  QSize frameSize;
  double frameRate;
  ColorConversion colorConversionType;

  bool decodeOneFrame();

  // The input file context
  AVCodecWrapper videoCodec;        //< The video decoder codec
  AVCodecContextWrapper decCtx;     //< The decoder context
  AVFrameWrapper frame;             //< The frame that we use for decoding
  AVPacketWrapper pkt;              //< A place for the curren (frame) input buffer
  bool endOfFile;                   //< Are we at the end of file (draining mode)?
  
  // Private struct for navigation. We index frames by frame number and FFMpeg uses the pts.
  // This connects both values.
  struct pictureIdx
  {
    pictureIdx(qint64 frame, qint64 pts) : frame(frame), pts(pts) {}
    qint64 frame;
    qint64 pts;
  };

  //// These are filled after opening a file (after scanBitstream was called)
  //QList<pictureIdx> keyFrameList;  //< A list of pairs (frameNr, PTS) that we can seek to.
  //pictureIdx getClosestSeekableFrameNumberBefore(int frameIdx);

  // Seek the stream to the given pts value, flush the decoder and load the first packet so
  // that we are ready to start decoding from this pts.
  bool seekToPTS(qint64 pts);
  
  // The buffer and the index that was requested in the last call to getOneFrame
  int currentOutputBufferFrameIndex;
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(const de265_image *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyFrameToOutputBuffer(); // Copy the raw data from the frame to the currentOutputBuffer
#endif

  // Caching
  QHash<int, statisticsData> curFrameStats;  // cache of the statistics for the current POC [statsTypeID]
  int statsCacheCurFrameIdx;                 // the POC of the statistics that are in the curPOCStats
  // Copy the motion information (if present) from the frame to a loca buffer
  void copyFrameMotionInformation();

  // Get information about the current format
  void getFormatInfo();
};

#endif // FFMPEGDECODER_H
