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

#include <common/Expected.h>
#include <common/FFMpegLibrariesTypes.h>
#include <libHandling/SharedLibraryLoader.h>
#include <libHandling/libraryFunctions/AVFormatFunctions.h>
#include <libHandling/libraryFunctions/AvCodecFunctions.h>
#include <libHandling/libraryFunctions/AvUtilFunctions.h>
#include <libHandling/libraryFunctions/SwResampleFunctions.h>

#include <filesystem>

namespace LibFFmpeg
{

struct LibraryInfo
{
  std::string           name;
  std::filesystem::path path;
  std::string           version;
};

class FFmpegLibrariesInterface
{
public:
  FFmpegLibrariesInterface()  = default;
  ~FFmpegLibrariesInterface() = default;

  using LoadingResultAndLog = std::pair<bool, Log>;
  LoadingResultAndLog tryLoadFFmpegLibrariesInPath(const std::filesystem::path &path);

  std::vector<LibraryInfo> getLibrariesInfo() const;

  functions::AvFormatFunctions   avformat{};
  functions::AvCodecFunctions    avcodec{};
  functions::AvUtilFunctions     avutil{};
  functions::SwResampleFunctions swresample{};

private:
  bool
  tryLoadLibrariesBindFunctionsAndCheckVersions(const std::filesystem::path &absoluteDirectoryPath,
                                                const LibraryVersions &      libraryVersions,
                                                Log &                        log);

  void unloadAllLibraries();

  SharedLibraryLoader libAvutil;
  SharedLibraryLoader libSwresample;
  SharedLibraryLoader libAvcodec;
  SharedLibraryLoader libAvformat;

  LibraryVersions libraryVersions{};

  static std::string logListFFmpeg;
  static void        avLogCallback(void *ptr, int level, const char *fmt, va_list vargs);

  friend class FFmpegLibrariesInterfaceBuilder;
};

} // namespace LibFFmpeg
