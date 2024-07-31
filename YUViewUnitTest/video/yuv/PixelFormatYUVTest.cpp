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

#include <video/yuv/PixelFormatYUV.h>

namespace video::yuv::test
{

namespace
{

std::vector<PixelFormatYUV> getAllFormats()
{
  std::vector<PixelFormatYUV> allFormats;

  for (const auto subsampling : SubsamplingMapper.getValues())
  {
    for (const auto bitsPerSample : BitDepthList)
    {
      const auto endianList =
          (bitsPerSample > 8) ? std::vector<bool>({false, true}) : std::vector<bool>({false});

      // Planar
      for (const auto planeOrder : PlaneOrderMapper.getValues())
        for (const auto bigEndian : endianList)
          allFormats.push_back(PixelFormatYUV(subsampling, bitsPerSample, planeOrder, bigEndian));

      // Packet
      for (const auto packingOrder : getSupportedPackingFormats(subsampling))
        for (const auto bytePacking : {false, true})
          for (const auto bigEndian : endianList)
            allFormats.push_back(
                PixelFormatYUV(subsampling, bitsPerSample, packingOrder, bytePacking, bigEndian));
    }
  }

  for (auto predefinedFormat : PredefinedPixelFormatMapper.getValues())
    allFormats.push_back(PixelFormatYUV(predefinedFormat));

  return allFormats;
}

} // namespace

TEST(PixelFormatYUVTest, testFormatFromToString)
{
  for (const auto fmt : getAllFormats())
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
    EXPECT_EQ(fmt.isPlanar(), fmtNew.isPlanar()) << "Format " << name << " planar missmatch";
    EXPECT_EQ(fmt.getChromaOffset(), fmtNew.getChromaOffset())
        << "Format " << name << " chroma offset missmatch";
    EXPECT_EQ(fmt.getPlaneOrder(), fmtNew.getPlaneOrder())
        << "Format " << name << " plane order missmatch";
    EXPECT_EQ(fmt.isUVInterleaved(), fmtNew.isUVInterleaved())
        << "Format " << name << " uv inteleaved missmatch";
    EXPECT_EQ(fmt.getPackingOrder(), fmtNew.getPackingOrder())
        << "Format " << name << " packing order missmatch";
    EXPECT_EQ(fmt.isBytePacking(), fmtNew.isBytePacking())
        << "Format " << name << " byte packing missmatch";

    if (fmt.getBitsPerSample())
    {
      EXPECT_EQ(fmt.isBigEndian(), fmtNew.isBigEndian()) << "Format " << name << " endianess wrong";
    }
  }
}

} // namespace video::yuv::test
