/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "color_config.h"

namespace parser::av1
{

namespace
{

using namespace parser::av1;

auto ColorPrimariesCodingMap = std::map<unsigned, ColorPrimaries>({{1, ColorPrimaries::CP_BT_709},
                                                                   {2, ColorPrimaries::CP_UNSPECIFIED},
                                                                   {4, ColorPrimaries::CP_BT_470_M},
                                                                   {5, ColorPrimaries::CP_BT_470_B_G},
                                                                   {6, ColorPrimaries::CP_BT_601},
                                                                   {7, ColorPrimaries::CP_SMPTE_240},
                                                                   {8, ColorPrimaries::CP_GENERIC_FILM},
                                                                   {9, ColorPrimaries::CP_BT_2020},
                                                                   {10, ColorPrimaries::CP_XYZ},
                                                                   {11, ColorPrimaries::CP_SMPTE_431},
                                                                   {12, ColorPrimaries::CP_SMPTE_432},
                                                                   {22, ColorPrimaries::CP_EBU_3213}});

auto ColorPrimariesNames =
    std::map<ColorPrimaries, std::string>({{ColorPrimaries::CP_BT_709, "CP_BT_709"},
                                           {ColorPrimaries::CP_UNSPECIFIED, "CP_UNSPECIFIED"},
                                           {ColorPrimaries::CP_BT_470_M, "CP_BT_470_M"},
                                           {ColorPrimaries::CP_BT_470_B_G, "CP_BT_470_B_G"},
                                           {ColorPrimaries::CP_BT_601, "CP_BT_601"},
                                           {ColorPrimaries::CP_SMPTE_240, "CP_SMPTE_240"},
                                           {ColorPrimaries::CP_GENERIC_FILM, "CP_GENERIC_FILM"},
                                           {ColorPrimaries::CP_BT_2020, "CP_BT_2020"},
                                           {ColorPrimaries::CP_XYZ, "CP_XYZ"},
                                           {ColorPrimaries::CP_SMPTE_431, "CP_SMPTE_431"},
                                           {ColorPrimaries::CP_SMPTE_432, "CP_SMPTE_432"},
                                           {ColorPrimaries::CP_EBU_3213, "CP_EBU_3213"}});

auto TransferCharacteristicsCodingMap =
    std::map<unsigned, TransferCharacteristics>({{0, TransferCharacteristics::TC_RESERVED},
                                                 {1, TransferCharacteristics::TC_BT_709},
                                                 {2, TransferCharacteristics::TC_UNSPECIFIED},
                                                 {4, TransferCharacteristics::TC_BT_470_M},
                                                 {5, TransferCharacteristics::TC_BT_470_B_G},
                                                 {6, TransferCharacteristics::TC_BT_601},
                                                 {7, TransferCharacteristics::TC_SMPTE_240},
                                                 {8, TransferCharacteristics::TC_LINEAR},
                                                 {9, TransferCharacteristics::TC_LOG_100},
                                                 {10, TransferCharacteristics::TC_LOG_100_SQRT10},
                                                 {11, TransferCharacteristics::TC_IEC_61966},
                                                 {12, TransferCharacteristics::TC_BT_1361},
                                                 {13, TransferCharacteristics::TC_SRGB},
                                                 {14, TransferCharacteristics::TC_BT_2020_10_BIT},
                                                 {15, TransferCharacteristics::TC_BT_2020_12_BIT},
                                                 {16, TransferCharacteristics::TC_SMPTE_2084},
                                                 {17, TransferCharacteristics::TC_SMPTE_428},
                                                 {18, TransferCharacteristics::TC_HLG}});

auto TransferCharacteristicsNames = std::map<TransferCharacteristics, std::string>(
    {{TransferCharacteristics::TC_RESERVED, "TC_RESERVED"},
     {TransferCharacteristics::TC_BT_709, "TC_BT_709"},
     {TransferCharacteristics::TC_UNSPECIFIED, "TC_UNSPECIFIED"},
     {TransferCharacteristics::TC_BT_470_M, "TC_BT_470_M"},
     {TransferCharacteristics::TC_BT_470_B_G, "TC_BT_470_B_G"},
     {TransferCharacteristics::TC_BT_601, "TC_BT_601"},
     {TransferCharacteristics::TC_SMPTE_240, "TC_SMPTE_240"},
     {TransferCharacteristics::TC_LINEAR, "TC_LINEAR"},
     {TransferCharacteristics::TC_LOG_100, "TC_LOG_100"},
     {TransferCharacteristics::TC_LOG_100_SQRT10, "TC_LOG_100_SQRT10"},
     {TransferCharacteristics::TC_IEC_61966, "TC_IEC_61966"},
     {TransferCharacteristics::TC_BT_1361, "TC_BT_1361"},
     {TransferCharacteristics::TC_SRGB, "TC_SRGB"},
     {TransferCharacteristics::TC_BT_2020_10_BIT, "TC_BT_2020_10_BIT"},
     {TransferCharacteristics::TC_BT_2020_12_BIT, "TC_BT_2020_12_BIT"},
     {TransferCharacteristics::TC_SMPTE_2084, "TC_SMPTE_2084"},
     {TransferCharacteristics::TC_SMPTE_428, "TC_SMPTE_428"},
     {TransferCharacteristics::TC_HLG, "TC_HLG"}});

auto MatrixCoefficientsCodingMap =
    std::map<unsigned, MatrixCoefficients>({{0, MatrixCoefficients::MC_IDENTITY},
                                            {1, MatrixCoefficients::MC_BT_709},
                                            {2, MatrixCoefficients::MC_UNSPECIFIED},
                                            {3, MatrixCoefficients::MC_RESERVED},
                                            {4, MatrixCoefficients::MC_FCC},
                                            {5, MatrixCoefficients::MC_BT_470_B_G},
                                            {6, MatrixCoefficients::MC_BT_601},
                                            {7, MatrixCoefficients::MC_SMPTE_240},
                                            {8, MatrixCoefficients::MC_SMPTE_YCGCO},
                                            {9, MatrixCoefficients::MC_BT_2020_NCL},
                                            {10, MatrixCoefficients::MC_BT_2020_CL},
                                            {11, MatrixCoefficients::MC_SMPTE_2085},
                                            {12, MatrixCoefficients::MC_CHROMAT_NCL},
                                            {13, MatrixCoefficients::MC_CHROMAT_CL},
                                            {14, MatrixCoefficients::MC_ICTCP}});

auto MatrixCoefficientsNames = std::map<MatrixCoefficients, std::string>(
    {{MatrixCoefficients::MC_IDENTITY, "MC_IDENTITY"},
     {MatrixCoefficients::MC_BT_709, "MC_BT_709"},
     {MatrixCoefficients::MC_UNSPECIFIED, "MC_UNSPECIFIED"},
     {MatrixCoefficients::MC_RESERVED, "MC_RESERVED"},
     {MatrixCoefficients::MC_FCC, "MC_FCC"},
     {MatrixCoefficients::MC_BT_470_B_G, "MC_BT_470_B_G"},
     {MatrixCoefficients::MC_BT_601, "MC_BT_601"},
     {MatrixCoefficients::MC_SMPTE_240, "MC_SMPTE_240"},
     {MatrixCoefficients::MC_SMPTE_YCGCO, "MC_SMPTE_YCGCO"},
     {MatrixCoefficients::MC_BT_2020_NCL, "MC_BT_2020_NCL"},
     {MatrixCoefficients::MC_BT_2020_CL, "MC_BT_2020_CL"},
     {MatrixCoefficients::MC_SMPTE_2085, "MC_SMPTE_2085"},
     {MatrixCoefficients::MC_CHROMAT_NCL, "MC_CHROMAT_NCL"},
     {MatrixCoefficients::MC_CHROMAT_CL, "MC_CHROMAT_CL"},
     {MatrixCoefficients::MC_ICTCP, "MC_ICTCP"}});

auto getColorPrimariesMeaningMap()
{
  reader::MeaningMap meaning;
  for (const auto &entry : ColorPrimariesCodingMap)
    meaning[entry.first] = ColorPrimariesNames[entry.second];
  return meaning;
}

auto getTransferCharacteristicsMeaningMap()
{
  reader::MeaningMap meaning;
  for (const auto &entry : TransferCharacteristicsCodingMap)
    meaning[entry.first] = TransferCharacteristicsNames[entry.second];
  return meaning;
}

auto getMatrixCoefficientsMeaningMap()
{
  reader::MeaningMap meaning;
  for (const auto &entry : MatrixCoefficientsCodingMap)
    meaning[entry.first] = MatrixCoefficientsNames[entry.second];
  return meaning;
}

} // namespace

using namespace reader;

unsigned to_int(ColorPrimaries colorPrimaries)
{
  for (const auto &entry : ColorPrimariesCodingMap)
    if (entry.second == colorPrimaries)
      return entry.first;
  return {};
}

unsigned to_int(TransferCharacteristics transferCharacteristics)
{
  for (const auto &entry : TransferCharacteristicsCodingMap)
    if (entry.second == transferCharacteristics)
      return entry.first;
  return {};
}

unsigned to_int(MatrixCoefficients matrixCoefficients)
{
  for (const auto &entry : MatrixCoefficientsCodingMap)
    if (entry.second == matrixCoefficients)
      return entry.first;
  return {};
}

unsigned to_int(ChromaSamplePosition chromaSamplePosition)
{
  if (chromaSamplePosition == ChromaSamplePosition::CSP_UNKNOWN)
    return 0;
  if (chromaSamplePosition == ChromaSamplePosition::CSP_VERTICAL)
    return 0;
  if (chromaSamplePosition == ChromaSamplePosition::CSP_COLOCATED)
    return 0;
  if (chromaSamplePosition == ChromaSamplePosition::CSP_RESERVED)
    return 0;
  return {};
}

void color_config::parse(SubByteReaderLogging &reader, int seq_profile)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "color_config()");

  this->high_bitdepth = reader.readFlag("high_bitdepth");
  if (seq_profile == 2 && this->high_bitdepth)
  {
    this->twelve_bit = reader.readFlag("twelve_bit");
    this->BitDepth   = twelve_bit ? 12 : 10;
  }
  else if (seq_profile <= 2)
  {
    this->BitDepth = this->high_bitdepth ? 10 : 8;
  }
  reader.logCalculatedValue("BitDepth", int64_t(this->BitDepth));

  if (seq_profile == 1)
    this->mono_chrome = false;
  else
    this->mono_chrome = reader.readFlag("mono_chrome");

  this->NumPlanes = this->mono_chrome ? 1 : 3;
  reader.logCalculatedValue("NumPlanes", int64_t(this->NumPlanes));

  this->color_description_present_flag = reader.readFlag("color_description_present_flag");
  if (this->color_description_present_flag)
  {
    auto color_primaries_index = reader.readBits(
        "color_primaries",
        8,
        Options().withMeaningMap(getColorPrimariesMeaningMap()).withMeaning("Unused"));
    if (ColorPrimariesCodingMap.count(color_primaries_index) != 0)
      this->color_primaries = ColorPrimariesCodingMap[color_primaries_index];

    auto transfer_characteristics_index = reader.readBits(
        "transfer_characteristics",
        8,
        Options().withMeaningMap(getTransferCharacteristicsMeaningMap()).withMeaning("Reserved"));
    if (TransferCharacteristicsCodingMap.count(transfer_characteristics_index) > 0)
      this->transfer_characteristics =
          TransferCharacteristicsCodingMap[transfer_characteristics_index];

    auto matrix_coefficients_index = reader.readBits(
        "matrix_coefficients",
        8,
        Options().withMeaningMap(getMatrixCoefficientsMeaningMap()).withMeaning("Reserved"));
    if (MatrixCoefficientsCodingMap.count(matrix_coefficients_index) > 0)
      this->matrix_coefficients = MatrixCoefficientsCodingMap[matrix_coefficients_index];
  }

  if (this->mono_chrome)
  {
    this->color_range            = reader.readFlag("color_range");
    this->subsampling_x          = true;
    this->subsampling_y          = true;
    this->chroma_sample_position = ChromaSamplePosition::CSP_UNKNOWN;
    this->separate_uv_delta_q    = false;
  }
  else if (this->color_primaries == ColorPrimaries::CP_BT_709 &&
           this->transfer_characteristics == TransferCharacteristics::TC_SRGB &&
           this->matrix_coefficients == MatrixCoefficients::MC_IDENTITY)
  {
    this->color_range   = 1;
    this->subsampling_x = false;
    this->subsampling_y = false;
  }
  else
  {
    this->color_range = reader.readFlag("color_range");
    if (seq_profile == 0)
    {
      this->subsampling_x = true;
      this->subsampling_y = true;
    }
    else if (seq_profile == 1)
    {
      this->subsampling_x = false;
      this->subsampling_y = false;
    }
    else
    {
      if (this->BitDepth == 12)
      {
        this->subsampling_x = reader.readFlag("subsampling_x");
        if (this->subsampling_x)
          this->subsampling_y = reader.readFlag("subsampling_y");
        else
          this->subsampling_y = false;
      }
      else
      {
        this->subsampling_x = true;
        this->subsampling_y = false;
      }
    }
    if (this->subsampling_x && this->subsampling_y)
    {
      auto chroma_sample_position_meaning = std::vector<std::string>(
          {"Unknown (in this case the source video transfer function must be "
           "signaled outside the AV1 bitstream)",
           "Horizontally co-located with (0, 0) luma sample, vertical position in "
           "the middle between two luma samples",
           "co-located with (0, 0) luma sample",
           "Reserved"});
      auto chromaPositionIndex = reader.readBits(
          "chroma_sample_position", 2, Options().withMeaningVector(chroma_sample_position_meaning));

      auto chromaSamplePositionVec =
          std::vector<ChromaSamplePosition>({ChromaSamplePosition::CSP_UNKNOWN,
                                             ChromaSamplePosition::CSP_VERTICAL,
                                             ChromaSamplePosition::CSP_COLOCATED,
                                             ChromaSamplePosition::CSP_RESERVED});
      this->chroma_sample_position = chromaSamplePositionVec[chromaPositionIndex];
    }
  }
  if (!this->mono_chrome)
    this->separate_uv_delta_q = reader.readFlag("separate_uv_delta_q");

  std::string subsamplingFormat = "Unknown";
  if (subsampling_x && subsampling_y)
    subsamplingFormat = "4:2:0";
  else if (subsampling_x && !subsampling_y)
    subsamplingFormat = "4:2:2";
  else if (!subsampling_x && !subsampling_y)
    subsamplingFormat = "4:4:4";
  reader.logArbitrary("Subsampling format", subsamplingFormat);
}

} // namespace parser::av1