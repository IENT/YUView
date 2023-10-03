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

#include "AvUtilFunctions.h"

#include "Functions.h"

namespace LibFFmpeg::functions
{

std::optional<AvUtilFunctions> tryBindAVUtilFunctionsFromLibrary(SharedLibraryLoader &lib, Log &log)
{
  if (!lib)
  {
    log.push_back("Binding of avUtil functions failed. Library is not loaded.");
    return {};
  }

  AvUtilFunctions functions;

  lib.tryResolveFunction(functions.avutil_version, "avutil_version");
  lib.tryResolveFunction(functions.av_frame_alloc, "av_frame_alloc");
  lib.tryResolveFunction(functions.av_frame_free, "av_frame_free");
  lib.tryResolveFunction(functions.av_mallocz, "av_mallocz");
  lib.tryResolveFunction(functions.av_dict_set, "av_dict_set");
  lib.tryResolveFunction(functions.av_dict_get, "av_dict_get");
  lib.tryResolveFunction(functions.av_frame_get_side_data, "av_frame_get_side_data");
  lib.tryResolveFunction(functions.av_frame_get_metadata, "av_frame_get_metadata");
  lib.tryResolveFunction(functions.av_log_set_callback, "av_log_set_callback");
  lib.tryResolveFunction(functions.av_log_set_level, "av_log_set_level");
  lib.tryResolveFunction(functions.av_pix_fmt_desc_get, "av_pix_fmt_desc_get");
  lib.tryResolveFunction(functions.av_pix_fmt_desc_next, "av_pix_fmt_desc_next");
  lib.tryResolveFunction(functions.av_pix_fmt_desc_get_id, "av_pix_fmt_desc_get_id");

  std::vector<std::string> missingFunctions;

  checkForMissingFunctionAndLog(functions.avutil_version, "avutil_version", missingFunctions, log);
  if (!functions.avutil_version)
  {
    log.push_back("Binding avutil functions failed.Missing function avutil_version");
    return {};
  }

  const auto version = Version::fromFFmpegVersion(functions.avutil_version());

  checkForMissingFunctionAndLog(functions.av_frame_alloc, "av_frame_alloc", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_frame_free, "av_frame_free", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_frame_free, "av_frame_free", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_mallocz, "av_mallocz", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_dict_set, "av_dict_set", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_dict_get, "av_dict_get", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_frame_get_side_data, "av_frame_get_side_data", missingFunctions, log);

  if (version.major < 57)
    checkForMissingFunctionAndLog(
        functions.av_frame_get_metadata, "av_frame_get_metadata", missingFunctions, log);

  checkForMissingFunctionAndLog(
      functions.av_log_set_callback, "av_log_set_callback", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_log_set_level, "av_log_set_level", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_pix_fmt_desc_get, "av_pix_fmt_desc_get", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_pix_fmt_desc_next, "av_pix_fmt_desc_next", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_pix_fmt_desc_get_id, "av_pix_fmt_desc_get_id", missingFunctions, log);

  if (!missingFunctions.empty())
  {
    log.push_back("Binding avUtil functions failed. Missing functions: " +
                  to_string(missingFunctions));
    return {};
  }

  log.push_back("Binding of avUtil functions successful.");
  return functions;
}

} // namespace LibFFmpeg::functions
