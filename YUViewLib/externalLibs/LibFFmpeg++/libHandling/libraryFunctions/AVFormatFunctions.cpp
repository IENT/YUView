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

#include "AVFormatFunctions.h"

#include "Functions.h"

namespace LibFFmpeg::functions
{

std::optional<AvFormatFunctions> tryBindFunctionsFromLibrary(SharedLibraryLoader &lib, Log &log)
{
  if (!lib)
  {
    log.push_back("Binding of avFormat functions failed. Library is not loaded.");
    return {};
  }

  AvFormatFunctions functions;

  lib.tryResolveFunction(functions.avformat_version, "avformat_version");
  lib.tryResolveFunction(functions.av_register_all, "av_register_all");
  lib.tryResolveFunction(functions.avformat_open_input, "avformat_open_input");
  lib.tryResolveFunction(functions.avformat_close_input, "avformat_close_input");
  lib.tryResolveFunction(functions.avformat_find_stream_info, "avformat_find_stream_info");
  lib.tryResolveFunction(functions.av_read_frame, "av_read_frame");
  lib.tryResolveFunction(functions.av_seek_frame, "av_seek_frame");

  std::vector<std::string> missingFunctions;

  checkForMissingFunctionAndLog(
      functions.avformat_version, "avformat_version", missingFunctions, log);

  if (!functions.avformat_version)
  {
    log.push_back("Binding avFormat functions failed. Missing function avformat_version");
    return {};
  }

  const auto version = Version::fromFFmpegVersion(functions.avformat_version());

  if (version.major < 59)
    checkForMissingFunctionAndLog(
        functions.av_register_all, "av_register_all", missingFunctions, log);

  checkForMissingFunctionAndLog(
      functions.avformat_open_input, "avformat_open_input", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avformat_close_input, "avformat_close_input", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avformat_find_stream_info, "avformat_find_stream_info", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_read_frame, "av_read_frame", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_seek_frame, "av_seek_frame", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avformat_version, "avformat_version", missingFunctions, log);

  if (!missingFunctions.empty())
  {
    log.push_back("Binding avFormat functions failed. Missing functions: " +
                  to_string(missingFunctions));
    return {};
  }

  log.push_back("Binding of avFormat functions successful.");
  return functions;
}

} // namespace LibFFmpeg::functions
