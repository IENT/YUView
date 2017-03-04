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

#include "FFmpegDecoder.h"

#include <cstring>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProgressDialog>
#include <QSettings>
#include "mainwindow.h"
#include "typedef.h"

#include "libswresample/version.h"

#define FFmpegDecoder_DEBUG_OUTPUT 0
#if FFmpegDecoder_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

FFmpegFunctions::FFmpegFunctions() { memset(this, 0, sizeof(*this)); }

FFmpegDecoder::FFmpegDecoder()
{
  // No error (yet)
  decodingError = ffmpeg_noError;

  // Set default values
  pixelFormat = AV_PIX_FMT_NONE;
  fmt_ctx = nullptr;
  videoStreamIdx = -1;
  videoCodec = nullptr;
  decCtx = nullptr;
  frame = nullptr;
  nrFrames = -1;
  endOfFile = false;
  pktInitialized = false;
  frameRate = -1;
  colorConversionType = BT709;

  // Initialize the file watcher and install it (if enabled)
  fileChanged = false;
  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &FFmpegDecoder::fileSystemWatcherFileChanged);
  updateFileWatchSetting();

  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;
}

FFmpegDecoder::~FFmpegDecoder()
{
  // Free all the allocated data structures
  if (pktInitialized)
    av_packet_unref(&pkt);
  if (decCtx)
    avcodec_free_context(&decCtx);
  if (frame)
    av_frame_free(&frame);
  if (fmt_ctx)
    avformat_close_input(&fmt_ctx);
}

bool FFmpegDecoder::openFile(QString fileName, FFmpegDecoder *otherDec)
{ 
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  loadFFmpegLibraries();

  fullFilePath = fileName;
  fileInfo = QFileInfo(fileName);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return false;

  if (decodingError != ffmpeg_noError)
    return false;

  // Initialize libavformat and register all the muxers, demuxers and protocols. 
  av_register_all();

  // Open the input file
  int ret = avformat_open_input(&fmt_ctx, fileName.toStdString().c_str(), nullptr, nullptr);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not open the input file (avformat_open_input). Return code %1.").arg(ret));

  // Find the stream info
  ret = avformat_find_stream_info(fmt_ctx, NULL);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not find stream information (avformat_find_stream_info). Return code %1.").arg(ret));

  // Get the first video stream 
  videoStreamIdx = -1;
  for(unsigned int i=0; i < fmt_ctx->nb_streams; i++)
    if(fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
    {
      videoStreamIdx = i;
      break;
    }
  if(videoStreamIdx==-1)
    return setOpeningError(QStringLiteral("Could not find a video stream."));
  
  videoCodec = avcodec_find_decoder(fmt_ctx->streams[videoStreamIdx]->codecpar->codec_id);
  if(!videoCodec)
    return setOpeningError(QStringLiteral("Could not find a video decoder (avcodec_find_decoder)"));

  // Allocate the decoder context
  decCtx = avcodec_alloc_context3(videoCodec);
  if(!decCtx)
    return setOpeningError(QStringLiteral("Could not allocate video deocder (avcodec_alloc_context3)"));

  AVCodecParameters *origin_par = fmt_ctx->streams[videoStreamIdx]->codecpar;
  ret = avcodec_parameters_to_context(decCtx, origin_par);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not copy codec parameters (avcodec_parameters_to_context). Return code %1.").arg(ret));

  // Open codec
  ret = avcodec_open2(decCtx, videoCodec, nullptr);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not open the video codec (avcodec_open2). Return code %1.").arg(ret));

  // Allocate the frame
  frame = av_frame_alloc();
  if (!frame)
    return setOpeningError(QStringLiteral("Could not allocate frame (av_frame_alloc)."));

  if (otherDec)
  {
    // Copy the key picture list and nrFrames from the other decoder
    keyFrameList = otherDec->keyFrameList;
    nrFrames = otherDec->nrFrames;
  }
  else
    if (!scanBitstream())
      return setOpeningError(QStringLiteral("Error scanning bitstream for key pictures."));

  // Initialize an empty packet
  av_init_packet(&pkt);
  pktInitialized = true;
  pkt.data = nullptr;
  pkt.size = 0;

  // Get the frame rate, picture size and color conversion mode
  frameRate = fmt_ctx->streams[videoStreamIdx]->avg_frame_rate.num / double(fmt_ctx->streams[videoStreamIdx]->avg_frame_rate.den);
  frameSize.setWidth(origin_par->width);
  frameSize.setHeight(origin_par->height);
  pixelFormat = decCtx->pix_fmt;
  if (origin_par->color_space == AVCOL_SPC_BT2020_NCL || origin_par->color_space == AVCOL_SPC_BT2020_CL)
    colorConversionType = BT2020;
  else
    colorConversionType = BT709;

  // Get the first video stream packet into the packet buffer.
  do
  {
    ret = av_read_frame(fmt_ctx, &pkt);
  } while (pkt.stream_index != videoStreamIdx);

  // Opening the deocder was successfull. We can now start to decode frames. Decode the first frame.
  loadYUVFrameData(0);

  return true;
}

bool FFmpegDecoder::decodeOneFrame()
{
  if (decodingError != ffmpeg_noError)
    return false;

  // First, try if there is a frame waiting in the decoder
  int retRecieve = avcodec_receive_frame(decCtx, frame);
  if (retRecieve == 0)
  {
    // We recieved a frame.
    // Recieved a frame
    DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s", frame->width, frame->height, frame->pts, frame->pict_type, frame->key_frame ? "key frame" : "");
    return true;
  }
  if (retRecieve < 0 && retRecieve != AVERROR(EAGAIN))
  {
    // An error occured
    setDecodingError(QStringLiteral("Error recieving frame (avcodec_receive_frame)"));
    return false;
  }
  
  // There was no frame waiting in the decoder. Feed data to the decoder until it returns AVERROR(EAGAIN)
  int retPush;
  do
  {
    // Push the video packet to the decoder
    if (endOfFile)
      retPush = avcodec_send_packet(decCtx, nullptr);
    else
      retPush = avcodec_send_packet(decCtx, &pkt);

    if (retPush < 0 && retPush != AVERROR(EAGAIN))
    {
      setDecodingError(QStringLiteral("Error sending packet (avcodec_send_packet)"));
      return false;
    }
    if (retPush != AVERROR(EAGAIN))
      DEBUG_FFMPEG("Send packet PTS %ld duration %ld flags %d", pkt.pts, pkt.duration, pkt.flags);

    if (!endOfFile && retPush == 0)
    {
      // Pushing was successfull, read the next video packet ...
      do
      {
        // Unref the old packet
        av_packet_unref(&pkt);
        // Get the next one
        int ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR_EOF)
        {
          // No more packets. End of file. Enter draining mode.
          DEBUG_FFMPEG("No more packets. End of file.");
          endOfFile = true;
        }
        else if (ret < 0)
        {
          setDecodingError(QStringLiteral("Error reading packet (av_read_frame)"));
          return false;
        }
      } while (!endOfFile && pkt.stream_index != videoStreamIdx);
    }
  } while (retPush == 0);

  // Now retry to get a frame
  retRecieve = avcodec_receive_frame(decCtx, frame);
  if (retRecieve == 0)
  {
    // We recieved a frame.
    // Recieved a frame
    DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s", frame->width, frame->height, frame->pts, frame->pict_type, frame->key_frame ? "key frame" : "");
    return true;
  }
  if (endOfFile && retRecieve == AVERROR_EOF)
  {
    // There are no more frames. If we want more frames, we have to seek to the start of the sequence and restart decoding.

  }
  if (retRecieve < 0 && retRecieve != AVERROR(EAGAIN))
  {
    // An error occured
    setDecodingError(QStringLiteral("Error recieving  frame (avcodec_receive_frame)"));
    return false;
  }
  
  return false;
}

yuvPixelFormat FFmpegDecoder::getYUVPixelFormat()
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

void FFmpegDecoder::loadFFmpegLibraries()
{
  // Try to load the ffmpeg libraries from the current working directory and several other directories.
  // Unfortunately relative paths like "./" do not work: (at least on windows)

  // First try the directory that is saved in the settings (if it exists).
  QSettings settings;
  QString settingsPath = settings.value("FFMpegPath",true).toString();
  loadFFmpegLibraryInPath(settingsPath);
  if (decodingError == ffmpeg_noError)
    // Success
    return;

  // Next, try the current working directory
  loadFFmpegLibraryInPath(QDir::currentPath() + "/");
  if (decodingError == ffmpeg_noError)
    // Success
    return;

  // Try the subdirectory "ffmpeg"
  loadFFmpegLibraryInPath(QDir::currentPath() + "/ffmpeg/");
  if (decodingError == ffmpeg_noError)
    // Success
    return;

  // Try the path of the YUView.exe
  loadFFmpegLibraryInPath(QCoreApplication::applicationDirPath() + "/");
  if (decodingError == ffmpeg_noError)
    // Success
    return;

  // Try the path of the YUView.exe -> sub directory "ffmpeg"
  loadFFmpegLibraryInPath(QCoreApplication::applicationDirPath() + "/ffmpeg/");
  if (decodingError == ffmpeg_noError)
    return;

  // Last try: Do not use any path.
  // Just try to call QLibrary::load so that the system folder will be searched.
  loadFFmpegLibraryInPath("");
}

void FFmpegDecoder::loadFFmpegLibraryInPath(QString path)
{
  // Clear the error state if one was set. 
  decodingError = ffmpeg_noError;
  errorString.clear();
  libAvutil.unload();
  libSwresample.unload();
  libAvcodec.unload();
  libAvformat.unload();

  // We will load the following libraries (in this order): 
  // avutil, swresample, avcodec, avformat.

  if (!path.isEmpty())
  {
    // A path was given. Search that path for the libraries.
    QString foundLibPath[4];
    QString ext = is_Q_OS_WIN ? "dll" : (is_Q_OS_LINUX ? "so" : ".dylib");
    
    QDir dir(path);
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

    // Check if all four libraries were found.
    if (foundLibPath[0].isEmpty())
      return setLibraryError(QStringLiteral("avutil library not found in path %1.").arg(foundLibPath[0]));
    if (foundLibPath[1].isEmpty())
      return setLibraryError(QStringLiteral("swresample library not found in path %1.").arg(foundLibPath[1]));
    if (foundLibPath[2].isEmpty())
      return setLibraryError(QStringLiteral("avcodec library not found in path %1.").arg(foundLibPath[2]));
    if (foundLibPath[3].isEmpty())
      return setLibraryError(QStringLiteral("avformat library not found in path %1.").arg(foundLibPath[3]));
    
    // We found some libraries. Try to load them.
    libAvutil.setFileName(foundLibPath[0]);
    if (!libAvutil.load())
      return setLibraryError(QStringLiteral("avutil library %1 could not be loaded.").arg(foundLibPath[0]));
    libSwresample.setFileName(foundLibPath[1]);
    if (!libSwresample.load())
      return setLibraryError(QStringLiteral("swresample library %1 could not be loaded.").arg(foundLibPath[1]));
    libAvcodec.setFileName(foundLibPath[2]);
    if (!libAvcodec.load())
      return setLibraryError(QStringLiteral("avcodec library %1 could not be loaded.").arg(foundLibPath[2]));
    libAvformat.setFileName(foundLibPath[3]);
    if (!libAvformat.load())
      return setLibraryError(QStringLiteral("avformat library %1 could not be loaded.").arg(foundLibPath[3]));

    // For the last test: Try to get pointers to all the libraries.
    bindFunctionsFromLibraries();
  }
  else
  {
    // No path was provided. We will try to open the libraries without looking for the files.
    // The ffmpeg libraries are named using a major version number. E.g: avutil-55.dll
    // However, we are just using a very limited set of functions that were available for a long
    // time and will probably also be available in future versions. Because of that, we will just
    // try out different numbers for the major version of the libraries.

    // This is how we the library name is constructed per platform
    auto constructLibName = [](QString lib, int ver)
    { 
      if (is_Q_OS_WIN)
        return lib + "-" + QString::number(ver);
      if (is_Q_OS_LINUX)
        return "lib" + lib + "-ffmpeg.so." + QString::number(ver);
      // TODO: MAC
    };

    // Start with the avutil library
    for (int i = LIBAVUTIL_VERSION_MAJOR-5; i < LIBAVUTIL_VERSION_MAJOR+10; i++)
    {
      libAvutil.setFileName(constructLibName("avutil", i));
      if (libAvutil.load())
        break;
    }
    if (!libAvutil.isLoaded())
      return setLibraryError(QStringLiteral("avutil library with versions from %1 to %2 could not be loaded.").arg(LIBAVUTIL_VERSION_MAJOR-5).arg(LIBAVUTIL_VERSION_MAJOR+10));

    // Next, the swresample library. 
    for (int i = LIBSWRESAMPLE_VERSION_MAJOR-1; i < LIBSWRESAMPLE_VERSION_MAJOR+5; i++)
    {
      libSwresample.setFileName(constructLibName("swresample", i));
      if (libSwresample.load())
        break;
    }
    if (!libSwresample.isLoaded())
      return setLibraryError(QStringLiteral("swresample library with versions from %1 to %2 could not be loaded.").arg(LIBSWRESAMPLE_VERSION_MAJOR-1).arg(LIBSWRESAMPLE_VERSION_MAJOR+5));

    // avcodec
    for (int i = LIBAVCODEC_VERSION_MAJOR-5; i < LIBAVCODEC_VERSION_MAJOR+10; i++)
    {
      libAvcodec.setFileName(constructLibName("avcodec", i));
      if (libAvcodec.load())
        break;
    }
    if (!libAvcodec.isLoaded())
      return setLibraryError(QStringLiteral("avcodec library with versions from %1 to %2 could not be loaded.").arg(LIBAVCODEC_VERSION_MAJOR-5).arg(LIBAVCODEC_VERSION_MAJOR+10));

    // avformat
    for (int i = LIBAVFORMAT_VERSION_MAJOR-5; i < LIBAVFORMAT_VERSION_MAJOR+10; i++)
    {
      libAvformat.setFileName(constructLibName("avformat", i));
      if (libAvformat.load())
        break;
    }
    if (!libAvformat.isLoaded())
      return setLibraryError(QStringLiteral("avformat library with versions from %1 to %2 could not be loaded.").arg(LIBAVFORMAT_VERSION_MAJOR-5).arg(LIBAVFORMAT_VERSION_MAJOR+10));
  }
}

void FFmpegDecoder::bindFunctionsFromLibraries()
{
  // Loading the libraries was successfull. Get/check function pointers.
  // From avformat
  if (!resolveAvFormat(av_register_all, "av_register_all")) return;
  if (!resolveAvFormat(avformat_open_input, "avformat_open_input")) return;
  if (!resolveAvFormat(avformat_close_input, "avformat_close_input")) return;
  if (!resolveAvFormat(avformat_find_stream_info, "avformat_find_stream_info")) return;
  if (!resolveAvFormat(av_read_frame, "av_read_frame")) return;
  if (!resolveAvFormat(av_seek_frame, "av_seek_frame")) return;

  // From avcodec
  if (!resolveAvCodec(avcodec_find_decoder, "avcodec_find_decoder")) return;
  if (!resolveAvCodec(avcodec_alloc_context3, "avcodec_alloc_context3")) return;
  if (!resolveAvCodec(avcodec_open2, "avcodec_open2")) return;
  if (!resolveAvCodec(avcodec_parameters_to_context, "avcodec_parameters_to_context")) return;
  if (!resolveAvCodec(avcodec_free_context, "avcodec_free_context")) return;
  if (!resolveAvCodec(av_init_packet, "av_init_packet")) return;
  if (!resolveAvCodec(av_packet_unref, "av_packet_unref")) return;
  if (!resolveAvCodec(avcodec_send_packet, "avcodec_send_packet")) return;
  if (!resolveAvCodec(avcodec_receive_frame, "avcodec_receive_frame")) return;
  if (!resolveAvCodec(avcodec_flush_buffers, "avcodec_flush_buffers")) return;

  // From avutil
  if (!resolveAvUtil(av_frame_alloc, "av_frame_alloc")) return;
  if (!resolveAvUtil(av_frame_free, "av_frame_free")) return;
}

QFunctionPointer FFmpegDecoder::resolveAvUtil(const char *symbol)
{
  QFunctionPointer ptr = libAvutil.resolve(symbol);
  if (!ptr) 
    setLibraryError(QStringLiteral("Error loading the avutil library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T FFmpegDecoder::resolveAvUtil(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolveAvUtil(symbol));
}

QFunctionPointer FFmpegDecoder::resolveAvFormat(const char *symbol)
{
  QFunctionPointer ptr = libAvformat.resolve(symbol);
  if (!ptr) 
    setLibraryError(QStringLiteral("Error loading the avformat library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T FFmpegDecoder::resolveAvFormat(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolveAvFormat(symbol));
}

QFunctionPointer FFmpegDecoder::resolveAvCodec(const char *symbol)
{
  QFunctionPointer ptr = libAvcodec.resolve(symbol);
  if (!ptr) 
    setLibraryError(QStringLiteral("Error loading the avcodec library: Can't find function %1.").arg(symbol));
  return ptr;
}

bool FFmpegDecoder::scanBitstream()
{
  // Seek to the beginning of the stream.
  // The stream should be at the beginning when calling this function, but it does not hurt.
  int ret = av_seek_frame(fmt_ctx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
  if (ret != 0)
    // Seeking failed. Maybe the stream is not opened correctly?
    return false;

  // Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidgetList l = QApplication::topLevelWidgets();
  QWidget *mainWindow = nullptr;
  for (QWidget *w : l)
  {
    MainWindow *mw = dynamic_cast<MainWindow*>(w);
    if (mw)
      mainWindow = mw;
  }
  // Create the dialog
  qint64 maxPTS = fmt_ctx->duration * fmt_ctx->streams[videoStreamIdx]->time_base.den / fmt_ctx->streams[videoStreamIdx]->time_base.num / AV_TIME_BASE;
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // Initialize an empty packet
  AVPacket p;
  av_init_packet(&p);
  p.data = nullptr;
  p.size = 0;

  qint64 lastKeyFramePTS = 0;
  do
  {
    // Get one packet
    ret = av_read_frame(fmt_ctx, &p);

    if (ret == 0 && p.stream_index == videoStreamIdx)
    {
      // Next video frame found
      if (p.flags & AV_PKT_FLAG_KEY)
      {
        if (nrFrames == -1)
          nrFrames = 0;
        keyFrameList.append(pictureIdx(nrFrames, p.pts));
        lastKeyFramePTS = p.pts;
      }
      if (p.pts < lastKeyFramePTS)
      {
        // What now? Can this happen? If this happens, the frame count/PTS combination of the last key frame
        // is wrong.
        keyFrameList.clear();
        nrFrames = -1;
        return false;
      }
      nrFrames++;

      // Update the progress dialog
      if (progress.wasCanceled())
      {
        keyFrameList.clear();
        nrFrames = -1;
        return false;
      }
      int newPercentValue = p.pts * 100 / maxPTS;
      if (newPercentValue != curPercentValue)
      {
        progress.setValue(newPercentValue);
        curPercentValue = newPercentValue;
      }
    }

    // Unref the packet
    av_packet_unref(&p);
  } while (ret == 0);

  progress.close();

  // Seek back to the beginning of the stream.
  ret = av_seek_frame(fmt_ctx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
  if (ret != 0)
    // Seeking failed.
    return false;

  return true;
}

template <typename T> T FFmpegDecoder::resolveAvCodec(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolveAvCodec(symbol));
}

QList<infoItem> FFmpegDecoder::getFileInfoList() const
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

QByteArray FFmpegDecoder::loadYUVFrameData(int frameIdx)
{
  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    return currentOutputBuffer;
  }
  
  // We have to decode the requested frame.
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and start decoding from there.
    pictureIdx seekFrameIdxAndPTS = getClosestSeekableFrameNumberBefore(frameIdx);

    DEBUG_FFMPEG("FFmpegDecoder::loadYUVData Seek to frame %lld PTS %lld", seekFrameIdxAndPTS.frame, seekFrameIdxAndPTS.pts);
    seekToPTS(seekFrameIdxAndPTS.pts);
    currentOutputBufferFrameIndex = seekFrameIdxAndPTS.frame - 1;
  }
  else if (frameIdx > currentOutputBufferFrameIndex+2)
  {
    // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
    // Check if there is a random access point closer to the requested frame than the position that we are at right now.
    pictureIdx seekFrameIdxAndPTS = getClosestSeekableFrameNumberBefore(frameIdx);
    if (seekFrameIdxAndPTS.frame > currentOutputBufferFrameIndex)
    {
      // Yes we can (and should) seek ahead in the file
      DEBUG_FFMPEG("FFmpegDecoder::loadYUVData Seek to frame %lld PTS %lld", seekFrameIdxAndPTS.frame, seekFrameIdxAndPTS.pts);
      seekToPTS(seekFrameIdxAndPTS.pts);
      currentOutputBufferFrameIndex = seekFrameIdxAndPTS.frame - 1;
    }
  }

  // Perform the decoding right now blocking the main thread.
  // Decode frames until we receive the one we are looking for.
  while (decodeOneFrame())
  {
    currentOutputBufferFrameIndex++;

    // We have decoded one frame. Get the pixel format.
    if (currentOutputBufferFrameIndex == frameIdx)
    {
      // This is the frame that we want to decode

      // Put image data into buffer
      copyFrameToOutputBuffer();

      return currentOutputBuffer;
    }
  }

  return QByteArray();
}

void FFmpegDecoder::copyFrameToOutputBuffer()
{
  DEBUG_FFMPEG("FFmpegDecoder::copyFrameToOutputBuffer frame %d", currentOutputBufferFrameIndex);

  // At first get how many bytes we are going to write  
  yuvPixelFormat pixFmt = getYUVPixelFormat();
  int nrBytesPerSample = pixFmt.bitsPerSample <= 8 ? 1 : 2;
  int nrBytes = frameSize.width() * frameSize.height() * nrBytesPerSample;
  nrBytes += 2 * frameSize.width() / pixFmt.getSubsamplingHor() * frameSize.height() / pixFmt.getSubsamplingVer() * nrBytesPerSample;

  // Is the output big enough?
  if (currentOutputBuffer.capacity() < nrBytes)
    currentOutputBuffer.resize(nrBytes);

  char* dst_c = currentOutputBuffer.data();
  int lengthY = frame->linesize[0] * frame->height;
  int lengthC = frame->linesize[1] * frame->height / 2;
  memcpy(dst_c                    , frame->data[0], lengthY);
  memcpy(dst_c + lengthY          , frame->data[1], frame->linesize[1] * frame->height / 2);
  memcpy(dst_c + lengthY + lengthC, frame->data[2], frame->linesize[2] * frame->height / 2);
}

void FFmpegDecoder::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(fullFilePath);
  else
    fileWatcher.removePath(fullFilePath);
}

bool FFmpegDecoder::reloadItemSource()
{
  if (decodingError != ffmpeg_noError)
    // Nothing is working, so there is nothing to reset.
    return false;

  // TODO: Drop everything we know about the bitstream, and reload it from scratch.
  return false;
}

bool FFmpegDecoder::checkForLibraries(QString path)
{
  // Create a FFMpefDecoder instance and try to load the ffmpeg libraries from the given directory.
  FFmpegDecoder dec;
  dec.loadFFmpegLibraryInPath(path);
  return (dec.decodingError == ffmpeg_noError);
}

FFmpegDecoder::pictureIdx FFmpegDecoder::getClosestSeekableFrameNumberBefore(int frameIdx)
{
  pictureIdx ret = keyFrameList.first();
  for (auto f : keyFrameList)
  {
    if (f.frame >= frameIdx)
      // This key picture is after the given index. Return the last found key picture.
      return ret;
    ret = f;
  }
  return ret;
}

bool FFmpegDecoder::seekToPTS(qint64 pts)
{
  int ret = av_seek_frame(fmt_ctx, videoStreamIdx, pts, AVSEEK_FLAG_BACKWARD);
  if (ret != 0)
    return false;

  endOfFile = false;

  // Flush the video decoder buffer
  avcodec_flush_buffers(decCtx);
  
  // Get the first video stream packet into the packet buffer.
  do
  {
    // Unref the packet that we hold right now
    av_packet_unref(&pkt);
    ret = av_read_frame(fmt_ctx, &pkt);
  } while (pkt.stream_index != videoStreamIdx);

  return true;
}

void FFmpegDecoder::getFormatInfo()
{
  QString out;

  int index = 0;
  AVFormatContext *ic = fmt_ctx;
  
  out.append(QString("Input %1, %2\n").arg(index).arg(ic->iformat->name));
  
  //dump_metadata(NULL, ic->metadata, "  ");
  if (ic->duration != AV_NOPTS_VALUE)
  {
    int hours, mins, secs, us;
    int64_t duration = ic->duration + 5000;
    secs  = duration / AV_TIME_BASE;
    us    = duration % AV_TIME_BASE;
    mins  = secs / 60;
    secs %= 60;
    hours = mins / 60;
    mins %= 60;
    out.append(QString("  Duration: %1:%2:%3.%4\n").arg(hours).arg(mins).arg(secs).arg((100 * us) / AV_TIME_BASE));
  } 
  else 
  {
    out.append(QString("  Duration: N/A\n"));
  }

  if (ic->start_time != AV_NOPTS_VALUE) 
  {
    int secs, us;
    secs = ic->start_time / AV_TIME_BASE;
    us   = abs(ic->start_time % AV_TIME_BASE);
    out.append(QString("  Start: %1.%2\n").arg(secs).arg(us));
  }
  
  if (ic->bit_rate)
    out.append(QString("  Bitrate: %1 kb/s\n").arg(ic->bit_rate / 1000));
  else
    out.append(QString("  Bitrate: N/A kb/s\n"));
  
  for (unsigned int i = 0; i < ic->nb_chapters; i++)
  {
    AVChapter *ch = ic->chapters[i];
    double start = ch->start * ch->time_base.num / double(ch->time_base.den);
    double end   = ch->end   * ch->time_base.num / double(ch->time_base.den);
    out.append(QString("  Chapter #%1:%2 start %3, end %4\n").arg(index).arg(i).arg(start).arg(end));

    //dump_metadata(NULL, ch->metadata, "    ");
  }
 
  for (unsigned int i = 0; i < ic->nb_streams; i++)
  {
    // Get the stream format of stream i
    //char buf[256];
    //int flags = ic->iformat->flags;
    //AVStream *st = ic->streams[i];

    // ...
    
  }
}
