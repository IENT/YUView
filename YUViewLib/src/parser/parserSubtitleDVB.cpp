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

#include "parserSubtitleDVB.h"

#include "parserCommonMacros.h"

using namespace parserCommon;

bool parsePageCompositionSegment(reader_helper &reader, unsigned int segment_length)
{
    reader_sub_level s(reader, "page_composition_segment()");

    unsigned int page_time_out;
    READBITS(page_time_out, 8);
    unsigned int page_version_number;
    READBITS(page_version_number, 4);
    unsigned int page_state;
    READBITS(page_state, 2);
    unsigned int reserved;
    READBITS(reserved, 2);

    unsigned int processed_length = 2;
    while (processed_length < segment_length)
    {
        unsigned int region_id;
        READBITS(region_id, 8);
        unsigned int reserved;
        READBITS(reserved, 8);
        unsigned int region_horizontal_address;
        READBITS(region_horizontal_address, 8);
        unsigned int region_vertical_address;
        READBITS(region_vertical_address, 8);
        processed_length += 6;
    }

    return true;
}

bool parseRegionCompositionSegment(reader_helper &reader, unsigned int segment_length)
{
    reader_sub_level s(reader, "region_composition_segment()");

    unsigned int region_id;
    unsigned int region_version_number;
    bool region_fill_flag;
    unsigned int reserved;
    unsigned int region_width ;
    unsigned int region_height ;
    unsigned int region_level_of_compatibility;
    unsigned int region_depth;
    unsigned int CLUT_id;
    unsigned int region_8_bit_pixel_code;
    unsigned int region_4_bit_pixel_code;
    unsigned int region_2_bit_pixel_code;

    READBITS(region_id, 8);
    READBITS(region_version_number, 4);
    READFLAG(region_fill_flag);
    READBITS(reserved, 3);
    READBITS(region_width ,16);
    READBITS(region_height ,16);
    READBITS(region_level_of_compatibility, 3);
    READBITS(region_depth, 3);
    READBITS(reserved, 2);
    READBITS(CLUT_id, 8);
    READBITS(region_8_bit_pixel_code, 8);
    READBITS(region_4_bit_pixel_code, 4);
    READBITS(region_2_bit_pixel_code, 2);
    READBITS(reserved, 2);

    unsigned int processed_length = 10;
    while (processed_length < segment_length)
    {
        unsigned int object_id;
        unsigned int object_type;
        unsigned int object_provider_flag;
        unsigned int object_horizontal_position;
        unsigned int object_vertical_position;

        READBITS(object_id, 16);
        READBITS(object_type, 2);
        READBITS(object_provider_flag, 2);
        READBITS(object_horizontal_position, 12);
        READBITS(reserved, 4);
        READBITS(object_vertical_position, 12);
        processed_length += 6;

        if (object_type == 0x01 || object_type == 0x02)
        {
            unsigned int foreground_pixel_code;
            READBITS(foreground_pixel_code, 8);
            unsigned int background_pixel_code;
            READBITS(background_pixel_code, 8);
            processed_length += 2;
        }
    }

    return true;
}

bool parseCLUTDefinitionSegment(reader_helper &reader, unsigned int segment_length)
{
    reader_sub_level s(reader, "CLUT_definition_segment()");

    unsigned int CLUT_id;
    READBITS(CLUT_id, 8);
    unsigned int CLUT_version_number;
    READBITS(CLUT_version_number, 4);
    unsigned int reserved;
    READBITS(reserved, 4);

    unsigned int processed_length = 2;
    while (processed_length < segment_length)
    {
        unsigned int CLUT_entry_id;
        READBITS(CLUT_entry_id, 8);
        bool flag_2_bit_entry_CLUT;
        READFLAG(flag_2_bit_entry_CLUT);
        bool flag_4_bit_entry_CLUT;
        READFLAG(flag_4_bit_entry_CLUT);
        bool flag_8_bit_entry_CLUT;
        READFLAG(flag_8_bit_entry_CLUT);
        unsigned int reserved;
        READBITS(reserved, 4);
        bool full_range_flag;
        READFLAG(full_range_flag);
        processed_length += 2;

        unsigned int Y_value;
        unsigned int Cr_value;
        unsigned int Cb_value;
        unsigned int T_value ;
        if (full_range_flag)
        {
            READBITS(Y_value, 8);
            READBITS(Cr_value, 8);
            READBITS(Cb_value, 8);
            READBITS(T_value, 8);
            processed_length += 4;
        }
        else
        {
            READBITS(Y_value, 6);
            READBITS(Cr_value, 4);
            READBITS(Cb_value, 4);
            READBITS(T_value, 2);
            processed_length += 2;
        }
    }

    return true;
}

bool parse_2_bit_pixel_code_string(reader_helper &reader, bool &end, unsigned int &bitsRead)
{
  reader_sub_level s(reader, "2-bit/pixel_code_string()");

  unsigned int next_bits;
  READBITS(next_bits, 2);
  bitsRead += 2;

  if (next_bits == 0)
  {
    bool switch_1;
    READFLAG(switch_1);
    bitsRead++;
    if (switch_1)
    {
      unsigned int run_length_3_10;
      READBITS(run_length_3_10, 3);
      unsigned int two_bit_pixel_code;
      READBITS(two_bit_pixel_code, 2);
      bitsRead += 5;
    }
    else
    {
      bool switch_2;
      READFLAG(switch_2);
      bitsRead++;
      if (!switch_2)
      {
        unsigned int switch_3;
        READBITS(switch_3, 2);
        bitsRead += 2;
        if (switch_3 == 0)
        {
          end = true;
        }
        if (switch_3 == 2)
        {
          unsigned int run_length_12_27;
          READBITS(run_length_12_27, 4);
          unsigned int two_bit_pixel_code;
          READBITS(two_bit_pixel_code, 2);
          bitsRead += 6;
        }
        if (switch_3 == 3)
        {
          unsigned int run_length_29_284;
          READBITS(run_length_29_284, 8);
          unsigned int two_bit_pixel_code;
          READBITS(two_bit_pixel_code, 2);
          bitsRead += 10;
        }
      }
    }
  }

  return true;
}

bool parse_4_bit_pixel_code_string(reader_helper &reader, bool &end, unsigned int &bitsRead)
{
  reader_sub_level s(reader, "4-bit/pixel_code_string()");

  unsigned int next_bits;
  READBITS(next_bits, 4);
  bitsRead += 4;
  if (next_bits == 0)
  {
    bool switch_1;
    READFLAG(switch_1);
    bitsRead++;
    if (!switch_1)
    {
      READBITS(next_bits, 3);
      bitsRead += 3;
      if (next_bits == 0)
        end = true;
    }
    else
    {
      bool switch_2;
      READFLAG(switch_2);
      bitsRead++;
      if (!switch_2)
      {
        unsigned int run_length_4_7;
        READBITS(run_length_4_7, 2);
        unsigned int four_bit_pixel_code;
        READBITS(four_bit_pixel_code, 4);
        bitsRead += 6;
      }
      else
      {
        unsigned int switch_3;
        READBITS(switch_3, 2);
        bitsRead += 2;
        if (switch_3 == 2)
        {
          unsigned int run_length_9_24;
          READBITS(run_length_9_24, 4);
          unsigned int four_bit_pixel_code;
          READBITS(four_bit_pixel_code, 4);
          bitsRead += 8;
        }
        if (switch_3 == 3)
        {
          unsigned int run_length_25_280;
          READBITS(run_length_25_280, 8);
          unsigned int four_bit_pixel_code;
          READBITS(four_bit_pixel_code, 4);
          bitsRead += 12;
        }
      }
    }
  }

  return true;
}

bool parse_8_bit_pixel_code_string(reader_helper &reader, bool &end, unsigned int &bitsRead)
{
  reader_sub_level s(reader, "8-bit/pixel_code_string()");

  unsigned int next_bits;
  READBITS(next_bits, 8);
  bitsRead += 8;
  if (next_bits == 0)
  {
    bool switch_1;
    READFLAG(switch_1);
    bitsRead++;
    if (!switch_1)
    {
      READBITS(next_bits, 7);
      bitsRead += 7;
      if (next_bits == 0)
        end = true;
    }
    else
    {
      unsigned int run_length_3_127;
      READBITS(run_length_3_127, 7);
      unsigned int eight_bit_pixel_code;
      READBITS(eight_bit_pixel_code, 8);
      bitsRead += 15;
    }
  }

  return true;
}

bool parsePixelDataSubBlock(reader_helper &reader, unsigned int &processed_length)
{
  reader_sub_level s(reader, "pixel-data_sub-block()");

  QMap<int,QString> data_type_meaning;
  data_type_meaning[0x10] = "2-bit/pixel code string";
  data_type_meaning[0x11] = "4-bit/pixel code string";
  data_type_meaning[0x12] = "8-bit/pixel code string";
  data_type_meaning[0x20] = "2_to_4-bit_map-table data";
  data_type_meaning[0x21] = "2_to_8-bit_map-table data";
  data_type_meaning[0x22] = "4_to_8-bit_map-table data";
  data_type_meaning[0xF0] = "end of object line code";
  data_type_meaning[-1] = "reserved";
  unsigned int data_type;
  READBITS_M(data_type, 8, data_type_meaning);
  processed_length++;

  bool end = false;
  unsigned int bitsRead = 0;
  if (data_type == 0x10)
  {
    while (!end)
    {
      if (!parse_2_bit_pixel_code_string(reader, end, bitsRead))
        return reader.addErrorMessageChildItem("Error parsing 2-bit/pixel_code_string");
    }
    while (bitsRead % 8 != 0)
    {
      unsigned int stuff_bits_2;
      READBITS(stuff_bits_2, 2);
      bitsRead += 2;
    }
    processed_length += bitsRead / 8;
  }
  if (data_type == 0x11)
  {
    while (!end)
    {
      if (!parse_4_bit_pixel_code_string(reader, end, bitsRead))
        return reader.addErrorMessageChildItem("Error parsing 4-bit/pixel_code_string");
    }
    while (bitsRead % 8 != 0)
    {
      unsigned int stuff_bits_4;
      READBITS(stuff_bits_4, 4);
      bitsRead += 4;
    }
    processed_length += bitsRead / 8;
  }
  if (data_type == 0x12)
  {
    while (!end)
    {
      if (!parse_8_bit_pixel_code_string(reader, end, bitsRead))
        return reader.addErrorMessageChildItem("Error parsing 8-bit/pixel_code_string");
    }
    processed_length += bitsRead / 8;
  }
  if (data_type == 0x20)
  {
    QList<unsigned int> table_2_to_4_bit_map;
    for (int i = 0; i < 2; i++)
    {
      READBITS_A(table_2_to_4_bit_map, 8, i);
    }
    processed_length += 2;
  }
  if (data_type == 0x21)
  {
    QList<unsigned int> table_2_to_8_bit_map;
    for (int i = 0; i < 4; i++)
    {
      READBITS_A(table_2_to_8_bit_map, 8, i);
    }
    processed_length += 4;
  }
  if (data_type == 0x22)
  {
    QList<unsigned int> table_4_to_8_bit_map;
    for (int i = 0; i < 16; i++)
    {
      READBITS_A(table_4_to_8_bit_map, 8, i);
    }
    processed_length += 16;
  }

  return true;
}

bool parseProgressivePixelBlock(reader_helper &reader)
{
  reader_sub_level s(reader, "progressive_pixel_block()");

  unsigned int bitmap_width;
  unsigned int bitmap_height;
  unsigned int compressed_data_block_length;
  
  READBITS(bitmap_width, 16);
  READBITS(bitmap_height , 16);
  READBITS(compressed_data_block_length , 16);

  for (unsigned int i = 0; i < compressed_data_block_length; i++)
  {
    unsigned int compressed_bitmap_data_byte;
    READBITS(compressed_bitmap_data_byte , 8);
  }

  return true;
}

bool parseObjectDataSegment(reader_helper &reader, unsigned int segment_length)
{
    reader_sub_level s(reader, "object_data_segment()");

    unsigned int object_id;
    unsigned int object_version_number;
    unsigned int object_coding_method;
    READBITS(object_id, 16);
    READBITS(object_version_number, 4);
    READBITS(object_coding_method, 2);

    bool non_modifying_colour_flag;
    READFLAG(non_modifying_colour_flag);
    bool reserved;
    READFLAG(reserved);

    if (object_coding_method == 0)
    {
        unsigned int top_field_data_block_length;
        unsigned int bottom_field_data_block_length;
        READBITS(top_field_data_block_length, 16);
        READBITS(bottom_field_data_block_length, 16);
        
        unsigned int processed_length = 0;
        while (processed_length < top_field_data_block_length)
        {
           if (!parsePixelDataSubBlock(reader, processed_length))
            return reader.addErrorMessageChildItem("Error parsing pixel data sub block.");
        }

        processed_length = 0;
        while (processed_length < bottom_field_data_block_length)
        {
          if (!parsePixelDataSubBlock(reader, processed_length))
            return reader.addErrorMessageChildItem("Error parsing pixel data sub block.");
        }

        unsigned int stuffing_length = segment_length - 7 - top_field_data_block_length - bottom_field_data_block_length;
        if (stuffing_length == 1)
        {
          unsigned int stuff_bits_8;
          READBITS(stuff_bits_8, 8);
        }
    }
    else if (object_coding_method == 1)
    {
        unsigned int number_of_codes;
        READBITS(number_of_codes, 8);
        for (unsigned int i=1; i <= number_of_codes; i++)
        {
          unsigned int character_code;
          READBITS(character_code, 16);
        }
    }
    else if (object_coding_method == 2)
    {
      if (!parseProgressivePixelBlock(reader))
        return reader.addErrorMessageChildItem("Error parsing progressive pixel block.");
    }
    else
        return reader.addErrorMessageChildItem("Invalid object_coding_method value");

    return true;
}

bool parseDisplayDefinitionSegment(reader_helper &reader)
{
    reader_sub_level s(reader, "display_definition_segment()");

    unsigned int dds_version_number;
    READBITS(dds_version_number, 4);
    bool display_window_flag;
    READFLAG(display_window_flag);
    unsigned int reserved;
    READBITS(reserved, 3);
    unsigned int display_width;
    READBITS(display_width, 16);
    unsigned int display_height;
    READBITS(display_height, 16);

    if (display_window_flag)
    {
        unsigned int display_window_horizontal_position_minimum;
        READBITS(display_window_horizontal_position_minimum, 16);
        unsigned int display_window_horizontal_position_maximum;
        READBITS(display_window_horizontal_position_maximum, 16);
        unsigned int display_window_vertical_position_minimum;
        READBITS(display_window_vertical_position_minimum, 16);
        unsigned int display_window_vertical_position_maximum;
        READBITS(display_window_vertical_position_maximum, 16);
    }

    return true;
}

bool parserDisparityShiftUpdateSequence(reader_helper &reader)
{
  reader_sub_level s(reader, "disparity_shift_update_sequence()");

  unsigned int disparity_shift_update_sequence_length;
  READBITS(disparity_shift_update_sequence_length, 8);
  unsigned int interval_duration;
  READBITS(interval_duration, 24);
  unsigned int division_period_count;
  READBITS(division_period_count, 8);
  for (unsigned int i = 0; i < division_period_count; i++)
  {
    unsigned int interval_count;
    READBITS(interval_count, 8);
    unsigned int disparity_shift_update_integer_part;
    READBITS(disparity_shift_update_integer_part, 8);
  }

  return true;
}

bool parseDisparitySignalingSegment(reader_helper &reader)
{
    reader_sub_level s(reader, "display_definition_segment()");

    unsigned int sync_byte;
    unsigned int segment_type;
    unsigned int page_id;
    unsigned int segment_length;
    unsigned int dss_version_number;
    bool disparity_shift_update_sequence_page_flag;
    unsigned int reserved;
    unsigned int page_default_disparity_shift ;

    READBITS(sync_byte, 8);
    READBITS(segment_type, 8);
    READBITS(page_id, 6);
    READBITS(segment_length, 6);
    READBITS(dss_version_number , 4);
    READFLAG(disparity_shift_update_sequence_page_flag);
    READBITS(reserved, 3);
    READBITS(page_default_disparity_shift, 8);

    if (disparity_shift_update_sequence_page_flag)
    {
      if (!parserDisparityShiftUpdateSequence(reader))
        return reader.addErrorMessageChildItem("Error parsing disparitry shift update sequence.");
    }
    unsigned int processed_length = 0;
    while (processed_length < segment_length)
    { 
      unsigned int region_id;
      bool disparity_shift_update_sequence_region_flag;
      unsigned int reserved;
      unsigned int number_of_subregions_minus_1;

      READBITS(region_id, 8);
      READFLAG(disparity_shift_update_sequence_region_flag);
      READBITS(reserved, 5);
      READBITS(number_of_subregions_minus_1, 2);

      for (unsigned int n = 0; n <= number_of_subregions_minus_1; n++)
      {
        if (number_of_subregions_minus_1 > 0) 
        {
          unsigned int subregion_horizontal_position;
          READBITS(subregion_horizontal_position, 16);
          unsigned int subregion_width;
          READBITS(subregion_width, 16);
        }
        unsigned int subregion_disparity_shift_integer_part;
        READBITS(subregion_disparity_shift_integer_part, 8);
        unsigned int subregion_disparity_shift_fractional_part;
        READBITS(subregion_disparity_shift_fractional_part, 8);
        unsigned int reserved;
        READBITS(reserved, 4);
        if (disparity_shift_update_sequence_region_flag)
        {
          if (!parserDisparityShiftUpdateSequence(reader))
            return reader.addErrorMessageChildItem("Error parsing disparitry shift update sequence.");
        }
      }
    }

    return true;
}

bool parseCLUTParameters(reader_helper &reader, unsigned int &output_bit_depth)
{
  reader_sub_level s(reader, "CLUT_parameters()");

  unsigned int CLUT_entry_max_number;
  unsigned int colour_component_type;
  bool reserved_zero_future_use;
  unsigned int dynamic_range_and_colour_gamut;

  READBITS(CLUT_entry_max_number, 2);
  READBITS(colour_component_type, 2);
  READBITS(output_bit_depth, 3);
  READFLAG(reserved_zero_future_use);
  READBITS(dynamic_range_and_colour_gamut, 8);

  return true;
}

bool parseAlternativeCLUTSegment(reader_helper &reader, unsigned int segment_length)
{
    reader_sub_level s(reader, "alternative_CLUT_segment()");

    unsigned int CLUT_id;
    READBITS(CLUT_id, 8);
    unsigned int CLUT_version_number;
    READBITS(CLUT_version_number, 4);
    unsigned int reserved_zero_future_use;
    READBITS(reserved_zero_future_use, 4);
    
    unsigned int output_bit_depth;
    if (!parseCLUTParameters(reader, output_bit_depth))
      return reader.addErrorMessageChildItem("Error parsing CLUT parameters.");

    unsigned int processed_length = 4;
    while (processed_length < segment_length)
    {
        unsigned int luma_value;
        unsigned int chroma1_value;
        unsigned int chroma2_value;
        unsigned int T_value;

        if (output_bit_depth == 0)
        {
          READBITS(luma_value, 8);
          READBITS(chroma1_value, 8);
          READBITS(chroma2_value, 8);
          READBITS(T_value, 8);
          
          processed_length += 4;
        }
        if (output_bit_depth == 0)
        {
          READBITS(luma_value, 10);
          READBITS(chroma1_value, 10);
          READBITS(chroma2_value, 10);
          READBITS(T_value, 10);

          processed_length += 5;
        }
    }

    return true;
}

int subtitle_dvb::parseDVBSubtitleSegment(QByteArray data, parserCommon::TreeItem *parent, QString *segmentTypeName)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  if (!parent)
    return -1;

  // Create a sub byte parser to access the bits
  reader_helper reader(data, parent, "subtitling_segment()");
  if (data.length() < 6)
    return reader.addErrorMessageChildItem("The subtitling_segment header must have at least six byte");
    
  unsigned int sync_byte;
  READBITS(sync_byte, 8);
  if (sync_byte != 15)
    return reader.addErrorMessageChildItem("The sync_byte must be 0x00001111 (15).");

  unsigned int segment_type;
  QMap<int,QString> segment_type_meaning;
  for (int i = 0; i < 16; i++)
    segment_type_meaning[i] = "reserved for future use";
  segment_type_meaning[16] = "page composition segment";
  segment_type_meaning[17] = "region composition segment";
  segment_type_meaning[18] = "CLUT definition segment";
  segment_type_meaning[19] = "object data segment";
  segment_type_meaning[20] = "display definition segment";
  segment_type_meaning[21] = "disparity signalling segment";
  segment_type_meaning[22] = "alternative_CLUT_segment";
  for (int i = 23; i <= 127; i++)
    segment_type_meaning[i] = "reserved for future use";
  segment_type_meaning[128] = "end of display set segment";
  for (int i = 129; i < 254; i++)
    segment_type_meaning[i] = "private data";
  segment_type_meaning[255] = "stuffing";
  READBITS_M(segment_type, 8, segment_type_meaning);
  if (segmentTypeName)
    *segmentTypeName = segment_type_meaning[segment_type];

  unsigned int page_id;
  READBITS(page_id, 16);

  unsigned int segment_length;
  READBITS(segment_length, 16);

  if (data.length() < 6 + int(segment_length))
    return reader.addErrorMessageChildItem("The segment_length is longer then the data to parse.");

  // Parsing of the individial segment data is not implemented ... yet? 
  // Feel free to implement these. The specification is freely available :)

  bool success = true;
  if (segment_type == 0x10)
    success = parsePageCompositionSegment(reader, segment_length);
  if (segment_type == 0x11)
    success = parseRegionCompositionSegment(reader, segment_length);
  if (segment_type == 0x12)
    success = parseCLUTDefinitionSegment(reader, segment_length);
  if (segment_type == 0x13)
    success = parseObjectDataSegment(reader, segment_length);
  if (segment_type == 0x14)
    success = parseDisplayDefinitionSegment(reader);
  if (segment_type == 0x15)
    success = parseDisparitySignalingSegment(reader);
  if (segment_type == 0x16)
    success = parseAlternativeCLUTSegment(reader, segment_length);
  if (segment_type == 0x80)
  {
      if (segment_length != 0)
        success = reader.addErrorMessageChildItem("The end_of_display_set_segment should not contain any data");
  }
  if (!success)
    throw std::logic_error("Error parsing segment");

  return 6 + segment_length;
}