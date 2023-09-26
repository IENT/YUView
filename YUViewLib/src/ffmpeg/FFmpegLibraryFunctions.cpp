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

namespace FFmpeg
{

namespace
{

template <typename T>
bool resolveFunction(LibraryLoader &       lib,
                     std::function<T> &    function,
                     const char *          symbolName,
                     LibraryLoadingResult &result)
{
  auto ptr = lib.resolve(symbolName);
  if (!ptr)
  {
    result.errorMessage = "Function " + std::string(symbolName) + " not found.";
    result.addLogLine("Function " + std::string(symbolName) + " not found.");
    return false;
  }

  function = reinterpret_cast<T *>(ptr);
  return true;
}

bool bindLibraryFunctionsAndUpdateResult(LibraryLoader &                            lib,
                                         FFmpegLibraryFunctions::AvFormatFunctions &functions,
                                         LibraryLoadingResult &                     result)
{
  if (!resolveFunction(lib, functions.avformat_version, "avformat_version", result))
    return false;

  auto versionRaw = functions.avformat_version();
  if (AV_VERSION_MAJOR(versionRaw) < 59)
    if (!resolveFunction(lib, functions.av_register_all, "av_register_all", result))
      return false;

  if (!resolveFunction(lib, functions.avformat_open_input, "avformat_open_input", result))
    return false;
  if (!resolveFunction(lib, functions.avformat_close_input, "avformat_close_input", result))
    return false;
  if (!resolveFunction(
          lib, functions.avformat_find_stream_info, "avformat_find_stream_info", result))
    return false;
  if (!resolveFunction(lib, functions.av_read_frame, "av_read_frame", result))
    return false;
  if (!resolveFunction(lib, functions.av_seek_frame, "av_seek_frame", result))
    return false;
  if (!resolveFunction(lib, functions.avformat_version, "avformat_version", result))
    return false;

  result.addLogLine("Binding avFormat functions successfull.");
  return true;
}

bool bindLibraryFunctionsAndUpdateResult(LibraryLoader &                           lib,
                                         FFmpegLibraryFunctions::AvCodecFunctions &functions,
                                         LibraryLoadingResult &                    result)
{
  if (!resolveFunction(lib, functions.avcodec_find_decoder, "avcodec_find_decoder", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_alloc_context3, "avcodec_alloc_context3", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_open2, "avcodec_open2", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_free_context, "avcodec_free_context", result))
    return false;
  if (!resolveFunction(lib, functions.av_init_packet, "av_init_packet", result))
    return false;
  if (!resolveFunction(lib, functions.av_packet_alloc, "av_packet_alloc", result))
    return false;
  if (!resolveFunction(lib, functions.av_packet_free, "av_packet_free", result))
    return false;
  if (!resolveFunction(lib, functions.av_packet_unref, "av_packet_unref", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_flush_buffers, "avcodec_flush_buffers", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_version, "avcodec_version", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_get_name, "avcodec_get_name", result))
    return false;
  if (!resolveFunction(lib, functions.avcodec_parameters_alloc, "avcodec_parameters_alloc", result))
    return false;

  // The following functions are part of the new API. If they are not available, we use the old API.
  // If available, we should however use it.
  if (resolveFunction(lib, functions.avcodec_send_packet, "avcodec_send_packet", result) &&
      resolveFunction(lib, functions.avcodec_receive_frame, "avcodec_receive_frame", result) &&
      resolveFunction(
          lib, functions.avcodec_parameters_to_context, "avcodec_parameters_to_context", result))
    functions.newParametersAPIAvailable = true;
  else
  {
    // If the new API is not available, then the old one must be available.
    if (!resolveFunction(lib, functions.avcodec_decode_video2, "avcodec_decode_video2", result))
      return false;
  }

  result.addLogLine("Binding avCodec functions successfull.");
  return true;
}

bool bindLibraryFunctionsAndUpdateResult(LibraryLoader &                          lib,
                                         FFmpegLibraryFunctions::AvUtilFunctions &functions,
                                         LibraryLoadingResult &                   result)
{
  if (!resolveFunction(lib, functions.avutil_version, "avutil_version", result))
    return false;

  auto versionRaw = functions.avutil_version();

  if (!resolveFunction(lib, functions.av_frame_alloc, "av_frame_alloc", result))
    return false;
  if (!resolveFunction(lib, functions.av_frame_free, "av_frame_free", result))
    return false;
  if (!resolveFunction(lib, functions.av_mallocz, "av_mallocz", result))
    return false;
  if (!resolveFunction(lib, functions.av_dict_set, "av_dict_set", result))
    return false;
  if (!resolveFunction(lib, functions.av_dict_get, "av_dict_get", result))
    return false;
  if (!resolveFunction(lib, functions.av_frame_get_side_data, "av_frame_get_side_data", result))
    return false;
  if (AV_VERSION_MAJOR(versionRaw) < 57)
    if (!resolveFunction(lib, functions.av_frame_get_metadata, "av_frame_get_metadata", result))
      return false;
  if (!resolveFunction(lib, functions.av_log_set_callback, "av_log_set_callback", result))
    return false;
  if (!resolveFunction(lib, functions.av_log_set_level, "av_log_set_level", result))
    return false;
  if (!resolveFunction(lib, functions.av_pix_fmt_desc_get, "av_pix_fmt_desc_get", result))
    return false;
  if (!resolveFunction(lib, functions.av_pix_fmt_desc_next, "av_pix_fmt_desc_next", result))
    return false;
  if (!resolveFunction(lib, functions.av_pix_fmt_desc_get_id, "av_pix_fmt_desc_get_id", result))
    return false;

  result.addLogLine("Binding avUtil functions successfull.");
  return true;
}

bool bindLibraryFunctionsAndUpdateResult(LibraryLoader &                             lib,
                                         FFmpegLibraryFunctions::SwResampleFunction &functions,
                                         LibraryLoadingResult &                      result)
{
  if (!resolveFunction(lib, functions.swresample_version, "swresample_version", result))
    return false;

  result.addLogLine("Binding swresample functions successfull.");
  return true;
}

StringVec formatPossibleLibraryNames(std::string libraryName, int version)
{
  // The ffmpeg libraries are named using a major version number. E.g: avutil-55.dll on windows.
  // On linux, the libraries may be named differently. On Ubuntu they are named
  // libavutil-ffmpeg.so.55. On arch linux the name is libavutil.so.55. We will try to look for both
  // namings. On MAC os (installed with homebrew), there is a link to the lib named
  // libavutil.54.dylib.

  if (is_Q_OS_WIN)
    return {libraryName + "-" + std::to_string(version)};
  if (is_Q_OS_LINUX)
    return {"lib" + libraryName + "-ffmpeg.so." + std::to_string(version),
            "lib" + libraryName + ".so." + std::to_string(version)};
  if (is_Q_OS_MAC)
    return {"lib" + libraryName + "." + std::to_string(version) + ".dylib"};

  return {};
}

} // namespace

FFmpegLibraryFunctions::~FFmpegLibraryFunctions()
{
  this->unloadAllLibraries();
}

LibraryLoadingResult
FFmpegLibraryFunctions::loadFFmpegLibraryInPath(const std::filesystem::path &path,
                                                const LibraryVersion &       libraryVersion)
{
  // We will load the following libraries (in this order):
  // avutil, swresample, avcodec, avformat.

  LibraryLoadingResult loadingResult;

  std::filesystem::path absoluteDirectoryPath;
  if (!path.empty())
  {
    if (!std::filesystem::exists(path))
    {
      loadingResult.errorMessage = "The given path (" + path.string() + ") could not be found";
      return loadingResult;
    }

    absoluteDirectoryPath = std::filesystem::absolute(path);
    loadingResult.addLogLine("Using absolute path " + absoluteDirectoryPath.string());
  }

  this->unloadAllLibraries();
  loadingResult.addLogLine("Unload libraries");

  auto loadLibrary = [&loadingResult, &absoluteDirectoryPath](
                         LibraryLoader &lib, std::string libName, int version) {
    const auto libraryNames = formatPossibleLibraryNames(libName, version);
    for (const auto &possibleLibName : libraryNames)
    {
      const auto filePath = absoluteDirectoryPath / possibleLibName;
      const auto success  = lib.load(filePath);
      loadingResult.addLogLine("Loading library " + filePath.string() +
                               (success ? " succeded" : " failed"));
      return success;
    }
  };

  if (loadLibrary(this->libAvutil, "avutil", libraryVersion.avutil.major) &&
      loadLibrary(this->libSwresample, "swresample", libraryVersion.swresample.major) &&
      loadLibrary(this->libAvcodec, "avcodec", libraryVersion.avcodec.major) &&
      loadLibrary(this->libAvformat, "avformat", libraryVersion.avformat.major))
  {
    if (bindLibraryFunctionsAndUpdateResult(this->libAvformat, this->avformat, loadingResult) &&
        bindLibraryFunctionsAndUpdateResult(this->libAvcodec, this->avcodec, loadingResult) &&
        bindLibraryFunctionsAndUpdateResult(this->libAvutil, this->avutil, loadingResult) &&
        bindLibraryFunctionsAndUpdateResult(this->libSwresample, this->swresample, loadingResult))
    {
      loadingResult.success = true;
      loadingResult.addLogLine("Loading of ffmpeg libraries successfull");
      return loadingResult;
    }
  }

  this->unloadAllLibraries();
  loadingResult.addLogLine("Unload libraries");

  return loadingResult;
}

void FFmpegLibraryFunctions::unloadAllLibraries()
{
  this->libAvutil.unload();
  this->libSwresample.unload();
  this->libAvcodec.unload();
  this->libAvformat.unload();
}

LibraryPaths FFmpegLibraryFunctions::getLibraryPaths() const
{
  LibraryPaths paths;
  paths.avFormat   = this->libAvformat.getLibraryPath();
  paths.avCodec    = this->libAvcodec.getLibraryPath();
  paths.avUtil     = this->libAvutil.getLibraryPath();
  paths.swResample = this->libSwresample.getLibraryPath();
  return paths;
}

} // namespace FFmpeg
