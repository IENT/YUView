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

#include <common/Testing.h>

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

} // namespace video::yuv::test
