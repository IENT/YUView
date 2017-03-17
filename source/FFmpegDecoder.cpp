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

using namespace FFmpeg;

#define FFmpegDecoder_DEBUG_OUTPUT 0
#if FFmpegDecoder_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

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
  frameRate = -1;
  colorConversionType = BT709;
  pkt = nullptr;
  streamCodecID = AV_CODEC_ID_NONE;

  // Initialize the file watcher and install it (if enabled)
  fileChanged = false;
  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &FFmpegDecoder::fileSystemWatcherFileChanged);
  updateFileWatchSetting();

  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;
  statsCacheCurFrameIdx = -1;
}

FFmpegDecoder::~FFmpegDecoder()
{
  // Free all the allocated data structures
  if (pkt)
  {
    ff.deletePacket(pkt);
    pkt = nullptr;
  }
  if (decCtx)
    ff.avcodec_free_context(&decCtx);
  if (frame)
    ff.av_frame_free(&frame);
  if (fmt_ctx)
    ff.avformat_close_input(&fmt_ctx);
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
  ff.av_register_all();

  // Open the input file
  int ret = ff.avformat_open_input(&fmt_ctx, fileName.toStdString().c_str(), nullptr, nullptr);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not open the input file (avformat_open_input). Return code %1.").arg(ret));

  // Find the stream info
  ret = ff.avformat_find_stream_info(fmt_ctx, NULL);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not find stream information (avformat_find_stream_info). Return code %1.").arg(ret));

  // Get the first video stream
  videoStreamIdx = -1;
  unsigned int nb_streams = ff.AVFormatContextGetNBStreams(fmt_ctx);
  for(unsigned int i=0; i < nb_streams; i++)
  {
    AVMediaType streamType;
    if (ff.newParametersAPIAvailable)
      streamType = ff.AVFormatContextGetCodecTypeFromCodecpar(fmt_ctx, i);
    else
      streamType = ff.AVFormatContextGetCodecTypeFromCodec(fmt_ctx, i);

    if(streamType == AVMEDIA_TYPE_VIDEO)
    {
      videoStreamIdx = i;
      break;
    }
  }
  if(videoStreamIdx==-1)
    return setOpeningError(QStringLiteral("Could not find a video stream."));

  if (ff.newParametersAPIAvailable)
    streamCodecID = ff.AVFormatContextGetCodecIDFromCodecpar(fmt_ctx, videoStreamIdx);
  else
    streamCodecID = ff.AVFormatContextGetCodecIDFromCodec(fmt_ctx, videoStreamIdx);

  videoCodec = ff.avcodec_find_decoder(streamCodecID);
  if(!videoCodec)
    return setOpeningError(QStringLiteral("Could not find a video decoder (avcodec_find_decoder)"));

  // Allocate the decoder context
  decCtx = ff.avcodec_alloc_context3(videoCodec);
  if(!decCtx)
    return setOpeningError(QStringLiteral("Could not allocate video deocder (avcodec_alloc_context3)"));

  AVCodecParameters *origin_par = nullptr;
  if (ff.newParametersAPIAvailable)
  {
    // Use the new avcodec_parameters_to_context function.
    AVStream *str = ff.AVFormatContextGetStream(fmt_ctx, videoStreamIdx);
    origin_par = ff.AVStreamGetCodecpar(str);

    ret = ff.avcodec_parameters_to_context(decCtx, origin_par);
    if (ret < 0)
      return setOpeningError(QStringLiteral("Could not copy codec parameters (avcodec_parameters_to_context). Return code %1.").arg(ret));
  }
  else
  {
    // The new parameters API is not available. Perform what the function would do.
    // This is equal to the implementation of avcodec_parameters_to_context.
    AVStream *str = ff.AVFormatContextGetStream(fmt_ctx, videoStreamIdx);
    AVCodecContext *ctxSrc = ff.AVStreamGetCodec(str);
    if (!ff.AVCodecContextCopyParameters(ctxSrc, decCtx))
      return setOpeningError(QStringLiteral("Could not copy decoder parameters from stream decoder."));
  }

  // Ask the decoder to provide motion vectors (if possible)
  AVDictionary *opts = nullptr;
  ff.av_dict_set(&opts, "flags2", "+export_mvs", 0);

  // Open codec
  ret = ff.avcodec_open2(decCtx, videoCodec, &opts);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not open the video codec (avcodec_open2). Return code %1.").arg(ret));

  // Allocate the frame
  frame = ff.av_frame_alloc();
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
  assert(pkt == nullptr);
  pkt = ff.getNewPacket();
  ff.av_init_packet(pkt);

  // Get the frame rate, picture size and color conversion mode
  AVRational avgFrameRate = ff.AVFormatContextGetAvgFrameRate(fmt_ctx, videoStreamIdx);
  frameRate = avgFrameRate.num / double(avgFrameRate.den);
  pixelFormat = ff.AVCodecContextGetPixelFormat(decCtx);

  int w,h;
  AVColorSpace colSpace;
  if (ff.newParametersAPIAvailable)
  {
    // Get values from the AVCodecParameters API
    w = ff.AVCodecParametersGetWidth(origin_par);
    h = ff.AVCodecParametersGetHeight(origin_par);
    colSpace = ff.AVCodecParametersGetColorSpace(origin_par);
  }
  else
  {
    // Get values from the old *codec
    w = ff.AVCodecContexGetWidth(decCtx);
    h = ff.AVCodecContextGetHeight(decCtx);
    colSpace = ff.AVCodecContextGetColorSpace(decCtx);
  }
  frameSize.setWidth(w);
  frameSize.setHeight(h);

  if (colSpace == AVCOL_SPC_BT2020_NCL || colSpace == AVCOL_SPC_BT2020_CL)
    colorConversionType = BT2020;
  else
    colorConversionType = BT709;

  // Get the first video stream packet into the packet buffer.
  do
  {
    ret = ff.av_read_frame(fmt_ctx, pkt);
    if (ret < 0)
      return setOpeningError(QStringLiteral("Could not retrieve first packet of the video stream."));
  }
  while (ff.AVPacketGetStreamIndex(pkt) != videoStreamIdx);

  // Opening the deocder was successfull. We can now start to decode frames. Decode the first frame.
  loadYUVFrameData(0);

  return true;
}

bool FFmpegDecoder::decodeOneFrame()
{
  if (decodingError != ffmpeg_noError)
    return false;

  if (!ff.newParametersAPIAvailable)
  {
    // Old API using avcodec_decode_video2
    int got_frame;
    do
    {
      int ret = ff.avcodec_decode_video2(decCtx, frame, &got_frame, pkt);
      if (ret < 0)
      {
        setDecodingError(QStringLiteral("Error decoding frame (avcodec_decode_video2). Return code %1").arg(ret));
        return false;
      }
      DEBUG_FFMPEG("Called avcodec_decode_video2 for packet PTS %ld duration %ld flags %d got_frame %d",
        ff.AVPacketGetPTS(pkt),
        ff.AVPacketGetDuration(pkt),
        ff.AVPacketGetFlags(pkt),
        got_frame);

      if (endOfFile)
      {
        // There are no more frames to get from the bitstream.
        // We just keep on calling avcodec_decode_video2(...) with an unallocated packet until it returns no more frames.
        return (got_frame != 0);
      }

      // Read the next packet for the next call to avcodec_decode_video2.
      do
      {
        // Unref the old packet
        ff.av_packet_unref(pkt);
        // Get the next one
        int ret = ff.av_read_frame(fmt_ctx, pkt);
        if (ret == AVERROR_EOF)
        {
          // No more packets. End of file. Enter draining mode.
          DEBUG_FFMPEG("No more packets. End of file.");
          ff.av_packet_unref(pkt);
          endOfFile = true;
        }
        else if (ret < 0)
        {
          setDecodingError(QStringLiteral("Error reading packet (av_read_frame). Return code %1").arg(ret));
          return false;
        }
      } while (!endOfFile && ff.AVPacketGetStreamIndex(pkt) != videoStreamIdx);
    } while (!got_frame);

    DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s",
      ff.AVFrameGetWidth(frame),
      ff.AVFrameGetHeight(frame),
      ff.AVFrameGetPTS(frame),
      ff.AVFrameGetPictureType(frame),
      ff.AVFrameGetKeyFrame(frame) ? "key frame" : "");
    return true;
  }

  // First, try if there is a frame waiting in the decoder
  int retRecieve = ff.avcodec_receive_frame(decCtx, frame);
  if (retRecieve == 0)
  {
    // We recieved a frame.
    // Recieved a frame
    DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s",
      ff.AVFrameGetWidth(frame),
      ff.AVFrameGetHeight(frame),
      ff.AVFrameGetPTS(frame),
      ff.AVFrameGetPictureType(frame),
      ff.AVFrameGetKeyFrame(frame) ? "key frame" : "");
    return true;
  }
  if (retRecieve < 0 && retRecieve != AVERROR_EAGAIN)
  {
    // An error occured
    setDecodingError(QStringLiteral("Error recieving frame (avcodec_receive_frame)"));
    return false;
  }

  // There was no frame waiting in the decoder. Feed data to the decoder until it returns AVERROR_EAGAIN
  int retPush;
  do
  {
    // Push the video packet to the decoder
    if (endOfFile)
      retPush = ff.avcodec_send_packet(decCtx, nullptr);
    else
      retPush = ff.avcodec_send_packet(decCtx, pkt);

    if (retPush < 0 && retPush != AVERROR_EAGAIN)
    {
      setDecodingError(QStringLiteral("Error sending packet (avcodec_send_packet)"));
      return false;
    }
    if (retPush != AVERROR_EAGAIN)
      DEBUG_FFMPEG("Send packet PTS %ld duration %ld flags %d",
        ff.AVPacketGetPTS(pkt),
        ff.AVPacketGetDuration(pkt),
        ff.AVPacketGetFlags(pkt) );

    if (!endOfFile && retPush == 0)
    {
      // Pushing was successfull, read the next video packet ...
      do
      {
        // Unref the old packet
        ff.av_packet_unref(pkt);
        // Get the next one
        int ret = ff.av_read_frame(fmt_ctx, pkt);
        if (ret == AVERROR_EOF)
        {
          // No more packets. End of file. Enter draining mode.
          DEBUG_FFMPEG("No more packets. End of file.");
          endOfFile = true;
        }
        else if (ret < 0)
        {
          setDecodingError(QStringLiteral("Error reading packet (av_read_frame). Return code %1").arg(ret));
          return false;
        }
      } while (!endOfFile && ff.AVPacketGetStreamIndex(pkt) != videoStreamIdx);
    }
  } while (retPush == 0);

  // Now retry to get a frame
  retRecieve = ff.avcodec_receive_frame(decCtx, frame);
  if (retRecieve == 0)
  {
    // We recieved a frame.
    // Recieved a frame
    DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s",
      ff.AVFrameGetWidth(frame),
      ff.AVFrameGetHeight(frame),
      ff.AVFrameGetPTS(frame),
      ff.AVFrameGetPictureType(frame),
      ff.AVFrameGetKeyFrame(frame) ? "key frame" : "");
    return true;
  }
  if (endOfFile && retRecieve == AVERROR_EOF)
  {
    // There are no more frames. If we want more frames, we have to seek to the start of the sequence and restart decoding.

  }
  if (retRecieve < 0 && retRecieve != AVERROR_EAGAIN)
  {
    // An error occured
    setDecodingError(QStringLiteral("Error recieving  frame (avcodec_receive_frame). Return code %1").arg(retRecieve));
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
  if (ff.loadFFmpegLibraryInPath(settingsPath))
    // Success
    return;

  // Next, try the current working directory
  if (ff.loadFFmpegLibraryInPath(QDir::currentPath() + "/"))
    // Success
    return;

  // Try the subdirectory "ffmpeg"
  if (ff.loadFFmpegLibraryInPath(QDir::currentPath() + "/ffmpeg/"))
    // Success
    return;

  // Try the path of the YUView.exe
  if (ff.loadFFmpegLibraryInPath(QCoreApplication::applicationDirPath() + "/"))
    // Success
    return;

  // Try the path of the YUView.exe -> sub directory "ffmpeg"
  if (ff.loadFFmpegLibraryInPath(QCoreApplication::applicationDirPath() + "/ffmpeg/"))
    return;

  // Last try: Do not use any path.
  // Just try to call QLibrary::load so that the system folder will be searched.
  if (ff.loadFFmpegLibraryInPath(""))
    return;

  // Loading the libraries failed
  decodingError = ffmpeg_errorLoadingLibrary;
  errorString = ff.libErrorString();
}

bool FFmpegDecoder::scanBitstream()
{
  // Seek to the beginning of the stream.
  // The stream should be at the beginning when calling this function, but it does not hurt.
  int ret = ff.av_seek_frame(fmt_ctx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
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
  int64_t duration = ff.AVFormatContextGetDuration(fmt_ctx);
  AVRational timeBase = ff.AVFormatContextGetTimeBase(fmt_ctx, videoStreamIdx);

  qint64 maxPTS = duration * timeBase.den / timeBase.num / AV_TIME_BASE;
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // Initialize an empty packet (data and size set to 0).
  AVPacket *p = ff.getNewPacket();
  ff.av_init_packet(p);

  qint64 lastKeyFramePTS = 0;
  do
  {
    // Get one packet
    ret = ff.av_read_frame(fmt_ctx, p);

    if (ret == 0 && ff.AVPacketGetStreamIndex(p) == videoStreamIdx)
    {
      int64_t pts = ff.AVPacketGetPTS(p);

      // Next video frame found
      if (ff.AVPacketGetFlags(p) & AV_PKT_FLAG_KEY)
      {
        if (nrFrames == -1)
          nrFrames = 0;
        keyFrameList.append(pictureIdx(nrFrames, pts));
        lastKeyFramePTS = pts;
      }
      if (pts < lastKeyFramePTS)
      {
        // What now? Can this happen? If this happens, the frame count/PTS combination of the last key frame
        // is wrong.
        keyFrameList.clear();
        nrFrames = -1;
        // Free the packet (this will automatically unref the packet as weel)
        ff.av_packet_unref(p);
        ff.deletePacket(p);
        return false;
      }
      nrFrames++;

      // Update the progress dialog
      if (progress.wasCanceled())
      {
        keyFrameList.clear();
        nrFrames = -1;
        // Free the packet (this will automatically unref the packet as weel)
        ff.av_packet_unref(p);
        ff.deletePacket(p);
        return false;
      }
      int newPercentValue = pts * 100 / maxPTS;
      if (newPercentValue != curPercentValue)
      {
        progress.setValue(newPercentValue);
        curPercentValue = newPercentValue;
      }
    }

    // Unref the packet
    ff.av_packet_unref(p);
  } while (ret == 0);

  // Delete the packet again
  ff.deletePacket(p);

  progress.close();

  // Seek back to the beginning of the stream.
  ret = ff.av_seek_frame(fmt_ctx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
  if (ret != 0)
    // Seeking failed.
    return false;

  return true;
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

QList<infoItem> FFmpegDecoder::getDecoderInfo() const
{
  QList<infoItem> retList;

  retList.append(infoItem("Lib Path", ff.getLibPath(), "The library was loaded from this path."));
  retList.append(infoItem("Lib Version", ff.getLibVersionString(), "The version of the loaded libraries"));
  retList.append(infoItem("Codec", QString(ff.avcodec_get_name(streamCodecID)), "The codec of the stream that was opened"));

  return retList;
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

      // Get the motion vectors from the image as well...
      copyFrameMotionInformation();
      statsCacheCurFrameIdx = currentOutputBufferFrameIndex;

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
  int nrBytesY = frameSize.width() * frameSize.height() * nrBytesPerSample;
  int nrBytesC = frameSize.width() / pixFmt.getSubsamplingHor() * frameSize.height() / pixFmt.getSubsamplingVer() * nrBytesPerSample;
  int nrBytes = nrBytesY + 2 * nrBytesC;

  // Is the output big enough?
  if (currentOutputBuffer.capacity() < nrBytes)
    currentOutputBuffer.resize(nrBytes);

  // Copy line by line. The linesize of the source may be larger than the width of the frame.
  // This may be because the frame buffer is (8) byte aligned. Also the internal decoded
  // resolution may be larger than the output frame size.
  uint8_t *src = ff.AVFrameGetData(frame, 0);
  int linesize = ff.AVFrameGetLinesize(frame, 0);
  char* dst = currentOutputBuffer.data();
  int wDst = frameSize.width();
  int hDst = frameSize.height();
  for (int y = 0; y < hDst; y++)
  {
    // Copy one line
    memcpy(dst, src, wDst);
    // Goto the next line in input and output (these offsets/strides may differ)
    dst += wDst;
    src += linesize;
  }

  // Chroma
  wDst = frameSize.width() / pixFmt.getSubsamplingHor();
  hDst = frameSize.height() / pixFmt.getSubsamplingVer();
  for (int c = 0; c < 2; c++)
  {
    uint8_t *src = ff.AVFrameGetData(frame, 1+c);
    linesize = ff.AVFrameGetLinesize(frame, 1+c);
    dst = currentOutputBuffer.data();
    dst += (nrBytesY + ((c == 0) ? 0 : nrBytesC));
    for (int y = 0; y < hDst; y++)
    {
      memcpy(dst, src, wDst);
      // Goto the next line
      dst += wDst;
      src += linesize;
    }
  }
}

void FFmpegDecoder::copyFrameMotionInformation()
{
  DEBUG_FFMPEG("FFmpegDecoder::copyFrameMotionInformation frame %d", currentOutputBufferFrameIndex);
  
  // Clear the local statistics cache
  curFrameStats.clear();

  // Try to get the motion information
  AVFrameSideData *sd = ff.av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
  if (sd)
  {
    AVMotionVector *mvs = (AVMotionVector*)ff.getSideDataData(sd);
    int nrMVs = ff.getSideDataNrMotionVectors(sd);
    for (int i = 0; i < nrMVs; i++)
    {
      int32_t source;
      uint8_t w,h;
      int16_t src_x, src_y, dst_x, dst_y;
      ff.getMotionVectorValues(mvs, i, source, w, h, src_x, src_y, dst_x, dst_y);

      // dst marks the center of the current block so the block position is:
      int blockX = dst_x - w/2;
      int blockY = dst_y - h/2;
      int16_t mvX = dst_x - src_x;
      int16_t mvY = dst_y - src_y;

      curFrameStats[0].addBlockValue(blockX, blockY, w, h, (int)source);
      curFrameStats[1].addBlockVector(blockX, blockY, w, h, mvX, mvY);
    }
  }
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
  dec.ff.loadFFmpegLibraryInPath(path);
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
  int ret = ff.av_seek_frame(fmt_ctx, videoStreamIdx, pts, AVSEEK_FLAG_BACKWARD);
  if (ret != 0)
  {
    DEBUG_FFMPEG("FFmpegDecoder::seekToPTS Error PTS %ld. Return Code %d", pts, ret);
    return false;
  }

  // Flush the video decoder buffer
  ff.avcodec_flush_buffers(decCtx);

  // Get the first video stream packet into the packet buffer.
  do
  {
    // Unref the packet that we hold right now
    if (!endOfFile)
      ff.av_packet_unref((AVPacket*)pkt);
    ret = ff.av_read_frame(fmt_ctx, (AVPacket*)pkt);
  } while (ff.AVPacketGetStreamIndex(pkt) != videoStreamIdx);

  // We seeked somewhere, so we are not at the end of the file anymore.
  endOfFile = false;

  DEBUG_FFMPEG("FFmpegDecoder::seekToPTS Successfully seeked to PTS %d", pts);
  return true;
}

statisticsData FFmpegDecoder::getStatisticsData(int frameIdx, int typeIdx)
{
  if (frameIdx != statsCacheCurFrameIdx)
  {
    if (currentOutputBufferFrameIndex == frameIdx)
      // We will have to decode the current frame again to get the internals/statistics
      // This can be done like this:
      currentOutputBufferFrameIndex ++;

    loadYUVFrameData(frameIdx);
  }

  return curFrameStats[typeIdx];
}

void FFmpegDecoder::getFormatInfo()
{
  /*QString out;

  int index = 0;
  AVFormatContext *ic = fmt_ctx;*/

  //out.append(QString("Input %1, %2\n").arg(index).arg(ic->iformat->name));

  ////dump_metadata(NULL, ic->metadata, "  ");
  //if (ic->duration != AV_NOPTS_VALUE)
  //{
  //  int hours, mins, secs, us;
  //  int64_t duration = ic->duration + 5000;
  //  secs  = duration / AV_TIME_BASE;
  //  us    = duration % AV_TIME_BASE;
  //  mins  = secs / 60;
  //  secs %= 60;
  //  hours = mins / 60;
  //  mins %= 60;
  //  out.append(QString("  Duration: %1:%2:%3.%4\n").arg(hours).arg(mins).arg(secs).arg((100 * us) / AV_TIME_BASE));
  //}
  //else
  //{
  //  out.append(QString("  Duration: N/A\n"));
  //}

  //if (ic->start_time != AV_NOPTS_VALUE)
  //{
  //  int secs, us;
  //  secs = ic->start_time / AV_TIME_BASE;
  //  us   = abs(ic->start_time % AV_TIME_BASE);
  //  out.append(QString("  Start: %1.%2\n").arg(secs).arg(us));
  //}
  //
  //if (ic->bit_rate)
  //  out.append(QString("  Bitrate: %1 kb/s\n").arg(ic->bit_rate / 1000));
  //else
  //  out.append(QString("  Bitrate: N/A kb/s\n"));
  //
  //for (unsigned int i = 0; i < ic->nb_chapters; i++)
  //{
  //  AVChapter *ch = ic->chapters[i];
  //  double start = ch->start * ch->time_base.num / double(ch->time_base.den);
  //  double end   = ch->end   * ch->time_base.num / double(ch->time_base.den);
  //  out.append(QString("  Chapter #%1:%2 start %3, end %4\n").arg(index).arg(i).arg(start).arg(end));

  //  //dump_metadata(NULL, ch->metadata, "    ");
  //}

  //for (unsigned int i = 0; i < ic->nb_streams; i++)
  //{
  //  // Get the stream format of stream i
  //  //char buf[256];
  //  //int flags = ic->iformat->flags;
  //  //AVStream *st = ic->streams[i];

  //  // ...
  //
  //}
}
