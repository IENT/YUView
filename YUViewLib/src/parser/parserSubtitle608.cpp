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

#include "parserSubtitle608.h"

#include "parserCommon.h"
#include "parserCommonMacros.h"

using namespace parserCommon;

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

QString getCCDataBytesMeaning(unsigned int byte1, unsigned int byte2)
{
  // Remove the parity bits
  byte1 &= 0x7f;
  byte2 &= 0x7f;

  auto colorFromIndex = [](int i) 
  {
    const QStringList c = QStringList() << "Green" << "Blue" << "Cyan" << "Red" << "Yellow" << "Magenta";
    int idx = i/2 - 1;
    if (idx >= 0 && idx <= 5)
      return c[idx];
    return QString("White");
  };
  auto styleFromIndex = [](int i)
  {
    if (i == 14)
      return "Italic";
    if (i == 15)
      return "Italic Underlined";
    if (i % 2 == 1)
      return "Underlined";
    return "";
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

    QString color = colorFromIndex(byte2);
    QString style = styleFromIndex(byte2);
    int indent = indentFromIndex(byte2);

    return QString("PAC (Font and color) Idx %1 - Row %2 Color %3 Indent %4 %5").arg(pacIdx).arg(row).arg(color).arg(indent).arg(style);
  } 
  else if ((byte1 == 0x11 && byte2 >= 0x20 && byte2 <= 0x2f ) || (byte1 == 0x17 && byte2 >= 0x2e && byte2 <= 0x2f))
  {
    const int idx = byte2 - 0x20;
    if (idx >= 32)
      return "Textattribut - invalid index";

    QString color = colorFromIndex(idx);
    QString style = styleFromIndex(idx);

    return QString("Textattribut (Font and color) Idx %1 - Color %2 ").arg(idx).arg(color).arg(style);
  } 
  else if (byte1 == 0x14 || byte1 == 0x15 || byte1 == 0x1c) 
  {
    const QStringList m = QStringList() << "resume caption loading" << "backspace (overwrite last char)" << "alarm off (unused)" << "alarm on (unused)" << "delete to end of row (clear line)" << "roll up 2 (scroll size)" << "roll up 3 (scroll size)" << "roll up 4 (scroll size)" << "flashes captions on (0.25 seconds once per second)" << "resume direct captioning (start caption text)" << "text restart (start non-caption text)" << "resume text display (resume non-caption text)" << "erase display memory (clear screen)" << "carriage return (scroll lines up)" << "erase non displayed memory (clear buffer)" << "end of caption (display buffer)";
    if (byte2 >= 32 && byte2 <= 47)
      return m.at(byte2 - 32);
    return "Unknown command";
  } 
  else if (byte1 >= 0x11 && byte1 <= 0x13) 
  {
    if (byte1 == 0x11)
      return "Special North American chracter";
    if (byte2 == 0x12)
      return "Special Spanish/French or miscellaneous character";
    if (byte2 == 0x13)
      return "Special Portuguese/German/Danish character";
  } 
  else if (byte1 >= 0x20) 
    return QString("Standard characters '%1%2'").arg(char(byte1)).arg(char(byte2));
  else if (byte1 == 0x17 && byte2 >= 0x21 && byte2 <= 0x23) 
  {
    int nrSpaces = byte2 - 0x20;
    return QString("Tab offsets (spacing) - %1 spaces").arg(nrSpaces);
  }
  
  return "Non data code";
}

QString getCCDataPacketMeaning(unsigned int cc_packet_data)
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

  return getCCDataBytesMeaning(byte1, byte2);
}

int subtitle_608::parse608SubtitlePacket(QByteArray data, parserCommon::TreeItem *parent)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  if (!parent)
    return -1;

  // Create a sub byte parser to access the bits
  parserCommon::reader_helper reader(data, parent, "subtitling_608()");

  if (data.size() != 10 && data.size() != 20)
    throw std::logic_error("Unknown packt length. Length should be 10 or 20 bytes");

  unsigned int nrMessages = data.size() == 20 ? 2 : 1;
  for (size_t i = 0; i < nrMessages; i++)
  {
    // The packet should start with a size indicator of 10
    unsigned int sizeIndicator;
    READBITS(sizeIndicator, 32);

    if (sizeIndicator != 10)
      throw std::logic_error("The size indicator should be 10");

    unsigned int tag;
    QString (*tag_meaning)(unsigned int) = [](unsigned int value)
    {
      QString r;
      for (unsigned int j = 0; j < 4; j++)
      {
        char c = (value >> (3 - j) * 8) & 0xff;
        r += QString(c);
      }
      return r;
    };
    READBITS_M(tag, 32, tag_meaning);
    
    if (tag != 1667527730 && tag != 1667522932)
      throw std::logic_error("Unknown tag. Should be cdt2 or cdat.");

    unsigned int ccByte0, ccByte1;
    READBITS(ccByte0, 8);
    READBITS(ccByte1, 8);

    auto bytesMeaning = getCCDataBytesMeaning(ccByte0, ccByte1);
    LOGSTRVAL("CC Bytes Decoded", bytesMeaning);
  }
  
  return reader.nrBytesRead();
}

int subtitle_608::parse608DataPayloadCCDataPacket(parserCommon::reader_helper &reader, unsigned int &ccData)
{
    READBITS_M(ccData, 24, &getCCDataPacketMeaning);
    return 3;
}
