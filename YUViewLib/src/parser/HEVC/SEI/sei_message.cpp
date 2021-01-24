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

#include "active_parameter_sets.h"
#include "alternative_transfer_characteristics.h"
#include "buffering_period.h"
#include "content_light_level_info.h"
#include "mastering_display_colour_volume.h"
#include "parser/common/SubByteReaderLoggingOptions.h"
#include "parser/common/functions.h"
#include "pic_timing.h"
#include "user_data_unregistered.h"

namespace parser::hevc
{

namespace
{

auto prefixSEINameMap = reader::MeaningMap({{0, "buffering_period"},
                                            {1, "pic_timing"},
                                            {2, "pan_scan_rect"},
                                            {3, "filler_payload"},
                                            {4, "user_data_registered_itu_t_t35"},
                                            {5, "user_data_unregistered"},
                                            {6, "recovery_point"},
                                            {9, "scene_info"},
                                            {15, "picture_snapshot"},
                                            {16, "progressive_refinement_segment_start"},
                                            {17, "progressive_refinement_segment_end"},
                                            {19, "film_grain_characteristics"},
                                            {22, "post_filter_hint"},
                                            {23, "tone_mapping_info"},
                                            {45, "frame_packing_arrangement"},
                                            {47, "display_orientation"},
                                            {56, "green_metadata"},
                                            {128, "structure_of_pictures_info"},
                                            {129, "active_parameter_sets"},
                                            {130, "decoding_unit_info"},
                                            {131, "temporal_sub_layer_zero_index"},
                                            {133, "scalable_nesting"},
                                            {134, "region_refresh_info"},
                                            {135, "no_display"},
                                            {136, "time_code"},
                                            {137, "mastering_display_colour_volume"},
                                            {138, "segmented_rect_frame_packing_arrangement"},
                                            {139, "temporal_motion_constrained_tile_sets"},
                                            {140, "chroma_resampling_filter_hint"},
                                            {141, "knee_function_info"},
                                            {142, "colour_remapping_info"},
                                            {143, "deinterlaced_field_identification"},
                                            {144, "content_light_level_info"},
                                            {145, "dependent_rap_indication"},
                                            {146, "coded_region_completion"},
                                            {147, "alternative_transfer_characteristics"},
                                            {148, "ambient_viewing_environment"},
                                            {160, "layers_not_present"},
                                            {161, "inter_layer_constrained_tile_sets"},
                                            {162, "bsp_nesting"},
                                            {163, "bsp_initial_arrival_time"},
                                            {164, "sub_bitstream_property"},
                                            {165, "alpha_channel_info"},
                                            {166, "overlay_info"},
                                            {167, "temporal_mv_prediction_constraints"},
                                            {168, "frame_field_info"},
                                            {176, "three_dimensional_reference_displays_info"},
                                            {177, "depth_representation_info"},
                                            {178, "multiview_scene_info"},
                                            {179, "multiview_acquisition_info"},
                                            {180, "multiview_view_position"},
                                            {181, "alternative_depth_info"}});

auto suffixSEINameMap = reader::MeaningMap({{3, "filler_payload"},
                                            {4, "user_data_registered_itu_t_t35"},
                                            {5, "user_data_unregistered"},
                                            {17, "progressive_refinement_segment_end"},
                                            {22, "post_filter_hint"},
                                            {132, "decoded_picture_hash"},
                                            {146, "coded_region_completion"}});

} // namespace

using namespace parser::reader;

SEIParsingResult unknown_sei::parse(reader::SubByteReaderLogging &          reader,
                                    bool                                    reparse,
                                    VPSMap &                                vpsMap,
                                    SPSMap &                                spsMap,
                                    std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)reparse;
  (void)vpsMap;
  (void)spsMap;
  (void)associatedSPS;

  SubByteReaderLoggingSubLevel subLevel(reader, "unknown_sei()");

  unsigned i = 0;
  while (reader.canReadBits(8))
  {
    reader.readBytes(formatArray("raw_byte", i++), 8);
  }

  return SEIParsingResult::OK;
}

SEIParsingResult sei_message::parse(reader::SubByteReaderLogging &          reader,
                                    VPSMap &                                vpsMap,
                                    SPSMap &                                spsMap,
                                    std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
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

    reader.logCalculatedValue("payloadType",
                              this->payloadType,
                              Options()
                                  .withMeaningMap(this->seiNalUnitType == NalType::PREFIX_SEI_NUT
                                                      ? prefixSEINameMap
                                                      : suffixSEINameMap)
                                  .withMeaning("reserved_sei_message"));

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
  }

  // For reparsing, first save all data and then call the same function that we will (possibly) also
  // call for reparsing.
  auto payloadData = reader.readBytes("", this->payloadSize, Options().withLoggingDisabled());
  auto currentLoggingTreeItem = reader.getCurrentItemTree();
  this->payloadReader         = SubByteReaderLogging(payloadData, currentLoggingTreeItem);

  // When reading the data above, emulation prevention was alread removed.
  this->payloadReader.disableEmulationPrevention();

  return this->parsePayloadData(false, vpsMap, spsMap, associatedSPS);
}

SEIParsingResult sei_message::reparse(VPSMap &                                vpsMap,
                                      SPSMap &                                spsMap,
                                      std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  return this->parsePayloadData(true, vpsMap, spsMap, associatedSPS);
}

std::string sei_message::getPayloadTypeName() const
{
  auto list =
      (this->seiNalUnitType == NalType::PREFIX_SEI_NUT ? prefixSEINameMap : suffixSEINameMap);
  if (list.count(this->payloadType) == 0)
    return "unknown";
  return list.at(this->payloadType);
}

SEIParsingResult
sei_message::parsePayloadData(bool                                    reparse,
                              VPSMap &                                vpsMap,
                              SPSMap &                                spsMap,
                              std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  if (this->parsingDone)
    throw std::logic_error("Parsing of SEI is already done");

  if (!reparse)
  {
    if (this->seiNalUnitType == NalType::PREFIX_SEI_NUT)
    {
      if (this->payloadType == 0)
        this->payload = std::make_shared<buffering_period>();
      else if (this->payloadType == 1)
        this->payload = std::make_shared<pic_timing>();
      else if (this->payloadType == 5)
        this->payload = std::make_shared<user_data_unregistered>();
      else if (this->payloadType == 129)
        this->payload = std::make_shared<active_parameter_sets>();
      else if (this->payloadType == 137)
        this->payload = std::make_shared<mastering_display_colour_volume>();
      else if (this->payloadType == 144)
        this->payload = std::make_shared<content_light_level_info>();
      else if (this->payloadType == 147)
        this->payload = std::make_shared<alternative_transfer_characteristics>();
      else
        this->payload = std::make_shared<unknown_sei>();
    }
    else
    {
      if (this->payloadType == 5)
        this->payload = std::make_shared<user_data_unregistered>();
      else
        this->payload = std::make_shared<unknown_sei>();
    }
  }

  auto result = this->payload->parse(this->payloadReader, reparse, vpsMap, spsMap, associatedSPS);

  if (result == SEIParsingResult::OK)
    this->parsingDone = true;
  if (reparse && result == SEIParsingResult::WAIT_FOR_PARAMETER_SETS)
    throw std::logic_error("Reparsing of SEI failed");

  return result;
}

} // namespace parser::hevc
