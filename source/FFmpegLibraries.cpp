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

#include "FFmpegLibraries.h"

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

FFmpegLibraries::FFmpegLibraries()
{
  // No error (yet)
  librariesLoadingError = false;
  decodingError = false;
  readingFileError = false;

  librariesLoaded = false;
  
  // Set default values
  pixelFormat = AV_PIX_FMT_NONE;
  
  endOfFile = false;
  frameRate = -1;
  colorConversionType = BT709_LimitedRange;
  duration = -1;
  timeBase.num = 0;
  timeBase.den = 0;
  
  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;
  statsCacheCurFrameIdx = -1;
}

FFmpegLibraries::~FFmpegLibraries()
{
  // Free all the allocated data structures
  if (pkt)
    pkt.free_packet();
  /*if (decCtx)
    ff.avcodec_free_context(&decCtx);*/
  if (frame)
    frame.free_frame(ff);
  fmt_ctx.avformat_close_input(ff);
}

bool FFmpegLibraries::openFile(QString fileName)
{
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  // The libraries are only loaded on demand. This way a FFmpegLibraries instance can exist without loading the libraries.
  if (!loadFFmpegLibraries())
    return false;

  // Open the input file
  int ret = ff.open_input(fmt_ctx, fileName);
  if (ret < 0)
    return setOpeningError(QStringLiteral("Could not open the input file (open_input). Return code %1.").arg(ret));

  // What is the input format?
  AVInputFormatWrapper inp_format = fmt_ctx.get_input_format();
  
  // Get the first video stream
  for(unsigned int idx=0; idx < fmt_ctx.get_nb_streams(); idx++)
  {
    AVStreamWrapper stream = fmt_ctx.get_stream(idx);
    AVMediaType streamType =  stream.getCodecType();
    if(streamType == AVMEDIA_TYPE_VIDEO)
    {
      video_stream = stream;
      break;
    }
  }
  if(!video_stream)
    return setOpeningError(QStringLiteral("Could not find a video stream."));

  AVCodecID streamCodecID = video_stream.getCodecID();
  videoCodec = ff.find_decoder(streamCodecID);

  if(!videoCodec)
    return setOpeningError(QStringLiteral("Could not find a video decoder (avcodec_find_decoder)"));

  //// Allocate the decoder context
  //decCtx = ff.alloc_decoder(videoCodec);
  //if(!decCtx)
  //  return setOpeningError(QStringLiteral("Could not allocate video deocder (avcodec_alloc_context3)"));

  //ff.parse_decoder_parameters(decCtx, video_stream);

  //// Ask the decoder to provide motion vectors (if possible)
  //AVDictionaryWrapper opts;
  //if (ff.av_dict_set(opts, "flags2", "+export_mvs", 0))
  //  return setOpeningError(QStringLiteral("Could not request motion vector retrieval.").arg(ret));

  //// Open codec
  //ret = ff.avcodec_open2(decCtx, videoCodec, opts);
  //if (ret < 0)
  //  return setOpeningError(QStringLiteral("Could not open the video codec (avcodec_open2). Return code %1.").arg(ret));

  //frame.allocate_frame(ff);
  //if (!frame)
  //  return setOpeningError(QStringLiteral("Could not allocate frame (av_frame_alloc)."));

  // Initialize an empty packet
  pkt.allocate_paket(ff);

  // Get the frame rate, picture size and color conversion mode
  AVRational avgFrameRate = video_stream.get_avg_frame_rate();
  if (avgFrameRate.den == 0)
    frameRate = -1;
  else
    frameRate = avgFrameRate.num / double(avgFrameRate.den);
  pixelFormat = decCtx.get_pixel_format();
  duration = fmt_ctx.get_duration();
  timeBase = video_stream.get_time_base();

  AVColorSpace colSpace = video_stream.get_colorspace();
  int w = video_stream.get_frame_width();
  int h = video_stream.get_frame_height();
  frameSize.setWidth(w);
  frameSize.setHeight(h);

  if (colSpace == AVCOL_SPC_BT2020_NCL || colSpace == AVCOL_SPC_BT2020_CL)
    colorConversionType = BT2020_LimitedRange;
  else if (colSpace == AVCOL_SPC_BT470BG || colSpace == AVCOL_SPC_SMPTE170M)
    colorConversionType = BT601_LimitedRange;
  else
    colorConversionType = BT709_LimitedRange;

  // goToNextPacket();
  //// Get the first video stream packet into the packet buffer.
  //do
  //{
  //  ret = fmt_ctx.read_frame(ff, pkt);
  //  if (ret < 0)
  //    return setOpeningError(QStringLiteral("Could not retrieve first packet of the video stream."));
  //}
  //while (pkt.get_stream_index() != video_stream.get_index());
  
  return true;
}

bool FFmpegLibraries::decodeOneFrame()
{
  if (decodingError)
    return false;

  return ff.decode_frame(decCtx, fmt_ctx, frame, pkt, endOfFile, video_stream.get_index());
  
  //if (!ff.newParametersAPIAvailable)
  {
  //  // Old API using avcodec_decode_video2
  //  int got_frame;
  //  do
  //  {
  //    int ret = ff.avcodec_decode_video2(decCtx, frame, &got_frame, pkt);
  //    if (ret < 0)
  //    {
  //      setDecodingError(QStringLiteral("Error decoding frame (avcodec_decode_video2). Return code %1").arg(ret));
  //      return false;
  //    }
  //    DEBUG_FFMPEG("Called avcodec_decode_video2 for packet PTS %ld duration %ld flags %d got_frame %d",
  //      ff.AVPacketGetPTS(pkt),
  //      ff.AVPacketGetDuration(pkt),
  //      ff.AVPacketGetFlags(pkt),
  //      got_frame);

  //    if (endOfFile)
  //    {
  //      // There are no more frames to get from the bitstream.
  //      // We just keep on calling avcodec_decode_video2(...) with an unallocated packet until it returns no more frames.
  //      return (got_frame != 0);
  //    }

  //    // Read the next packet for the next call to avcodec_decode_video2.
  //    do
  //    {
  //      // Unref the old packet
  //      ff.av_packet_unref(pkt);
  //      // Get the next one
  //      int ret = ff.av_read_frame(fmt_ctx, pkt);
  //      if (ret == AVERROR_EOF)
  //      {
  //        // No more packets. End of file. Enter draining mode.
  //        DEBUG_FFMPEG("No more packets. End of file.");
  //        ff.av_packet_unref(pkt);
  //        endOfFile = true;
  //      }
  //      else if (ret < 0)
  //      {
  //        setDecodingError(QStringLiteral("Error reading packet (av_read_frame). Return code %1").arg(ret));
  //        return false;
  //      }
  //    } while (!endOfFile && ff.AVPacketGetStreamIndex(pkt) != videoStreamIdx);
  //  } while (!got_frame);

  //  DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s",
  //    ff.AVFrameGetWidth(frame),
  //    ff.AVFrameGetHeight(frame),
  //    ff.AVFrameGetPTS(frame),
  //    ff.AVFrameGetPictureType(frame),
  //    ff.AVFrameGetKeyFrame(frame) ? "key frame" : "");
  //  return true;
  }

  //// First, try if there is a frame waiting in the decoder
  //int retRecieve = ff.avcodec_receive_frame(decCtx, frame);
  //if (retRecieve == 0)
  //{
  //  // We recieved a frame.
  //  // Recieved a frame
  //  DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s",
  //    ff.AVFrameGetWidth(frame),
  //    ff.AVFrameGetHeight(frame),
  //    ff.AVFrameGetPTS(frame),
  //    ff.AVFrameGetPictureType(frame),
  //    ff.AVFrameGetKeyFrame(frame) ? "key frame" : "");
  //  return true;
  //}
  //if (retRecieve < 0 && retRecieve != AVERROR(EAGAIN) && retRecieve != -35)
  //{
  //  // An error occured
  //  setDecodingError(QStringLiteral("Error recieving frame (avcodec_receive_frame)"));
  //  return false;
  //}
}

yuvPixelFormat FFmpegLibraries::getYUVPixelFormat()
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

bool FFmpegLibraries::loadFFmpegLibraries()
{
  if (librariesLoaded)
    return true;

  // Try to load the ffmpeg libraries from the current working directory and several other directories.
  // Unfortunately relative paths like "./" do not work: (at least on windows)

  // First try the specific FFMpeg libraries (if set)
  QSettings settings;
  settings.beginGroup("Decoders");
  QString avFormatLib = settings.value("FFMpeg.avformat", "").toString();
  QString avCodecLib = settings.value("FFMpeg.avcodec", "").toString();
  QString avUtilLib = settings.value("FFMpeg.avutil", "").toString();
  QString swResampleLib = settings.value("FFMpeg.swresample", "").toString();
  librariesLoaded = ff.loadFFMpegLibrarySpecific(avFormatLib, avCodecLib, avUtilLib, swResampleLib);
  if (librariesLoaded)
    return true;
  
  // Next, we will try some other paths / options
  QStringList possibilites;
  QString decoderSearchPath = settings.value("SearchPath", "").toString();
  if (!decoderSearchPath.isEmpty())
    possibilites.append(decoderSearchPath);                                   // Try the specific search path (if one is set)
  possibilites.append(QDir::currentPath() + "/");                             // Try the current working directory
  possibilites.append(QDir::currentPath() + "/ffmpeg/");
  possibilites.append(QCoreApplication::applicationDirPath() + "/");          // Try the path of the YUView.exe
  possibilites.append(QCoreApplication::applicationDirPath() + "/ffmpeg/");
  possibilites.append("");                                                    // Just try to call QLibrary::load so that the system folder will be searched.

  for (QString path : possibilites)
  {
    librariesLoaded = ff.loadFFmpegLibraryInPath(path);
    if (librariesLoaded)
      return true;
  }

  // Loading the libraries failed
  librariesLoadingError = true;
  return false;
}

//bool FFmpegLibraries::scanBitstream()
//{
//  // Seek to the beginning of the stream.
//  // The stream should be at the beginning when calling this function, but it does not hurt.
//  int ret = ff.seek_frame(fmt_ctx, video_stream.get_index(), 0);
//  if (ret != 0)
//    // Seeking failed. Maybe the stream is not opened correctly?
//    return false;
//
//  // Show a modal QProgressDialog while this operation is running.
//  // If the user presses cancel, we will cancel and return false (opening the file failed).
//  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
//  QWidgetList l = QApplication::topLevelWidgets();
//  QWidget *mainWindow = nullptr;
//  for (QWidget *w : l)
//  {
//    MainWindow *mw = dynamic_cast<MainWindow*>(w);
//    if (mw)
//      mainWindow = mw;
//  }
//  // Create the dialog
//  int64_t duration = fmt_ctx.get_duration();
//  AVRational timeBase = video_stream.get_time_base();
//
//  int64_t maxPTS = duration * timeBase.den / timeBase.num / 1000;
//  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
//  int curPercentValue = 0;
//  QProgressDialog progress("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow);
//  progress.setMinimumDuration(1000);  // Show after 1s
//  progress.setAutoClose(false);
//  progress.setAutoReset(false);
//  progress.setWindowModality(Qt::WindowModal);
//
//  // Initialize an empty packet (data and size set to 0).
//  AVPacketWrapper p(ff);
//
//  int64_t lastKeyFramePTS = 0;
//  const int stream_idx = video_stream.get_index();
//  do
//  {
//    // Get one packet
//    ret = fmt_ctx.read_frame(ff, p);
//
//    if (ret == 0 && p.get_stream_index() == stream_idx)
//    {
//      int64_t pts = p.get_pts();
//
//      // Next video frame found
//      if (p.get_flags() & AV_PKT_FLAG_KEY)
//      {
//        if (nrFrames == -1)
//          nrFrames = 0;
//        keyFrameList.append(pictureIdx(nrFrames, pts));
//        lastKeyFramePTS = pts;
//      }
//      if (pts < lastKeyFramePTS)
//      {
//        // What now? Can this happen? If this happens, the frame count/PTS combination of the last key frame
//        // is wrong.
//        keyFrameList.clear();
//        nrFrames = -1;
//        // Free the packet (this will automatically unref the packet as weel)
//        p.unref_packet(ff);
//        return false;
//      }
//      nrFrames++;
//
//      // Update the progress dialog
//      if (progress.wasCanceled())
//      {
//        keyFrameList.clear();
//        nrFrames = -1;
//        // Free the packet (this will automatically unref the packet as weel)
//        p.unref_packet(ff);
//        return false;
//      }
//      int newPercentValue = pts * 100 / maxPTS;
//      if (newPercentValue != curPercentValue)
//      {
//        progress.setValue(newPercentValue);
//        curPercentValue = newPercentValue;
//      }
//    }
//
//    // Unref the packet
//    p.unref_packet(ff);
//  } while (ret == 0);
//
//  progress.close();
//
//  // Seek back to the beginning of the stream.
//  ret = ff.seek_frame(fmt_ctx, video_stream.get_index(), 0);
//  if (ret != 0)
//    // Seeking failed.
//    return false;
//
//  return true;
//}

QString FFmpegLibraries::decoderErrorString() const
{
  QString retError = errorString;
  for (QString e : ff.getErrors())
    retError += e + "\n";
  return retError;
}

QList<infoItem> FFmpegLibraries::getDecoderInfo() const
{
  QList<infoItem> retList;

  retList.append(infoItem("Lib Path", ff.getLibPath(), "The library was loaded from this path."));
  retList.append(infoItem("Lib Version", ff.getLibVersionString(), "The version of the loaded libraries"));
  if (decCtx)
    retList.append(infoItem("Codec", decCtx.codec_id_string, "The codec of the stream that was opened"));

  return retList;
}

QByteArray FFmpegLibraries::loadYUVFrameData(int frameIdx)
{
  // TODO: Do this
  Q_UNUSED(frameIdx);

  //// At first check if the request is for the frame that has been requested in the
  //// last call to this function.
  //if (frameIdx == currentOutputBufferFrameIndex)
  //{
  //  assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
  //  return currentOutputBuffer;
  //}

  //// We have to decode the requested frame.
  //if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  //{
  //  // The requested frame lies before the current one. We will have to rewind and start decoding from there.
  //  pictureIdx seekFrameIdxAndPTS = getClosestSeekableFrameNumberBefore(frameIdx);

  //  DEBUG_FFMPEG("FFmpegLibraries::loadYUVData Seek to frame %lld PTS %lld", seekFrameIdxAndPTS.frame, seekFrameIdxAndPTS.pts);
  //  seekToPTS(seekFrameIdxAndPTS.pts);
  //  currentOutputBufferFrameIndex = seekFrameIdxAndPTS.frame - 1;
  //}
  //else if (frameIdx > currentOutputBufferFrameIndex+2)
  //{
  //  // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
  //  // Check if there is a random access point closer to the requested frame than the position that we are at right now.
  //  pictureIdx seekFrameIdxAndPTS = getClosestSeekableFrameNumberBefore(frameIdx);
  //  if (seekFrameIdxAndPTS.frame > currentOutputBufferFrameIndex)
  //  {
  //    // Yes we can (and should) seek ahead in the file
  //    DEBUG_FFMPEG("FFmpegLibraries::loadYUVData Seek to frame %lld PTS %lld", seekFrameIdxAndPTS.frame, seekFrameIdxAndPTS.pts);
  //    seekToPTS(seekFrameIdxAndPTS.pts);
  //    currentOutputBufferFrameIndex = seekFrameIdxAndPTS.frame - 1;
  //  }
  //}

  //// Perform the decoding right now blocking the main thread.
  //// Decode frames until we receive the one we are looking for.
  //while (decodeOneFrame())
  //{
  //  currentOutputBufferFrameIndex++;

  //  // We have decoded one frame. Get the pixel format.
  //  if (currentOutputBufferFrameIndex == frameIdx)
  //  {
  //    // This is the frame that we want to decode

  //    // Put image data into buffer
  //    copyFrameToOutputBuffer();

  //    // Get the motion vectors from the image as well...
  //    // TODO: Only perform this if the statistics are shown. 
  //    copyFrameMotionInformation();
  //    statsCacheCurFrameIdx = currentOutputBufferFrameIndex;

  //    return currentOutputBuffer;
  //  }
  //}

  return QByteArray();
}

void FFmpegLibraries::copyFrameToOutputBuffer()
{
  DEBUG_FFMPEG("FFmpegLibraries::copyFrameToOutputBuffer frame %d", currentOutputBufferFrameIndex);

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
  uint8_t *src = frame.get_data(0);
  int linesize = frame.get_line_size(0);
  char* dst = currentOutputBuffer.data();
  int wDst = frameSize.width() * nrBytesPerSample;
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
  wDst = frameSize.width() / pixFmt.getSubsamplingHor() * nrBytesPerSample;
  hDst = frameSize.height() / pixFmt.getSubsamplingVer();
  for (int c = 0; c < 2; c++)
  {
    uint8_t *src = frame.get_data(1+c);
    linesize = frame.get_line_size(1+c);
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

void FFmpegLibraries::copyFrameMotionInformation()
{
  DEBUG_FFMPEG("FFmpegLibraries::copyFrameMotionInformation frame %d", currentOutputBufferFrameIndex);
  
  // Clear the local statistics cache
  curFrameStats.clear();

  // Try to get the motion information
  AVFrameSideDataWrapper sideData = ff.get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
  if (sideData)
  {
    int nrMVs = sideData.get_number_motion_vectors();
    for (int i = 0; i < nrMVs; i++)
    {
      AVMotionVectorWrapper mv = sideData.get_motion_vector(i);
      if (mv)
      {
        // dst marks the center of the current block so the block position is:
        int blockX = mv.dst_x - mv.w/2;
        int blockY = mv.dst_y - mv.h/2;
        int16_t mvX = mv.dst_x - mv.src_x;
        int16_t mvY = mv.dst_y - mv.src_y;

        curFrameStats[mv.source < 0 ? 0 : 1].addBlockValue(blockX, blockY, mv.w, mv.h, (int)mv.source);
        curFrameStats[mv.source < 0 ? 2 : 3].addBlockVector(blockX, blockY, mv.w, mv.h, mvX, mvY);
      }
    }
  }
}

QByteArray FFmpegLibraries::getVideoContextExtradata()
{
  // Get the video stream
  if (!video_stream)
    return QByteArray();
  AVCodecContextWrapper codec = video_stream.getCodec();
  if (!codec)
    return QByteArray();
  return codec.get_extradata();
}

bool FFmpegLibraries::seekToPTS(int64_t pts)
{
  int ret = ff.seek_frame(fmt_ctx, video_stream.get_index(), pts);
  if (ret != 0)
  {
    DEBUG_FFMPEG("FFmpegLibraries::seekToPTS Error PTS %ld. Return Code %d", pts, ret);
    return false;
  }
    
  //// Flush the video decoder buffer
  //ff.flush_buffers(decCtx);

  // We seeked somewhere, so we are not at the end of the file anymore.
  endOfFile = false;

  DEBUG_FFMPEG("FFmpegLibraries::seekToPTS Successfully seeked to PTS %d", pts);
  return true;
}

statisticsData FFmpegLibraries::getStatisticsData(int frameIdx, int typeIdx)
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

bool FFmpegLibraries::checkLibraryFiles(QString avCodecLib, QString avFormatLib, QString avUtilLib, QString swResampleLib, QString & error)
{
  QStringList errors;
  bool result = FFmpegVersionHandler::checkLibraryFiles(avCodecLib, avFormatLib, avUtilLib, swResampleLib, errors);
  for (QString e : errors)
    error += e + "\n";
  return result;
}

void FFmpegLibraries::getFormatInfo()
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

int64_t FFmpegLibraries::getMaxPTS()
{
  return duration * timeBase.den / timeBase.num / 1000;
}

bool FFmpegLibraries::goToNextVideoPacket()
{
  // Load the next video stream packet into the packet buffer
  int ret = 0;
  do
  {
    if (pkt)
      // Unref the packet
      pkt.unref_packet(ff);

    ret = fmt_ctx.read_frame(ff, pkt);
  }
  while (ret == 0 && pkt.get_stream_index() != video_stream.get_index());

  if (ret < 0)
  {
    endOfFile = true;
    return false;
  }
  return true;
}

avPacketInfo_t FFmpegLibraries::getPacketInfo()
{
  avPacketInfo_t info;
  info.stream_index = pkt.get_stream_index();
  info.pts = pkt.get_pts();
  info.dts = pkt.get_dts();
  info.duration = pkt.get_duration();
  info.flag_keyframe = pkt.get_flag_keyframe();
  info.flag_corrupt = pkt.get_flag_corrupt();
  info.flag_discard = pkt.get_flag_discard();
  info.data_size = pkt.get_data_size();
  info.data = pkt.get_data();
  return info;
}