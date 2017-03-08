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

#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "fileInfoWidget.h"
#include "videoHandlerYUV.h"
#include <QLibrary>
#include <QFileSystemWatcher>

using namespace YUV_Internals;

struct FFmpegFunctions
{
  FFmpegFunctions();

  // From avformat
  void  (*av_register_all)           ();
  int   (*avformat_open_input)       (AVFormatContext **ps, const char *url, AVInputFormat *fmt, AVDictionary **options);
  void  (*avformat_close_input)      (AVFormatContext **s);
  int   (*avformat_find_stream_info) (AVFormatContext *ic, AVDictionary **options);
  int   (*av_read_frame)             (AVFormatContext *s, AVPacket *pkt);
  int   (*av_seek_frame)             (AVFormatContext *s, int stream_index, int64_t timestamp, int flags);
  
  // From avcodec
  AVCodec        *(*avcodec_find_decoder)  (enum AVCodecID id);
  AVCodecContext *(*avcodec_alloc_context3) (const AVCodec *codec);
  int             (*avcodec_open2)         (AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
  void            (*avcodec_free_context)  (AVCodecContext **avctx);
  void            (*av_init_packet)        (AVPacket *pkt);
  void            (*av_packet_unref)       (AVPacket *pkt);
  void            (*avcodec_flush_buffers) (AVCodecContext *avctx);
  AVPacket       *(*av_packet_alloc)       (void);
  void            (*av_packet_free)        (AVPacket **pkt);

  // The following functions are part of the new API.
  // The following function is quite new. We will check if it is available.
  // If not, we will use the old decoding API.
  int             (*avcodec_send_packet)   (AVCodecContext *avctx, const AVPacket *avpkt);
  int             (*avcodec_receive_frame) (AVCodecContext *avctx, AVFrame *frame);
  int             (*avcodec_parameters_to_context) (AVCodecContext *codec, const AVCodecParameters *par);
  bool newParametersAPIAvailable;
  // This function is deprecated. So we only use it if the new API is not available.
  int             (*avcodec_decode_video2) (AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, const AVPacket *avpkt);

  // From avutil
  AVFrame  *(*av_frame_alloc)  (void);
  void      (*av_frame_free)   (AVFrame **frame);
  void     *(*av_mallocz)      (size_t size);
};

// This class wraps the ffmpeg library in a demand-load fashion.
class FFmpegDecoder : public QObject, public FFmpegFunctions
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
  QString decoderErrorString() const { return errorString; }
  bool errorInDecoder() const { return decodingError != ffmpeg_noError; }
  bool errorLoadingLibraries() const { return decodingError == ffmpeg_errorLoadingLibrary; }
  bool errorOpeningFile() const { return decodingError == ffmpeg_errorOpeningFile; }

  // Load the raw YUV data for the given frame
  QByteArray loadYUVFrameData(int frameIdx);

  // Was the file changed by some other application?
  bool isFileChanged() { bool b = fileChanged; fileChanged = false; return b; }
  // Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
  void updateFileWatchSetting();
  // Reload the input file
  bool reloadItemSource();

  // Check the given path for loadable ffmpeg libraries.
  static bool checkForLibraries(QString path);

private slots:
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

private:

  // Try to load the ffmpeg libraries. Different paths wil be tried. 
  // If this fails, decoderError is set and errorString provides a error description.
  void loadFFmpegLibraries();
  // Try to load the ffmpeg libraries from the given path. If path is empty, the system dirs will be tried.
  // loadFFMpegLibraries uses this function. In case of error, decoderError is true.
  void loadFFmpegLibraryInPath(QString path);
  // If the libraries were opened successfully, this function will attempt to get all the needed function pointers.
  // If this fails, decoderError will be set.
  void bindFunctionsFromLibraries();

  QFunctionPointer resolveAvUtil(const char *symbol);
  template <typename T> bool resolveAvUtil(T &ptr, const char *symbol);
  QFunctionPointer resolveAvFormat(const char *symbol);
  template <typename T> bool resolveAvFormat(T &ptr, const char *symbol);
  QFunctionPointer resolveAvCodec(const char *symbol, bool failIsError);
  template <typename T> bool resolveAvCodec(T &ptr, const char *symbol, bool failIsError=true);

  // Scan the entire stream. Get the number of frames that we can decode and the key frames
  // that we can seek to.
  bool scanBitstream();

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
  void setLibraryError(const QString &reason) { decodingError = ffmpeg_errorLoadingLibrary; errorString = reason; }
  void setDecodingError(const QString &reason)  { decodingError = ffmpeg_errorDecoding; errorString = reason; }

  // The pixel format. This is valid after openFile was called (and succeeded).
  AVPixelFormat pixelFormat;
  QSize frameSize;
  double frameRate;
  ColorConversion colorConversionType;

  bool decodeOneFrame();

  // The input file context
  AVFormatContext *fmt_ctx;
  int videoStreamIdx;         //< The stream index of the video stream that we will decode
  AVCodec *videoCodec;        //< The video decoder codec
  AVCodecContext *decCtx;     //< The decoder context
  AVFrame *frame;             //< The frame that we use for decoding
  AVPacket *pkt;              //< A place for the curren (frame) input buffer
  bool endOfFile;             //< Are we at the end of file (draining mode)?

  //// Copy the data from frame to currentDecFrameRaw
  //void copyFrameToBuffer();

  // The information on the file which was opened with openFile
  QString   fullFilePath;
  QFileInfo fileInfo;

  QLibrary libAvutil;
  QLibrary libSwresample;
  QLibrary libAvcodec;
  QLibrary libAvformat;

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

  // Get information about the current format
  void getFormatInfo();
};

#endif // FFMPEGDECODER_H
