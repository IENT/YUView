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

#include "FrameFormatGuess.h"

#include <common/Functions.h>

#include <regex>

namespace filesource::frameFormatGuess
{

using namespace std::string_view_literals;
using functions::toUnsigned;

namespace
{

GuessedFrameFormat guessFrameSizeFPSAndBitDepthFromName(const std::string &name)
{
  // The regular expressions to match. They are sorted from most detailed to least so that the
  // most detailed ones are tested first.
  // First matches Something_2160x1440_60_8_more.yuv or omething_2160x1440_60_8b.yuv or
  // Something_2160x1440_60Hz_8_more.yuv Second matches Something_2160x1440_60_more.yuv or
  // Something_2160x1440_60.yuv Thirs matches Something_2160x1440_more.yuv or
  // Something_2160x1440.yuv
  const auto REGEXP_LIST = {"([0-9]+)(?:x|\\*)([0-9]+)_([0-9]+)(?:hz)?_([0-9]+)b?[\\._]",
                            "([0-9]+)(?:x|\\*)([0-9]+)_([0-9]+)(?:hz)?[\\._]",
                            "([0-9]+)(?:x|\\*)([0-9]+)[\\._]"};

  for (const auto &regExpStr : REGEXP_LIST)
  {
    std::smatch      frameSizeMatch;
    const std::regex strExpr(regExpStr);
    if (std::regex_search(name, frameSizeMatch, strExpr))
    {
      GuessedFrameFormat result;

      const auto width  = toUnsigned(frameSizeMatch[1].str());
      const auto height = toUnsigned(frameSizeMatch[2].str());
      if (width && height)
        result.frameSize = Size(*width, *height);

      if (frameSizeMatch.size() >= 4)
        if (const auto frameRate = toUnsigned(frameSizeMatch[3].str()))
          result.frameRate = *frameRate;

      if (frameSizeMatch.size() >= 5)
        if (const auto bitDepth = toUnsigned(frameSizeMatch[4].str()))
          result.bitDepth = *bitDepth;

      return result;
    }
  }

  return {};
}

GuessedFrameFormat guessFrameSizeAndFrameRateFromResolutionIndicators(const std::string &name)
{
  using RegexpAndSize             = std::pair<std::string_view, Size>;
  const auto REGEXP_AND_SIZE_LIST = {RegexpAndSize({"1080p([0-9]+)", Size(1920, 1080)}),
                                     RegexpAndSize({"720p([0-9]+)", Size(1280, 720)})};

  for (const auto &[regExpStr, frameSizeForMatch] : REGEXP_AND_SIZE_LIST)
  {
    std::smatch      frameSizeMatch;
    const std::regex strExpr(regExpStr.data());
    if (std::regex_search(name, frameSizeMatch, strExpr))
    {
      GuessedFrameFormat result;

      result.frameSize = frameSizeForMatch;
      if (const auto frameRate = toUnsigned(frameSizeMatch[1].str()))
        result.frameRate = *frameRate;

      return result;
    }
  }
  return {};
}

std::optional<Size> guessFrameSizeFromAcronymResolutionIndicators(const std::string &name)
{
  if (name.find("_cif") != std::string::npos)
    return Size(352, 288);
  else if (name.find("_qcif") != std::string::npos)
    return Size(176, 144);
  else if (name.find("_4cif") != std::string::npos)
    return Size(704, 576);
  else if (name.find("uhd") != std::string::npos)
    return Size(3840, 2160);
  else if (name.find("hd") != std::string::npos)
    return Size(1920, 1080);
  else if (name.find("1080p") != std::string::npos)
    return Size(1920, 1080);
  else if (name.find("720p") != std::string::npos)
    return Size(1280, 720);

  return {};
}

std::optional<int> guessFPSFromFPSOrHzIndicators(const std::string &name)
{
  std::smatch      frameSizeMatch;
  const std::regex strExpr("([0-9]+)(fps|hz)");
  if (std::regex_search(name, frameSizeMatch, strExpr))
    return toUnsigned(frameSizeMatch[1].str());
  return {};
}

std::optional<unsigned> guessBitDepthFromName(const std::string &name)
{
  const auto REGEXP_LIST = {
      "(8|9|10|12|16)-?(bit)",                // E.g. 10bit, 10BIT, 10-bit, 10-BIT
      "(?:_|\\.|-)(8|9|10|12|16)b(?:_|\\.|-)" // E.g. _16b_ .8b. -12b-
  };

  for (const auto &regExpStr : REGEXP_LIST)
  {
    std::smatch      bitDepthMatcher;
    const std::regex strExpr(regExpStr);
    if (std::regex_search(name, bitDepthMatcher, strExpr))
    {
      if (const auto bitDepth = toUnsigned(bitDepthMatcher[1].str()))
        return bitDepth;
    }
  }

  return {};
}

std::optional<video::DataLayout> guessIsPackedFromName(const std::string &name)
{
  std::smatch      packedMatch;
  const std::regex strExpr("(?:_|\\.|-)packed(?:_|\\.|-)");
  if (std::regex_search(name, packedMatch, strExpr))
    return video::DataLayout::Packed;
  return {};
}

} // namespace

bool GuessedFrameFormat::operator==(const GuessedFrameFormat &rhs) const
{
  return this->frameSize == rhs.frameSize && //
         this->frameRate == rhs.frameRate && //
         this->bitDepth == rhs.bitDepth &&   //
         this->dataLayout == rhs.dataLayout;
}

FileInfoForGuess getFileInfoForGuessFromPath(const std::filesystem::path filePath)
{
  FileInfoForGuess fileInfoForGuess;

  // Use UTF-8 via Qt so CJK filenames do not throw on path.string() (ACP).
  fileInfoForGuess.filename         = functions::fsPathToUtf8String(filePath.filename());
  fileInfoForGuess.parentFolderName = functions::fsPathToUtf8String(filePath.parent_path());

  try
  {
    fileInfoForGuess.fileSize = static_cast<int64_t>(std::filesystem::file_size(filePath));
  }
  catch (...)
  {
  }

  return fileInfoForGuess;
}

GuessedFrameFormat guessFrameFormat(const FileInfoForGuess &fileInfo)
{
  if (fileInfo.filename.empty())
    return {};

  GuessedFrameFormat result;

  for (auto const &name : {fileInfo.filename, fileInfo.parentFolderName})
  {
    const auto nameLower = functions::toLower(name);

    if (!result.frameSize)
      result = guessFrameSizeFPSAndBitDepthFromName(nameLower);

    if (!result.frameSize)
      result = guessFrameSizeAndFrameRateFromResolutionIndicators(nameLower);

    if (!result.frameSize)
      result.frameSize = guessFrameSizeFromAcronymResolutionIndicators(nameLower);

    if (!result.frameSize)
      continue;

    if (!result.frameRate)
      result.frameRate = guessFPSFromFPSOrHzIndicators(nameLower);

    if (!result.bitDepth)
      result.bitDepth = guessBitDepthFromName(nameLower);

    if (!result.dataLayout)
      result.dataLayout = guessIsPackedFromName(nameLower);
  }

  return result;
}

} // namespace filesource::frameFormatGuess
