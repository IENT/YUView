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

#include <QDir>
#include <regex>

namespace video::yuv
{

Subsampling findSubsamplingTypeIndicatorInName(std::string name)
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

  auto match = sm.str(0).substr(1, 3);
  if (auto format = SubsamplingMapper.getValue(match))
    return *format;

  return Subsampling::UNKNOWN;
}

std::vector<unsigned> getDetectionBitDepthList(unsigned forceAsFirst)
{
  std::vector<unsigned> bitDepthList;
  if (forceAsFirst >= 8 && forceAsFirst <= 16)
    bitDepthList.push_back(forceAsFirst);

  for (auto bitDepth : {10u, 12u, 14u, 16u, 8u})
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

bool checkFormat(const PixelFormatYUV pixelFormat, const Size frameSize, const int64_t fileSize)
{
  int bpf = pixelFormat.bytesPerFrame(frameSize);
  return (bpf != 0 && (fileSize % bpf) == 0);
}

PixelFormatYUV testFormatFromSizeAndNamePlanar(std::string name,
                                               const Size  size,
                                               unsigned    bitDepth,
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
                  checkFormat(fmt, size, fileSize))
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
                  checkFormat(fmt, size, fileSize))
                return fmt;
            }
          }
        }
      }
    }
  }
  return {};
}

PixelFormatYUV testFormatFromSizeAndNamePacked(std::string name,
                                               const Size  size,
                                               unsigned    bitDepth,
                                               Subsampling detectedSubsampling,
                                               int64_t     fileSize)
{
  // Check V210
  std::regex  strExpr("(?:_|\\.|-)(v210|V210)(?:_|\\.|-)");
  std::smatch sm;
  if (std::regex_search(name, sm, strExpr))
  {
    auto fmt = PixelFormatYUV(PredefinedPixelFormat::V210);
    if (checkFormat(fmt, size, fileSize))
      return fmt;
  }

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
            std::stringstream formatName;
            formatName << functions::toLower(PackingOrderMapper.getName(packing));
            formatName << SubsamplingMapper.getName(subsampling);
            if (bitDepth > 8)
              formatName << std::to_string(bitDepth) + endianness;
            auto fmt = PixelFormatYUV(subsampling, bitDepth, packing, false, endianness == "be");
            if (name.find(formatName.str()) != std::string::npos &&
                checkFormat(fmt, size, fileSize))
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
                checkFormat(fmt, size, fileSize))
              return fmt;
          }
        }
      }
    }
  }

  return {};
}

PixelFormatYUV guessFormatFromSizeAndName(const Size       size,
                                          unsigned         bitDepth,
                                          DataLayout       dataLayout,
                                          int64_t          fileSize,
                                          const QFileInfo &fileInfo)
{
  // We are going to check two strings (one after the other) for indicators on the YUV format.
  // 1: The file name, 2: The folder name that the file is contained in.

  // The full name of the file
  auto fileName = fileInfo.baseName().toLower().toStdString();
  if (fileName.empty())
    return {};
  fileName += ".";

  // The name of the folder that the file is in
  auto dirName = fileInfo.absoluteDir().dirName().toLower().toStdString();

  if (fileInfo.suffix().toLower() == "v210")
  {
    auto fmt = PixelFormatYUV(PredefinedPixelFormat::V210);
    if (checkFormat(fmt, size, fileSize))
      return fmt;
  }

  for (const auto &name : {fileName, dirName})
  {
    // Check if the filename contains NV12
    if (name.find("nv12") != std::string::npos)
    {
      // This should be a 8 bit semi-planar yuv 4:2:0 file with interleaved UV components and YYYYUV
      // order
      auto fmt = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV, false, {}, true);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }

    // Check if the filename contains NV21
    if (name.find("nv21") != std::string::npos)
    {
      // This should be a 8 bit semi-planar yuv 4:2:0 file with interleaved UV components and YYYYVU
      // order
      auto fmt = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YVU, false, {}, true);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }

    auto subsampling = findSubsamplingTypeIndicatorInName(name);

    // First, lets see if there is a YUV format defined as FFMpeg names them:
    // First the YUV order, then the subsampling, then a 'p' if the format is planar, then the
    // number of bits (if > 8), finally 'le' or 'be' if bits is > 8. E.g: yuv420p, yuv420p10le,
    // yuv444p16be
    if (dataLayout == DataLayout::Packed)
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
      auto fmt = PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::AYUV, false, false);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }

    // Another one are the formaty of "gray8le" to "gray16le" which are planar 400 formats
    for (auto depth : getDetectionBitDepthList(bitDepth))
    {
      if (name.find("gray" + std::to_string(depth) + "le") != std::string::npos)
      {
        auto fmt = PixelFormatYUV(Subsampling::YUV_400, depth);
        if (checkFormat(fmt, size, fileSize))
          return fmt;
      }
    }

    // OK that did not work so far. Try other things. Just check if the file name contains one of
    // the subsampling strings. Further parameters: YUV plane order, little endian. The first format
    // to match the file size wins.
    auto bitDepths = BitDepthList;
    if (bitDepth != 0)
      // We already extracted a bit depth from the name. Only try that.
      bitDepths = {bitDepth};

    for (const auto &[subsampling, subsamplingName] : SubsamplingMapper)
    {
      auto nameLower = functions::toLower(name);
      if (nameLower.find(subsamplingName) != std::string::npos)
      {
        // Try this subsampling with all bitDepths
        for (auto bd : bitDepths)
        {
          PixelFormatYUV fmt;
          if (dataLayout == DataLayout::Packed)
            fmt = PixelFormatYUV(subsampling, bd, PackingOrder::YUV);
          else
            fmt = PixelFormatYUV(subsampling, bd, PlaneOrder::YUV);
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
  if (bitDepth != 0)
    // We already extracted a bit depth from the name. Only try that.
    testBitDepths.push_back(bitDepth);
  else
    // We don't know the bit depth. Try different values.
    testBitDepths = {8, 9, 10, 12, 14, 16};

  for (const auto &subsampling : testSubsamplings)
  {
    for (auto bd : testBitDepths)
    {
      auto fmt = PixelFormatYUV(subsampling, bd, PlaneOrder::YUV);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }
  }

  // Guessing failed
  return {};
}

} // namespace video::yuv
