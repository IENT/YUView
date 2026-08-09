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

#include "PixelFormatYUVGuess.h"

#include <common/Functions.h>

#include <regex>

using filesource::frameFormatGuess::FileInfoForGuess;
using filesource::frameFormatGuess::GuessedFrameFormat;

namespace video::yuv
{

namespace
{

Subsampling findSubsamplingTypeIndicatorInName(const std::string &name)
{
  std::string matcher = "(?:_|\\.|-)(";
  for (const auto subsampling : SubsamplingMapper.getNames())
    matcher += std::string(subsampling) + "|";
  matcher.pop_back(); // Remove last |
  matcher += ")(?:_|\\.|-)";

  std::smatch sm;
  std::regex  strExpr(matcher);
  if (!std::regex_search(name, sm, strExpr))
    return Subsampling::UNKNOWN;

  const auto match = sm.str(0).substr(1, 3);
  if (const auto format = SubsamplingMapper.getValue(match))
    return *format;

  return Subsampling::UNKNOWN;
}

std::vector<unsigned> getDetectionBitDepthList(const std::optional<unsigned> &detectedBitrate)
{
  if (detectedBitrate)
    return {*detectedBitrate};
  return {10u, 12u, 14u, 16u, 8u};
}

std::vector<Subsampling> getDetectionSubsamplingList(Subsampling subsamplingToForceAsFirst,
                                                     bool        packed)
{
  std::vector<Subsampling> detectionSubsampleList;
  if (packed)
  {
    detectionSubsampleList.push_back(Subsampling::YUV_444);
    detectionSubsampleList.push_back(Subsampling::YUV_422);
  }
  else
    detectionSubsampleList.push_back(Subsampling::YUV_420);
  detectionSubsampleList.push_back(Subsampling::YUV_422);
  detectionSubsampleList.push_back(Subsampling::YUV_444);
  detectionSubsampleList.push_back(Subsampling::YUV_400);

  std::vector<Subsampling> subsamplingList;
  if (subsamplingToForceAsFirst != Subsampling::UNKNOWN)
    subsamplingList.push_back(subsamplingToForceAsFirst);
  for (auto subsampling : detectionSubsampleList)
    if (subsampling != subsamplingToForceAsFirst)
      subsamplingList.push_back(subsampling);
  return subsamplingList;
}

bool doesPixelFormatMatchFileSize(const PixelFormatYUV         &pixelFormat,
                                  const std::optional<Size>    &frameSize,
                                  const std::optional<int64_t> &fileSize)
{
  if (!fileSize || !frameSize)
    return true;

  const auto bytesPerFrame = pixelFormat.bytesPerFrame(*frameSize);
  if (bytesPerFrame <= 0)
    return false;

  const auto isFileSizeAMultipleOfFrameSize = (*fileSize % bytesPerFrame) == 0;
  return isFileSizeAMultipleOfFrameSize;
}

PixelFormatYUV testFormatFromSizeAndNamePlanar(const std::string            &name,
                                               const GuessedFrameFormat      guessedFrameFormat,
                                               const Subsampling             detectedSubsampling,
                                               const std::optional<int64_t> &fileSize)
{
  const auto planarYUVToTest = std::map<std::string, PlaneOrder>({{"yuv", PlaneOrder::YUV},
                                                                  {"yuvj", PlaneOrder::YUV},
                                                                  {"yvu", PlaneOrder::YVU},
                                                                  {"yuva", PlaneOrder::YUVA},
                                                                  {"yvua", PlaneOrder::YVUA}});

  auto bitDepthList = getDetectionBitDepthList(guessedFrameFormat.bitDepth);

  for (auto &entry : planarYUVToTest)
  {
    for (auto subsampling : getDetectionSubsamplingList(detectedSubsampling, false))
    {
      for (auto bitDepth : bitDepthList)
      {
        auto endiannessList = std::vector<std::string>({"le"});
        if (bitDepth > 8)
          endiannessList.push_back("be");

        for (const auto &endianness : endiannessList)
        {
          for (std::string interlacedString : {"uvi", "interlaced", ""})
          {
            const bool interlaced = !interlacedString.empty();
            {
              std::stringstream formatName;
              formatName << entry.first << SubsamplingMapper.getName(subsampling) << "p";
              if (bitDepth > 8)
                formatName << bitDepth << endianness;
              formatName << interlacedString;
              auto fmt = PixelFormatYUV(
                subsampling, bitDepth, entry.second, endianness == "be", {}, interlaced);
              if (name.find(formatName.str()) != std::string::npos &&
                  doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileSize))
                return fmt;
            }

            if (subsampling == detectedSubsampling && detectedSubsampling != Subsampling::UNKNOWN)
            {
              // Also try the string without the subsampling indicator (which we already detected)
              std::stringstream formatName;
              formatName << entry.first << "p";
              if (bitDepth > 8)
                formatName << bitDepth << endianness;
              formatName << interlacedString;
              auto fmt = PixelFormatYUV(
                subsampling, bitDepth, entry.second, endianness == "be", {}, interlaced);
              if (name.find(formatName.str()) != std::string::npos &&
                  doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileSize))
                return fmt;
            }
          }
        }
      }
    }
  }
  return {};
}

PixelFormatYUV testFormatFromSizeAndNamePacked(const std::string            &name,
                                               const GuessedFrameFormat      guessedFrameFormat,
                                               const Subsampling             detectedSubsampling,
                                               const std::optional<int64_t> &fileSize)
{
  // Check V210
  std::regex  strExpr("(?:_|\\.|-)(v210|V210)(?:_|\\.|-)");
  std::smatch sm;
  if (std::regex_search(name, sm, strExpr))
  {
    const auto fmt = PixelFormatYUV(PredefinedPixelFormat::V210);
    if (doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileSize))
      return fmt;
  }

  const auto bitDepthList = getDetectionBitDepthList(guessedFrameFormat.bitDepth);

  for (const auto subsampling : getDetectionSubsamplingList(detectedSubsampling, true))
  {
    const auto packingTypes = getSupportedPackingFormats(subsampling);
    for (auto packing : packingTypes)
    {
      for (auto bitDepth : bitDepthList)
      {
        auto endiannessList = std::vector<std::string>({"le"});
        if (bitDepth > 8)
          endiannessList.push_back("be");

        for (const auto &endianness : endiannessList)
        {
          {
            std::stringstream formatName;
            formatName << functions::toLower(PackingOrderMapper.getName(packing));
            formatName << SubsamplingMapper.getName(subsampling);
            if (bitDepth > 8)
              formatName << std::to_string(bitDepth) + endianness;
            auto fmt = PixelFormatYUV(subsampling, bitDepth, packing, false, endianness == "be");
            if (name.find(formatName.str()) != std::string::npos &&
                doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileSize))
              return fmt;
          }

          if (subsampling == detectedSubsampling && detectedSubsampling != Subsampling::UNKNOWN)
          {
            // Also try the string without the subsampling indicator (which we already detected)
            std::stringstream formatName;
            formatName << functions::toLower(PackingOrderMapper.getName(packing));
            if (bitDepth > 8)
              formatName << bitDepth << endianness;
            auto fmt = PixelFormatYUV(subsampling, bitDepth, packing, false, endianness == "be");
            if (name.find(formatName.str()) != std::string::npos &&
                doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileSize))
              return fmt;
          }
        }
      }
    }
  }

  return {};
}

std::optional<PixelFormatYUV>
checkSpecificFileExtensions(const GuessedFrameFormat &guessedFrameFormat,
                            const FileInfoForGuess   &fileInfo)
{
  const auto fileExtension = std::filesystem::path(fileInfo.filename).extension();

  if (fileExtension == ".raw")
  {
    const auto rawBayerFormat =
      PixelFormatYUV(Subsampling::YUV_400, guessedFrameFormat.bitDepth.value_or(8));
    if (doesPixelFormatMatchFileSize(
          rawBayerFormat, guessedFrameFormat.frameSize, fileInfo.fileSize))
      return rawBayerFormat;
  }

  if (fileExtension == ".v210" || fileExtension == ".V210")
  {
    const auto v210Format = PixelFormatYUV(PredefinedPixelFormat::V210);
    if (doesPixelFormatMatchFileSize(v210Format, guessedFrameFormat.frameSize, fileInfo.fileSize))
      return v210Format;
  }

  return {};
}

std::optional<PixelFormatYUV> checForNVIndicator(const std::string_view             name,
                                                 const std::optional<Size>         &frameSize,
                                                 const std::optional<std::int64_t> &fileSize)
{
  if (name.find("nv12") != std::string::npos)
  {
    // This should be a 8 bit semi-planar yuv 4:2:0 file with interleaved UV components and YYYYUV
    // order
    const auto fmt = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV, false, {}, true);
    if (doesPixelFormatMatchFileSize(fmt, frameSize, fileSize))
      return fmt;
  }

  if (name.find("nv21") != std::string::npos)
  {
    // This should be a 8 bit semi-planar yuv 4:2:0 file with interleaved UV components and YYYYVU
    // order
    auto fmt = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YVU, false, {}, true);
    if (doesPixelFormatMatchFileSize(fmt, frameSize, fileSize))
      return fmt;
  }

  return {};
}

std::optional<PixelFormatYUV>
checkFFmpegPixelFormatNames(const std::string        &name,
                            const GuessedFrameFormat &guessedFrameFormat,
                            const FileInfoForGuess   &fileInfo)
{
  // Check for names as FFMpeg names them:
  // First the YUV order, then the subsampling, then a 'p' if the format is planar, then the
  // number of bits (if > 8), finally 'le' or 'be' if bits is > 8. E.g: yuv420p, yuv420p10le,
  // yuv444p16be

  const auto subsampling = findSubsamplingTypeIndicatorInName(name);

  const auto checkPackedFormatsFirst = (guessedFrameFormat.dataLayout == DataLayout::Packed);
  if (checkPackedFormatsFirst)
  {
    if (const auto fmt =
          testFormatFromSizeAndNamePacked(name, guessedFrameFormat, subsampling, fileInfo.fileSize))
      return fmt;
    if (const auto fmt =
          testFormatFromSizeAndNamePlanar(name, guessedFrameFormat, subsampling, fileInfo.fileSize))
      return fmt;
  }
  else
  {
    if (const auto fmt =
          testFormatFromSizeAndNamePlanar(name, guessedFrameFormat, subsampling, fileInfo.fileSize))
      return fmt;
    if (const auto fmt =
          testFormatFromSizeAndNamePacked(name, guessedFrameFormat, subsampling, fileInfo.fileSize))
      return fmt;
  }

  // One more FFMpeg format description that does not match the pattern above is: "ayuv64le"
  if (name.find("ayuv64le") != std::string::npos)
  {
    // Check if the format and the file size match
    auto fmt = PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::AYUV, false, false);
    if (doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileInfo.fileSize))
      return fmt;
  }

  // Another one are the formaty of "gray8le" to "gray16le" which are planar 400 formats
  for (auto bitDepth : getDetectionBitDepthList(guessedFrameFormat.bitDepth))
  {
    if (name.find("gray" + std::to_string(bitDepth) + "le") != std::string::npos)
    {
      auto fmt = PixelFormatYUV(Subsampling::YUV_400, bitDepth);
      if (doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileInfo.fileSize))
        return fmt;
    }
  }

  return {};
}

std::optional<PixelFormatYUV>
checkForSubsamplingIndiatorInName(const std::string        &name,
                                  const GuessedFrameFormat &guessedFrameFormat,
                                  const FileInfoForGuess   &fileInfo)
{
  // Just check if the file name contains one of  the subsampling strings. Further parameters: YUV
  // plane order, little endian. The first format to match the file size wins.
  auto bitDepths = BitDepthList;
  if (guessedFrameFormat.bitDepth)
    // We already extracted a bit depth from the name. Only try that.
    bitDepths = {*guessedFrameFormat.bitDepth};

  for (const auto &[subsampling, subsamplingName] : SubsamplingMapper)
  {
    const auto nameLower = functions::toLower(name);
    if (nameLower.find(subsamplingName) != std::string::npos)
    {
      // Try this subsampling with all bitDepths
      for (const auto bitDepth : bitDepths)
      {
        PixelFormatYUV fmt;
        if (guessedFrameFormat.dataLayout == DataLayout::Packed)
          fmt = PixelFormatYUV(subsampling, bitDepth, PackingOrder::YUV);
        else
          fmt = PixelFormatYUV(subsampling, bitDepth, PlaneOrder::YUV);
        if (doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileInfo.fileSize))
          return fmt;
      }
    }
  }

  return {};
}

std::optional<PixelFormatYUV> ignoreNameAndJustCheckIfSomeBasicFormatsMatchTheFileSize(
  const GuessedFrameFormat &guessedFrameFormat, const FileInfoForGuess &fileInfo)
{
  const auto testSubsamplings =
    std::vector<Subsampling>({Subsampling::YUV_420, Subsampling::YUV_444, Subsampling::YUV_422});

  std::vector<int> testBitDepths;
  if (guessedFrameFormat.bitDepth)
    testBitDepths.push_back(*guessedFrameFormat.bitDepth);
  else
    testBitDepths = {8, 9, 10, 12, 14, 16};

  for (const auto &subsampling : testSubsamplings)
  {
    for (const auto bd : testBitDepths)
    {
      auto fmt = PixelFormatYUV(subsampling, bd, PlaneOrder::YUV);
      if (doesPixelFormatMatchFileSize(fmt, guessedFrameFormat.frameSize, fileInfo.fileSize))
        return fmt;
    }
  }

  return {};
}

} // namespace

PixelFormatYUV guessPixelFormatFromSizeAndName(const GuessedFrameFormat &guessedFrameFormat,
                                               const FileInfoForGuess   &fileInfo)
{
  if (fileInfo.filename.empty())
    return {};

  if (const auto pixelFormat = checkSpecificFileExtensions(guessedFrameFormat, fileInfo))
    return *pixelFormat;

  for (const auto &name :
       {functions::toLower(fileInfo.filename), functions::toLower(fileInfo.parentFolderName)})
  {
    if (const auto pixelFormat =
          checForNVIndicator(name, guessedFrameFormat.frameSize, fileInfo.fileSize))
      return *pixelFormat;

    if (const auto pixelFormat = checkFFmpegPixelFormatNames(name, guessedFrameFormat, fileInfo))
      return *pixelFormat;

    if (const auto pixelFormat =
          checkForSubsamplingIndiatorInName(name, guessedFrameFormat, fileInfo))
      return *pixelFormat;
  }

  if (const auto pixelFormat =
        ignoreNameAndJustCheckIfSomeBasicFormatsMatchTheFileSize(guessedFrameFormat, fileInfo))
    return *pixelFormat;

  return {};
}

} // namespace video::yuv
