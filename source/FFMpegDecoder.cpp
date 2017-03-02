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

#include "FFMpegDecoder.h"

#include <cstring>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include "typedef.h"

#define FFMPEGDECODER_DEBUG_OUTPUT 1
#if FFMPEGDECODER_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

FFMpegFunctions::FFMpegFunctions() { memset(this, 0, sizeof(*this)); }

// Initialize static members
bool FFMpegDecoder::ffmpegLoaded = false;
QLibrary FFMpegDecoder::libAvutil;
QLibrary FFMpegDecoder::libSwresample;
QLibrary FFMpegDecoder::libAvcodec;
QLibrary FFMpegDecoder::libAvformat;

FFMpegDecoder::FFMpegDecoder() : decoderError(false)
{
  // Set default values
  pixelFormat = AV_PIX_FMT_NONE;
  fmt_ctx = nullptr;
  videoStreamIdx = -1;
  decCtx = nullptr;
  frame = nullptr;
  nrFrames = -1;

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  loadFFMPegLibrary();
}

FFMpegDecoder::~FFMpegDecoder()
{
  // Free all the allocated data structures
  if (decCtx)
    avcodec_free_context(&decCtx);
  if (fmt_ctx)
    avformat_close_input(&fmt_ctx);
  if (frame)
    av_frame_free(&frame);
}

bool FFMpegDecoder::openFile(QString fileName) 
{ 
  fileInfo = QFileInfo(fileName);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return false;

  if (!decoderError)
  {
    // Initialize libavformat and register all the muxers, demuxers and protocols. 
    av_register_all();

    // Open the input file
    int ret = avformat_open_input(&fmt_ctx, fileName.toStdString().c_str(), nullptr, nullptr);
    if (ret < 0)
    {
      setError(QStringLiteral("Could not open the input file (avformat_open_input). Return code %1.").arg(ret));
      return false;
    }

    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0)
    {
      setError(QStringLiteral("Could not find a video stream (av_find_best_stream). Return code %1.").arg(ret));
      return false;
    }

    // Get the stream ...
    videoStreamIdx = ret;
    AVStream *st = fmt_ctx->streams[videoStreamIdx];
    // ... and try to open a decoder for the stream.
    decCtx = st->codec;
    AVCodec *dec = avcodec_find_decoder(decCtx->codec_id);

    if (!dec)
    {
      setError(QStringLiteral("Could not find a video decoder (avcodec_find_decoder)"));
      return false;
    }

    ret = avcodec_open2(decCtx, dec, nullptr);
    if (ret < 0)
    {
      setError(QStringLiteral("Could not open the video codec (avcodec_open2). Return code %1.").arg(ret));
      return false;
    }

    // Allocate the frame
    frame = av_frame_alloc();
    if (!frame)
    {
      setError(QStringLiteral("Could not allocate frame (av_frame_alloc)."));
      return false;
    }

    // Initialize an empty packet
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;

    // Get the first video stream packet into the packet buffer.
    do
    {
      ret = av_read_frame(fmt_ctx, &pkt);
    } while (pkt.stream_index != videoStreamIdx);

    // Opening the deocder was successfull. We can now start to decode frames. Decode the first frame.
    decodeOneFrame();
    // DEBUG - Seek 50 frames
    for (int i=0; i<50; i++)
      decodeOneFrame();
    
    // Calculate the number of frames in the sequence from the duration and the average framerate.
    AVRational fps = st->avg_frame_rate;
    qint64 duration = fmt_ctx->duration;
    nrFrames = int(duration * fps.num / fps.den / AV_TIME_BASE) + 1;
    
    // We have decoded one frame. Get the pixel format.
    pixelFormat = decCtx->pix_fmt;
  }

  return true;
}

void FFMpegDecoder::decodeOneFrame()
{
  /*Instead of valid input, send NULL to the avcodec_send_packet() (decoding) or avcodec_send_frame() (encoding) functions. This will enter draining mode.
    Call avcodec_receive_frame() (decoding) or avcodec_receive_packet() (encoding) in a loop until AVERROR_EOF is returned. The functions will not return AVERROR(EAGAIN), unless you forgot to enter draining mode.
    Before decoding can be resumed again, the codec has to be reset with avcodec_flush_buffers().*/
  
  bool gotFrame = false;
  // Fill the decoder buffer until it returns AVERROR(EAGAIN)
  int retPush;
  do
  {
    // Push the video packet to the decoder
    retPush = avcodec_send_packet(decCtx, &pkt);
    if (retPush < 0 && retPush != AVERROR(EAGAIN))
    {
      setError(QStringLiteral("Error sending packet (avcodec_send_packet)"));
      return;
    }
    if (retPush != AVERROR(EAGAIN))
      DEBUG_FFMPEG("Send packet PTS %d duration %d flags %d", pkt.pts, pkt.duration, pkt.flags);

    if (retPush == 0)
    {
      // Pushing was successfull, read the next video packet ...
      do
      {
        // Unref the old packet
        av_packet_unref(&pkt);
        // Get the next one
        int ret = av_read_frame(fmt_ctx, &pkt);
        if (ret < 0)
        {
          setError(QStringLiteral("Error reading packet (av_read_frame)"));
          return;
        }
      } while (pkt.stream_index != videoStreamIdx);
    }
  } while (retPush == 0);

  // Now get as many frames as possible from the decoder
  int retRecieve;
  do
  {
    retRecieve = avcodec_receive_frame(decCtx, frame);
    if (retRecieve >= 0)
    {
      // Recieved a frame
      DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %d type %d %s", frame->width, frame->height, frame->pts, frame->pict_type, frame->key_frame ? "key frame" : "");
      gotFrame = true;

      // Get the frame size
      frameSize.setWidth(frame->width);
      frameSize.setHeight(frame->height);

      copyFrameToBuffer();
    }
  } while (retRecieve == 0);
}

yuvPixelFormat FFMpegDecoder::getYUVPixelFormat()
{
  // YUV 4:2:0 formats
  if (pixelFormat == AV_PIX_FMT_YUV420P)
    return yuvPixelFormat(YUV_420, 8);
  if (pixelFormat == AV_PIX_FMT_YUV420P16LE)
    return yuvPixelFormat(YUV_420, 16);
  if (pixelFormat == AV_PIX_FMT_YUV420P16BE)
    return yuvPixelFormat(YUV_420, 16, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV420P9BE)
    return yuvPixelFormat(YUV_420, 9, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV420P9LE)
    return yuvPixelFormat(YUV_420, 9);
  if (pixelFormat == AV_PIX_FMT_YUV420P10BE)
    return yuvPixelFormat(YUV_420, 10, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV420P10LE)
    return yuvPixelFormat(YUV_420, 10);
  if (pixelFormat == AV_PIX_FMT_YUV420P12BE)
    return yuvPixelFormat(YUV_420, 12, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV420P12LE)
    return yuvPixelFormat(YUV_420, 12);
  if (pixelFormat == AV_PIX_FMT_YUV420P14BE)
    return yuvPixelFormat(YUV_420, 14, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV420P14LE)
    return yuvPixelFormat(YUV_420, 14);

  // YUV 4:2:2 formats
  if (pixelFormat == AV_PIX_FMT_YUV422P)
    return yuvPixelFormat(YUV_422, 8);
  if (pixelFormat == AV_PIX_FMT_YUV422P16LE)
    return yuvPixelFormat(YUV_422, 16);
  if (pixelFormat == AV_PIX_FMT_YUV422P16BE)
    return yuvPixelFormat(YUV_422, 16, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV422P10BE)
    return yuvPixelFormat(YUV_422, 10, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV422P10LE)
    return yuvPixelFormat(YUV_422, 10);
  if (pixelFormat == AV_PIX_FMT_YUV422P9BE)
    return yuvPixelFormat(YUV_422, 9, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV422P9LE)
    return yuvPixelFormat(YUV_422, 9);
  if (pixelFormat == AV_PIX_FMT_YUV422P12BE)
    return yuvPixelFormat(YUV_422, 12, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV422P12LE)
    return yuvPixelFormat(YUV_422, 12);
  if (pixelFormat == AV_PIX_FMT_YUV422P14BE)
    return yuvPixelFormat(YUV_422, 14, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV422P14LE)
    return yuvPixelFormat(YUV_422, 14);

  // YUV 4:4:4 formats
  if (pixelFormat == AV_PIX_FMT_YUV444P)
    return yuvPixelFormat(YUV_444, 8);
  if (pixelFormat == AV_PIX_FMT_YUV444P16LE)
    return yuvPixelFormat(YUV_444, 16);
  if (pixelFormat == AV_PIX_FMT_YUV444P16BE)
    return yuvPixelFormat(YUV_444, 16, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV444P9BE)
    return yuvPixelFormat(YUV_444, 9, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV444P9LE)
    return yuvPixelFormat(YUV_444, 9);
  if (pixelFormat == AV_PIX_FMT_YUV444P10BE)
    return yuvPixelFormat(YUV_444, 10, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV444P10LE)
    return yuvPixelFormat(YUV_444, 10);
  if (pixelFormat == AV_PIX_FMT_YUV444P12BE)
    return yuvPixelFormat(YUV_444, 12, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV444P12LE)
    return yuvPixelFormat(YUV_444, 12);
  if (pixelFormat == AV_PIX_FMT_YUV444P14BE)
    return yuvPixelFormat(YUV_444, 14, Order_YUV, true);
  if (pixelFormat == AV_PIX_FMT_YUV444P14LE)
    return yuvPixelFormat(YUV_444, 14);

  return yuvPixelFormat();
}

void FFMpegDecoder::loadFFMPegLibrary()
{
  // Try to load the ffmpeg libraries from the current working directory.
  // Unfortunately relative paths like this do not work: (at least on windows)
  // We will load the following libraries (in this order): 
  // avutil, swresample, avcodec, avformat.

  // Find a path that contains all the libraries
  // TODO: On windows, the ffmpeg libraries carry their version in the name, 
  //       how is this done on linux if the libraries are installed by other means?
  // We will try the following paths
  QStringList const libPaths = QStringList()
    << QDir::currentPath() + "/"
    << QDir::currentPath() + "/ffmpeg/"
    << QCoreApplication::applicationDirPath() + "/"
    << QCoreApplication::applicationDirPath() + "/ffmpeg/";

  QString foundLibPath[4];
  QString ext = is_Q_OS_WIN ? "dll" : (is_Q_OS_LINUX ? "so" : ".dylib");
  bool ffmpegFound = false;
  for (auto &libPath : libPaths)
  {
    QDir dir(libPath);
    QFileInfoList files = dir.entryInfoList(QDir::Files);

    foundLibPath[0].clear();
    foundLibPath[1].clear();
    foundLibPath[2].clear();
    foundLibPath[3].clear();
    for (QFileInfo file : files)
    {
      if (file.suffix() == ext && file.baseName().startsWith("avutil"))
        foundLibPath[0] = file.absoluteFilePath();
      if (file.suffix() == ext && file.baseName().startsWith("swresample"))
        foundLibPath[1] = file.absoluteFilePath();
      if (file.suffix() == ext && file.baseName().startsWith("avcodec"))
        foundLibPath[2] = file.absoluteFilePath();
      if (file.suffix() == ext && file.baseName().startsWith("avformat"))
        foundLibPath[3] = file.absoluteFilePath();
    }

    ffmpegFound = (!foundLibPath[0].isEmpty() && !foundLibPath[1].isEmpty() && 
                   !foundLibPath[2].isEmpty() && !foundLibPath[3].isEmpty());
    if (ffmpegFound)
      break;
  }

  if (ffmpegFound)
  {
    // We found the ffmpeg libraries. Load them.
    libAvutil.setFileName(foundLibPath[0]);
    if (!libAvutil.load())
      return setError(libAvutil.errorString());
    libSwresample.setFileName(foundLibPath[1]);
    if (!libSwresample.load())
      return setError(libSwresample.errorString());
    libAvcodec.setFileName(foundLibPath[2]);
    if (!libAvcodec.load())
      return setError(libAvcodec.errorString());
    libAvformat.setFileName(foundLibPath[3]);
    if (!libAvformat.load())
      return setError(libAvformat.errorString());
  }

  // Loading the libraries was successfull. Get/check function pointers.
  // From avformat
  if (!resolveAvFormat(av_register_all, "av_register_all")) return;
  if (!resolveAvFormat(avformat_open_input, "avformat_open_input")) return;
  if (!resolveAvFormat(avformat_close_input, "avformat_close_input")) return;
  if (!resolveAvFormat(av_find_best_stream, "av_find_best_stream")) return;
  if (!resolveAvFormat(av_read_frame, "av_read_frame")) return;

  // From avcodec
  if (!resolveAvCodec(avcodec_find_decoder, "avcodec_find_decoder")) return;
  if (!resolveAvCodec(avcodec_open2, "avcodec_open2")) return;
  if (!resolveAvCodec(avcodec_free_context, "avcodec_free_context")) return;
  if (!resolveAvCodec(av_init_packet, "av_init_packet")) return;
  if (!resolveAvCodec(av_packet_unref, "av_packet_unref")) return;
  if (!resolveAvCodec(avcodec_send_packet, "avcodec_send_packet")) return;
  if (!resolveAvCodec(avcodec_receive_frame, "avcodec_receive_frame")) return;

  // From avutil
  if (!resolveAvUtil(av_frame_alloc, "av_frame_alloc")) return;
  if (!resolveAvUtil(av_frame_free, "av_frame_free")) return;
}

void FFMpegDecoder::setError(const QString &reason)
{
  decoderError = true;
  errorString = reason;
}

QFunctionPointer FFMpegDecoder::resolveAvUtil(const char *symbol)
{
  QFunctionPointer ptr = libAvutil.resolve(symbol);
  if (!ptr) 
    setError(QStringLiteral("Error loading the avutil library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T FFMpegDecoder::resolveAvUtil(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolveAvUtil(symbol));
}

QFunctionPointer FFMpegDecoder::resolveAvFormat(const char *symbol)
{
  QFunctionPointer ptr = libAvformat.resolve(symbol);
  if (!ptr) 
    setError(QStringLiteral("Error loading the avformat library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T FFMpegDecoder::resolveAvFormat(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolveAvFormat(symbol));
}

QFunctionPointer FFMpegDecoder::resolveAvCodec(const char *symbol)
{
  QFunctionPointer ptr = libAvcodec.resolve(symbol);
  if (!ptr) 
    setError(QStringLiteral("Error loading the avcodec library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T FFMpegDecoder::resolveAvCodec(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolveAvCodec(symbol));
}

QList<infoItem> FFMpegDecoder::getFileInfoList() const
{
  QList<infoItem> infoList;

  if (fileInfo.exists() && fileInfo.isFile())
  {
    // The full file path
    infoList.append(infoItem("File Path", fileInfo.absoluteFilePath()));

    // The file creation time
    QString createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
    infoList.append(infoItem("Time Created", createdtime));

    // The last modification time
    QString modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    infoList.append(infoItem("Time Modified", modifiedtime));

    // The file size in bytes
    QString fileSize = QString("%1").arg(fileInfo.size());
    infoList.append(infoItem("Nr Bytes", fileSize));
  }

  return infoList;
}

QByteArray FFMpegDecoder::loadYUVFrameData(int frameIdx)
{
  // TODO
  // For now, just return the one image that we decoded
  return currentDecFrameRaw;
}

void FFMpegDecoder::copyFrameToBuffer()
{
  // At first get how many bytes we are going to write  
  yuvPixelFormat pixFmt = getYUVPixelFormat();
  int nrBytesPerSample = pixFmt.bitsPerSample <= 8 ? 1 : 2;
  int nrBytes = frameSize.width() * frameSize.height() * nrBytesPerSample;
  nrBytes += 2 * frameSize.width() / pixFmt.getSubsamplingHor() * frameSize.height() / pixFmt.getSubsamplingVer() * nrBytesPerSample;

  // Is the output big enough?
  if (currentDecFrameRaw.capacity() < nrBytes)
    currentDecFrameRaw.resize(nrBytes);

  char* dst_c = currentDecFrameRaw.data();
  int lengthY = frame->linesize[0] * frame->height;
  int lengthC = frame->linesize[1] * frame->height / 2;
  memcpy(dst_c                    , frame->data[0], lengthY);
  memcpy(dst_c + lengthY          , frame->data[1], frame->linesize[1] * frame->height / 2);
  memcpy(dst_c + lengthY + lengthC, frame->data[2], frame->linesize[2] * frame->height / 2);
}