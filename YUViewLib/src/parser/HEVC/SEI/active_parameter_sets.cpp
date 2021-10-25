/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "active_parameter_sets.h"

#include "../video_parameter_set_rbsp.h"
#include <parser/common/functions.h>

namespace parser::hevc
{

using namespace reader;

SEIParsingResult active_parameter_sets::parse(SubByteReaderLogging &                  reader,
                                              bool                                    reparse,
                                              VPSMap &                                vpsMap,
                                              SPSMap &                                spsMap,
                                              std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)spsMap;
  (void)associatedSPS;

  std::unique_ptr<SubByteReaderLoggingSubLevel> subLevel;
  if (reparse)
    reader.stashAndReplaceCurrentTreeItem(this->reparseTreeItem);
  else
    subLevel.reset(new SubByteReaderLoggingSubLevel(reader, "active_parameter_sets()"));

  this->active_video_parameter_set_id = reader.readBits("active_video_parameter_set_id", 4);
  this->self_contained_cvs_flag       = reader.readFlag("self_contained_cvs_flag");
  this->no_parameter_set_update_flag  = reader.readFlag("no_parameter_set_update_flag");
  this->num_sps_ids_minus1            = reader.readUEV("num_sps_ids_minus1");
  for (unsigned i = 0; i <= this->num_sps_ids_minus1; i++)
    this->active_seq_parameter_set_id.push_back(
        reader.readUEV(formatArray("active_seq_parameter_set_id", i)));

  if (!reparse)
  {
    if (vpsMap.count(this->active_video_parameter_set_id) == 0)
    {
      this->reparseTreeItem = reader.getCurrentItemTree();
      return SEIParsingResult::WAIT_FOR_PARAMETER_SETS;
    }
  }
  else
  {
    if (vpsMap.count(this->active_video_parameter_set_id) == 0)
      throw std::logic_error("VPS with the given active_video_parameter_set_id not found");
  }
  auto vps = vpsMap.at(this->active_video_parameter_set_id);

  unsigned int MaxLayersMinus1 = std::min((unsigned int)62, vps->vps_max_layers_minus1);
  for (unsigned int i = (vps->vps_base_layer_internal_flag ? 1 : 0); i <= MaxLayersMinus1; i++)
    this->layer_sps_idx.push_back(reader.readUEV(formatArray("layer_sps_idx", i)));

  reader.popTreeItem();
  return SEIParsingResult::OK;
}

} // namespace parser::hevc