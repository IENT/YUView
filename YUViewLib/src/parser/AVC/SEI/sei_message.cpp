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

#include "sei_message.h"

#include "../Typedef.h"
#include "buffering_period.h"
#include "pic_timing.h"
#include "unknown_sei.h"
#include "user_data_unregistered.h"

namespace parser::avc
{

using namespace reader;

namespace
{

auto payloadNameMap = MeaningMap({{0, "buffering_period"},
                                  {1, "pic_timing"},
                                  {2, "pan_scan_rect"},
                                  {3, "filler_payload"},
                                  {4, "user_data_registered_itu_t_t35"},
                                  {5, "user_data_unregistered"},
                                  {6, "recovery_point"},
                                  {7, "dec_ref_pic_marking_repetition"},
                                  {8, "spare_pic"},
                                  {9, "scene_info"},
                                  {10, "sub_seq_info"},
                                  {11, "sub_seq_layer_characteristics"},
                                  {12, "sub_seq_characteristics"},
                                  {13, "full_frame_freeze"},
                                  {14, "full_frame_freeze_release"},
                                  {15, "full_frame_snapshot"},
                                  {16, "progressive_refinement_segment_start"},
                                  {17, "progressive_refinement_segment_end"},
                                  {18, "motion_constrained_slice_group_set"},
                                  {19, "film_grain_characteristics"},
                                  {20, "deblocking_filter_display_preference"},
                                  {21, "stereo_video_info"},
                                  {22, "post_filter_hint"},
                                  {23, "tone_mapping_info"},
                                  {24, "scalability_info"},
                                  {25, "sub_pic_scalable_layer"},
                                  {26, "non_required_layer_rep"},
                                  {27, "priority_layer_info"},
                                  {28, "layers_not_present"},
                                  {29, "layer_dependency_change"},
                                  {30, "scalable_nesting"},
                                  {31, "base_layer_temporal_hrd"},
                                  {32, "quality_layer_integrity_check"},
                                  {33, "redundant_pic_property"},
                                  {34, "tl0_dep_rep_index"},
                                  {35, "tl_switching_point"},
                                  {36, "parallel_decoding_info"},
                                  {37, "mvc_scalable_nesting"},
                                  {38, "view_scalability_info"},
                                  {39, "multiview_scene_info"},
                                  {40, "multiview_acquisition_info"},
                                  {41, "non_required_view_component"},
                                  {42, "view_dependency_change"},
                                  {43, "operation_points_not_present"},
                                  {44, "base_view_temporal_hrd"},
                                  {45, "frame_packing_arrangement"},
                                  {46, "multiview_view_position"},
                                  {47, "display_orientation"},
                                  {48, "mvcd_scalable_nesting"},
                                  {49, "mvcd_view_scalability_info"},
                                  {50, "depth_representation_info"},
                                  {51, "three_dimensional_reference_displays_info"},
                                  {52, "depth_timing"},
                                  {53, "depth_sampling_info"},
                                  {54, "constrained_depth_parameter_set_identifier"}});

}

SEIParsingResult sei_message::parse(reader::SubByteReaderLogging &          reader,
                                    SPSMap &                                spsMap,
                                    std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "sei_message()");

  this->payloadType = 0;
  {
    unsigned byte;
    do
    {
      byte = reader.readBits(
          "last_payload_type_byte",
          8,
          Options().withMeaningMap({{255, "ff_byte"}}).withMeaning("last_payload_type_byte"));
      this->payloadType += byte;
    } while (byte == 255);
  }

  reader.logCalculatedValue(
      "payloadType",
      this->payloadType,
      Options().withMeaningMap(payloadNameMap).withMeaning("reserved_sei_message"));

  this->payloadSize = 0;
  {
    unsigned byte;
    do
    {
      byte = reader.readBits(
          "last_payload_size_byte",
          8,
          Options().withMeaningMap({{255, "ff_byte"}}).withMeaning("last_payload_size_byte"));
      this->payloadSize += byte;
    } while (byte == 255);
  }
  reader.logCalculatedValue("payloadSize", this->payloadSize);

  // For reparsing, first save all data and then call the same function that we will (possibly) also
  // call for reparsing.
  auto payloadData = reader.readBytes("", this->payloadSize, Options().withLoggingDisabled());
  auto currentLoggingTreeItem = reader.getCurrentItemTree();
  this->payloadReader         = SubByteReaderLogging(payloadData, currentLoggingTreeItem);

  // When reading the data above, emulation prevention was alread removed.
  this->payloadReader.disableEmulationPrevention();

  return this->parsePayloadData(false, spsMap, associatedSPS);
}

SEIParsingResult sei_message::reparse(SPSMap &                                spsMap,
                                      std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  return this->parsePayloadData(true, spsMap, associatedSPS);
}

std::string sei_message::getPayloadTypeName() const
{
  if (payloadNameMap.count(this->payloadType) == 0)
    return "unknown";
  return payloadNameMap.at(this->payloadType);
}

SEIParsingResult sei_message::parsePayloadData(
    bool reparse, SPSMap &spsMap, std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  if (this->parsingDone)
    throw std::logic_error("Parsing of SEI is already done");

  if (this->payloadType == 0)
    this->payload = std::make_shared<buffering_period>();
  else if (this->payloadType == 1)
    this->payload = std::make_shared<pic_timing>();
  else if (this->payloadType == 5)
    this->payload = std::make_shared<user_data_unregistered>();
  else
    this->payload = std::make_shared<unknown_sei>();

  auto result = this->payload->parse(this->payloadReader, reparse, spsMap, associatedSPS);

  if (result == SEIParsingResult::OK)
    this->parsingDone = true;
  if (reparse && result == SEIParsingResult::WAIT_FOR_PARAMETER_SETS)
    throw std::logic_error("Reparsing of SEI failed");

  return result;
}

} // namespace parser::avc