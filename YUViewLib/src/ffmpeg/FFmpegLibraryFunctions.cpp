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

#include "FFmpegLibraryFunctions.h"
#include <QDir>

namespace FFmpeg
{

namespace
{

template <typename T>
bool resolveFunction(QLibrary &        lib,
                     std::function<T> &function,
                     const char *      symbolName,
                     QStringList *     logList)
{
  auto ptr = lib.resolve(symbolName);
  if (!ptr)
  {
    if (logList)
      logList->append(QString("Function %1 not found.").arg(symbolName));
    return false;
  }

  function = reinterpret_cast<T *>(ptr);
  return true;
}

bool bindLibraryFunctions(QLibrary &                                 lib,
                          FFmpegLibraryFunctions::AvFormatFunctions &functions,
                          QStringList *                              log)
{
  if (!resolveFunction(lib, functions.avformat_version, "avformat_version", log))
    return false;

  auto versionRaw = functions.avformat_version();
  if (AV_VERSION_MAJOR(versionRaw) < 59)
    if (!resolveFunction(lib, functions.av_register_all, "av_register_all", log))
      return false;

  if (!resolveFunction(lib, functions.avformat_open_input, "avformat_open_input", log))
    return false;
  if (!resolveFunction(lib, functions.avformat_close_input, "avformat_close_input", log))
    return false;
  if (!resolveFunction(lib, functions.avformat_find_stream_info, "avformat_find_stream_info", log))
    return false;
  if (!resolveFunction(lib, functions.av_read_frame, "av_read_frame", log))
    return false;
  if (!resolveFunction(lib, functions.av_seek_frame, "av_seek_frame", log))
    return false;
  if (!resolveFunction(lib, functions.avformat_version, "avformat_version", log))
    return false;
  return true;
}

bool bindLibraryFunctions(QLibrary &                                lib,
                          FFmpegLibraryFunctions::AvCodecFunctions &functions,
                          QStringList *                             log)
{
  if (!resolveFunction(lib, functions.avcodec_find_decoder, "avcodec_find_decoder", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_alloc_context3, "avcodec_alloc_context3", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_open2, "avcodec_open2", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_free_context, "avcodec_free_context", log))
    return false;
  if (!resolveFunction(lib, functions.av_init_packet, "av_init_packet", log))
    return false;
  if (!resolveFunction(lib, functions.av_packet_alloc, "av_packet_alloc", log))
    return false;
  if (!resolveFunction(lib, functions.av_packet_free, "av_packet_free", log))
    return false;
  if (!resolveFunction(lib, functions.av_packet_unref, "av_packet_unref", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_flush_buffers, "avcodec_flush_buffers", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_version, "avcodec_version", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_get_name, "avcodec_get_name", log))
    return false;
  if (!resolveFunction(lib, functions.avcodec_parameters_alloc, "avcodec_parameters_alloc", log))
    return false;

  // The following functions are part of the new API. If they are not available, we use the old API.
  // If available, we should however use it.
  if (resolveFunction(lib, functions.avcodec_send_packet, "avcodec_send_packet", log) &&
      resolveFunction(lib, functions.avcodec_receive_frame, "avcodec_receive_frame", log) &&
      resolveFunction(
          lib, functions.avcodec_parameters_to_context, "avcodec_parameters_to_context", log))
    functions.newParametersAPIAvailable = true;
  else
  {
    // If the new API is not available, then the old one must be available.
    if (!resolveFunction(lib, functions.avcodec_decode_video2, "avcodec_decode_video2", log))
      return false;
  }

  return true;
}

bool bindLibraryFunctions(QLibrary &                               lib,
                          FFmpegLibraryFunctions::AvUtilFunctions &functions,
                          QStringList *                            log)
{
  if (!resolveFunction(lib, functions.avutil_version, "avutil_version", log))
    return false;

  auto versionRaw = functions.avutil_version();

  if (!resolveFunction(lib, functions.av_frame_alloc, "av_frame_alloc", log))
    return false;
  if (!resolveFunction(lib, functions.av_frame_free, "av_frame_free", log))
    return false;
  if (!resolveFunction(lib, functions.av_mallocz, "av_mallocz", log))
    return false;
  if (!resolveFunction(lib, functions.av_dict_set, "av_dict_set", log))
    return false;
  if (!resolveFunction(lib, functions.av_dict_get, "av_dict_get", log))
    return false;
  if (!resolveFunction(lib, functions.av_frame_get_side_data, "av_frame_get_side_data", log))
    return false;
  if (AV_VERSION_MAJOR(versionRaw) < 57)
    if (!resolveFunction(lib, functions.av_frame_get_metadata, "av_frame_get_metadata", log))
      return false;
  if (!resolveFunction(lib, functions.av_log_set_callback, "av_log_set_callback", log))
    return false;
  if (!resolveFunction(lib, functions.av_log_set_level, "av_log_set_level", log))
    return false;
  if (!resolveFunction(lib, functions.av_pix_fmt_desc_get, "av_pix_fmt_desc_get", log))
    return false;
  if (!resolveFunction(lib, functions.av_pix_fmt_desc_next, "av_pix_fmt_desc_next", log))
    return false;
  if (!resolveFunction(lib, functions.av_pix_fmt_desc_get_id, "av_pix_fmt_desc_get_id", log))
    return false;

  return true;
}

bool bindLibraryFunctions(QLibrary &                                  lib,
                          FFmpegLibraryFunctions::SwResampleFunction &functions,
                          QStringList *                               log)
{
  return resolveFunction(lib, functions.swresample_version, "swresample_version", log);
}

} // namespace

FFmpegLibraryFunctions::~FFmpegLibraryFunctions()
{
  this->unloadAllLibraries();
}

bool FFmpegLibraryFunctions::loadFFmpegLibraryInPath(QString path, LibraryVersion &libraryVersion)
{
  // We will load the following libraries (in this order):
  // avutil, swresample, avcodec, avformat.

  if (!path.isEmpty())
  {
    QDir givenPath(path);
    if (!givenPath.exists())
    {
      this->log("The given path is invalid");
      return false;
    }

    // Get the absolute path
    path = givenPath.absolutePath() + "/";
    this->log("Absolute path " + path);
  }

  // The ffmpeg libraries are named using a major version number. E.g: avutil-55.dll on windows.
  // On linux, the libraries may be named differently. On Ubuntu they are named
  // libavutil-ffmpeg.so.55. On arch linux the name is libavutil.so.55. We will try to look for both
  // namings. On MAC os (installed with homebrew), there is a link to the lib named
  // libavutil.54.dylib.
  unsigned nrNames = (is_Q_OS_LINUX) ? 2 : ((is_Q_OS_WIN) ? 1 : 1);
  bool     success = false;
  for (unsigned i = 0; i < nrNames; i++)
  {
    this->unloadAllLibraries();

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

    auto loadLibrary =
        [this, &constructLibName, &path](QLibrary &lib, QString libName, unsigned version) {
          auto filename = constructLibName.arg(libName).arg(version);
          lib.setFileName(path + filename);
          auto success = lib.load();
          this->log("Loading library " + filename + (success ? " succeded" : " failed"));
          return success;
        };

    if (!loadLibrary(this->libAvutil, "avutil", libraryVersion.avutil.major))
      continue;
    if (!loadLibrary(this->libSwresample, "swresample", libraryVersion.swresample.major))
      continue;
    if (!loadLibrary(this->libAvcodec, "avcodec", libraryVersion.avcodec.major))
      continue;
    if (!loadLibrary(this->libAvformat, "avformat", libraryVersion.avformat.major))
      continue;

    break;
  }

  if (!success)
  {
    this->unloadAllLibraries();
    return false;
  }

  success = (bindLibraryFunctions(this->libAvformat, this->avformat, this->logList) &&
             bindLibraryFunctions(this->libAvcodec, this->avcodec, this->logList) &&
             bindLibraryFunctions(this->libAvutil, this->avutil, this->logList) &&
             bindLibraryFunctions(this->libSwresample, this->swresample, this->logList));
  this->log(QString("Binding functions ") + (success ? "successfull" : "failed"));

  return success;
}

bool FFmpegLibraryFunctions::loadFFMpegLibrarySpecific(QString avFormatLib,
                                                       QString avCodecLib,
                                                       QString avUtilLib,
                                                       QString swResampleLib)
{
  this->unloadAllLibraries();

  auto loadLibrary = [this](QLibrary &lib, QString libPath) {
    lib.setFileName(libPath);
    auto success = lib.load();
    this->log("Loading library " + libPath + (success ? " succeded" : " failed"));
    return success;
  };

  auto success = (loadLibrary(this->libAvutil, avUtilLib) &&         //
                  loadLibrary(this->libSwresample, swResampleLib) && //
                  loadLibrary(this->libAvcodec, avCodecLib) &&       //
                  loadLibrary(this->libAvformat, avFormatLib));

  if (!success)
  {
    this->unloadAllLibraries();
    return false;
  }

  success = (bindLibraryFunctions(this->libAvformat, this->avformat, this->logList) &&
             bindLibraryFunctions(this->libAvcodec, this->avcodec, this->logList) &&
             bindLibraryFunctions(this->libAvutil, this->avutil, this->logList) &&
             bindLibraryFunctions(this->libSwresample, this->swresample, this->logList));
  this->log(QString("Binding functions ") + (success ? "successfull" : "failed"));

  return success;
}

void FFmpegLibraryFunctions::addLibNamesToList(QString         libName,
                                               QStringList &   l,
                                               const QLibrary &lib) const
{
  l.append(libName);
  if (lib.isLoaded())
  {
    l.append(lib.fileName());
    l.append(lib.fileName());
  }
  else
  {
    l.append("None");
    l.append("None");
  }
}

void FFmpegLibraryFunctions::unloadAllLibraries()
{
  this->log("Unloading all loaded libraries");
  this->libAvutil.unload();
  this->libSwresample.unload();
  this->libAvcodec.unload();
  this->libAvformat.unload();
}

QStringList FFmpegLibraryFunctions::getLibPaths() const
{
  QStringList libPaths;

  auto addName = [&libPaths](QString name, const QLibrary &lib) {
    libPaths.append(name);
    if (lib.isLoaded())
    {
      libPaths.append(lib.fileName());
      libPaths.append(lib.fileName());
    }
    else
    {
      libPaths.append("None");
      libPaths.append("None");
    }
  };

  addName("AVCodec", this->libAvcodec);
  addName("AVFormat", this->libAvformat);
  addName("AVUtil", this->libAvutil);
  addName("SwResample", this->libSwresample);

  return libPaths;
}

void FFmpegLibraryFunctions::log(QString message)
{
  if (this->logList)
    this->logList->append(message);
}

} // namespace FFmpeg