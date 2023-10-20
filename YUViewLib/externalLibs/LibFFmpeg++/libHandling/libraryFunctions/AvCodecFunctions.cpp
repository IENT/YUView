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

#include "AvCodecFunctions.h"

#include "Functions.h"

namespace LibFFmpeg::functions
{

std::optional<AvCodecFunctions> tryBindAVCodecFunctionsFromLibrary(SharedLibraryLoader &lib,
                                                                   Log &                log)
{
  if (!lib)
  {
    log.push_back("Binding of avCodec functions failed. Library is not loaded.");
    return {};
  }

  AvCodecFunctions functions;

  lib.tryResolveFunction(functions.avcodec_find_decoder, "avcodec_find_decoder");
  lib.tryResolveFunction(functions.avcodec_alloc_context3, "avcodec_alloc_context3");
  lib.tryResolveFunction(functions.avcodec_open2, "avcodec_open2");
  lib.tryResolveFunction(functions.avcodec_free_context, "avcodec_free_context");
  lib.tryResolveFunction(functions.av_packet_alloc, "av_packet_alloc");
  lib.tryResolveFunction(functions.av_packet_free, "av_packet_free");
  lib.tryResolveFunction(functions.av_init_packet, "av_init_packet");
  lib.tryResolveFunction(functions.av_packet_unref, "av_packet_unref");
  lib.tryResolveFunction(functions.avcodec_flush_buffers, "avcodec_flush_buffers");
  lib.tryResolveFunction(functions.avcodec_version, "avcodec_version");
  lib.tryResolveFunction(functions.avcodec_get_name, "avcodec_get_name");
  lib.tryResolveFunction(functions.avcodec_parameters_alloc, "avcodec_parameters_alloc");
  lib.tryResolveFunction(functions.avcodec_send_packet, "avcodec_send_packet");
  lib.tryResolveFunction(functions.avcodec_receive_frame, "avcodec_receive_frame");
  lib.tryResolveFunction(functions.avcodec_parameters_to_context, "avcodec_parameters_to_context");
  lib.tryResolveFunction(functions.avcodec_decode_video2, "avcodec_decode_video2");

  std::vector<std::string> missingFunctions;

  checkForMissingFunctionAndLog(
      functions.avcodec_find_decoder, "avcodec_find_decoder", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_alloc_context3, "avcodec_alloc_context3", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.avcodec_open2, "avcodec_open2", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_free_context, "avcodec_free_context", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_packet_alloc, "av_packet_alloc", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_packet_free, "av_packet_free", missingFunctions, log);
  checkForMissingFunctionAndLog(functions.av_init_packet, "av_init_packet", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.av_packet_unref, "av_packet_unref", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_flush_buffers, "avcodec_flush_buffers", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_version, "avcodec_version", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_get_name, "avcodec_get_name", missingFunctions, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_parameters_alloc, "avcodec_parameters_alloc", missingFunctions, log);

  std::vector<std::string> missingFunctionsNewApi;

  checkForMissingFunctionAndLog(
      functions.avcodec_send_packet, "avcodec_send_packet", missingFunctionsNewApi, log);
  checkForMissingFunctionAndLog(
      functions.avcodec_receive_frame, "avcodec_receive_frame", missingFunctionsNewApi, log);
  checkForMissingFunctionAndLog(functions.avcodec_parameters_to_context,
                                "avcodec_parameters_to_context",
                                missingFunctionsNewApi,
                                log);

  if (missingFunctionsNewApi.empty())
  {
    log.push_back("New Decoding API found. Skipping check for old API function.");
    functions.newParametersAPIAvailable = true;
  }
  else
  {
    log.push_back("New Decoding API not found. Missing functions " +
                  to_string(missingFunctionsNewApi) + ". Checking old decoding API.");
    checkForMissingFunctionAndLog(
        functions.avcodec_decode_video2, "avcodec_decode_video2", missingFunctions, log);
  }

  if (!missingFunctions.empty())
  {
    log.push_back("Binding avCodec functions failed. Missing functions: " +
                  to_string(missingFunctions));
    return {};
  }

  log.push_back("Binding of avCodec functions successful.");
  return functions;
}

} // namespace LibFFmpeg::functions
