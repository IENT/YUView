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

#include <decoder/decoderLibde265.h>
#include <filesource/FileSourceAnnexBFile.h>

#include <array>
#include <cstdint>
#include <filesystem>

namespace
{

using decoder::DecoderState;

std::uint32_t calculateCRC32(const QByteArray &data)
{
  std::uint32_t crc = 0xFFFFFFFF;
  for (const auto byte : data)
  {
    crc ^= static_cast<std::uint8_t>(byte);
    for (int bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
  }
  return crc ^ 0xFFFFFFFF;
}

constexpr auto BITSTREAM_SINTEL =
  "Sintel_2k_100frames_.C._Copyright_Blender_Foundation_www.sintel.org.hevc";

TEST(decoderLibde265Test, openDecoder_shouldOpenAndHaveNoError)
{
  decoder::decoderLibde265 decoder(0);

  EXPECT_FALSE(decoder.errorInDecoder()) << decoder.decoderErrorString().toStdString();
  EXPECT_EQ(decoder.state(), decoder::DecoderState::NeedsMoreData);
}

TEST(decoderLibde265Test, decodeFrames_shouldMatchChecksums)
{
  decoder::decoderLibde265 decoder(0);
  ASSERT_FALSE(decoder.errorInDecoder()) << decoder.decoderErrorString().toStdString();

  FileSourceAnnexBFile bitstream{std::filesystem::path(BITSTREAM_SINTEL)};
  ASSERT_TRUE(bitstream.isOk()) << "Could not open bitstream: " << BITSTREAM_SINTEL;

  std::vector<std::uint32_t> frameChecksums;
  bool                       repushData = false;
  while (!decoder.errorInDecoder() && decoder.state() != DecoderState::EndOfBitstream)
  {
    while (decoder.state() == DecoderState::NeedsMoreData)
    {
      QByteArray data;
      const auto isEnd = bitstream.atEnd();
      if (!isEnd)
        data = bitstream.getNextNALUnit(repushData);
      repushData = !decoder.pushData(data);
    }

    if (decoder.state() == DecoderState::RetrieveFrames)
    {
      if (decoder.decodeNextFrame())
      {
        const auto frameData = decoder.getRawFrameData();
        ASSERT_FALSE(frameData.isEmpty()) << "Decoder returned an empty frame.";

        frameChecksums.push_back(calculateCRC32(frameData));
      }
    }
  }

  ASSERT_FALSE(decoder.errorInDecoder()) << decoder.decoderErrorString().toStdString();
  ASSERT_THAT(frameChecksums,
              testing::ElementsAre(292326432,
                                   4025293154,
                                   427591811,
                                   4271976228,
                                   632364717,
                                   2094939897,
                                   280963286,
                                   1911120849,
                                   2587303005,
                                   3975381071,
                                   990774166,
                                   3341483494,
                                   1528911705,
                                   4216673964,
                                   2946110487,
                                   4094781602,
                                   864670306,
                                   2116167840,
                                   1030216726,
                                   3285277609,
                                   656160880,
                                   3500596204,
                                   2279528383,
                                   3762625655,
                                   2685807507,
                                   2246337885,
                                   632986981,
                                   245002303,
                                   3002334368,
                                   3128761749,
                                   153534558,
                                   3385899073,
                                   2551351627,
                                   4190464354,
                                   643091422,
                                   1234686632,
                                   4057798401,
                                   3045359204,
                                   93420102,
                                   1908374358,
                                   704023994,
                                   1174731133,
                                   2199145852,
                                   666799623,
                                   3200084366,
                                   483117394,
                                   1863469769,
                                   1632228904,
                                   2702005201,
                                   3932453590,
                                   1779375177,
                                   2568177825,
                                   3109556174,
                                   3559001989,
                                   1202919005,
                                   965485165,
                                   4243719743,
                                   2458715622,
                                   911571610,
                                   1536081726,
                                   2126253216,
                                   1692254787,
                                   45519563,
                                   1109907692,
                                   1822995327,
                                   3541701121,
                                   4177339672,
                                   1546987825,
                                   1175966364,
                                   2903883208,
                                   1292557244,
                                   2589304192,
                                   153385422,
                                   499845826,
                                   2827474776,
                                   2802937722,
                                   3893712673,
                                   1652943715,
                                   1795444089,
                                   2439216501,
                                   3225494097,
                                   250419742,
                                   3729306323,
                                   2581262177,
                                   1304206862,
                                   3947820291,
                                   2601290322,
                                   2029048872,
                                   2796619675,
                                   4069127913,
                                   1821967019,
                                   840603426,
                                   2032933007,
                                   1326935440,
                                   1286746927,
                                   1965281736,
                                   1737414699,
                                   864199549,
                                   620930984,
                                   3269880786));
}

} // namespace
