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

#include "Subtitle608.h"

#include "parser/common/Macros.h"

#include <stdexcept>

namespace
{

bool checkByteParity(int val)
{
  int nrOneBits = 0;
  for (int i = 0; i < 8; i++)
  {
    if (val & (1<<i))
      nrOneBits++;
  }
  // Parity even?
  return nrOneBits % 2 == 1;
}

std::string getCCDataBytesMeaning(int64_t byte1And2)
{
  auto byte1 = unsigned((byte1And2 >> 8) & 0xFF);
  auto byte2 = unsigned(byte1And2 & 0xFF);

  // Remove the parity bits
  byte1 &= 0x7f;
  byte2 &= 0x7f;

  auto colorFromIndex = [](int i) 
  {
    auto c = std::vector<std::string>({"Green", "Blue", "Cyan", "Red", "Yellow", "Magenta"});
    int idx = i/2 - 1;
    if (idx >= 0 && idx <= 5)
      return c[idx];
    return std::string("White");
  };
  auto styleFromIndex = [](int i)
  {
    if (i == 14)
      return std::string("Italic");
    if (i == 15)
      return std::string("Italic Underlined");
    if (i % 2 == 1)
      return std::string("Underlined");
    return std::string();
  };
  auto indentFromIndex = [](int i)
  {
    if (i >= 16)
      return (i - 16) * 2;
    return 0;
  };
  
  if ((byte1 == 0x10 && (byte2 >= 0x40 && byte2 <= 0x5f)) || ((byte1 >= 0x11 && byte1 <= 0x17) && (byte2 >= 0x40 && byte2 <= 0x7f)))
  {
    const int pacIdx = ((byte1<<1) & 0x0e) | ((byte2>>5) & 0x01);
    
    const int row_map[] = {11, -1, 1, 2, 3, 4, 12, 13, 14, 15, 5, 6, 7, 8, 9, 10};
    const int row = row_map[pacIdx] - 1;
    if (row < 0)
      return "PAC - Invalid index";
    
    byte2 &= 0x1f;

    auto color = colorFromIndex(byte2);
    auto style = styleFromIndex(byte2);
    int indent = indentFromIndex(byte2);

    return "PAC (Font and color) Idx " + std::to_string(pacIdx) + " - Row " + std::to_string(row) + " Color " + color + " Indent " + std::to_string(indent) + " " + style;
  } 
  else if ((byte1 == 0x11 && byte2 >= 0x20 && byte2 <= 0x2f ) || (byte1 == 0x17 && byte2 >= 0x2e && byte2 <= 0x2f))
  {
    const int idx = byte2 - 0x20;
    if (idx >= 32)
      return "Textattribut - invalid index";

    auto color = colorFromIndex(idx);
    auto style = styleFromIndex(idx);

    return "Textattribut (Font and color) Idx " + std::to_string(idx) + " - Color " + color + " " + style;
  } 
  else if (byte1 == 0x14 || byte1 == 0x15 || byte1 == 0x1c) 
  {
    auto m = std::vector<std::string>({"resume caption loading", "backspace (overwrite last char)", "alarm off (unused)", "alarm on (unused)", "delete to end of row (clear line)", "roll up 2 (scroll size)", "roll up 3 (scroll size)", "roll up 4 (scroll size)", "flashes captions on (0.25 seconds once per second)", "resume direct captioning (start caption text)", "text restart (start non-caption text)", "resume text display (resume non-caption text)", "erase display memory (clear screen)", "carriage return (scroll lines up)", "erase non displayed memory (clear buffer)", "end of caption (display buffer)"});
    if (byte2 >= 32 && byte2 <= 47)
      return m.at(byte2 - 32);
    return "Unknown command";
  } 
  else if (byte1 >= 0x11 && byte1 <= 0x13) 
  {
    if (byte1 == 0x11)
      return "Special North American character";
    if (byte2 == 0x12)
      return "Special Spanish/French or miscellaneous character";
    if (byte2 == 0x13)
      return "Special Portuguese/German/Danish character";
  } 
  else if (byte1 >= 0x20) 
    return "Standard characters '" + std::string(1, char(byte1)) + std::string(1, char(byte2)) + "'";
  else if (byte1 == 0x17 && byte2 >= 0x21 && byte2 <= 0x23) 
  {
    int nrSpaces = byte2 - 0x20;
    return "Tab offsets (spacing) - " + std::to_string(nrSpaces) + " spaces";
  }
  
  return "Non data code";
}

std::string getCCDataPacketMeaning(int64_t cc_packet_data)
{
  // Parse the 3 bytes as 608
  unsigned int byte0 = (cc_packet_data >> 16) & 0xff;
  unsigned int byte1 = (cc_packet_data >> 8) & 0xff;
  unsigned int byte2 = cc_packet_data & 0xff;
    
  // Ignore if this flag is not set
  bool cc_valid = ((byte0 & 4) > 0);
  if (!cc_valid)
    return "";

  /* cc_type: 
   * 0 ntsc_cc_field_1
   * 1 ntsc_cc_field_2
   * 2 dtvcc_packet_data
   * dtvcc_packet_start
   */
  int cc_type = byte0 & 0x03;
  if (cc_type == 1)
    // For now, we ignore field 2 tags although we could also parse them ...
    return "";
  if (cc_type == 3 || cc_type == 2)
    return "";
    
  // Check the parity
  if (!checkByteParity(byte2))
    return "";
  if (!checkByteParity(byte1))
    byte1 = 0x7f;
  
  if ((byte0 == 0xfa || byte0 == 0xfc || byte0 == 0xfd) && ((byte1 & 0x7f) == 0 && (byte2 & 0x7f) == 0))
    return "";

  int64_t byte1And2 = (byte1 << 8) + byte2;
  return getCCDataBytesMeaning(byte1And2);
}

} // namespace

namespace parser::subtitle
{

using namespace reader;

void sub_608::parse608SubtitlePacket(ByteVector data, TreeItem *parent)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  if (!parent)
    return;

  // Create a sub byte parser to access the bits
  ReaderHelperNew reader(data, parent, "subtitling_608()");

  if (data.size() != 10 && data.size() != 20)
    throw std::logic_error("Unknown packt length. Length should be 10 or 20 bytes");

  unsigned int nrMessages = data.size() == 20 ? 2 : 1;
  for (unsigned i = 0; i < nrMessages; i++)
  {
    // The packet should start with a size indicator of 10
    reader.readBits("sizeIndicator", 32, Options().withCheckEqualTo(10));

    auto tag_meaning = [](int64_t value)
    {
      std::string r;
      for (unsigned j = 0; j < 4; j++)
      {
        char c = (value >> (3 - j) * 8) & 0xff;
        r.push_back(c);
      }
      return r;
    };
    auto tag = reader.readBits("tag", 32, Options().withMeaningFunction(tag_meaning));
    
    if (tag != 1667527730 && tag != 1667522932)
      throw std::logic_error("Unknown tag. Should be cdt2 or cdat.");

    reader.readBits("ccByte0/1", 16, Options().withMeaningFunction(getCCDataBytesMeaning));
  }  
}

unsigned sub_608::parse608DataPayloadCCDataPacket(ReaderHelperNew &reader)
{
    return reader.readBits("ccData", 24, Options().withMeaningFunction(getCCDataPacketMeaning));
}

} // namespace parser