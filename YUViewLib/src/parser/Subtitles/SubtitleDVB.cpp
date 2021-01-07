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

#include "SubtitleDVB.h"

#include "parser/common/SubByteReaderLogging.h"
#include "parser/common/functions.h"

#include <stdexcept>

namespace
{

using namespace parser::reader;

void parsePageCompositionSegment(SubByteReaderLogging &reader, unsigned segment_length)
{
  SubByteReaderLoggingSubLevel s(reader, "page_composition_segment()");

  reader.readBits("page_time_out", 8);
  reader.readBits("page_version_number", 4);
  reader.readBits("page_state", 2);
  reader.readBits("reserved", 2);

  auto processed_length = 2u;
  while (processed_length < segment_length)
  {
    reader.readBits("region_id", 8);
    reader.readBits("reserved", 8);
    reader.readBits("region_horizontal_address", 8);
    reader.readBits("region_vertical_address", 8);
    processed_length += 6;
  }
}

void parseRegionCompositionSegment(SubByteReaderLogging &reader, unsigned segment_length)
{
  SubByteReaderLoggingSubLevel s(reader, "region_composition_segment()");

  reader.readBits("region_id", 8);
  reader.readBits("region_version_number", 4);
  reader.readFlag("region_fill_flag");
  reader.readBits("reserved", 3);
  reader.readBits("region_width", 16);
  reader.readBits("region_height", 16);
  reader.readBits("region_level_of_compatibility", 3);
  reader.readBits("region_depth", 3);
  reader.readBits("reserved", 2);
  reader.readBits("CLUT_id", 8);
  reader.readBits("region_8_bit_pixel_code", 8);
  reader.readBits("region_4_bit_pixel_code", 4);
  reader.readBits("region_2_bit_pixel_code", 2);
  reader.readBits("reserved", 2);

  unsigned processed_length = 10;
  while (processed_length < segment_length)
  {
    reader.readBits("object_id", 16);
    auto object_type = reader.readBits("object_type", 2);
    reader.readBits("object_provider_flag", 2);
    reader.readBits("object_horizontal_position", 12);
    reader.readBits("reserved", 4);
    reader.readBits("object_vertical_position", 12);
    processed_length += 6;

    if (object_type == 0x01 || object_type == 0x02)
    {
      reader.readBits("foreground_pixel_code", 8);
      reader.readBits("background_pixel_code", 8);
      processed_length += 2;
    }
  }
}

void parseCLUTDefinitionSegment(SubByteReaderLogging &reader, unsigned segment_length)
{
  SubByteReaderLoggingSubLevel s(reader, "CLUT_definition_segment()");

  reader.readBits("CLUT_id", 8);
  reader.readBits("CLUT_version_number", 4);
  reader.readBits("reserved", 4);

  auto processed_length = 2u;
  while (processed_length < segment_length)
  {
    reader.readBits("CLUT_entry_id", 8);
    reader.readFlag("flag_2_bit_entry_CLUT");
    reader.readFlag("flag_4_bit_entry_CLUT");
    reader.readFlag("flag_8_bit_entry_CLUT");
    reader.readBits("reserved", 4);
    auto full_range_flag = reader.readFlag("full_range_flag");
    processed_length += 2;

    if (full_range_flag)
    {
      reader.readBits("Y_value", 8);
      reader.readBits("Cr_value", 8);
      reader.readBits("Cb_value", 8);
      reader.readBits("T_value", 8);
      processed_length += 4;
    }
    else
    {
      reader.readBits("Y_value", 6);
      reader.readBits("Cr_value", 4);
      reader.readBits("Cb_value", 4);
      reader.readBits("T_value", 2);
      processed_length += 2;
    }
  }
}

std::tuple<bool, unsigned> parse_2_bit_pixel_code_string(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "2-bit/pixel_code_string()");

  bool end      = false;
  auto bitsRead = 0u;

  auto next_bits = reader.readBits("next_bits", 2);
  bitsRead += 2;

  if (next_bits == 0)
  {
    auto switch_1 = reader.readFlag("switch_1");
    bitsRead++;
    if (switch_1)
    {
      reader.readBits("run_length_3_10", 3);
      reader.readBits("two_bit_pixel_code", 2);
      bitsRead += 5;
    }
    else
    {
      auto switch_2 = reader.readFlag("switch_2");
      bitsRead++;
      if (!switch_2)
      {
        auto switch_3 = reader.readBits("switch_3", 2);
        bitsRead += 2;
        if (switch_3 == 0)
        {
          end = true;
        }
        if (switch_3 == 2)
        {
          reader.readBits("run_length_12_27", 4);
          reader.readBits("two_bit_pixel_code", 2);
          bitsRead += 6;
        }
        if (switch_3 == 3)
        {
          reader.readBits("run_length_29_284", 8);
          reader.readBits("two_bit_pixel_code", 2);
          bitsRead += 10;
        }
      }
    }
  }

  return {end, bitsRead};
}

std::tuple<bool, unsigned> parse_4_bit_pixel_code_string(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "4-bit/pixel_code_string()");

  bool end      = false;
  auto bitsRead = 0u;

  auto next_bits = reader.readBits("next_bits", 4);
  bitsRead += 4;
  if (next_bits == 0)
  {
    auto switch_1 = reader.readFlag("switch_1");
    bitsRead++;
    if (!switch_1)
    {
      reader.readBits("next_bits", 3);
      bitsRead += 3;
      if (next_bits == 0)
        end = true;
    }
    else
    {
      auto switch_2 = reader.readFlag("switch_2");
      bitsRead++;
      if (!switch_2)
      {
        reader.readBits("run_length_4_7", 2);
        reader.readBits("four_bit_pixel_code", 4);
        bitsRead += 6;
      }
      else
      {
        auto switch_3 = reader.readBits("switch_3", 2);
        bitsRead += 2;
        if (switch_3 == 2)
        {
          reader.readBits("run_length_9_24", 4);
          reader.readBits("four_bit_pixel_code", 4);
          bitsRead += 8;
        }
        if (switch_3 == 3)
        {
          reader.readBits("run_length_25_280", 8);
          reader.readBits("four_bit_pixel_code", 4);
          bitsRead += 12;
        }
      }
    }
  }

  return {end, bitsRead};
}

std::tuple<bool, unsigned> parse_8_bit_pixel_code_string(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "8-bit/pixel_code_string()");

  bool end      = false;
  auto bitsRead = 0u;

  auto next_bits = reader.readBits("next_bits", 8);
  bitsRead += 8;
  if (next_bits == 0)
  {
    auto switch_1 = reader.readFlag("switch_1");
    bitsRead++;
    if (!switch_1)
    {
      reader.readBits("next_bits", 7);
      bitsRead += 7;
      if (next_bits == 0)
        end = true;
    }
    else
    {
      reader.readBits("run_length_3_127", 7);
      reader.readBits("eight_bit_pixel_code", 8);
      bitsRead += 15;
    }
  }

  return {end, bitsRead};
}

unsigned parsePixelDataSubBlock(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "pixel-data_sub-block()");

  unsigned data_type = reader.readBits("data_type",
                                       8,
                                       Options()
                                           .withMeaningMap({{0x10, "2-bit/pixel code string"},
                                                            {0x11, "4-bit/pixel code string"},
                                                            {0x12, "8-bit/pixel code string"},
                                                            {0x20, "2_to_4-bit_map-table data"},
                                                            {0x21, "2_to_8-bit_map-table data"},
                                                            {0x22, "4_to_8-bit_map-table data"},
                                                            {0xF0, "end of object line code"}})
                                           .withMeaning("reserved"));

  auto end      = false;
  auto bitsRead = 0u;
  if (data_type == 0x10)
  {
    while (!end)
    {
      auto [pEnd, pBitsRead] = parse_2_bit_pixel_code_string(reader);
      end                    = pEnd;
      bitsRead += pBitsRead;
    }
    while (bitsRead % 8 != 0)
    {
      reader.readBits("stuff_bits_2", 2);
      bitsRead += 2;
    }
  }
  else if (data_type == 0x11)
  {
    while (!end)
    {
      auto [pEnd, pBitsRead] = parse_4_bit_pixel_code_string(reader);
      end                    = pEnd;
      bitsRead += pBitsRead;
    }
    while (bitsRead % 8 != 0)
    {
      reader.readBits("stuff_bits_4", 4);
      bitsRead += 4;
    }
  }
  else if (data_type == 0x12)
  {
    while (!end)
    {
      auto [pEnd, pBitsRead] = parse_8_bit_pixel_code_string(reader);
      end                    = pEnd;
      bitsRead += pBitsRead;
    }
  }
  else if (data_type == 0x20)
  {
    for (unsigned i = 0; i < 2; i++)
      reader.readBits(parser::formatArray("table_2_to_4_bit_map", i), 8);
    bitsRead += 2 * 8;
  }
  else if (data_type == 0x21)
  {
    for (unsigned i = 0; i < 4; i++)
      reader.readBits(parser::formatArray("table_2_to_8_bit_map", i), 8);
    bitsRead += 4 * 8;
  }
  else if (data_type == 0x22)
  {
    for (unsigned i = 0; i < 16; i++)
      reader.readBits(parser::formatArray("table_4_to_8_bit_map", i), 8);
    bitsRead += 16 * 8;
  }

  return 1 + bitsRead / 8;
}

void parseProgressivePixelBlock(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "progressive_pixel_block()");

  reader.readBits("bitmap_width", 16);
  reader.readBits("bitmap_height", 16);
  auto compressed_data_block_length = reader.readBits("compressed_data_block_length", 16);

  for (unsigned i = 0; i < compressed_data_block_length; i++)
    reader.readBits("compressed_bitmap_data_byte", 8);
}

void parseObjectDataSegment(SubByteReaderLogging &reader, unsigned segment_length)
{
  SubByteReaderLoggingSubLevel s(reader, "object_data_segment()");

  reader.readBits("object_id", 16);
  reader.readBits("object_version_number", 4);
  auto object_coding_method = reader.readBits("object_coding_method", 2);

  reader.readFlag("non_modifying_colour_flag");
  reader.readFlag("reserved");

  if (object_coding_method == 0)
  {
    auto top_field_data_block_length    = reader.readBits("top_field_data_block_length", 16);
    auto bottom_field_data_block_length = reader.readBits("bottom_field_data_block_length", 16);

    auto processed_length = 0u;
    while (processed_length < top_field_data_block_length)
    {
      processed_length += parsePixelDataSubBlock(reader);
    }

    processed_length = 0;
    while (processed_length < bottom_field_data_block_length)
      processed_length += parsePixelDataSubBlock(reader);

    auto stuffing_length =
        segment_length - 7 - top_field_data_block_length - bottom_field_data_block_length;
    if (stuffing_length == 1)
      reader.readBits("stuff_bits_8", 8);
  }
  else if (object_coding_method == 1)
  {
    auto number_of_codes = reader.readBits("number_of_codes", 8);
    for (unsigned i = 1; i <= number_of_codes; i++)
      reader.readBits("character_code", 16);
  }
  else if (object_coding_method == 2)
    parseProgressivePixelBlock(reader);
  else
    throw std::logic_error("Invalid object_coding_method value");
}

void parseDisplayDefinitionSegment(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "display_definition_segment()");

  reader.readBits("dds_version_number", 4);
  auto display_window_flag = reader.readFlag("display_window_flag");
  reader.readBits("reserved", 3);
  reader.readBits("display_width", 16);
  reader.readBits("display_height", 16);

  if (display_window_flag)
  {
    reader.readBits("display_window_horizontal_position_minimum", 16);
    reader.readBits("display_window_horizontal_position_maximum", 16);
    reader.readBits("display_window_vertical_position_minimum", 16);
    reader.readBits("display_window_vertical_position_maximum", 16);
  }
}

void parserDisparityShiftUpdateSequence(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "disparity_shift_update_sequence()");

  reader.readBits("disparity_shift_update_sequence_length", 8);
  reader.readBits("interval_duration", 24);
  auto division_period_count = reader.readBits("division_period_count", 8);

  for (unsigned i = 0; i < division_period_count; i++)
  {
    reader.readBits("interval_count", 8);
    reader.readBits("disparity_shift_update_integer_part", 8);
  }
}

bool parseDisparitySignalingSegment(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "display_definition_segment()");

  reader.readBits("sync_byte", 8);
  reader.readBits("segment_type", 8);
  reader.readBits("page_id", 6);
  auto segment_length = reader.readBits("segment_length", 6);
  reader.readBits("dss_version_number", 4);
  auto disparity_shift_update_sequence_page_flag =
      reader.readFlag("disparity_shift_update_sequence_page_flag");
  reader.readBits("reserved", 3);
  reader.readBits("page_default_disparity_shift", 8);

  if (disparity_shift_update_sequence_page_flag)
    parserDisparityShiftUpdateSequence(reader);

  auto processed_length = 0u;
  while (processed_length < segment_length)
  {
    reader.readBits("region_id", 8);
    auto disparity_shift_update_sequence_region_flag =
        reader.readFlag("disparity_shift_update_sequence_region_flag");
    reader.readBits("reserved", 5);
    auto number_of_subregions_minus_1 = reader.readBits("number_of_subregions_minus_1", 2);

    for (unsigned n = 0; n <= number_of_subregions_minus_1; n++)
    {
      if (number_of_subregions_minus_1 > 0)
      {
        reader.readBits("subregion_horizontal_position", 16);
        reader.readBits("subregion_width", 16);
      }
      reader.readBits("subregion_disparity_shift_integer_part", 8);
      reader.readBits("subregion_disparity_shift_fractional_part", 8);
      reader.readBits("reserved", 4);
      if (disparity_shift_update_sequence_region_flag)
        parserDisparityShiftUpdateSequence(reader);
    }
  }

  return true;
}

unsigned parseCLUTParameters(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel s(reader, "CLUT_parameters()");

  reader.readBits("CLUT_entry_max_number", 2);
  reader.readBits("colour_component_type", 2);
  auto output_bit_depth = reader.readBits("output_bit_depth", 3);
  reader.readFlag("reserved_zero_future_use");
  reader.readBits("dynamic_range_and_colour_gamut", 8);

  return output_bit_depth;
}

void parseAlternativeCLUTSegment(SubByteReaderLogging &reader, unsigned segment_length)
{
  SubByteReaderLoggingSubLevel s(reader, "alternative_CLUT_segment()");

  reader.readBits("CLUT_id", 8);
  reader.readBits("CLUT_version_number", 4);
  reader.readBits("reserved_zero_future_use", 4);

  unsigned output_bit_depth = parseCLUTParameters(reader);

  unsigned processed_length = 4;
  while (processed_length < segment_length)
  {
    if (output_bit_depth == 0)
    {
      reader.readBits("luma_value", 8);
      reader.readBits("chroma1_value", 8);
      reader.readBits("chroma2_value", 8);
      reader.readBits("T_value", 8);

      processed_length += 4;
    }
    if (output_bit_depth == 0)
    {
      reader.readBits("luma_value", 10);
      reader.readBits("chroma1_value", 10);
      reader.readBits("chroma2_value", 10);
      reader.readBits("T_value", 10);

      processed_length += 5;
    }
  }
}

} // namespace

namespace parser::subtitle
{

std::tuple<size_t, std::string> dvb::parseDVBSubtitleSegment(ByteVector &data, TreeItem *parent)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet.
  // We want to parse the item and then set a good description.
  QString specificDescription;
  if (!parent)
    return {};

  // Create a sub byte parser to access the bits
  SubByteReaderLogging reader(data, parent, "subtitling_segment()");
  if (data.size() < 6)
    throw std::logic_error("The subtitling_segment header must have at least six byte");

  reader.readBits("sync_byte", 8, Options().withCheckEqualTo(15));

  unsigned    segment_type;
  std::string segment_type_name;
  {
    Options opt;
    for (int i = 0; i < 16; i++)
      opt.meaningMap[i] = "reserved for future use";
    opt.meaningMap[16] = "page composition segment";
    opt.meaningMap[17] = "region composition segment";
    opt.meaningMap[18] = "CLUT definition segment";
    opt.meaningMap[19] = "object data segment";
    opt.meaningMap[20] = "display definition segment";
    opt.meaningMap[21] = "disparity signalling segment";
    opt.meaningMap[22] = "alternative_CLUT_segment";
    for (int i = 23; i <= 127; i++)
      opt.meaningMap[i] = "reserved for future use";
    opt.meaningMap[128] = "end of display set segment";
    for (int i = 129; i < 254; i++)
      opt.meaningMap[i] = "private data";
    opt.meaningMap[255] = "stuffing";

    segment_type      = reader.readBits("segment_type", 8, opt);
    segment_type_name = opt.meaningMap[segment_type];
  }

  reader.readBits("page_id", 16);

  auto minSegmentLength = int64_t(data.size() - 6);
  auto segment_length =
      reader.readBits("segment_length", 16, Options().withCheckGreater(minSegmentLength));

  if (segment_type == 0x10)
    parsePageCompositionSegment(reader, segment_length);
  else if (segment_type == 0x11)
    parseRegionCompositionSegment(reader, segment_length);
  else if (segment_type == 0x12)
    parseCLUTDefinitionSegment(reader, segment_length);
  else if (segment_type == 0x13)
    parseObjectDataSegment(reader, segment_length);
  else if (segment_type == 0x14)
    parseDisplayDefinitionSegment(reader);
  else if (segment_type == 0x15)
    parseDisparitySignalingSegment(reader);
  else if (segment_type == 0x16)
    parseAlternativeCLUTSegment(reader, segment_length);
  else if (segment_type == 0x80)
  {
    if (segment_length != 0)
      throw std::logic_error("The end_of_display_set_segment should not contain any data.");
  }

  return {6 + segment_length, segment_type_name};
}

} // namespace parser::subtitle