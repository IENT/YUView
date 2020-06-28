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

#include "yuvPixelFormatGuess.h"

#include <QSize>
#include <QDir>

namespace YUV_Internals
{

Subsampling findSubsamplingTypeIndicatorInName(QString name)
{
  QString subsamplingMatcher;
  for (auto subsampling : subsamplingNameList)
    subsamplingMatcher += subsampling + "|";
  subsamplingMatcher.chop(1);
  subsamplingMatcher = "(?:_|\\.|-)(" + subsamplingMatcher + ")(?:_|\\.|-)";
  QRegExp exp(subsamplingMatcher);
  if (exp.indexIn(name) > -1)
  {
    auto match = exp.cap(1);
    auto matchIndex = subsamplingNameList.indexOf(match);
    if (matchIndex >= 0)
      return Subsampling(matchIndex);
  }
  return Subsampling::UNKNOWN;
}

QList<int> getDetectionBitDepthList(int forceAsFirst)
{
  const auto detectionBitDepthList = QList<int>() << 9 << 10 << 12 << 14 << 16 << 8;

  QList<int> bitDepthList;
  if (forceAsFirst >= 8 && forceAsFirst <= 16)
    bitDepthList.append(forceAsFirst);

  for (auto bitDepth : detectionBitDepthList)
  {
    if (!bitDepthList.contains(bitDepth))
      bitDepthList.append(bitDepth);
  }
  return bitDepthList;
}

QList<Subsampling> getDetectionSubsamplingList(Subsampling forceAsFirst, bool packed)
{
  QList<Subsampling> detectionSubsampleList;
  if (packed)
    detectionSubsampleList << Subsampling::YUV_444 << Subsampling::YUV_422;
  else
    detectionSubsampleList << Subsampling::YUV_420 << Subsampling::YUV_422 << Subsampling::YUV_444 << Subsampling::YUV_400;

  QList<Subsampling> subsamplingList;
  if (forceAsFirst != Subsampling::UNKNOWN)
    subsamplingList.append(forceAsFirst);
  for (auto s : detectionSubsampleList)
    if (!subsamplingList.contains(s))
      subsamplingList.append(s);
  return subsamplingList;
}

bool checkFormat(const YUV_Internals::yuvPixelFormat pixelFormat, const QSize frameSize, const int64_t fileSize)
{
  int bpf = pixelFormat.bytesPerFrame(frameSize);
  return (bpf != 0 && (fileSize % bpf) == 0);
}

yuvPixelFormat testFormatFromSizeAndNamePlanar(QString name, const QSize size, int bitDepth, Subsampling detectedSubsampling, int64_t fileSize)
{
  // First all planar formats
  auto planarYUVOrderList = QStringList() << "yuv" << "yuvj" << "yvu" << "yuva" << "yvua";
  auto bitDepthList = getDetectionBitDepthList(bitDepth);

  for (int o = 0; o < planarYUVOrderList.count(); o++)
  {
    auto planeOrder = (o > 0) ? planeOrderList[o-1] : planeOrderList[0];

    for (auto subsampling : getDetectionSubsamplingList(detectedSubsampling, false))
    {
      for (int bitDepth : bitDepthList)
      {
        auto endianessList = QStringList() << "le";
        if (bitDepth > 8)
          endianessList << "be";

        for (const QString &endianess : endianessList)
        {
          for (QString interlacedString : {"UVI", "interlaced", ""})
          {
            const bool interlaced = !interlacedString.isEmpty();
            {
              QString formatName = planarYUVOrderList[o] + subsamplingToString(subsampling) + "p";
              if (bitDepth > 8)
                formatName += QString::number(bitDepth) + endianess;
              formatName += interlacedString;
              auto fmt = yuvPixelFormat(subsampling, bitDepth, planeOrder, endianess=="be");
              fmt.uvInterleaved = interlaced;
              if (name.contains(formatName) && checkFormat(fmt, size, fileSize))
                return fmt;
            }
            
            if (subsampling == detectedSubsampling && detectedSubsampling != Subsampling::UNKNOWN)
            {
              // Also try the string without the subsampling indicator (which we already detected)
              QString formatName = planarYUVOrderList[o] + "p";
              if (bitDepth > 8)
                formatName += QString::number(bitDepth) + endianess;
              formatName += interlacedString;
              auto fmt = yuvPixelFormat(subsampling, bitDepth, planeOrder, endianess=="be");
              fmt.uvInterleaved = interlaced;
              if (name.contains(formatName) && checkFormat(fmt, size, fileSize))
                return fmt;
            }
          }
        }
      }
    }
  }
  return {};
}

yuvPixelFormat testFormatFromSizeAndNamePacked(QString name, const QSize size, int bitDepth, Subsampling detectedSubsampling, int64_t fileSize)
{
  auto bitDepthList = getDetectionBitDepthList(bitDepth);

  for (auto subsampling : getDetectionSubsamplingList(detectedSubsampling, true))
  {
    // What packing formats are supported by this subsampling?
    auto packingTypes = getSupportedPackingFormats(subsampling);
    for (auto packing : packingTypes)
    {
      for (int bitDepth : bitDepthList)
      {
        auto endianessList = QStringList() << "le";
        if (bitDepth > 8)
          endianessList << "be";

        for (const QString &endianess : endianessList)
        {
          {
            QString formatName = getPackingFormatString(packing).toLower() + subsamplingToString(subsampling);
            if (bitDepth > 8)
              formatName += QString::number(bitDepth) + endianess;
            auto fmt = yuvPixelFormat(subsampling, bitDepth, packing, false, endianess=="be");
            if (name.contains(formatName) && checkFormat(fmt, size, fileSize))
              return fmt;
          }

          if (subsampling == detectedSubsampling && detectedSubsampling != Subsampling::UNKNOWN)
          {
            // Also try the string without the subsampling indicator (which we already detected)
            QString formatName = getPackingFormatString(packing).toLower();
            if (bitDepth > 8)
              formatName += QString::number(bitDepth) + endianess;
            auto fmt = yuvPixelFormat(subsampling, bitDepth, packing, false, endianess=="be");
            if (name.contains(formatName) && checkFormat(fmt, size, fileSize))
              return fmt;
          }
        }
      }
    }
  }
  return {};
}

yuvPixelFormat guessFormatFromSizeAndName(const QSize size, int bitDepth, bool packed, int64_t fileSize, const QFileInfo &fileInfo)
{
  // We are going to check two strings (one after the other) for indicators on the YUV format.
  // 1: The file name, 2: The folder name that the file is contained in.
  QStringList checkStrings;

  // The full name of the file
  auto fileName = fileInfo.baseName();
  if (fileName.isEmpty())
    return {};
  checkStrings.append(fileName);

  // The name of the folder that the file is in
  auto dirName = fileInfo.absoluteDir().dirName();
  checkStrings.append(dirName);

  if (fileInfo.suffix() == "nv21")
  {
    // This should be a 8 bit planar yuv 4:2:0 file with interleaved UV components and YVU order
    auto fmt = yuvPixelFormat(Subsampling::YUV_420, 8, PlaneOrder::YVU);
    fmt.uvInterleaved = true;
    auto bpf = fmt.bytesPerFrame(size);
    if (bpf != 0 && (fileSize % bpf) == 0)
    {
      // Bits per frame and file size match
      return fmt;
    }
  }

  for (const QString &name : checkStrings)
  {
    auto subsampling = findSubsamplingTypeIndicatorInName(name);
    
    // First, lets see if there is a YUV format defined as FFMpeg names them:
    // First the YUV order, then the subsampling, then a 'p' if the format is planar, then the number of bits (if > 8), finally 'le' or 'be' if bits is > 8.
    // E.g: yuv420p, yuv420p10le, yuv444p16be
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
    if (name.contains("ayuv64le"))
    {
      // Check if the format and the file size match
      auto fmt = yuvPixelFormat(Subsampling::YUV_444, 16, PackingOrder::AYUV, false, false);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }

    // OK that did not work so far. Try other things. Just check if the file name contains one of the subsampling strings.
    // Further parameters: YUV plane order, little endian. The first format to match the file size wins.
    auto bitDepths = bitDepthList;
    if (bitDepth != -1)
      // We already extracted a bit depth from the name. Only try that.
      bitDepths = QList<int>() << bitDepth;
    for (auto subsampling : subsamplingList)
    {
      if (name.contains(subsamplingToString(subsampling), Qt::CaseInsensitive))
      {
        // Try this subsampling with all bitDepths
        for (auto bd : bitDepths)
        {
          yuvPixelFormat fmt;
          if (packed)
            fmt = yuvPixelFormat(subsampling, bd, PackingOrder::YUV);
          else
            fmt = yuvPixelFormat(subsampling, bd, PlaneOrder::YUV);
          if (checkFormat(fmt, size, fileSize))
            return fmt;
        }
      }
    }
  }

  // Nothing using the name worked so far. Check some formats. The first one that matches the file size wins.
  const auto testSubsamplings = QList<Subsampling>() << Subsampling::YUV_420 << Subsampling::YUV_444 << Subsampling::YUV_422;

  QList<int> testBitDepths;
  if (bitDepth != -1)
    // We already extracted a bit depth from the name. Only try that.
    testBitDepths << bitDepth;
  else
    // We don't know the bit depth. Try different values.
    testBitDepths << 8 << 9 << 10 << 12 << 14 << 16;

  for (const auto &subsampling : testSubsamplings)
  {
    for (auto bd : testBitDepths)
    {
      auto fmt = yuvPixelFormat(subsampling, bd, PlaneOrder::YUV);
      if (checkFormat(fmt, size, fileSize))
        return fmt;
    }
  }

  // Guessing failed
  return {};
}

} // namespace YUV_Internals
