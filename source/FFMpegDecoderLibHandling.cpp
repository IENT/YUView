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

#include "FFMpegDecoderLibHandling.h"

#include <QDir>
#include "typedef.h"

FFmpegLibraryFunctions::FFmpegLibraryFunctions()
{
  av_register_all = nullptr;
  avformat_open_input = nullptr;
  avformat_close_input = nullptr;
  avformat_find_stream_info = nullptr;
  av_read_frame = nullptr;
  av_seek_frame = nullptr;
  avformat_version = nullptr;

  avcodec_find_decoder = nullptr;
  avcodec_alloc_context3 = nullptr;
  avcodec_open2 = nullptr;
  avcodec_free_context = nullptr;
  av_init_packet = nullptr;
  av_packet_unref = nullptr;
  avcodec_flush_buffers = nullptr;
  avcodec_version = nullptr;

  avcodec_send_packet = nullptr;
  avcodec_receive_frame = nullptr;
  avcodec_parameters_to_context = nullptr;
  newParametersAPIAvailable = false;
  avcodec_decode_video2 = nullptr;

  av_frame_alloc = nullptr;
  av_frame_free = nullptr;
  av_mallocz = nullptr;
  avutil_version = nullptr;

  swresample_version = nullptr;
}

bool FFmpegLibraryFunctions::bindFunctionsFromAVFormatLib()
{
  if (!resolveAvFormat(av_register_all, "av_register_all")) return false;
  if (!resolveAvFormat(avformat_open_input, "avformat_open_input")) return false;
  if (!resolveAvFormat(avformat_close_input, "avformat_close_input")) return false;
  if (!resolveAvFormat(avformat_find_stream_info, "avformat_find_stream_info")) return false;
  if (!resolveAvFormat(av_read_frame, "av_read_frame")) return false;
  if (!resolveAvFormat(av_seek_frame, "av_seek_frame")) return false;
  if (!resolveAvFormat(avformat_version, "avformat_version")) return false;
  return true;
}

bool FFmpegLibraryFunctions::bindFunctionsFromAVCodecLib()
{
  if (!resolveAvCodec(avcodec_find_decoder, "avcodec_find_decoder")) return false;
  if (!resolveAvCodec(avcodec_alloc_context3, "avcodec_alloc_context3")) return false;
  if (!resolveAvCodec(avcodec_open2, "avcodec_open2")) return false;
  if (!resolveAvCodec(avcodec_free_context, "avcodec_free_context")) return false;
  if (!resolveAvCodec(av_init_packet, "av_init_packet")) return false;
  if (!resolveAvCodec(av_packet_unref, "av_packet_unref")) return false;
  if (!resolveAvCodec(avcodec_flush_buffers, "avcodec_flush_buffers")) return false;
  if (!resolveAvCodec(avcodec_version, "avcodec_version")) return false;
  if (!resolveAvCodec(avcodec_get_name, "avcodec_get_name")) return false;

  // The following functions are part of the new API. If they are not available, we use the old API.
  // If available, we should however use it.
  newParametersAPIAvailable = true;
  newParametersAPIAvailable &= resolveAvCodec(avcodec_send_packet, "avcodec_send_packet", false);
  newParametersAPIAvailable &= resolveAvCodec(avcodec_receive_frame, "avcodec_receive_frame", false);
  newParametersAPIAvailable &= resolveAvCodec(avcodec_parameters_to_context, "avcodec_parameters_to_context", false);

  if (!newParametersAPIAvailable)
    // If the new API is not available, then the old one must be available.
    if (!resolveAvCodec(avcodec_decode_video2, "avcodec_decode_video2")) return false;

  return true;
}

bool FFmpegLibraryFunctions::bindFunctionsFromAVUtilLib()
{
  if (!resolveAvUtil(av_frame_alloc, "av_frame_alloc")) return false;
  if (!resolveAvUtil(av_frame_free, "av_frame_free")) return false;
  if (!resolveAvUtil(av_mallocz, "av_mallocz")) return false;
  if (!resolveAvUtil(avutil_version, "avutil_version")) return false;
  if (!resolveAvUtil(av_dict_set, "av_dict_set")) return false;
  if (!resolveAvUtil(av_frame_get_side_data, "av_frame_get_side_data")) return false;
  return true;
}

bool FFmpegLibraryFunctions::bindFunctionsFromSWResampleLib()
{
  return resolveSwresample(swresample_version, "swresample_version");
}

bool FFmpegLibraryFunctions::bindFunctionsFromLibraries()
{
  // Loading the libraries was successfull. Get/check function pointers.
  return bindFunctionsFromAVFormatLib() && bindFunctionsFromAVCodecLib() && bindFunctionsFromAVUtilLib() && bindFunctionsFromSWResampleLib();
}

QFunctionPointer FFmpegLibraryFunctions::resolveAvUtil(const char *symbol)
{
  QFunctionPointer ptr = libAvutil.resolve(symbol);
  if (!ptr)
    setLibraryError(QStringLiteral("Error loading the avutil library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> bool FFmpegLibraryFunctions::resolveAvUtil(T &fun, const char *symbol)
{
  fun = reinterpret_cast<T>(resolveAvUtil(symbol));
  return (fun != nullptr);
}

QFunctionPointer FFmpegLibraryFunctions::resolveAvFormat(const char *symbol)
{
  QFunctionPointer ptr = libAvformat.resolve(symbol);
  if (!ptr)
    setLibraryError(QStringLiteral("Error loading the avformat library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> bool FFmpegLibraryFunctions::resolveAvFormat(T &fun, const char *symbol)
{
  fun = reinterpret_cast<T>(resolveAvFormat(symbol));
  return (fun != nullptr);
}

QFunctionPointer FFmpegLibraryFunctions::resolveAvCodec(const char *symbol, bool failIsError)
{
  // Failure to resolve the function is only an error if failIsError is set.
  QFunctionPointer ptr = libAvcodec.resolve(symbol);
  if (!ptr && failIsError)
    setLibraryError(QStringLiteral("Error loading the avcodec library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> bool FFmpegLibraryFunctions::resolveAvCodec(T &fun, const char *symbol, bool failIsError)
{
  fun = reinterpret_cast<T>(resolveAvCodec(symbol, failIsError));
  return (fun != nullptr);
}

QFunctionPointer FFmpegLibraryFunctions::resolveSwresample(const char *symbol)
{
  QFunctionPointer ptr = libSwresample.resolve(symbol);
  if (!ptr)
    setLibraryError(QStringLiteral("Error loading the swresample library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> bool FFmpegLibraryFunctions::resolveSwresample(T &fun, const char *symbol)
{
  fun = reinterpret_cast<T>(resolveSwresample(symbol));
  return (fun != nullptr);
}

bool FFmpegLibraryFunctions::loadFFmpegLibraryInPath(QString path, int libVersions[4])
{
  // Clear the error state if one was set.
  errorString.clear();
  libAvutil.unload();
  libSwresample.unload();
  libAvcodec.unload();
  libAvformat.unload();

  // We will load the following libraries (in this order):
  // avutil, swresample, avcodec, avformat.

  if (!path.isEmpty())
  {
    QDir givenPath(path);
    if (!givenPath.exists())
      // The given path is invalid
      return false;

    // Get the absolute path
    path = givenPath.absolutePath() + "/";
  }

  libPath = path;

  // The ffmpeg libraries are named using a major version number. E.g: avutil-55.dll on windows.
  // On linux, the libraries may be named differently. On Ubuntu they are named libavutil-ffmpeg.so.55.
  // On arch linux the name is libavutil.so.55. We will try to look for both namings.
  // On MAC os (installed with homebrew), there is a link to the lib named libavutil.54.dylib.
  int nrNames = (is_Q_OS_LINUX) ? 2 : ((is_Q_OS_WIN) ? 1 : 1);
  bool success = false;
  for (int i=0; i<nrNames; i++)
  {
    success = true;

    // This is how we the library name is constructed per platform
    QString constructLibName;
    if (is_Q_OS_WIN)
      constructLibName = "%1-%2";
    if (is_Q_OS_LINUX && i == 0)
      constructLibName = "lib%1-ffmpeg.so.%2";
    if (is_Q_OS_LINUX && i == 1)
      constructLibName = "lib%1.so.%2";
    if (is_Q_OS_MAC)
      constructLibName = "lib%1.%2.dylib";
    
    // Start with the avutil library
    libAvutil.setFileName(path + constructLibName.arg("avutil").arg(libVersions[0]));
    if (!libAvutil.load())
      success = false;

    // Next, the swresample library.
    libSwresample.setFileName(path + constructLibName.arg("swresample").arg(libVersions[1]));
    if (success && !libSwresample.load())
      success = false;

    // avcodec
    libAvcodec.setFileName(path + constructLibName.arg("avcodec").arg(libVersions[2]));
    if (success && !libAvcodec.load())
      success = false;

    // avformat
    libAvformat.setFileName(path + constructLibName.arg("avformat").arg(libVersions[3]));
    if (success && !libAvformat.load())
      success = false;

    if (success)
      break;
    else
    {
      libAvutil.unload();
      libSwresample.unload();
      libAvcodec.unload();
      libAvformat.unload();
    }
  }

  if (!success)
    return false;

  // For the last test: Try to get pointers to all the libraries.
  return bindFunctionsFromLibraries();
}

bool FFmpegLibraryFunctions::checkLibraryFile(QString libFilePath, ffmpegLibrary libType, QString &error)
{
  FFmpegLibraryFunctions libFunc;
  if (libType == ffmpegLib_AVFormat)
  {
    libFunc.libAvformat.setFileName(libFilePath);
    if (!libFunc.libAvformat.load())
    {
      error = "Error opening QLibrary.";
      return false;
    }
    error = libFunc.errorString;
    return libFunc.bindFunctionsFromAVFormatLib();
  }
  if (libType == ffmpegLib_AVCodec)
  {
    libFunc.libAvcodec.setFileName(libFilePath);
    if (!libFunc.libAvcodec.load())
    {
      error = "Error opening QLibrary.";
      return false;
    }
    error = libFunc.errorString;
    return libFunc.bindFunctionsFromAVCodecLib();
  }
  if (libType == ffmpegLib_AVUtil)
  {
    libFunc.libAvutil.setFileName(libFilePath);
    if (!libFunc.libAvutil.load())
    {
      error = "Error opening QLibrary.";
      return false;
    }
    error = libFunc.errorString;
    return libFunc.bindFunctionsFromAVUtilLib();
  }
  if (libType == ffmpegLib_SWResample)
  {
    libFunc.libSwresample.setFileName(libFilePath);
    if (!libFunc.libSwresample.load())
    {
      error = "Error opening QLibrary.";
      return false;
    }
    error = libFunc.errorString;
    return libFunc.bindFunctionsFromSWResampleLib();
  }
  return false;
}

// ----------------- FFmpegVersionHandler -------------------------------------------

FFmpegVersionHandler::FFmpegVersionHandler()
{
  libVersion.avcodec = -1;
  libVersion.avformat = -1;
  libVersion.avutil = -1;
  libVersion.swresample = -1;
}

bool FFmpegVersionHandler::loadFFmpegLibraryInPath(QString path)
{
  bool success = false;
  for (int i = 0; i < FFMpegVersion_Num; i++)
  {
    FFmpegVersions v = (FFmpegVersions)i;

    int verNum[4];
    verNum[0] = getLibVersionUtil(v);
    verNum[1] = getLibVersionSwresample(v);
    verNum[2] = getLibVersionCodec(v);
    verNum[3] = getLibVersionFormat(v);

    if (FFmpegLibraryFunctions::loadFFmpegLibraryInPath(path, verNum))
    {
      // This worked. Get the version number of the opened libraries and check them against the
      // versions we tried to open. Also get the minor and micro version numbers.
      libVersion.avutil = verNum[0];
      libVersion.swresample = verNum[1];
      libVersion.avcodec = verNum[2];
      libVersion.avformat = verNum[3];

      int avCodecVer = avcodec_version();
      if (AV_VERSION_MAJOR(avCodecVer) != libVersion.avcodec)
      {
        versionErrorString = "The openend libAvCodec returned a different major version than it's file name indicates.";
        // Try the next version
        continue;
      }
      libVersion.avcodec_minor = AV_VERSION_MINOR(avCodecVer);
      libVersion.avcodec_micro = AV_VERSION_MICRO(avCodecVer);

      int avFormatVer = avformat_version();
      if (AV_VERSION_MAJOR(avFormatVer) != libVersion.avformat)
      {
        versionErrorString = "The openend libAvFormat returned a different major version than it's file name indicates.";
        // Try the next version
        continue;
      }
      libVersion.avformat_minor = AV_VERSION_MINOR(avFormatVer);
      libVersion.avformat_micro = AV_VERSION_MICRO(avFormatVer);

      int avUtilVer = avutil_version();
      if (AV_VERSION_MAJOR(avUtilVer) != libVersion.avutil)
      {
        versionErrorString = "The openend libAvUtil returned a different major version than it's file name indicates.";
        // Try the next version
        continue;
      }
      libVersion.avutil_minor = AV_VERSION_MINOR(avUtilVer);
      libVersion.avutil_micro = AV_VERSION_MICRO(avUtilVer);

      int swresampleVer = swresample_version();
      if (AV_VERSION_MAJOR(swresampleVer) != libVersion.swresample)
      {
        versionErrorString = "The openend libSwresampleVer returned a different major version than it's file name indicates.";
        // Try the next version
        continue;
      }
      libVersion.swresample_minor = AV_VERSION_MINOR(swresampleVer);
      libVersion.swresample_micro = AV_VERSION_MICRO(swresampleVer);

      // Everything worked. We can break the loop over all versions that we support.
      success = true;
      break;
    }
    else
    {
      versionErrorString = FFmpegLibraryFunctions::libErrorString();
    }
  }

  if (success)
    versionErrorString.clear();
  return success;
}

int FFmpegVersionHandler::getLibVersionUtil(FFmpegVersions ver)
{
  switch (ver)
  {
  case FFMpegVersion_54_56_56_1:
    return 54;
  case FFMpegVersion_55_57_57_2:
    return 55;
  default:
    assert(false);
  }
  return -1;
}

int FFmpegVersionHandler::getLibVersionCodec(FFmpegVersions ver)
{
  switch (ver)
  {
  case FFMpegVersion_54_56_56_1:
    return 56;
  case FFMpegVersion_55_57_57_2:
    return 57;
  default:
    assert(false);
  }
  return -1;
}

int FFmpegVersionHandler::getLibVersionFormat(FFmpegVersions ver)
{
  switch (ver)
  {
  case FFMpegVersion_54_56_56_1:
    return 56;
  case FFMpegVersion_55_57_57_2:
    return 57;
  default:
    assert(false);
  }
  return -1;
}

int FFmpegVersionHandler::getLibVersionSwresample(FFmpegVersions ver)
{
  switch (ver)
  {
  case FFMpegVersion_54_56_56_1:
    return 1;
  case FFMpegVersion_55_57_57_2:
    return 2;
  default:
    assert(false);
  }
  return -1;
}

AVPacket *FFmpegVersionHandler::getNewPacket()
{
  if (libVersion.avcodec == 56)
  {
    AVPacket_56 *pkt = new AVPacket_56;
    pkt->data = nullptr;
    pkt->size = 0;
    return reinterpret_cast<AVPacket*>(pkt);
  }
  else if (libVersion.avcodec == 57)
  {
    AVPacket_57 *pkt = new AVPacket_57;
    pkt->data = nullptr;
    pkt->size = 0;
    return reinterpret_cast<AVPacket*>(pkt);
  }
  else
    assert(false);
  return nullptr;
}

void FFmpegVersionHandler::deletePacket(AVPacket *pkt)
{
  if (libVersion.avcodec == 56)
  {
    AVPacket_56 *p = reinterpret_cast<AVPacket_56*>(pkt);
    delete p;
  }
  else if (libVersion.avcodec == 57)
  {
    AVPacket_57 *p = reinterpret_cast<AVPacket_57*>(pkt);
    delete p;
  }
  else
    assert(false);
}


int FFmpegVersionHandler::AVPacketGetStreamIndex(AVPacket *pkt)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVPacket_56*>(pkt)->stream_index;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVPacket_57*>(pkt)->stream_index;
  else
    assert(false);
  return -1;
}

int64_t FFmpegVersionHandler::AVPacketGetPTS(AVPacket *pkt)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVPacket_56*>(pkt)->pts;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVPacket_57*>(pkt)->pts;
  else
    assert(false);
  return -1;
}

int FFmpegVersionHandler::AVPacketGetDuration(AVPacket *pkt)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVPacket_56*>(pkt)->duration;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVPacket_57*>(pkt)->duration;
  else
    assert(false);
  return -1;
}

int FFmpegVersionHandler::AVPacketGetFlags(AVPacket *pkt)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVPacket_56*>(pkt)->flags;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVPacket_57*>(pkt)->flags;
  else
    assert(false);
  return -1;
}

AVPixelFormat FFmpegVersionHandler::AVCodecContextGetPixelFormat(AVCodecContext *codecCtx)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVCodecContext_56*>(codecCtx)->pix_fmt;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVCodecContext_57*>(codecCtx)->pix_fmt;
  else
    assert(false);
  return AV_PIX_FMT_NONE;
}

int FFmpegVersionHandler::AVCodecContexGetWidth(AVCodecContext *codecCtx)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVCodecContext_56*>(codecCtx)->width;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVCodecContext_57*>(codecCtx)->width;
  else
    assert(false);
  return -1;
}

int FFmpegVersionHandler::AVCodecContextGetHeight(AVCodecContext *codecCtx)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVCodecContext_56*>(codecCtx)->height;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVCodecContext_57*>(codecCtx)->height;
  else
    assert(false);
  return -1;
}

AVColorSpace FFmpegVersionHandler::AVCodecContextGetColorSpace(AVCodecContext *codecCtx)
{
  if (libVersion.avcodec == 56)
    return reinterpret_cast<AVCodecContext_56*>(codecCtx)->colorspace;
  else if (libVersion.avcodec == 57)
    return reinterpret_cast<AVCodecContext_57*>(codecCtx)->colorspace;
  else
    assert(false);
  return AVCOL_SPC_UNSPECIFIED;
}

AVCodecContext *FFmpegVersionHandler::AVStreamGetCodec(AVStream *str)
{
  if (libVersion.avformat == 56)
    return reinterpret_cast<AVStream_56*>(str)->codec;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVStream_57*>(str)->codec;
  else
    assert(false);
  return nullptr;
}

AVCodecParameters *FFmpegVersionHandler::AVStreamGetCodecpar(AVStream *str)
{
  if (libVersion.avformat == 57)
    return reinterpret_cast<AVStream_57*>(str)->codecpar;
  else
    assert(false);
  return nullptr;
}

int FFmpegVersionHandler::AVCodecParametersGetWidth(AVCodecParameters *param)
{
  if (libVersion.avcodec)
    return reinterpret_cast<AVCodecParameters_57*>(param)->width;
  else
    assert(false);
  return -1;
}

int FFmpegVersionHandler::AVCodecParametersGetHeight(AVCodecParameters *param)
{
  if (libVersion.avcodec)
    return reinterpret_cast<AVCodecParameters_57*>(param)->height;
  else
    assert(false);
  return -1;
}

AVColorSpace FFmpegVersionHandler::AVCodecParametersGetColorSpace(AVCodecParameters *param)
{
  if (libVersion.avcodec)
    return reinterpret_cast<AVCodecParameters_57*>(param)->color_space;
  else
    assert(false);
  return AVCOL_SPC_UNSPECIFIED;
}

unsigned int FFmpegVersionHandler::AVFormatContextGetNBStreams(AVFormatContext *fmtCtx)
{
  if (libVersion.avformat == 56)
    return reinterpret_cast<AVFormatContext_56*>(fmtCtx)->nb_streams;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVFormatContext_57*>(fmtCtx)->nb_streams;
  else
    assert(false);
  return 0;
}

AVStream *FFmpegVersionHandler::AVFormatContextGetStream(AVFormatContext *fmtCtx, int streamIdx)
{
  if (libVersion.avformat == 56)
    return reinterpret_cast<AVFormatContext_56*>(fmtCtx)->streams[streamIdx];
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVFormatContext_57*>(fmtCtx)->streams[streamIdx];
  else
    assert(false);
  return nullptr;
}

AVMediaType FFmpegVersionHandler::AVFormatContextGetCodecTypeFromCodec(AVFormatContext *fmtCtx, int streamIdx)
{
  AVStream *str = AVFormatContextGetStream(fmtCtx, streamIdx);
  AVCodecContext *codecCtx = AVStreamGetCodec(str);

  if (libVersion.avformat == 56)
    return reinterpret_cast<AVCodecContext_56*>(codecCtx)->codec_type;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVCodecContext_57*>(codecCtx)->codec_type;
  else
    assert(false);
  return AVMEDIA_TYPE_UNKNOWN;
}

AVCodecID FFmpegVersionHandler::AVFormatContextGetCodecIDFromCodec(AVFormatContext *fmtCtx, int streamIdx)
{
  AVStream *str = AVFormatContextGetStream(fmtCtx, streamIdx);
  AVCodecContext *codecCtx = AVStreamGetCodec(str);

  if (libVersion.avformat == 56)
    return reinterpret_cast<AVCodecContext_56*>(codecCtx)->codec_id;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVCodecContext_57*>(codecCtx)->codec_id;
  else
    assert(false);
  return (AVCodecID)0;
}

AVMediaType FFmpegVersionHandler::AVFormatContextGetCodecTypeFromCodecpar(AVFormatContext *fmtCtx, int streamIdx)
{
  AVStream *str = AVFormatContextGetStream(fmtCtx, streamIdx);
  AVCodecParameters *codecpar = AVStreamGetCodecpar(str);

  if (libVersion.avformat == 57)
    return reinterpret_cast<AVCodecParameters_57*>(codecpar)->codec_type;
  else
    assert(false);
  return AVMEDIA_TYPE_UNKNOWN;
}

AVCodecID FFmpegVersionHandler::AVFormatContextGetCodecIDFromCodecpar(AVFormatContext *fmtCtx, int streamIdx)
{
  AVStream *str = AVFormatContextGetStream(fmtCtx, streamIdx);
  AVCodecParameters *codecpar = AVStreamGetCodecpar(str);

  if (libVersion.avformat == 57)
    return reinterpret_cast<AVCodecParameters_57*>(codecpar)->codec_id;
  else
    assert(false);
  return (AVCodecID)0;
}

AVRational FFmpegVersionHandler::AVFormatContextGetAvgFrameRate(AVFormatContext *fmtCtx, int streamIdx)
{
  AVStream *str = AVFormatContextGetStream(fmtCtx, streamIdx);

  if (libVersion.avformat == 56)
    return reinterpret_cast<AVStream_56*>(str)->avg_frame_rate;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVStream_57*>(str)->avg_frame_rate;
  else
    assert(false);
  return AVRational();
}

int64_t FFmpegVersionHandler::AVFormatContextGetDuration(AVFormatContext *fmtCtx)
{
  if (libVersion.avformat == 56)
    return reinterpret_cast<AVFormatContext_56*>(fmtCtx)->duration;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVFormatContext_56*>(fmtCtx)->duration;
  else
    assert(false);
  return -1;
}

AVRational FFmpegVersionHandler::AVFormatContextGetTimeBase(AVFormatContext *fmtCtx, int streamIdx)
{
  AVStream *str = AVFormatContextGetStream(fmtCtx, streamIdx);

  if (libVersion.avformat == 56)
    return reinterpret_cast<AVStream_56*>(str)->time_base;
  else if (libVersion.avformat == 57)
    return reinterpret_cast<AVStream_57*>(str)->time_base;
  else
    assert(false);
  return AVRational();
}

bool FFmpegVersionHandler::AVCodecContextCopyParameters(AVCodecContext *srcCtx, AVCodecContext *dstCtx)
{
  if (libVersion.avcodec == 56)
  {
    AVCodecContext_56 *dst = reinterpret_cast<AVCodecContext_56*>(dstCtx);
    AVCodecContext_56 *src = reinterpret_cast<AVCodecContext_56*>(srcCtx);

    dst->codec_type            = src->codec_type;
    dst->codec_id              = src->codec_id;
    dst->codec_tag             = src->codec_tag;
    dst->bit_rate              = src->bit_rate;

    // We don't parse these ...
    //decCtx->bits_per_coded_sample = ctx->bits_per_coded_sample;
    //decCtx->bits_per_raw_sample   = ctx->bits_per_raw_sample;
    //decCtx->profile               = ctx->profile;
    //decCtx->level                 = ctx->level;

    if (src->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      dst->pix_fmt                = src->pix_fmt;
      dst->width                  = src->width;
      dst->height                 = src->height;
      //dst->field_order            = src->field_order;
      dst->color_range            = src->color_range;
      dst->color_primaries        = src->color_primaries;
      dst->color_trc              = src->color_trc;
      dst->colorspace             = src->colorspace;
      dst->chroma_sample_location = src->chroma_sample_location;
      dst->sample_aspect_ratio    = src->sample_aspect_ratio;
      dst->has_b_frames           = src->has_b_frames;
    }

    // Extradata
    if (src->extradata_size != 0 && dst->extradata_size == 0)
    {
      assert(dst->extradata == nullptr);
      dst->extradata = (uint8_t*)av_mallocz(src->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
      if (dst->extradata == nullptr)
        return false;
      memcpy(dst->extradata, src->extradata, src->extradata_size);
      dst->extradata_size = src->extradata_size;
    }
  }
  else if (libVersion.avcodec == 57)
  {
    AVCodecContext_57 *dst = reinterpret_cast<AVCodecContext_57*>(dstCtx);
    AVCodecContext_57 *src = reinterpret_cast<AVCodecContext_57*>(srcCtx);

    dst->codec_type            = src->codec_type;
    dst->codec_id              = src->codec_id;
    dst->codec_tag             = src->codec_tag;
    dst->bit_rate              = src->bit_rate;

    // We don't parse these ...
    //decCtx->bits_per_coded_sample = ctx->bits_per_coded_sample;
    //decCtx->bits_per_raw_sample   = ctx->bits_per_raw_sample;
    //decCtx->profile               = ctx->profile;
    //decCtx->level                 = ctx->level;

    if (src->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      dst->pix_fmt                = src->pix_fmt;
      dst->width                  = src->width;
      dst->height                 = src->height;
      //dst->field_order            = src->field_order;
      dst->color_range            = src->color_range;
      dst->color_primaries        = src->color_primaries;
      dst->color_trc              = src->color_trc;
      dst->colorspace             = src->colorspace;
      dst->chroma_sample_location = src->chroma_sample_location;
      dst->sample_aspect_ratio    = src->sample_aspect_ratio;
      dst->has_b_frames           = src->has_b_frames;
    }

    // Extradata
    if (src->extradata_size != 0 && dst->extradata_size == 0)
    {
      assert(dst->extradata == nullptr);
      dst->extradata = (uint8_t*)av_mallocz(src->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
      if (dst->extradata == nullptr)
        return false;
      memcpy(dst->extradata, src->extradata, src->extradata_size);
      dst->extradata_size = src->extradata_size;
    }
  }
  else
    assert(false);
  return true;
}

int FFmpegVersionHandler::AVFrameGetWidth(AVFrame *frame)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->width;
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->width;
  else
    assert(false);
  return -1;
}

int FFmpegVersionHandler::AVFrameGetHeight(AVFrame *frame)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->height;
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->height;
  else
    assert(false);
  return -1;
}

int64_t FFmpegVersionHandler::AVFrameGetPTS(AVFrame *frame)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->pts;
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->pts;
  else
    assert(false);
  return -1;
}

AVPictureType FFmpegVersionHandler::AVFrameGetPictureType(AVFrame *frame)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->pict_type;
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->pict_type;
  else
    assert(false);
  return (AVPictureType)0;
}

int FFmpegVersionHandler::AVFrameGetKeyFrame(AVFrame *frame)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->key_frame;
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->key_frame;
  else
    assert(false);
  return -1;
}

int FFmpegVersionHandler::AVFrameGetLinesize(AVFrame *frame, int idx)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->linesize[idx];
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->linesize[idx];
  else
    assert(false);
  return -1;
}

uint8_t *FFmpegVersionHandler::AVFrameGetData(AVFrame *frame, int idx)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrame_54*>(frame)->data[idx];
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrame_55*>(frame)->data[idx];
  else
    assert(false);
  return nullptr;
}

AVFrameSideDataType FFmpegVersionHandler::getSideDataType(AVFrameSideData * sideData)
{
  if (libVersion.avutil == 54 || libVersion.avutil == 55)
    return reinterpret_cast<AVFrameSideData_54_55*>(sideData)->type;
  else
    assert(false);
  return AV_FRAME_DATA_PANSCAN;
}

uint8_t * FFmpegVersionHandler::getSideDataData(AVFrameSideData * sideData)
{
  if (libVersion.avutil == 54 || libVersion.avutil == 55)
    return reinterpret_cast<AVFrameSideData_54_55*>(sideData)->data;
  else
    assert(false);
  return nullptr;
}

int FFmpegVersionHandler::getSideDataNrMotionVectors(AVFrameSideData * sideData)
{
  if (libVersion.avutil == 54)
    return reinterpret_cast<AVFrameSideData_54_55*>(sideData)->size / sizeof(AVMotionVector_54);
  else if (libVersion.avutil == 55)
    return reinterpret_cast<AVFrameSideData_54_55*>(sideData)->size / sizeof(AVMotionVector_55);
  else
    assert(false);
  return 0;
}

void FFmpegVersionHandler::getMotionVectorValues(AVMotionVector * mv, int idx, int32_t &source, uint8_t &blockWidth, uint8_t &blockHeight, int16_t &src_x, int16_t &src_y, int16_t &dst_x, int16_t &dst_y)
{
  if (libVersion.avutil == 54)
  {
    AVMotionVector_54 *m = reinterpret_cast<AVMotionVector_54*>(mv);
    source = m[idx].source;
    blockWidth = m[idx].w;
    blockHeight = m[idx].h;
    src_x = m[idx].src_x;
    src_y = m[idx].src_y;
    dst_x = m[idx].dst_x;
    dst_y = m[idx].dst_y;
  }
  else if (libVersion.avutil == 55)
  {
    AVMotionVector_55 *m = reinterpret_cast<AVMotionVector_55*>(mv);
    source = m[idx].source;
    blockWidth = m[idx].w;
    blockHeight = m[idx].h;
    src_x = m[idx].src_x;
    src_y = m[idx].src_y;
    dst_x = m[idx].dst_x;
    dst_y = m[idx].dst_y;
  }
  else
    assert(false);
}

QString FFmpegVersionHandler::getLibVersionString() const
{
  QString s;
  s += QString("avUtil %1.%2.%3 ").arg(libVersion.avutil).arg(libVersion.avutil_minor).arg(libVersion.avutil_micro);
  s += QString("avFormat %1.%2.%3 ").arg(libVersion.avformat).arg(libVersion.avformat_minor).arg(libVersion.avformat_micro);
  s += QString("avCodec %1.%2.%3 ").arg(libVersion.avcodec).arg(libVersion.avcodec_minor).arg(libVersion.avformat_micro);
  s += QString("swresample %1.%2.%3").arg(libVersion.swresample).arg(libVersion.swresample_minor).arg(libVersion.swresample_micro);

  return s;
}

