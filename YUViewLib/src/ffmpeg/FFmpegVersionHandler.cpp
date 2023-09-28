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

#include "FFmpegVersionHandler.h"
#include <QDateTime>
#include <QDir>

namespace FFmpeg
{

namespace
{

LibraryVersions getLibraryVersionsFromLoadedLibraries(const FFmpegLibraryFunctions &libraries)
{
  const auto avFormatVersion = Version::fromFFmpegVersion(libraries.avformat.avformat_version());
  const auto avCodecVersion  = Version::fromFFmpegVersion(libraries.avcodec.avcodec_version());
  const auto avUtilVersion   = Version::fromFFmpegVersion(libraries.avutil.avutil_version());
  const auto swresampleVersion =
      Version::fromFFmpegVersion(libraries.swresample.swresample_version());

  return {avFormatVersion, avCodecVersion, avUtilVersion, swresampleVersion};
}

tl::expected<void, std::string>
checkMajorVersionWithLibraries(const LibraryVersions &librariesInfo,
                               const LibraryVersions &expectedMajorVersion)
{
  auto createErrorString = [](std::string_view libname,
                              const Version &  realVersion,
                              const Version &  expectedVersion) -> std::string {
    std::ostringstream stream;
    stream << "The openend " << libname << " returned a different version (" << realVersion
           << ") than we are looking for (" << expectedVersion << ").";
    return stream.str();
  };

  if (expectedMajorVersion.avcodec != librariesInfo.avcodec)
    return tl::unexpected(
        createErrorString("avcodec", librariesInfo.avcodec, expectedMajorVersion.avcodec));

  if (expectedMajorVersion.avformat != librariesInfo.avformat)
    return tl::unexpected(
        createErrorString("avformat", librariesInfo.avformat, expectedMajorVersion.avformat));

  if (expectedMajorVersion.avutil != librariesInfo.avutil)
    return tl::unexpected(
        createErrorString("avutil", librariesInfo.avutil, expectedMajorVersion.avutil));

  if (expectedMajorVersion.swresample != librariesInfo.swresample)
    return tl::unexpected(
        createErrorString("swresample", librariesInfo.swresample, expectedMajorVersion.swresample));

  return {};
}

// These FFmpeg versions are supported. The numbers indicate the major version of
// the following libraries in this order: Util, codec, format, swresample
// The versions are sorted from newest to oldest, so that we try to open the newest ones first.
auto SupportedMajorLibraryVersionCombinations = {
    LibraryVersions({Version(58), Version(60), Version(60), Version(4)}),
    LibraryVersions({Version(57), Version(59), Version(59), Version(4)}),
    LibraryVersions({Version(56), Version(58), Version(58), Version(3)}),
    LibraryVersions({Version(55), Version(57), Version(57), Version(2)}),
    LibraryVersions({Version(54), Version(56), Version(56), Version(1)}),
};

} // namespace

// bool FFmpegVersionHandler::AVCodecContextCopyParameters(AVCodecContext *srcCtx, AVCodecContext
// *dstCtx)
//{
//  if (libVersion.avcodec == 56)
//  {
//    AVCodecContext_56 *dst = reinterpret_cast<AVCodecContext_56*>(dstCtx);
//    AVCodecContext_56 *src = reinterpret_cast<AVCodecContext_56*>(srcCtx);
//
//    dst->codec_type            = src->codec_type;
//    dst->codec_id              = src->codec_id;
//    dst->codec_tag             = src->codec_tag;
//    dst->bit_rate              = src->bit_rate;
//
//    // We don't parse these ...
//    //decCtx->bits_per_coded_sample = ctx->bits_per_coded_sample;
//    //decCtx->bits_per_raw_sample   = ctx->bits_per_raw_sample;
//    //decCtx->profile               = ctx->profile;
//    //decCtx->level                 = ctx->level;
//
//    if (src->codec_type == AVMEDIA_TYPE_VIDEO)
//    {
//      dst->pix_fmt                = src->pix_fmt;
//      dst->width                  = src->width;
//      dst->height                 = src->height;
//      //dst->field_order            = src->field_order;
//      dst->color_range            = src->color_range;
//      dst->color_primaries        = src->color_primaries;
//      dst->color_trc              = src->color_trc;
//      dst->colorspace             = src->colorspace;
//      dst->chroma_sample_location = src->chroma_sample_location;
//      dst->sample_aspect_ratio    = src->sample_aspect_ratio;
//      dst->has_b_frames           = src->has_b_frames;
//    }
//
//    // Extradata
//    if (src->extradata_size != 0 && dst->extradata_size == 0)
//    {
//      assert(dst->extradata == nullptr);
//      dst->extradata = (uint8_t*)lib.av_mallocz(src->extradata_size +
//      AV_INPUT_BUFFER_PADDING_SIZE); if (dst->extradata == nullptr)
//        return false;
//      memcpy(dst->extradata, src->extradata, src->extradata_size);
//      dst->extradata_size = src->extradata_size;
//    }
//  }
//  else if (libVersion.avcodec == 57)
//  {
//    AVCodecContext_57 *dst = reinterpret_cast<AVCodecContext_57*>(dstCtx);
//    AVCodecContext_57 *src = reinterpret_cast<AVCodecContext_57*>(srcCtx);
//
//    dst->codec_type            = src->codec_type;
//    dst->codec_id              = src->codec_id;
//    dst->codec_tag             = src->codec_tag;
//    dst->bit_rate              = src->bit_rate;
//
//    // We don't parse these ...
//    //decCtx->bits_per_coded_sample = ctx->bits_per_coded_sample;
//    //decCtx->bits_per_raw_sample   = ctx->bits_per_raw_sample;
//    //decCtx->profile               = ctx->profile;
//    //decCtx->level                 = ctx->level;
//
//    if (src->codec_type == AVMEDIA_TYPE_VIDEO)
//    {
//      dst->pix_fmt                = src->pix_fmt;
//      dst->width                  = src->width;
//      dst->height                 = src->height;
//      //dst->field_order            = src->field_order;
//      dst->color_range            = src->color_range;
//      dst->color_primaries        = src->color_primaries;
//      dst->color_trc              = src->color_trc;
//      dst->colorspace             = src->colorspace;
//      dst->chroma_sample_location = src->chroma_sample_location;
//      dst->sample_aspect_ratio    = src->sample_aspect_ratio;
//      dst->has_b_frames           = src->has_b_frames;
//    }
//
//    // Extradata
//    if (src->extradata_size != 0 && dst->extradata_size == 0)
//    {
//      assert(dst->extradata == nullptr);
//      dst->extradata = (uint8_t*)lib.av_mallocz(src->extradata_size +
//      AV_INPUT_BUFFER_PADDING_SIZE); if (dst->extradata == nullptr)
//        return false;
//      memcpy(dst->extradata, src->extradata, src->extradata_size);
//      dst->extradata_size = src->extradata_size;
//    }
//  }
//  else
//    assert(false);
//  return true;
//}
//

AVCodecIDWrapper FFmpegVersionHandler::getCodecIDWrapper(AVCodecID id)
{
  const auto codecName = std::string(lib.avcodec.avcodec_get_name(id));
  return AVCodecIDWrapper(id, codecName);
}

AVCodecID FFmpegVersionHandler::getCodecIDFromWrapper(AVCodecIDWrapper &wrapper)
{
  if (wrapper.getCodecID() != AV_CODEC_ID_NONE)
    return wrapper.getCodecID();

  int         codecID = 1;
  std::string codecName;
  do
  {
    codecName = std::string(this->lib.avcodec.avcodec_get_name(AVCodecID(codecID)));
    if (codecName == wrapper.getCodecName())
    {
      wrapper.setCodecID(AVCodecID(codecID));
      return wrapper.getCodecID();
    }
    codecID++;
  } while (codecName != "unknown_codec");

  return AV_CODEC_ID_NONE;
}

bool FFmpegVersionHandler::configureDecoder(AVCodecContextWrapper &   decCtx,
                                            AVCodecParametersWrapper &codecpar)
{
  if (this->lib.avcodec.newParametersAPIAvailable)
  {
    // Use the new avcodec_parameters_to_context function.
    auto origin_par = codecpar.getCodecParameters();
    if (!origin_par)
      return false;
    auto ret = this->lib.avcodec.avcodec_parameters_to_context(decCtx.getCodec(), origin_par);
    if (ret < 0)
    {
      this->log(
          QString(
              "Could not copy codec parameters (avcodec_parameters_to_context). Return code %1.")
              .arg(ret));
      return false;
    }
  }
  else
  {
    // TODO: Is this even necessary / what is really happening here?

    // The new parameters API is not available. Perform what the function would do.
    // This is equal to the implementation of avcodec_parameters_to_context.
    // AVCodecContext *ctxSrc = videoStream.getCodec().getCodec();
    // int ret = lib.AVCodecContextCopyParameters(ctxSrc, decCtx.getCodec());
    // return setOpeningError(QStringLiteral("Could not copy decoder parameters from stream
    // decoder."));
  }
  return true;
}

int FFmpegVersionHandler::pushPacketToDecoder(AVCodecContextWrapper &decCtx, AVPacketWrapper &pkt)
{
  if (!pkt)
    return this->lib.avcodec.avcodec_send_packet(decCtx.getCodec(), nullptr);
  else
    return this->lib.avcodec.avcodec_send_packet(decCtx.getCodec(), pkt.getPacket());
}

int FFmpegVersionHandler::getFrameFromDecoder(AVCodecContextWrapper &decCtx, AVFrameWrapper &frame)
{
  return this->lib.avcodec.avcodec_receive_frame(decCtx.getCodec(), frame.getFrame());
}

void FFmpegVersionHandler::flush_buffers(AVCodecContextWrapper &decCtx)
{
  lib.avcodec.avcodec_flush_buffers(decCtx.getCodec());
}

QStringList FFmpegVersionHandler::logListFFmpeg;

FFmpegVersionHandler::FFmpegVersionHandler()
{
  this->librariesLoaded = false;
  this->lib.setLogList(&logList);
}

FFmpegVersionHandler::~FFmpegVersionHandler()
{
  this->lib.setLogList(nullptr);
}

void FFmpegVersionHandler::avLogCallback(void *, int level, const char *fmt, va_list vargs)
{
  QString msg;
  msg.vasprintf(fmt, vargs);
  auto now = QDateTime::currentDateTime();
  FFmpegVersionHandler::logListFFmpeg.append(now.toString("hh:mm:ss.zzz") +
                                             QString(" - L%1 - ").arg(level) + msg);
}

void FFmpegVersionHandler::loadFFmpegLibraries()
{
  if (this->librariesLoaded)
    return;

  // Try to load the ffmpeg libraries from the current working directory and several other
  // directories. Unfortunately relative paths like "./" do not work: (at least on windows)

  // First try the specific FFMpeg libraries (if set)
  QSettings settings;
  settings.beginGroup("Decoders");
  auto avFormatLib   = settings.value("FFmpeg.avformat", "").toString();
  auto avCodecLib    = settings.value("FFmpeg.avcodec", "").toString();
  auto avUtilLib     = settings.value("FFmpeg.avutil", "").toString();
  auto swResampleLib = settings.value("FFmpeg.swresample", "").toString();
  if (!avFormatLib.isEmpty() && //
      !avCodecLib.isEmpty() &&  //
      !avUtilLib.isEmpty() &&   //
      !swResampleLib.isEmpty())
  {
    this->log("Trying to load the libraries specified in the settings.");
    this->librariesLoaded =
        loadFFMpegLibrarySpecific(avFormatLib, avCodecLib, avUtilLib, swResampleLib);
  }
  else
    this->log("No ffmpeg libraries were specified in the settings.");

  if (!this->librariesLoaded)
  {
    // Next, we will try some other paths / options
    QStringList possibilites;
    auto        decoderSearchPath = settings.value("SearchPath", "").toString();
    if (!decoderSearchPath.isEmpty())
      possibilites.append(decoderSearchPath);       // Try the specific search path (if one is set)
    possibilites.append(QDir::currentPath() + "/"); // Try the current working directory
    possibilites.append(QDir::currentPath() + "/ffmpeg/");
    possibilites.append(QCoreApplication::applicationDirPath() +
                        "/"); // Try the path of the YUView.exe
    possibilites.append(QCoreApplication::applicationDirPath() + "/ffmpeg/");
    possibilites.append(
        ""); // Just try to call QLibrary::load so that the system folder will be searched.

    for (auto path : possibilites)
    {
      if (path.isEmpty())
        this->log("Trying to load the libraries in the system path");
      else
        this->log("Trying to load the libraries in the path " + path);

      this->librariesLoaded = loadFFmpegLibraryInPath(path);
      if (this->librariesLoaded)
        break;
    }
  }

  if (this->librariesLoaded)
    this->lib.avutil.av_log_set_callback(&FFmpegVersionHandler::avLogCallback);
}

bool FFmpegVersionHandler::loadingSuccessfull() const
{
  return this->librariesLoaded;
}

bool FFmpegVersionHandler::openInput(AVFormatContextWrapper &fmt, QString url)
{
  AVFormatContext *f_ctx = nullptr;
  int              ret =
      this->lib.avformat.avformat_open_input(&f_ctx, url.toStdString().c_str(), nullptr, nullptr);
  if (ret < 0)
  {
    this->log(QString("Error opening file (avformat_open_input). Ret code %1").arg(ret));
    return false;
  }
  if (f_ctx == nullptr)
  {
    this->log(QString("Error opening file (avformat_open_input). No format context returned."));
    return false;
  }

  // The wrapper will take ownership of this pointer
  fmt = AVFormatContextWrapper(f_ctx, this->libraryVersions);

  ret = lib.avformat.avformat_find_stream_info(fmt.getFormatCtx(), nullptr);
  if (ret < 0)
  {
    this->log(QString("Error opening file (avformat_find_stream_info). Ret code %1").arg(ret));
    return false;
  }

  return true;
}

AVCodecParametersWrapper FFmpegVersionHandler::allocCodecParameters()
{
  return AVCodecParametersWrapper(this->lib.avcodec.avcodec_parameters_alloc(),
                                  this->libraryVersions);
}

AVCodecWrapper FFmpegVersionHandler::findDecoder(AVCodecIDWrapper codecId)
{
  AVCodecID avCodecID = getCodecIDFromWrapper(codecId);
  AVCodec * c         = this->lib.avcodec.avcodec_find_decoder(avCodecID);
  if (c == nullptr)
    return {};
  return AVCodecWrapper(c, this->libraryVersions);
}

AVCodecContextWrapper FFmpegVersionHandler::allocDecoder(AVCodecWrapper &codec)
{
  return AVCodecContextWrapper(this->lib.avcodec.avcodec_alloc_context3(codec.getAVCodec()),
                               this->libraryVersions);
}

int FFmpegVersionHandler::dictSet(AVDictionaryWrapper &dict,
                                  const char *         key,
                                  const char *         value,
                                  int                  flags)
{
  AVDictionary *d   = dict.getDictionary();
  int           ret = this->lib.avutil.av_dict_set(&d, key, value, flags);
  dict.setDictionary(d);
  return ret;
}

StringPairVec
FFmpegVersionHandler::getDictionaryEntries(AVDictionaryWrapper d, QString key, int flags)
{
  StringPairVec      ret;
  AVDictionaryEntry *tag = NULL;
  while ((tag = this->lib.avutil.av_dict_get(d.getDictionary(), key.toLatin1().data(), tag, flags)))
  {
    StringPair pair;
    pair.first  = std::string(tag->key);
    pair.second = std::string(tag->value);
    ret.push_back(pair);
  }
  return ret;
}
int FFmpegVersionHandler::avcodecOpen2(AVCodecContextWrapper &decCtx,
                                       AVCodecWrapper &       codec,
                                       AVDictionaryWrapper &  dict)
{
  auto d   = dict.getDictionary();
  int  ret = this->lib.avcodec.avcodec_open2(decCtx.getCodec(), codec.getAVCodec(), &d);
  dict.setDictionary(d);
  return ret;
}

AVFrameSideDataWrapper FFmpegVersionHandler::getSideData(AVFrameWrapper &    frame,
                                                         AVFrameSideDataType type)
{
  auto sd = this->lib.avutil.av_frame_get_side_data(frame.getFrame(), type);
  return AVFrameSideDataWrapper(sd, this->libraryVersions);
}

AVDictionaryWrapper FFmpegVersionHandler::getMetadata(AVFrameWrapper &frame)
{
  AVDictionary *dict;
  if (this->libraryVersions.avutil.major < 57)
    dict = this->lib.avutil.av_frame_get_metadata(frame.getFrame());
  else
    dict = frame.getMetadata();
  return AVDictionaryWrapper(dict);
}

int FFmpegVersionHandler::seekFrame(AVFormatContextWrapper &fmt, int stream_idx, int64_t dts)
{
  int ret =
      this->lib.avformat.av_seek_frame(fmt.getFormatCtx(), stream_idx, dts, AVSEEK_FLAG_BACKWARD);
  return ret;
}

int FFmpegVersionHandler::seekBeginning(AVFormatContextWrapper &fmt)
{
  // This is "borrowed" from the ffmpeg sources
  // (https://ffmpeg.org/doxygen/4.0/ffmpeg_8c_source.html seek_to_start)
  this->log(QString("seek_beginning time %1").arg(fmt.getStartTime()));
  return lib.avformat.av_seek_frame(fmt.getFormatCtx(), -1, fmt.getStartTime(), 0);
}

bool FFmpegVersionHandler::loadFFmpegLibraryInPath(QString path)
{
  LibraryLoadingResult result;
  for (auto version : SupportedMajorLibraryVersionCombinations)
  {
    if (this->lib.loadFFmpegLibraryInPath(path, version))
    {
      this->log(QString("Checking versions avutil %1, swresample %2, avcodec %3, avformat %4")
                    .arg(version.avutil.major)
                    .arg(version.swresample.major)
                    .arg(version.avcodec.major)
                    .arg(version.avformat.major));

      result.addLogLine("Checking versions avutil " + to_string(version.avutil) + ", swresample " +
                        to_string(version.swresample) + ", avcodec " + to_string(version.avcodec) +
                        ", avformat " + to_string(version.avformat));

      const auto libraryVersions = getLibraryVersionsFromLoadedLibraries(this->lib);

      const auto checkVersionResult = checkMajorVersionWithLibraries(libraryVersions, version);
      if (checkVersionResult.has_value())
      {
        this->libraryVersions = libraryVersions;

        if (this->libraryVersions.avformat.major < 59)
          this->lib.avformat.av_register_all();

        result.success = true;
        result.addLogLine("Library version check passed.");
        return result;
      }
      else
      {
        result.addLogLine(checkVersionResult.error());
      }
    }
  }

  if (success && this->libVersion.avformat.major < 59)
    this->lib.avformat.av_register_all();

  return success;
}

bool FFmpegVersionHandler::loadFFMpegLibrarySpecific(QString avFormatLib,
                                                     QString avCodecLib,
                                                     QString avUtilLib,
                                                     QString swResampleLib)
{
  bool success = false;
  for (auto version : SupportedLibraryVersionCombinations)
  {
    this->log(QString("Checking versions avutil %1, swresample %2, avcodec %3, avformat %4")
                  .arg(version.avutil.major)
                  .arg(version.swresample.major)
                  .arg(version.avcodec.major)
                  .arg(version.avformat.major));
    if (lib.loadFFMpegLibrarySpecific(avFormatLib, avCodecLib, avUtilLib, swResampleLib))
    {
      this->log("Testing versions of the library. Currently looking for:");
      this->log(QString("avutil: %1.xx.xx").arg(version.avutil.major));
      this->log(QString("swresample: %1.xx.xx").arg(version.swresample.major));
      this->log(QString("avcodec: %1.xx.xx").arg(version.avcodec.major));
      this->log(QString("avformat: %1.xx.xx").arg(version.avformat.major));

      if ((success = checkVersionWithLib(this->lib, version, this->logList)))
      {
        this->libVersion = addMinorAndMicroVersion(this->lib, version);
        this->log("checking the library versions was successful.");
        break;
      }
    }
  }

  if (success && this->libVersion.avformat.major < 59)
    this->lib.avformat.av_register_all();

  return success;
}

bool FFmpegVersionHandler::checkLibraryFiles(QString      avCodecLib,
                                             QString      avFormatLib,
                                             QString      avUtilLib,
                                             QString      swResampleLib,
                                             QStringList &logging)
{
  FFmpegVersionHandler handler;
  bool                 success =
      handler.loadFFMpegLibrarySpecific(avFormatLib, avCodecLib, avUtilLib, swResampleLib);
  logging = handler.getLog();
  return success;
}

void FFmpegVersionHandler::enableLoggingWarning()
{
  lib.avutil.av_log_set_level(AV_LOG_WARNING);
}

AVPixFmtDescriptorWrapper
FFmpegVersionHandler::getAvPixFmtDescriptionFromAvPixelFormat(AVPixelFormat pixFmt)
{
  if (pixFmt == AV_PIX_FMT_NONE)
    return {};
  return AVPixFmtDescriptorWrapper(lib.avutil.av_pix_fmt_desc_get(pixFmt), this->libraryVersions);
}

AVPixelFormat
FFmpegVersionHandler::getAVPixelFormatFromPixelFormatYUV(video::yuv::PixelFormatYUV pixFmt)
{
  AVPixFmtDescriptorWrapper wrapper;
  wrapper.setValuesFromPixelFormatYUV(pixFmt);

  // We will have to search through all pixel formats which the library knows and compare them to
  // the one we are looking for. Unfortunately there is no other more direct search function in
  // libavutil.
  auto desc = this->lib.avutil.av_pix_fmt_desc_next(nullptr);
  while (desc != nullptr)
  {
    AVPixFmtDescriptorWrapper descWrapper(desc, this->libraryVersions);

    if (descWrapper == wrapper)
      return this->lib.avutil.av_pix_fmt_desc_get_id(desc);

    // Get the next descriptor
    desc = this->lib.avutil.av_pix_fmt_desc_next(desc);
  }

  return AV_PIX_FMT_NONE;
}

AVFrameWrapper FFmpegVersionHandler::allocateFrame()
{
  auto framePtr = this->lib.avutil.av_frame_alloc();
  return AVFrameWrapper(framePtr, this->libraryVersions);
}

void FFmpegVersionHandler::freeFrame(AVFrameWrapper &frame)
{
  auto framePtr = frame.getFrame();
  this->lib.avutil.av_frame_free(&framePtr);
  frame.clear();
}

AVPacketWrapper FFmpegVersionHandler::allocatePacket()
{
  auto rawPacket = this->lib.avcodec.av_packet_alloc();
  this->lib.avcodec.av_init_packet(rawPacket);
  return AVPacketWrapper(rawPacket, this->libraryVersions);
}

void FFmpegVersionHandler::unrefPacket(AVPacketWrapper &packet)
{
  this->lib.avcodec.av_packet_unref(packet.getPacket());
}

void FFmpegVersionHandler::freePacket(AVPacketWrapper &packet)
{
  auto packetPtr = packet.getPacket();
  this->lib.avcodec.av_packet_free(&packetPtr);
  packet.clear();
}

} // namespace FFmpeg
