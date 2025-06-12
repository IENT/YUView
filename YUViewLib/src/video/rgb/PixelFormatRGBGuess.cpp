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

#include <regex>
#include <string>

using namespace std::string_literals;
using filesource::frameFormatGuess::FileInfoForGuess;
using filesource::frameFormatGuess::GuessedFrameFormat;

namespace video::rgb
{

namespace
{

const auto DEFAULT_PIXEL_FORMAT = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);

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

bool doesPixelFormatMatchFileSize(const PixelFormatRGB         &pixelFormat,
                                  const Size                   &frameSize,
                                  const std::optional<int64_t> &fileSize)
{
  if (!fileSize)
    return true;

  const auto bytesPerFrame = pixelFormat.getBytesPerFrame(frameSize);
  if (bytesPerFrame <= 0)
    return false;

  const auto isFileSizeAMultipleOfFrameSize = (*fileSize % bytesPerFrame) == 0;
  return isFileSizeAMultipleOfFrameSize;
}

} // namespace

std::optional<PixelFormatRGB> checkForPixelFormatIndicatorInName(
  const std::string &filename, const Size &frameSize, const std::optional<std::int64_t> &fileSize)
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
                                              {16, "48"},
                                              {32, "32"},
                                              {32, "96"},
                                              {32, "128"}})
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

  stringToMatchingFormat["rgb565"] = PixelFormatRGB(PredefinedPixelFormat::RGB565);
  matcher += "rgb565|";
  stringToMatchingFormat["rgb565le"] = PixelFormatRGB(PredefinedPixelFormat::RGB565);
  matcher += "rgb565le|";
  stringToMatchingFormat["rgb565be"] = PixelFormatRGB(PredefinedPixelFormat::RGB565BE);
  matcher += "rgb565be";

  matcher += ")(?:_|\\.|-)";

  std::smatch sm;
  std::regex  strExpr(matcher);
  if (!std::regex_search(filename, sm, strExpr))
    return {};

  auto match     = sm.str(0);
  auto matchName = match.substr(1, match.size() - 2);

  auto format = stringToMatchingFormat[matchName];
  if (doesPixelFormatMatchFileSize(format, frameSize, fileSize))
  {
    if (format.getPredefinedPixelFormat())
      return format;

    const auto dataLayout = findDataLayoutInName(filename);
    return PixelFormatRGB(format.getBitsPerComponent(),
                          dataLayout,
                          format.getChannelOrder(),
                          format.getAlphaMode(),
                          format.getEndianess());
  }

  return {};
}

std::optional<PixelFormatRGB> checkForPixelFormatIndicatorInFileExtension(
  const std::string &filename, const Size &frameSize, const std::optional<std::int64_t> &fileSize)
{
  const auto fileExtension = std::filesystem::path(filename).extension();

  for (const auto &[channelOrder, name] : ChannelOrderMapper)
  {
    if (fileExtension == ("." + functions::toLower(name)))
    {
      auto format = PixelFormatRGB(8, DataLayout::Packed, channelOrder);
      if (doesPixelFormatMatchFileSize(format, frameSize, fileSize))
      {
        const auto dataLayout = findDataLayoutInName(filename);
        return PixelFormatRGB(format.getBitsPerComponent(),
                              dataLayout,
                              format.getChannelOrder(),
                              format.getAlphaMode(),
                              format.getEndianess());
      }
    }
  }
  return {};
}

std::optional<PixelFormatRGB> checkSpecificFileExtensions(
  const std::string &filename, const Size &frameSize, const std::optional<std::int64_t> &fileSize)
{
  const auto fileExtension = std::filesystem::path(filename).extension();

  if (fileExtension == ".cmyk")
  {
    const auto format = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::Last);
    if (doesPixelFormatMatchFileSize(format, frameSize, fileSize))
      return format;
  }
  if (fileExtension == ".rgb565")
  {
    const auto format = PixelFormatRGB(PredefinedPixelFormat::RGB565);
    if (doesPixelFormatMatchFileSize(format, frameSize, fileSize))
      return format;
  }
  if (fileExtension == ".rgb565be")
  {
    const auto format = PixelFormatRGB(PredefinedPixelFormat::RGB565BE);
    if (doesPixelFormatMatchFileSize(format, frameSize, fileSize))
      return format;
  }

  return {};
}

PixelFormatRGB guessPixelFormatFromSizeAndName(const GuessedFrameFormat &guessedFrameFormat,
                                               const FileInfoForGuess   &fileInfo)
{
  if (!guessedFrameFormat.frameSize || fileInfo.filename.empty())
    return {};

  const auto filename  = functions::toLower(fileInfo.filename);
  const auto frameSize = *guessedFrameFormat.frameSize;
  const auto fileSize  = fileInfo.fileSize;

  if (const auto pixelFormat = checkSpecificFileExtensions(filename, frameSize, fileSize))
    return *pixelFormat;

  if (const auto pixelFormat = checkForPixelFormatIndicatorInName(filename, frameSize, fileSize))
    return *pixelFormat;

  if (const auto pixelFormat =
        checkForPixelFormatIndicatorInFileExtension(filename, frameSize, fileSize))
    return *pixelFormat;

  if (const auto pixelFormat = checkForPixelFormatIndicatorInName(
        functions::toLower(fileInfo.parentFolderName), frameSize, fileSize))
    return *pixelFormat;

  if (guessedFrameFormat.frameSize)
    return DEFAULT_PIXEL_FORMAT;

  return {};
}

} // namespace video::rgb
