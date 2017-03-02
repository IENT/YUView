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

using namespace YUV_Internals;

struct FFMpegFunctions
{
  FFMpegFunctions();

  // From avformat
  void  (*av_register_all)      ();
  int   (*avformat_open_input)  (AVFormatContext **ps, const char *url, AVInputFormat *fmt, AVDictionary **options);
  void  (*avformat_close_input) (AVFormatContext **s);
  int   (*av_find_best_stream)  (AVFormatContext *ic, enum AVMediaType type, int wanted_stream_nb, int related_stream, AVCodec **decoder_ret, int flags);
  int   (*av_read_frame)        (AVFormatContext *s, AVPacket *pkt);
  
  // From avcodec
  AVCodec        *(*avcodec_find_decoder)   (enum AVCodecID id);
  int      (*avcodec_open2)         (AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
  void     (*avcodec_free_context)  (AVCodecContext **avctx);
  void     (*av_init_packet)        (AVPacket *pkt);
  void     (*av_packet_unref)       (AVPacket *pkt);
  int      (*avcodec_send_packet)   (AVCodecContext *avctx, const AVPacket *avpkt);
  int      (*avcodec_receive_frame) (AVCodecContext *avctx, AVFrame *frame);

  // From avutil
  AVFrame  *(*av_frame_alloc)  (void);
  void      (*av_frame_free)   (AVFrame **frame);
};

// This class wraps the ffmpeg library in a demand-load fashion.
class FFMpegDecoder : public FFMpegFunctions
{
public:
  FFMpegDecoder();
  ~FFMpegDecoder();

  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  bool openFile(QString fileName);

  // Get the pixel format and frame size. This is valid after openFile was called.
  yuvPixelFormat getYUVPixelFormat();
  QSize getFrameSize() { return frameSize; }

  // Get some infos on the file (like date changed, file size, etc...)
  QList<infoItem> getFileInfoList() const;

  // How many frames are in the file?
  int getNumberPOCs() const { return nrFrames; }

  // Get the error string (if openFile returend false)
  QString decoderErrorString() const { return errorString; }
  bool errorInDecoder() const { return decoderError; }

  // Load the raw YUV data for the given frame
  QByteArray loadYUVFrameData(int frameIdx);

private:
  void loadFFMPegLibrary();
  void setError(const QString &reason);

  QFunctionPointer resolveAvUtil(const char *symbol);
  template <typename T> T resolveAvUtil(T &ptr, const char *symbol);
  QFunctionPointer resolveAvFormat(const char *symbol);
  template <typename T> T resolveAvFormat(T &ptr, const char *symbol);
  QFunctionPointer resolveAvCodec(const char *symbol);
  template <typename T> T resolveAvCodec(T &ptr, const char *symbol);

  

  bool decoderError;
  QString errorString;

  // The pixel format. This is valid after openFile was called.
  AVPixelFormat pixelFormat;
  QSize frameSize;

  void decodeOneFrame();

  // The input file context
  AVFormatContext *fmt_ctx;
  int videoStreamIdx;         //< The stream index of the video stream that we will decode
  AVCodecContext *decCtx;     //< The decoder context
  AVFrame *frame;             //< The frame that we use for decoding
  AVPacket pkt;               //< A place for the curren (frame) input buffer

  QByteArray currentDecFrameRaw;
  // Copy the data from frame to currentDecFrameRaw
  void copyFrameToBuffer();

  // How many frames are in the sequence?
  int nrFrames;

  // The information on the file which was opened with openFile
  QFileInfo fileInfo;

  // We only need to open the ffmpeg libraries once so they are static here.
  // If needed, we will load ffmpeg and register it.
  static bool ffmpegLoaded;

  static QLibrary libAvutil;
  static QLibrary libSwresample;
  static QLibrary libAvcodec;
  static QLibrary libAvformat;
};

#endif // FFMPEGDECODER_H
