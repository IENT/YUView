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

#include <Testing.h>

#include <array>
#include <utility>

#include <video/yuv/PixelFormatYUV.h>

#include "PixelFormatYUVHelper.h"

namespace video::yuv::test
{

TEST(PixelFormatYUVTest, defaultConstructor_shouldSetDefaultFormat)
{
  const auto fmt = PixelFormatYUV();

  EXPECT_TRUE(fmt.isValid());
  EXPECT_EQ(fmt.getSubsampling(), Subsampling::YUV_420);
  EXPECT_EQ(fmt.getNrPlanes(), 3);
  EXPECT_EQ(fmt.getBitsPerSample(), 8);
  EXPECT_TRUE(fmt.getPlanarFormat().has_value());
  EXPECT_FALSE(fmt.hasAlpha());
  EXPECT_EQ(fmt.getEndianness(), Endianness::Little);
  EXPECT_EQ(fmt.getChromaOffset(), Offset({0, 1}));

  const auto fmtPlanar = fmt.getPlanarFormat();
  EXPECT_TRUE(fmtPlanar.has_value());
  EXPECT_EQ(fmtPlanar->subsampling, Subsampling::YUV_420);
  EXPECT_EQ(fmtPlanar->bitsPerSample, 8);
  EXPECT_EQ(fmtPlanar->planeOrder, PlaneOrder::YUV);
  EXPECT_EQ(fmtPlanar->endianness, Endianness::Little);
  EXPECT_EQ(fmtPlanar->chromaOffset, Offset({0, 1}));
  EXPECT_FALSE(fmtPlanar->uvPlanesInterleaved);
  EXPECT_FALSE(fmtPlanar->bytePacking);
}

TEST(PixelFormatYUVTest, getSubsamplingRatios_shouldReturnCorrectRatio)
{
  const auto testCases = std::array{
    std::pair{Subsampling::YUV_444, SubsamplingRatios{1, 1}},
    std::pair{Subsampling::YUV_422, SubsamplingRatios{2, 1}},
    std::pair{Subsampling::YUV_420, SubsamplingRatios{2, 2}},
    std::pair{Subsampling::YUV_440, SubsamplingRatios{1, 2}},
    std::pair{Subsampling::YUV_410, SubsamplingRatios{4, 4}},
    std::pair{Subsampling::YUV_411, SubsamplingRatios{4, 1}},
    std::pair{Subsampling::YUV_400, SubsamplingRatios{1, 1}},
    std::pair{Subsampling::UNKNOWN, SubsamplingRatios{1, 1}},
  };

  for (const auto &[subsampling, expectedRatios] : testCases)
  {
    const auto format = PixelFormatYUV(PlanarPixelFormat({subsampling}));
    const auto ratios = format.getSubsamplingRatios();

    EXPECT_EQ(ratios.horizontal, expectedRatios.horizontal)
      << "Horizontal ratio for planar format and subsampling "
      << SubsamplingMapper.getName(subsampling);
    EXPECT_EQ(ratios.vertical, expectedRatios.vertical)
      << "Vertical ratio for planar format and subsampling "
      << SubsamplingMapper.getName(subsampling);
  }

  for (const auto &[subsampling, expectedRatios] : testCases)
  {
    const auto format = PixelFormatYUV(PackedPixelFormat({subsampling}));
    const auto ratios = format.getSubsamplingRatios();

    EXPECT_EQ(ratios.horizontal, expectedRatios.horizontal)
      << "Horizontal ratio for packed format and subsampling "
      << SubsamplingMapper.getName(subsampling);
    EXPECT_EQ(ratios.vertical, expectedRatios.vertical)
      << "Vertical ratio for packed format and subsampling "
      << SubsamplingMapper.getName(subsampling);
  }

  for (const auto predefinedFormat : PredefinedPixelFormatMapper.getValues())
  {
    const auto format = PixelFormatYUV(predefinedFormat);
    const auto ratios = format.getSubsamplingRatios();

    EXPECT_EQ(ratios.horizontal, 2) << "Horizontal ratio for predefined format and subsampling "
                                    << PredefinedPixelFormatMapper.getName(predefinedFormat);
    EXPECT_EQ(ratios.vertical, 1) << "Vertical ratio for predefined format and subsampling "
                                  << PredefinedPixelFormatMapper.getName(predefinedFormat);
  }
}

TEST(PixelFormatYUVTest, testFormatFromToString)
{
  for (const auto fmt : getAllPixelFormats())
  {
    const auto name = fmt.getName();
    EXPECT_TRUE(fmt.isValid()) << "Format " << name << " is invalid.";
    EXPECT_FALSE(name.empty()) << "Format " << name << " getName is empty.";

    const auto fmtNew = PixelFormatYUV(name);
    EXPECT_EQ(fmt, fmtNew) << "New format " << fmtNew.getName() << " unequal to initial format "
                           << name;

    EXPECT_EQ(fmt.getSubsampling(), fmtNew.getSubsampling())
      << "Format " << name << " subsampling missmatch";
    EXPECT_EQ(fmt.getBitsPerSample(), fmtNew.getBitsPerSample())
      << "Format " << name << " bits per sample missmatch";
    EXPECT_EQ(fmt.getChromaOffset(), fmtNew.getChromaOffset())
      << "Format " << name << " chroma offset missmatch";

    if (const auto planarFormat = fmt.getPlanarFormat())
    {
      const auto planarFormatNew = fmtNew.getPlanarFormat();
      EXPECT_TRUE(planarFormatNew.has_value())
        << "Format " << name << " new format has no planar format";
      EXPECT_EQ(*planarFormat, *planarFormatNew) << "Format " << name << " planar format missmatch";
    }
    else if (const auto packedFormat = fmt.getPackedFormat())
    {
      const auto packedFormatNew = fmtNew.getPackedFormat();
      EXPECT_TRUE(packedFormatNew.has_value())
        << "Format " << name << " new format has no packed format";
      EXPECT_EQ(*packedFormat, *packedFormatNew) << "Format " << name << " packed format missmatch";
    }
    else if (const auto predefinedFormat = fmt.getPredefinedFormat())
    {
      const auto predefinedFormatNew = fmtNew.getPredefinedFormat();
      EXPECT_TRUE(predefinedFormatNew.has_value())
        << "Format " << name << " new format has no predefined format";
      EXPECT_EQ(*predefinedFormat, *predefinedFormatNew)
        << "Format " << name << " predefined format missmatch";
    }
    else
    {
      FAIL() << "Format " << name << " has no planar, packed or predefined format";
    }

    if (fmt.getBitsPerSample())
    {
      EXPECT_EQ(fmt.getEndianness(), fmtNew.getEndianness())
        << "Format " << name << " endianness wrong";
    }
  }
}

TEST(PixelFormatYUVTest, testPYUVFormat)
{
  // PYUV is only defined for 4:2:0 with 10, 12 or 14 bits, litte endian, PlaneOrder YUV
  for (const auto subsampling : SubsamplingMapper.getValues())
  {
    for (const auto bitsPerSample : BitDepthList)
    {
      const auto endianList = (bitsPerSample > 8)
                                ? std::vector<Endianness>({Endianness::Little, Endianness::Big})
                                : std::vector<Endianness>({Endianness::Little});

      for (const auto planeOrder : PlaneOrderMapper.getValues())
        for (const auto endianness : endianList)
          for (const auto uvPlanesInterleaved : {true, false})
          {
            const auto format =
              PixelFormatYUV(PlanarPixelFormat({.subsampling         = subsampling,
                                                .bitsPerSample       = bitsPerSample,
                                                .planeOrder          = planeOrder,
                                                .endianness          = endianness,
                                                .uvPlanesInterleaved = uvPlanesInterleaved,
                                                .bytePacking         = true}));

            const auto shouldBeValid =
              (subsampling == Subsampling::YUV_420 &&
               (bitsPerSample == 10 || bitsPerSample == 12 || bitsPerSample == 14) &&
               planeOrder == PlaneOrder::YUV && endianness == Endianness::Little &&
               !uvPlanesInterleaved);

            EXPECT_EQ(format.isValid(), shouldBeValid);
            if (format.isValid())
            {
              for (const auto frameSizeLuma :
                   {Size({1920, 1080}), Size({1280, 720}), Size({12, 10}), Size({10, 10})})
              {
                const auto nrSamplesChroma = frameSizeLuma.width / 2 * frameSizeLuma.height / 2;
                const auto frameSizeValid =
                  (((bitsPerSample == 10 || bitsPerSample == 14) && (nrSamplesChroma % 4 == 0)) ||
                   (bitsPerSample == 12 && (nrSamplesChroma % 2 == 0)));

                const auto nrBytesPerFrame = format.bytesPerFrame(frameSizeLuma);
                if (frameSizeValid)
                  EXPECT_GT(nrBytesPerFrame, 0)
                    << "Format " << format.getName() << " returned 0 bytes per frame for size "
                    << frameSizeLuma.width << "x" << frameSizeLuma.height;
                else
                  EXPECT_EQ(nrBytesPerFrame, 0)
                    << "Format " << format.getName()
                    << " returned non-zero bytes per frame for invalid size " << frameSizeLuma.width
                    << "x" << frameSizeLuma.height;
              }
            }
          }
    }
  }
}

TEST(PixelFormatYUVTest, testBytesPerFrame_validPixelFormat_shouldReturnCorrectNumberOfBytes)
{
  const Size size{16, 16};

  auto getBytes = [&size](const Subsampling &subsampling,
                          const int          bitsPerSample,
                          const PlaneOrder   planeOrder = PlaneOrder::YUV)
  {
    return PixelFormatYUV(PlanarPixelFormat({.subsampling   = subsampling,
                                             .bitsPerSample = bitsPerSample,
                                             .planeOrder    = planeOrder}))
      .bytesPerFrame(size);
  };

  EXPECT_EQ(getBytes(Subsampling::YUV_444, 8), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 9), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 10), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 12), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 14), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 16), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 8, PlaneOrder::YUVA), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 9, PlaneOrder::YUVA), 2048);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 10, PlaneOrder::YUVA), 2048);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 12, PlaneOrder::YUVA), 2048);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 14, PlaneOrder::YUVA), 2048);
  EXPECT_EQ(getBytes(Subsampling::YUV_444, 16, PlaneOrder::YUVA), 2048);

  EXPECT_EQ(getBytes(Subsampling::YUV_422, 8), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 9), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 10), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 12), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 14), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 16), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 8, PlaneOrder::YUVA), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 9, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 10, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 12, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 14, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_422, 16, PlaneOrder::YUVA), 1536);

  EXPECT_EQ(getBytes(Subsampling::YUV_420, 8), 384);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 9), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 10), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 12), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 14), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 16), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 8, PlaneOrder::YUVA), 640);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 9, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 10, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 12, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 14, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_420, 16, PlaneOrder::YUVA), 1280);

  EXPECT_EQ(getBytes(Subsampling::YUV_440, 8), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 9), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 10), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 12), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 14), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 16), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 8, PlaneOrder::YUVA), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 9, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 10, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 12, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 14, PlaneOrder::YUVA), 1536);
  EXPECT_EQ(getBytes(Subsampling::YUV_440, 16, PlaneOrder::YUVA), 1536);

  EXPECT_EQ(getBytes(Subsampling::YUV_410, 8), 288);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 9), 576);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 10), 576);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 12), 576);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 14), 576);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 16), 576);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 8, PlaneOrder::YUVA), 544);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 9, PlaneOrder::YUVA), 1088);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 10, PlaneOrder::YUVA), 1088);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 12, PlaneOrder::YUVA), 1088);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 14, PlaneOrder::YUVA), 1088);
  EXPECT_EQ(getBytes(Subsampling::YUV_410, 16, PlaneOrder::YUVA), 1088);

  EXPECT_EQ(getBytes(Subsampling::YUV_411, 8), 384);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 9), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 10), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 12), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 14), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 16), 768);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 8, PlaneOrder::YUVA), 640);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 9, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 10, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 12, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 14, PlaneOrder::YUVA), 1280);
  EXPECT_EQ(getBytes(Subsampling::YUV_411, 16, PlaneOrder::YUVA), 1280);

  EXPECT_EQ(getBytes(Subsampling::YUV_400, 8), 256);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 9), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 10), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 12), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 14), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 16), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 8, PlaneOrder::YUVA), 512);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 9, PlaneOrder::YUVA), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 10, PlaneOrder::YUVA), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 12, PlaneOrder::YUVA), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 14, PlaneOrder::YUVA), 1024);
  EXPECT_EQ(getBytes(Subsampling::YUV_400, 16, PlaneOrder::YUVA), 1024);

  auto getBytesBytePacking =
    [](const Subsampling &subsampling, const int bitsPerSample, const Size &frameSize)
  {
    return PixelFormatYUV(
             PlanarPixelFormat(
               {.subsampling = subsampling, .bitsPerSample = bitsPerSample, .bytePacking = true}))
      .bytesPerFrame(frameSize);
  };

  EXPECT_EQ(getBytesBytePacking(Subsampling::YUV_420, 10, size), 480);
  EXPECT_EQ(getBytesBytePacking(Subsampling::YUV_420, 12, size), 576);
  EXPECT_EQ(getBytesBytePacking(Subsampling::YUV_420, 14, size), 672);
  for (const auto bitDepth : {10, 12, 14})
    for (int width = 16; width < 32; width++)
      for (int height = 16; height < 32; height++)
      {
        // The condition is, that the number of samples (wxh) per plane should be a multiple
        // of the 4/8 for YUV 4:2:0.
        const auto nrSamples              = width * height;
        const auto isSizeOkForSubsampling = (width % 2 == 0 && height % 2 == 0);
        const auto doesSizeWorkForPYUV =
          ((bitDepth == 10 || bitDepth == 14) && nrSamples % 16 == 0) ||
          (bitDepth == 12 && nrSamples % 8 == 0);
        if (isSizeOkForSubsampling && doesSizeWorkForPYUV)
          EXPECT_NE(getBytesBytePacking(Subsampling::YUV_420, bitDepth, Size{width, height}), 0)
            << "Should be valid for width: " << width << " height: " << height
            << " bitDepth: " << bitDepth;
        else
          EXPECT_EQ(getBytesBytePacking(Subsampling::YUV_420, bitDepth, Size{width, height}), 0)
            << "Should be invalid for width: " << width << " height: " << height
            << " bitDepth: " << bitDepth;
      }

  auto getBytesPacked = [&size](const Subsampling &subsampling,
                                const int          bitsPerSample,
                                const PackingOrder packingOrder)
  {
    return PixelFormatYUV(PackedPixelFormat({.subsampling   = subsampling,
                                             .bitsPerSample = bitsPerSample,
                                             .packingOrder  = packingOrder}))
      .bytesPerFrame(size);
  };

  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 8, PackingOrder::YUV), 768);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 9, PackingOrder::YUV), 1536);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 10, PackingOrder::YUV), 1536);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 12, PackingOrder::YUV), 1536);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 14, PackingOrder::YUV), 1536);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 16, PackingOrder::YUV), 1536);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 8, PackingOrder::YUVA), 1024);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 9, PackingOrder::YUVA), 2048);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 10, PackingOrder::YUVA), 2048);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 12, PackingOrder::YUVA), 2048);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 14, PackingOrder::YUVA), 2048);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_444, 16, PackingOrder::YUVA), 2048);

  EXPECT_EQ(getBytesPacked(Subsampling::YUV_422, 8, PackingOrder::UYVY), 512);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_422, 9, PackingOrder::UYVY), 1024);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_422, 10, PackingOrder::UYVY), 1024);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_422, 12, PackingOrder::UYVY), 1024);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_422, 14, PackingOrder::UYVY), 1024);
  EXPECT_EQ(getBytesPacked(Subsampling::YUV_422, 16, PackingOrder::UYVY), 1024);

  EXPECT_EQ(PixelFormatYUV(PredefinedPixelFormat::V210).bytesPerFrame(Size{48, 2}), 256);
  EXPECT_EQ(PixelFormatYUV(PredefinedPixelFormat::V210).bytesPerFrame(Size{49, 2}), 512);
  EXPECT_EQ(PixelFormatYUV(PredefinedPixelFormat::V210).bytesPerFrame(Size{1, 1}), 0);

  EXPECT_EQ(PixelFormatYUV(PlanarPixelFormat{}).bytesPerFrame(Size{}), 0);
  EXPECT_EQ(PixelFormatYUV(PlanarPixelFormat{}).bytesPerFrame(Size{Size::MAX_DIMENSION, 1}), 0);
  EXPECT_EQ(
    PixelFormatYUV(PackedPixelFormat({.subsampling = Subsampling::YUV_420})).bytesPerFrame(size),
    0);
  EXPECT_EQ(
    PixelFormatYUV(PlanarPixelFormat({.subsampling = Subsampling::UNKNOWN})).bytesPerFrame(size),
    0);
}

TEST(PixelFormatYUVTest, testBytesPerFrame_invalidPixelFormat_shouldReturnZero)
{
  const Size size{16, 16};

  const auto unknownSubsampling =
    PixelFormatYUV(PlanarPixelFormat({.subsampling = Subsampling::UNKNOWN}));
  EXPECT_FALSE(unknownSubsampling.isValid());
  EXPECT_EQ(unknownSubsampling.bytesPerFrame(size), 0);

  const auto bytePackedUnknownSubsampling = PixelFormatYUV(PlanarPixelFormat(
    {.subsampling = Subsampling::UNKNOWN, .bitsPerSample = 10, .bytePacking = true}));
  EXPECT_FALSE(bytePackedUnknownSubsampling.isValid());
  EXPECT_EQ(bytePackedUnknownSubsampling.bytesPerFrame(size), 0);

  const auto bytePacked444 = PixelFormatYUV(PlanarPixelFormat(
    {.subsampling = Subsampling::YUV_444, .bitsPerSample = 10, .bytePacking = true}));
  EXPECT_FALSE(bytePacked444.isValid());
  EXPECT_EQ(bytePacked444.bytesPerFrame(size), 0);

  const auto bytePackedUnsupportedBitDepth = PixelFormatYUV(PlanarPixelFormat(
    {.subsampling = Subsampling::YUV_420, .bitsPerSample = 8, .bytePacking = true}));
  EXPECT_FALSE(bytePackedUnsupportedBitDepth.isValid());
  EXPECT_EQ(bytePackedUnsupportedBitDepth.bytesPerFrame(size), 0);

  const auto bytePackedWrongPlaneOrder =
    PixelFormatYUV(PlanarPixelFormat({.subsampling   = Subsampling::YUV_420,
                                      .bitsPerSample = 10,
                                      .planeOrder    = PlaneOrder::YVU,
                                      .bytePacking   = true}));
  EXPECT_FALSE(bytePackedWrongPlaneOrder.isValid());
  EXPECT_EQ(bytePackedWrongPlaneOrder.bytesPerFrame(size), 0);

  const auto bytePackedBigEndian =
    PixelFormatYUV(PlanarPixelFormat({.subsampling   = Subsampling::YUV_420,
                                      .bitsPerSample = 10,
                                      .endianness    = Endianness::Big,
                                      .bytePacking   = true}));
  EXPECT_FALSE(bytePackedBigEndian.isValid());
  EXPECT_EQ(bytePackedBigEndian.bytesPerFrame(size), 0);

  const auto bytePackedInterleaved =
    PixelFormatYUV(PlanarPixelFormat({.subsampling         = Subsampling::YUV_420,
                                      .bitsPerSample       = 10,
                                      .uvPlanesInterleaved = true,
                                      .bytePacking         = true}));
  EXPECT_FALSE(bytePackedInterleaved.isValid());
  EXPECT_EQ(bytePackedInterleaved.bytesPerFrame(size), 0);

  const auto bytePackedInvalidWidth = PixelFormatYUV(PlanarPixelFormat(
    {.subsampling = Subsampling::YUV_420, .bitsPerSample = 10, .bytePacking = true}));
  EXPECT_TRUE(bytePackedInvalidWidth.isValid());
  EXPECT_EQ(bytePackedInvalidWidth.bytesPerFrame(Size{15, 16}), 0);

  const auto bytePackedInvalidHeight = PixelFormatYUV(PlanarPixelFormat(
    {.subsampling = Subsampling::YUV_420, .bitsPerSample = 10, .bytePacking = true}));
  EXPECT_TRUE(bytePackedInvalidHeight.isValid());
  EXPECT_EQ(bytePackedInvalidHeight.bytesPerFrame(Size{16, 15}), 0);

  const auto packedByteFormat =
    PixelFormatYUV(PackedPixelFormat({.subsampling = Subsampling::YUV_420, .bytePacking = true}));
  EXPECT_FALSE(packedByteFormat.isValid());
  EXPECT_EQ(packedByteFormat.bytesPerFrame(size), 0);
}

} // namespace video::yuv::test
