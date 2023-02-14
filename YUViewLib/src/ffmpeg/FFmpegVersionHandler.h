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

#pragma once

#include "AVCodecContextWrapper.h"
#include "AVCodecIDWrapper.h"
#include "AVCodecParametersWrapper.h"
#include "AVCodecWrapper.h"
#include "AVFormatContextWrapper.h"
#include "AVFrameSideDataWrapper.h"
#include "AVFrameWrapper.h"
#include "AVPacketWrapper.h"
#include "AVPixFmtDescriptorWrapper.h"
#include "FFMpegLibrariesTypes.h"
#include "FFmpegLibraryFunctions.h"
#include <common/Typedef.h>

namespace FFmpeg
{

class FFmpegVersionHandler
{
public:
  FFmpegVersionHandler();

  virtual ~FFmpegVersionHandler();

  // Try to load the ffmpeg libraries and get all the function pointers.
  void loadFFmpegLibraries();
  bool loadingSuccessfull() const;

  QStringList getLibPaths() const { return lib.getLibPaths(); }
  QString     getLibVersionString() const;

  // Only these functions can be used to get valid versions of these wrappers (they have to use
  // ffmpeg functions to retrieve the needed information)
  AVCodecIDWrapper          getCodecIDWrapper(AVCodecID id);
  AVCodecID                 getCodecIDFromWrapper(AVCodecIDWrapper &wrapper);
  AVPixFmtDescriptorWrapper getAvPixFmtDescriptionFromAvPixelFormat(AVPixelFormat pixFmt);
  AVPixelFormat             getAVPixelFormatFromPixelFormatYUV(video::yuv::PixelFormatYUV pixFmt);

  AVFrameWrapper  allocateFrame();
  void            freeFrame(AVFrameWrapper &frame);
  AVPacketWrapper allocatePaket();
  void            unrefPacket(AVPacketWrapper &packet);
  void            freePacket(AVPacketWrapper &packet);

  bool configureDecoder(AVCodecContextWrapper &decCtx, AVCodecParametersWrapper &codecpar);

  // Push a packet to the given decoder using avcodec_send_packet
  int pushPacketToDecoder(AVCodecContextWrapper &decCtx, AVPacketWrapper &pkt);
  // Retrive a frame using avcodec_receive_frame
  int getFrameFromDecoder(AVCodecContextWrapper &decCtx, AVFrameWrapper &frame);

  void flush_buffers(AVCodecContextWrapper &decCtx);

  LibraryVersion libVersion;

  // Open the input file. This will call avformat_open_input and avformat_find_stream_info.
  bool openInput(AVFormatContextWrapper &fmt, QString url);
  // Try to find a decoder for the given codecID
  AVCodecWrapper findDecoder(AVCodecIDWrapper codecID);
  // Allocate the decoder (avcodec_alloc_context3)
  AVCodecContextWrapper allocDecoder(AVCodecWrapper &codec);
  // Set info in the dictionary
  int dictSet(AVDictionaryWrapper &dict, const char *key, const char *value, int flags);
  // Get all entries with the given key (leave empty for all)
  StringPairVec getDictionaryEntries(AVDictionaryWrapper d, QString key, int flags);
  // Allocate a new set of codec parameters
  AVCodecParametersWrapper allocCodecParameters();

  // Open the codec
  int avcodecOpen2(AVCodecContextWrapper &decCtx, AVCodecWrapper &codec, AVDictionaryWrapper &dict);
  // Get side/meta data
  AVFrameSideDataWrapper getSideData(AVFrameWrapper &frame, AVFrameSideDataType type);
  AVDictionaryWrapper    getMetadata(AVFrameWrapper &frame);

  // Seek to a specific frame
  int seekFrame(AVFormatContextWrapper &fmt, int stream_idx, int64_t dts);
  int seekBeginning(AVFormatContextWrapper &fmt);

  // All the function pointers of the ffmpeg library
  FFmpegLibraryFunctions lib;

  static AVPixelFormat convertYUVAVPixelFormat(video::yuv::PixelFormatYUV fmt);
  // Check if the given four files can be used to open FFmpeg.
  static bool checkLibraryFiles(QString      avCodecLib,
                                QString      avFormatLib,
                                QString      avUtilLib,
                                QString      swResampleLib,
                                QStringList &logging);

  // Logging. By default we set the logging level of ffmpeg to AV_LOG_ERROR (Log errors and
  // everything worse)
  static QStringList getFFmpegLog() { return logListFFmpeg; }
  void               enableLoggingWarning();

  QStringList getLog() const { return logList; }

private:
  // Try to load the FFmpeg libraries from the given path.
  // Try the system paths if no path is provided. This function can be called multiple times.
  bool loadFFmpegLibraryInPath(QString path);
  // Try to load the four specific library files
  bool loadFFMpegLibrarySpecific(QString avFormatLib,
                                 QString avCodecLib,
                                 QString avUtilLib,
                                 QString swResampleLib);
  bool librariesLoaded{};

  // Log what is happening when loading the libraries / opening files.
  void        log(QString message) { logList.append(message); }
  QStringList logList{};

  // FFmpeg has a callback where it loggs stuff. This log goes here.
  static QStringList logListFFmpeg;
  static void        avLogCallback(void *ptr, int level, const char *fmt, va_list vargs);
};

} // namespace FFmpeg
