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

#include "PixelFormatRGBGuess.h"

#include <common/Functions.h>

#include <QDir>
#include <regex>
#include <string>

using namespace std::string_literals;

namespace video::rgb
{

std::optional<PixelFormatRGB> findPixelFormatIndicatorInName(const std::string &fileName)
{
  std::string matcher = "(?:_|\\.|-)(";

  std::map<std::string, PixelFormatRGB> stringToMatchingFormat;
  for (const auto &[channelOrder, channelOrderName] : ChannelOrderMapper)
  {
    for (auto alphaMode : {AlphaMode::None, AlphaMode::First, AlphaMode::Last})
    {
      for (auto [bitDepth, bitDepthString] : {std::pair<unsigned, std::string>{8, ""},
                                              {8, "8"},
                                              {10, "10"},
                                              {12, "12"},
                                              {16, "16"},
                                              {16, "64"},
                                              {16, "48"}})
      {
        for (auto [endianness, endiannessName] :
             {std::pair<Endianness, std::string>{Endianness::Little, ""},
              {Endianness::Little, "le"},
              {Endianness::Big, "be"}})
        {
          std::string name;
          if (alphaMode == AlphaMode::First)
            name += "a";
          name += functions::toLower(channelOrderName);
          if (alphaMode == AlphaMode::Last)
            name += "a";
          name += bitDepthString + endiannessName;
          stringToMatchingFormat[name] =
              PixelFormatRGB(bitDepth, DataLayout::Packed, channelOrder, alphaMode, endianness);
          matcher += name + "|";
        }
      }
    }
  }

  matcher.pop_back(); // Remove last |
  matcher += ")(?:_|\\.|-)";

  std::smatch sm;
  std::regex  strExpr(matcher);
  if (!std::regex_search(fileName, sm, strExpr))
    return {};

  auto match     = sm.str(0);
  auto matchName = match.substr(1, match.size() - 2);
  return stringToMatchingFormat[matchName];
}

std::optional<PixelFormatRGB> findPixelFormatFromFileExtension(const std::string &ext)
{
  for (const auto &[channelOrder, name] : ChannelOrderMapper)
  {
    if (functions::toLower(ext) == functions::toLower(name))
      return PixelFormatRGB(8, DataLayout::Packed, channelOrder);
  }
  return {};
}

DataLayout findDataLayoutInName(const std::string &fileName)
{
  std::string matcher = "(?:_|\\.|-)(packed|planar)(?:_|\\.|-)";

  std::smatch sm;
  std::regex  strExpr(matcher);
  if (std::regex_search(fileName, sm, strExpr))
  {
    auto match = sm.str(0).substr(1, 6);
    return match == "planar" ? DataLayout::Planar : DataLayout::Packed;
  }

  return DataLayout::Packed;
}

PixelFormatRGB
guessFormatFromSizeAndName(const QFileInfo &fileInfo, const Size frameSize, const int64_t fileSize)
{
  // We are going to check two strings (one after the other) for indicators on the YUV format.
  // 1: The file name, 2: The folder name that the file is contained in.

  // The full name of the file
  auto fileName = fileInfo.baseName().toLower().toStdString();
  if (fileName.empty())
    return {};
  fileName += ".";

  const auto dirName       = fileInfo.absoluteDir().dirName().toLower().toStdString();
  const auto fileExtension = fileInfo.suffix().toLower().toStdString();

  auto isFileSizeMultipleOfFrameSizeInBytes = [&fileSize, &frameSize](const PixelFormatRGB &format)
  {
    const auto bpf = format.bytesPerFrame(frameSize);
    return bpf > 0 && (fileSize % bpf) == 0;
  };

  if (fileExtension == "cmyk")
  {
    const auto format = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::Last);
    if (isFileSizeMultipleOfFrameSizeInBytes(format))
      return format;
  }

  for (const auto &name : {fileName, dirName})
  {
    for (auto format :
         {findPixelFormatIndicatorInName(name), findPixelFormatFromFileExtension(fileExtension)})
    {
      if (format)
      {
        if (isFileSizeMultipleOfFrameSizeInBytes(*format))
        {
          auto dataLayout = findDataLayoutInName(name);
          format->setDataLayout(dataLayout);
          return *format;
        }
      }
    }
  }

  // Guessing failed
  return PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);
}

} // namespace video::rgb
