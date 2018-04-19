///*  This file is part of YUView - The YUV player with advanced analytics toolset
//*   <https://github.com/IENT/YUView>
//*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
//*
//*   This program is free software; you can redistribute it and/or modify
//*   it under the terms of the GNU General Public License as published by
//*   the Free Software Foundation; either version 3 of the License, or
//*   (at your option) any later version.
//*
//*   In addition, as a special exception, the copyright holders give
//*   permission to link the code of portions of this program with the
//*   OpenSSL library under certain conditions as described in each
//*   individual source file, and distribute linked combinations including
//*   the two.
//*
//*   You must obey the GNU General Public License in all respects for all
//*   of the code used other than OpenSSL. If you modify file(s) with this
//*   exception, you may extend this exception to your version of the
//*   file(s), but you are not obligated to do so. If you do not wish to do
//*   so, delete this exception statement from your version. If you delete
//*   this exception statement from all source files in the program, then
//*   also delete it here.
//*
//*   This program is distributed in the hope that it will be useful,
//*   but WITHOUT ANY WARRANTY; without even the implied warranty of
//*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//*   GNU General Public License for more details.
//*
//*   You should have received a copy of the GNU General Public License
//*   along with this program. If not, see <http://www.gnu.org/licenses/>.
//*/
//
//#include "FFmpegLibraries.h"
//
//#include <cstring>
//#include <QCoreApplication>
//#include <QDateTime>
//#include <QDir>
//#include <QProgressDialog>
//#include <QSettings>
//#include "mainwindow.h"
//#include "typedef.h"
//
//using namespace FFmpeg;
//
//#define DECODERFFMPEGLIBRARIES_DEBUG_OUTPUT 0
//#if DECODERFFMPEGLIBRARIES_DEBUG_OUTPUT && !NDEBUG
//#include <QDebug>
//#define DEBUG_FFMPEG qDebug
//#else
//#define DEBUG_FFMPEG(fmt,...) ((void)0)
//#endif
//
//FFmpegLibraries::FFmpegLibraries()
//{
//  // No error (yet)
//  librariesLoadingError = false;
//  readingFileError = false;
//
//  librariesLoaded = false;
//  
//  // Set default values
//  pixelFormat = AV_PIX_FMT_NONE;
//  
//  endOfFile = false;
//  frameRate = -1;
//  colorConversionType = BT709_LimitedRange;
//  duration = -1;
//  timeBase.num = 0;
//  timeBase.den = 0;
//}
//
//FFmpegLibraries::~FFmpegLibraries()
//{
//  // Free all the allocated data structures
//  if (pkt)
//    pkt.free_packet();
//  /*if (decCtx)
//    ff.avcodec_free_context(&decCtx);*/
//  if (frame)
//    frame.free_frame(ff);
//  fmt_ctx.avformat_close_input(ff);
//}
//
//bool FFmpegLibraries::openFile(QString fileName)
//{
//  
//}
//

//
////QString FFmpegLibraries::decoderErrorString() const
////{
////  QString retError = errorString;
////  for (QString e : ff.getErrors())
////    retError += e + "\n";
////  return retError;
////}
//
////QList<infoItem> FFmpegLibraries::getDecoderInfo() const
////{
////  QList<infoItem> retList;
////
////  retList.append(infoItem("Lib Path", ff.getLibPath(), "The library was loaded from this path."));
////  retList.append(infoItem("Lib Version", ff.getLibVersionString(), "The version of the loaded libraries"));
////  if (decCtx)
////    retList.append(infoItem("Codec", decCtx.codec_id_string, "The codec of the stream that was opened"));
////
////  return retList;
////}
//
////QByteArray FFmpegLibraries::loadYUVFrameData(int frameIdx)
////{
////  // TODO: Do this
////  Q_UNUSED(frameIdx);
////
////  //// At first check if the request is for the frame that has been requested in the
////  //// last call to this function.
////  //if (frameIdx == currentOutputBufferFrameIndex)
////  //{
////  //  assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
////  //  return currentOutputBuffer;
////  //}
////
////  //// We have to decode the requested frame.
////  //if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
////  //{
////  //  // The requested frame lies before the current one. We will have to rewind and start decoding from there.
////  //  pictureIdx seekFrameIdxAndPTS = getClosestSeekableFrameNumberBeforeBefore(frameIdx);
////
////  //  DEBUG_FFMPEG("FFmpegLibraries::loadYUVData Seek to frame %lld PTS %lld", seekFrameIdxAndPTS.frame, seekFrameIdxAndPTS.pts);
////  //  seekToPTS(seekFrameIdxAndPTS.pts);
////  //  currentOutputBufferFrameIndex = seekFrameIdxAndPTS.frame - 1;
////  //}
////  //else if (frameIdx > currentOutputBufferFrameIndex+2)
////  //{
////  //  // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
////  //  // Check if there is a random access point closer to the requested frame than the position that we are at right now.
////  //  pictureIdx seekFrameIdxAndPTS = getClosestSeekableFrameNumberBeforeBefore(frameIdx);
////  //  if (seekFrameIdxAndPTS.frame > currentOutputBufferFrameIndex)
////  //  {
////  //    // Yes we can (and should) seek ahead in the file
////  //    DEBUG_FFMPEG("FFmpegLibraries::loadYUVData Seek to frame %lld PTS %lld", seekFrameIdxAndPTS.frame, seekFrameIdxAndPTS.pts);
////  //    seekToPTS(seekFrameIdxAndPTS.pts);
////  //    currentOutputBufferFrameIndex = seekFrameIdxAndPTS.frame - 1;
////  //  }
////  //}
////
////  //// Perform the decoding right now blocking the main thread.
////  //// Decode frames until we receive the one we are looking for.
////  //while (decodeOneFrame())
////  //{
////  //  currentOutputBufferFrameIndex++;
////
////  //  // We have decoded one frame. Get the pixel format.
////  //  if (currentOutputBufferFrameIndex == frameIdx)
////  //  {
////  //    // This is the frame that we want to decode
////
////  //    // Put image data into buffer
////  //    copyFrameToOutputBuffer();
////
////  //    // Get the motion vectors from the image as well...
////  //    // TODO: Only perform this if the statistics are shown. 
////  //    copyFrameMotionInformation();
////  //    statsCacheCurFrameIdx = currentOutputBufferFrameIndex;
////
////  //    return currentOutputBuffer;
////  //  }
////  //}
////
////  return QByteArray();
////}
//
////void FFmpegLibraries::copyFrameToOutputBuffer()
////{
////  DEBUG_FFMPEG("FFmpegLibraries::copyFrameToOutputBuffer frame %d", currentOutputBufferFrameIndex);
////
////  // At first get how many bytes we are going to write
////  yuvPixelFormat pixFmt = getYUVPixelFormat();
////  int nrBytesPerSample = pixFmt.bitsPerSample <= 8 ? 1 : 2;
////  int nrBytesY = frameSize.width() * frameSize.height() * nrBytesPerSample;
////  int nrBytesC = frameSize.width() / pixFmt.getSubsamplingHor() * frameSize.height() / pixFmt.getSubsamplingVer() * nrBytesPerSample;
////  int nrBytes = nrBytesY + 2 * nrBytesC;
////
////  // Is the output big enough?
////  if (currentOutputBuffer.capacity() < nrBytes)
////    currentOutputBuffer.resize(nrBytes);
////
////  // Copy line by line. The linesize of the source may be larger than the width of the frame.
////  // This may be because the frame buffer is (8) byte aligned. Also the internal decoded
////  // resolution may be larger than the output frame size.
////  uint8_t *src = frame.get_data(0);
////  int linesize = frame.get_line_size(0);
////  char* dst = currentOutputBuffer.data();
////  int wDst = frameSize.width() * nrBytesPerSample;
////  int hDst = frameSize.height();
////  for (int y = 0; y < hDst; y++)
////  {
////    // Copy one line
////    memcpy(dst, src, wDst);
////    // Goto the next line in input and output (these offsets/strides may differ)
////    dst += wDst;
////    src += linesize;
////  }
////
////  // Chroma
////  wDst = frameSize.width() / pixFmt.getSubsamplingHor() * nrBytesPerSample;
////  hDst = frameSize.height() / pixFmt.getSubsamplingVer();
////  for (int c = 0; c < 2; c++)
////  {
////    uint8_t *src = frame.get_data(1+c);
////    linesize = frame.get_line_size(1+c);
////    dst = currentOutputBuffer.data();
////    dst += (nrBytesY + ((c == 0) ? 0 : nrBytesC));
////    for (int y = 0; y < hDst; y++)
////    {
////      memcpy(dst, src, wDst);
////      // Goto the next line
////      dst += wDst;
////      src += linesize;
////    }
////  }
////}
////
////void FFmpegLibraries::copyFrameMotionInformation()
////{
////  DEBUG_FFMPEG("FFmpegLibraries::copyFrameMotionInformation frame %d", currentOutputBufferFrameIndex);
////  
////  // Clear the local statistics cache
////  curFrameStats.clear();
////
////  // Try to get the motion information
////  AVFrameSideDataWrapper sideData = ff.get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
////  if (sideData)
////  {
////    int nrMVs = sideData.get_number_motion_vectors();
////    for (int i = 0; i < nrMVs; i++)
////    {
////      AVMotionVectorWrapper mv = sideData.get_motion_vector(i);
////      if (mv)
////      {
////        // dst marks the center of the current block so the block position is:
////        int blockX = mv.dst_x - mv.w/2;
////        int blockY = mv.dst_y - mv.h/2;
////        int16_t mvX = mv.dst_x - mv.src_x;
////        int16_t mvY = mv.dst_y - mv.src_y;
////
////        curFrameStats[mv.source < 0 ? 0 : 1].addBlockValue(blockX, blockY, mv.w, mv.h, (int)mv.source);
////        curFrameStats[mv.source < 0 ? 2 : 3].addBlockVector(blockX, blockY, mv.w, mv.h, mvX, mvY);
////      }
////    }
////  }
////}
//
//

//
////statisticsData FFmpegLibraries::getStatisticsData(int frameIdx, int typeIdx)
////{
////  if (frameIdx != statsCacheCurFrameIdx)
////  {
////    if (currentOutputBufferFrameIndex == frameIdx)
////      // We will have to decode the current frame again to get the internals/statistics
////      // This can be done like this:
////      currentOutputBufferFrameIndex ++;
////
////    loadYUVFrameData(frameIdx);
////  }
////
////  return curFrameStats[typeIdx];
////}
//
//bool FFmpegLibraries::checkLibraryFiles(QString avCodecLib, QString avFormatLib, QString avUtilLib, QString swResampleLib, QString & error)
//{
//  QStringList errors;
//  bool result = FFmpegVersionHandler::checkLibraryFiles(avCodecLib, avFormatLib, avUtilLib, swResampleLib, errors);
//  for (QString e : errors)
//    error += e + "\n";
//  return result;
//}
//
//void FFmpegLibraries::getFormatInfo()
//{
//  /*QString out;
//
//  int index = 0;
//  AVFormatContext *ic = fmt_ctx;*/
//
//  //out.append(QString("Input %1, %2\n").arg(index).arg(ic->iformat->name));
//
//  ////dump_metadata(NULL, ic->metadata, "  ");
//  //if (ic->duration != AV_NOPTS_VALUE)
//  //{
//  //  int hours, mins, secs, us;
//  //  int64_t duration = ic->duration + 5000;
//  //  secs  = duration / AV_TIME_BASE;
//  //  us    = duration % AV_TIME_BASE;
//  //  mins  = secs / 60;
//  //  secs %= 60;
//  //  hours = mins / 60;
//  //  mins %= 60;
//  //  out.append(QString("  Duration: %1:%2:%3.%4\n").arg(hours).arg(mins).arg(secs).arg((100 * us) / AV_TIME_BASE));
//  //}
//  //else
//  //{
//  //  out.append(QString("  Duration: N/A\n"));
//  //}
//
//  //if (ic->start_time != AV_NOPTS_VALUE)
//  //{
//  //  int secs, us;
//  //  secs = ic->start_time / AV_TIME_BASE;
//  //  us   = abs(ic->start_time % AV_TIME_BASE);
//  //  out.append(QString("  Start: %1.%2\n").arg(secs).arg(us));
//  //}
//  //
//  //if (ic->bit_rate)
//  //  out.append(QString("  Bitrate: %1 kb/s\n").arg(ic->bit_rate / 1000));
//  //else
//  //  out.append(QString("  Bitrate: N/A kb/s\n"));
//  //
//  //for (unsigned int i = 0; i < ic->nb_chapters; i++)
//  //{
//  //  AVChapter *ch = ic->chapters[i];
//  //  double start = ch->start * ch->time_base.num / double(ch->time_base.den);
//  //  double end   = ch->end   * ch->time_base.num / double(ch->time_base.den);
//  //  out.append(QString("  Chapter #%1:%2 start %3, end %4\n").arg(index).arg(i).arg(start).arg(end));
//
//  //  //dump_metadata(NULL, ch->metadata, "    ");
//  //}
//
//  //for (unsigned int i = 0; i < ic->nb_streams; i++)
//  //{
//  //  // Get the stream format of stream i
//  //  //char buf[256];
//  //  //int flags = ic->iformat->flags;
//  //  //AVStream *st = ic->streams[i];
//
//  //  // ...
//  //
//  //}
//}
//
//int64_t FFmpegLibraries::getMaxPTS()
//{
//  return duration * timeBase.den / timeBase.num / 1000;
//}
//
//bool FFmpegLibraries::goToNextVideoPacket()
//{
//  // Load the next video stream packet into the packet buffer
//  int ret = 0;
//  do
//  {
//    if (pkt)
//      // Unref the packet
//      pkt.unref_packet(ff);
//
//    ret = fmt_ctx.read_frame(ff, pkt);
//  }
//  while (ret == 0 && pkt.get_stream_index() != video_stream.get_index());
//
//  if (ret < 0)
//  {
//    endOfFile = true;
//    return false;
//  }
//  return true;
//}
