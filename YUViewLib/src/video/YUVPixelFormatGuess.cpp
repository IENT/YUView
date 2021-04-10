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

#include "YUVPixelFormatGuess.h"

#include "common/functions.h"

#include <QDir>
#include <regex>

namespace YUV_Internals
{

Subsampling findSubsamplingTypeIndicatorInName(std::string name)
{
  std::string subsamplingMatcher;
  for (auto subsampling : SubsamplingMapper.getNames())
    subsamplingMatcher += subsampling + "|";
  subsamplingMatcher.pop_back();
  subsamplingMatcher = "(?:_|\\.|-)(" + subsamplingMatcher + ")(?:_|\\.|-)";

  std::regex strExpr(subsamplingMatcher);

  std::smatch sm;
  if (!std::regex_match(name, sm, strExpr))
    return Subsampling::UNKNOWN;

  // BEFORE RELEASE: Make sure this is tested
  auto match = sm.str(0);
  if (auto format = SubsamplingMapper.getValue(match))
    return *format;

  return Subsampling::UNKNOWN;
}

std::vector<unsigned> getDetectionBitDepthList(unsigned forceAsFirst)
{
  std::vector<unsigned> bitDepthList;
  if (forceAsFirst >= 8 && forceAsFirst <= 16)
    bitDepthList.push_back(forceAsFirst);

  for (auto bitDepth : {8u, 10u, 12u, 14u, 16u, 8u})
  {
    if (!vectorContains(bitDepthList, bitDepth))
      bitDepthList.push_back(bitDepth);
  }
  return bitDepthList;
}

std::vector<Subsampling> getDetectionSubsamplingList(Subsampling forceAsFirst, bool packed)
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
  if (forceAsFirst != Subsampling::UNKNOWN)
    subsamplingList.push_back(forceAsFirst);
  for (auto s : detectionSubsampleList)
    if (!vectorContains(subsamplingList, s))
      subsamplingList.push_back(s);
  return subsamplingList;
}

bool checkFormat(const YUV_Internals::YUVPixelFormat pixelFormat,
                 const Size                          frameSize,
                 const int64_t                       fileSize)
{
  int bpf = pixelFormat.bytesPerFrame(frameSize);
  return (bpf != 0 && (fileSize % bpf) == 0);
}

YUVPixelFormat testFormatFromSizeAndNamePlanar(std::string name,
                                               const Size  size,
                                               int         bitDepth,
                                               Subsampling detectedSubsampling,
                                               int64_t     fileSize)
{
  // First all planar formats
  auto planarYUVToTest = std::map<std::string, PlaneOrder>({{"yuv", PlaneOrder::YUV},
                                                            {"yuvj", PlaneOrder::YUV},
                                                            {"yvu", PlaneOrder::YVU},
                                                            {"yuva", PlaneOrder::YUVA},
                                                            {"yvua", PlaneOrder::YVUA}});

  auto bitDepthList = getDetectionBitDepthList(bitDepth);

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
          for (std::string interlacedString : {"UVI", "interlaced", ""})
          {
            const bool interlaced = !interlacedString.empty();
            {
              auto subsamplingName = SubsamplingMapper.getName(subsampling);
              if (!subsamplingName)
                continue;
              auto formatName = entry.first + *subsamplingName + "p";
              if (bitDepth > 8)
                formatName += std::to_string(bitDepth) + endianness;
              formatName += interlacedString;
              auto fmt = YUVPixelFormat(
                  subsampling, bitDepth, entry.second, endianness == "be", {}, interlaced);
              if (name.find(formatName) != std::string::npos && checkFormat(fmt, size, fileSize))
                return fmt;
            }

            if (subsampling == detectedSubsampling && detectedSubsampling != Subsampling::UNKNOWN)
            {
              // Also try the string without the subsampling indicator (which we already detected)
              auto formatName = entry.first + "p";
              if (bitDepth > 8)
                formatName += std::to_string(bitDepth) + endianness;
              formatName += interlacedString;
              auto fmt = YUVPixelFormat(
                  subsampling, bitDepth, entry.second, endianness == "be", {}, interlaced);
              if (name.find(formatName) != std::string::npos && checkFormat(fmt, size, fileSize))
                return fmt;
            }
          }
        }
      }
    }
  }
  return {};
}

YUVPixelFormat testFormatFromSizeAndNamePacked(std::string name,
                                               const Size  size,
                                               int         bitDepth,
                                               Subsampling detectedSubsampling,
                                               int64_t     fileSize)
{
  auto bitDepthList = getDetectionBitDepthList(bitDepth);

  for (auto subsampling : getDetectionSubsamplingList(detectedSubsampling, true))
  {
    // What packing formats are supported by this subsampling?
    auto packingTypes = getSupportedPackingFormats(subsampling);
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
            auto packingNameLower = functions::toLower(*PackingOrderMapper.getName(packing));
            auto formatName       = packingNameLower + *SubsamplingMapper.getName(subsampling);
            if (bitDepth > 8)
              formatName += std::to_string(bitDepth) + endianness;
            auto fmt = YUVPixelFormat(subsampling, bitDepth, packing, false, endianness == "be");
            if (name.find(formatName) != std::string::npos && checkFormat(fmt, size, fileSize))
              return fmt;
          }

          if (subsampling == detectedSubsampling && detectedSubsampling != Subsampling::UNKNOWN)
          {
            // Also try the string without the subsampling indicator (which we already detected)
            auto formatName = functions::toLower(*PackingOrderMapper.getName(packing));
            if (bitDepth > 8)
              formatName += std::to_string(bitDepth) + endianness;
            auto fmt = YUVPixelFormat(subsampling, bitDepth, packing, false, endianness == "be");
            if (name.find(formatName) != std::string::npos && checkFormat(fmt, size, fileSize))
              return fmt;
          }
        }
      }
    }
  }
  return {};
}

YUVPixelFormat guessFormatFromSizeAndName(
    const Size size, int bitDepth, bool packed, int64_t fileSize, const QFileInfo &fileInfo)
{
  // We are going to check two strings (one after the other) for indicators on the YUV format.
  // 1: The file name, 2: The folder name that the file is contained in.
  std::vector<std::string> checkStrings;

  // The full name of the file
  auto fileName = fileInfo.baseName().toStdString();
  if (fileName.empty())
    return {};
  checkStrings.push_back(fileName);

  // The name of the folder that the file is in
  auto dirName = fileInfo.absoluteDir().dirName().toStdString();
  checkStrings.push_back(dirName);

  if (fileInfo.suffix().toLower() == "nv21")
  {
    // This should be a 8 bit planar yuv 4:2:0 file with interleaved UV components and YVU order
    auto fmt = YUVPixelFormat(Subsampling::YUV_420, 8, PlaneOrder::YVU, false, {}, true);
    auto bpf = fmt.bytesPerFrame(size);
    if (bpf != 0 && (fileSize % bpf) == 0)
    {
      // Bits per frame and file size match
      return fmt;
    }
  }

  for (const auto &name : checkStrings)
  {
    auto subsampling = findSubsamplingTypeIndicatorInName(name);

    // First, lets see if there is a YUV format defined as FFMpeg names them:
    // First the YUV order, then the subsampling, then a 'p' if the format is planar, then the
    // number of bits (if > 8), finally 'le' or 'be' if bits is > 8. E.g: yuv420p, yuv420p10le,
    // yuv444p16be
    if (packed)
    {
      // Check packed formats first
      auto fmt = testFormatFromSizeAndNamePacked(name, size, bitDepth, subsampling, fileSize);
      if (fmt.isValid())
        return fmt;
      fmt = testFormatFromSizeAndNamePlanar(name, size, bitDepth, subsampling, fileSize);
      if (fmt.isValid())
        return fmt;
    }
    else
    {
      // Check planar formats first
      auto fmt = testFormatFromSizeAndNamePlanar(name, size, bitDepth, subsampling, fileSize);
      if (fmt.isValid())
        return fmt;
      fmt = testFormatFromSizeAndNamePacked(name, size, bitDepth, subsampling, fileSize);
      if (fmt.isValid())
        return fmt;
    }

    // One more FFMpeg format description that does not match the pattern above is: "ayuv64le"
    if (name.find("ayuv64le") != std::string::npos)
    {
      // Check if the format and the file size match
      auto fmt = YUVPixelFormat(Subsampling::YUV_444, 16, PackingOrder::AYUV, false, false);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }

    // OK that did not work so far. Try other things. Just check if the file name contains one of
    // the subsampling strings. Further parameters: YUV plane order, little endian. The first format
    // to match the file size wins.
    auto bitDepths = BitDepthList;
    if (bitDepth != -1)
      // We already extracted a bit depth from the name. Only try that.
      bitDepths = std::vector<unsigned>({unsigned(bitDepth)});
    for (auto &subsamplingEntry : SubsamplingMapper.entries())
    {
      auto nameLower = functions::toLower(name);
      if (nameLower.find(subsamplingEntry.name) != std::string::npos)
      {
        // Try this subsampling with all bitDepths
        for (auto bd : bitDepths)
        {
          YUVPixelFormat fmt;
          if (packed)
            fmt = YUVPixelFormat(subsamplingEntry.value, bd, PackingOrder::YUV);
          else
            fmt = YUVPixelFormat(subsamplingEntry.value, bd, PlaneOrder::YUV);
          if (checkFormat(fmt, size, fileSize))
            return fmt;
        }
      }
    }
  }

  // Nothing using the name worked so far. Check some formats. The first one that matches the file
  // size wins.
  const auto testSubsamplings =
      std::vector<Subsampling>({Subsampling::YUV_420, Subsampling::YUV_444, Subsampling::YUV_422});

  std::vector<int> testBitDepths;
  if (bitDepth != -1)
    // We already extracted a bit depth from the name. Only try that.
    testBitDepths.push_back(bitDepth);
  else
    // We don't know the bit depth. Try different values.
    testBitDepths = {8, 9, 10, 12, 14, 16};

  for (const auto &subsampling : testSubsamplings)
  {
    for (auto bd : testBitDepths)
    {
      auto fmt = YUVPixelFormat(subsampling, bd, PlaneOrder::YUV);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }
  }

  // Guessing failed
  return {};
}

} // namespace YUV_Internals
