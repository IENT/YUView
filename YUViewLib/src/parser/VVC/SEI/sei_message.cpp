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

#include "buffering_period.h"
#include "pic_timing.h"
#include "decoding_unit_info.h"
#include "subpic_level_info.h"
#include "scalable_nesting.h"

namespace parser::vvc
{

using namespace parser::reader;

void sei_message::parse(ReaderHelperNew &                 reader,
                        NalType                           nal_unit_type,
                        unsigned                          nalTemporalID,
                        std::shared_ptr<buffering_period> lastBufferingPeriod)
{
  ReaderHelperNewSubLevel subLevel(reader, "sei_message");

  unsigned payloadtype_byte;
  do
  {
    payloadtype_byte = reader.readBits("payloadtype_byte", 8);
    this->payloadType += payloadtype_byte;
  } while (payloadtype_byte == 0xFF);

  unsigned payload_size_byte;
  do
  {
    payload_size_byte = reader.readBits("payload_size_byte", 8);
    this->payloadSize += payload_size_byte;
  } while (payload_size_byte == 0xFF);

  if (nal_unit_type == NalType::PREFIX_SEI_NUT)
  {
    if (this->payloadType == 0)
    {
      auto newBufferPeriod = std::make_shared<buffering_period>();
      newBufferPeriod->parse(reader);
      this->sei_payload_instance = newBufferPeriod;
    }
    else if (this->payloadType == 1)
    {
      auto newPicTiming = std::make_shared<pic_timing>();
      newPicTiming->parse(reader, nalTemporalID, lastBufferingPeriod);
      this->sei_payload_instance = newPicTiming;
    }
    // else if (this->payloadType == 3)
    // {
    //   this->filler_payload_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 4)
    // {
    //   this->user_data_registered_itu_t_t35_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 5)
    // {
    //   this->user_data_unregistered_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 19)
    // {
    //   this->film_grain_characteristics_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 45)
    // {
    //   this->frame_packing_arrangement_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 129)
    // {
    //   this->parameter_sets_inclusion_indication_instance.parse(reader, payloadSize);
    // }
    else if (this->payloadType == 130)
    {
      auto newDecodingUnitInfo = std::make_shared<decoding_unit_info>();
      newDecodingUnitInfo->parse(reader, nalTemporalID, lastBufferingPeriod);
      this->sei_payload_instance = newDecodingUnitInfo;
    }
    // else if (this->payloadType == 133)
    // {
    //   this->scalable_nesting_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 137)
    // {
    //   this->mastering_display_colour_volume_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 144)
    // {
    //   this->content_light_level_info_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 145)
    // {
    //   this->dependent_rap_indication_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 147)
    // {
    //   this->alternative_transfer_characteristics_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 148)
    // {
    //   this->ambient_viewing_environment_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 149)
    // {
    //   this->content_colour_volume_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 150)
    // {
    //   this->equirectangular_projection_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 153)
    // {
    //   this->generalized_cubemap_projection_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 154)
    // {
    //   this->sphere_rotation_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 155)
    // {
    //   this->regionwise_packing_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 156)
    // {
    //   this->omni_viewport_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 168)
    // {
    //   this->frame_field_info_instance.parse(reader, payloadSize);
    // }
    else if (this->payloadType == 203)
    {
      auto newSubpicLevelInfo = std::make_shared<subpic_level_info>();
      newSubpicLevelInfo->parse(reader);
      this->sei_payload_instance = newSubpicLevelInfo;
    }
    // else if (this->payloadType == 204)
    // {
    //   this->sample_aspect_ratio_info_instance.parse(reader, payloadSize);
    // }
    else
    {
      // reserved_message
      throw std::logic_error("Not implemented yet");
    }
  }
  else // Suffix SEI
  {
    // if (this->payloadType == 3)
    // {
    //   this->filler_payload_instance.parse(reader, payloadSize);
    // }
    // else if (this->payloadType == 132)
    // {
    //   this->decoded_picture_hash_instance.parse(reader, payloadSize);
    // }
    if (this->payloadType == 133)
    {
      auto newScalableNesting = std::make_shared<scalable_nesting>();
      newScalableNesting->parse(reader, nal_unit_type, nalTemporalID, lastBufferingPeriod);
      this->sei_payload_instance = newScalableNesting;
    }
    // else
    // {
    //   // reserved_message
    //   throw std::logic_error("Not implemented yet");
    // }
    throw std::logic_error("Not implemented yet");
  }

  auto more_data_in_payload = !(reader.byte_aligned() && reader.nrBytesRead() >= this->payloadSize);
  if (more_data_in_payload)
  {
    // TODO
    // if (this->payload_extension_present())
    // {
    //   this->sei_reserved_payload_extension_data =
    //       reader.readBits("sei_reserved_payload_extension_data", unknown)
    // }
    // this->sei_payload_bit_equal_to_one /* equal to 1 */ =
    //     reader.readFlag("sei_payload_bit_equal_to_one /* equal to 1 */");
    // while (!byte_aligned())
    // {
    //   this->sei_payload_bit_equal_to_zero /* equal to 0 */ =
    //       reader.readFlag("sei_payload_bit_equal_to_zero /* equal to 0 */");
    // }
  }
}

} // namespace parser::vvc
