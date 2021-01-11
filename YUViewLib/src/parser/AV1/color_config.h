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

#pragma once

#include "OpenBitstreamUnit.h"
#include "parser/common/CodingEnum.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser::av1
{

enum class ColorPrimaries
{
  CP_BT_709 = 1,
  CP_UNSPECIFIED,
  CP_BT_470_M = 4,
  CP_BT_470_B_G,
  CP_BT_601,
  CP_SMPTE_240,
  CP_GENERIC_FILM,
  CP_BT_2020,
  CP_XYZ,
  CP_SMPTE_431,
  CP_SMPTE_432,
  CP_EBU_3213 = 22
};

static CodingEnum<ColorPrimaries>
    colorPrimariesCoding({{1, ColorPrimaries::CP_BT_709, "CP_BT_709"},
                          {2, ColorPrimaries::CP_UNSPECIFIED, "CP_UNSPECIFIED"},
                          {4, ColorPrimaries::CP_BT_470_M, "CP_BT_470_M"},
                          {5, ColorPrimaries::CP_BT_470_B_G, "CP_BT_470_B_G"},
                          {6, ColorPrimaries::CP_BT_601, "CP_BT_601"},
                          {7, ColorPrimaries::CP_SMPTE_240, "CP_SMPTE_240"},
                          {8, ColorPrimaries::CP_GENERIC_FILM, "CP_GENERIC_FILM"},
                          {9, ColorPrimaries::CP_BT_2020, "CP_BT_2020"},
                          {10, ColorPrimaries::CP_XYZ, "CP_XYZ"},
                          {11, ColorPrimaries::CP_SMPTE_431, "CP_SMPTE_431"},
                          {12, ColorPrimaries::CP_SMPTE_432, "CP_SMPTE_432"},
                          {22, ColorPrimaries::CP_EBU_3213, "CP_EBU_3213"}},
                         ColorPrimaries::CP_UNSPECIFIED);

enum class TransferCharacteristics
{
  TC_RESERVED,
  TC_BT_709,
  TC_UNSPECIFIED,
  TC_BT_470_M,
  TC_BT_470_B_G,
  TC_BT_601,
  TC_SMPTE_240,
  TC_LINEAR,
  TC_LOG_100,
  TC_LOG_100_SQRT10,
  TC_IEC_61966,
  TC_BT_1361,
  TC_SRGB,
  TC_BT_2020_10_BIT,
  TC_BT_2020_12_BIT,
  TC_SMPTE_2084,
  TC_SMPTE_428,
  TC_HLG
};

static CodingEnum<TransferCharacteristics> transferCharacteristicsCoding(
    {{0, TransferCharacteristics::TC_RESERVED, "TC_RESERVED"},
     {1, TransferCharacteristics::TC_BT_709, "TC_BT_709"},
     {2, TransferCharacteristics::TC_UNSPECIFIED, "TC_UNSPECIFIED"},
     {4, TransferCharacteristics::TC_BT_470_M, "TC_BT_470_M"},
     {5, TransferCharacteristics::TC_BT_470_B_G, "TC_BT_470_B_G"},
     {6, TransferCharacteristics::TC_BT_601, "TC_BT_601"},
     {7, TransferCharacteristics::TC_SMPTE_240, "TC_SMPTE_240"},
     {8, TransferCharacteristics::TC_LINEAR, "TC_LINEAR"},
     {9, TransferCharacteristics::TC_LOG_100, "TC_LOG_100"},
     {10, TransferCharacteristics::TC_LOG_100_SQRT10, "TC_LOG_100_SQRT10"},
     {11, TransferCharacteristics::TC_IEC_61966, "TC_IEC_61966"},
     {12, TransferCharacteristics::TC_BT_1361, "TC_BT_1361"},
     {13, TransferCharacteristics::TC_SRGB, "TC_SRGB"},
     {14, TransferCharacteristics::TC_BT_2020_10_BIT, "TC_BT_2020_10_BIT"},
     {15, TransferCharacteristics::TC_BT_2020_12_BIT, "TC_BT_2020_12_BIT"},
     {16, TransferCharacteristics::TC_SMPTE_2084, "TC_SMPTE_2084"},
     {17, TransferCharacteristics::TC_SMPTE_428, "TC_SMPTE_428"},
     {18, TransferCharacteristics::TC_HLG, "TC_HLG"}},
    TransferCharacteristics::TC_RESERVED);

enum class MatrixCoefficients
{
  MC_IDENTITY,
  MC_BT_709,
  MC_UNSPECIFIED,
  MC_RESERVED,
  MC_FCC,
  MC_BT_470_B_G,
  MC_BT_601,
  MC_SMPTE_240,
  MC_SMPTE_YCGCO,
  MC_BT_2020_NCL,
  MC_BT_2020_CL,
  MC_SMPTE_2085,
  MC_CHROMAT_NCL,
  MC_CHROMAT_CL,
  MC_ICTCP
};

static CodingEnum<MatrixCoefficients>
    matrixCoefficientsCoding({{0, MatrixCoefficients::MC_IDENTITY, "MC_IDENTITY"},
                              {1, MatrixCoefficients::MC_BT_709, "MC_BT_709"},
                              {2, MatrixCoefficients::MC_UNSPECIFIED, "MC_UNSPECIFIED"},
                              {3, MatrixCoefficients::MC_RESERVED, "MC_RESERVED"},
                              {4, MatrixCoefficients::MC_FCC, "MC_FCC"},
                              {5, MatrixCoefficients::MC_BT_470_B_G, "MC_BT_470_B_G"},
                              {6, MatrixCoefficients::MC_BT_601, "MC_BT_601"},
                              {7, MatrixCoefficients::MC_SMPTE_240, "MC_SMPTE_240"},
                              {8, MatrixCoefficients::MC_SMPTE_YCGCO, "MC_SMPTE_YCGCO"},
                              {9, MatrixCoefficients::MC_BT_2020_NCL, "MC_BT_2020_NCL"},
                              {10, MatrixCoefficients::MC_BT_2020_CL, "MC_BT_2020_CL"},
                              {11, MatrixCoefficients::MC_SMPTE_2085, "MC_SMPTE_2085"},
                              {12, MatrixCoefficients::MC_CHROMAT_NCL, "MC_CHROMAT_NCL"},
                              {13, MatrixCoefficients::MC_CHROMAT_CL, "MC_CHROMAT_CL"},
                              {14, MatrixCoefficients::MC_ICTCP, "MC_ICTCP"}},
                             MatrixCoefficients::MC_UNSPECIFIED);

enum class ChromaSamplePosition
{
  CSP_UNKNOWN,
  CSP_VERTICAL,
  CSP_COLOCATED,
  CSP_RESERVED
};

static CodingEnum<ChromaSamplePosition>
    chromaSamplePositionCoding({{0, ChromaSamplePosition::CSP_UNKNOWN, "CSP_UNKNOWN"},
                                {1, ChromaSamplePosition::CSP_VERTICAL, "CSP_VERTICAL"},
                                {2, ChromaSamplePosition::CSP_COLOCATED, "CSP_COLOCATED"},
                                {3, ChromaSamplePosition::CSP_RESERVED, "CSP_RESERVED"}},
                               ChromaSamplePosition::CSP_UNKNOWN);

struct color_config
{
  void parse(reader::SubByteReaderLogging &reader, int seq_profile);

  bool     high_bitdepth{};
  bool     twelve_bit{};
  unsigned BitDepth{};
  bool     mono_chrome{};
  unsigned NumPlanes{};
  bool     color_description_present_flag{};

  ColorPrimaries          color_primaries{ColorPrimaries::CP_UNSPECIFIED};
  TransferCharacteristics transfer_characteristics{TransferCharacteristics::TC_UNSPECIFIED};
  MatrixCoefficients      matrix_coefficients{MatrixCoefficients::MC_UNSPECIFIED};

  bool color_range;
  bool subsampling_x;
  bool subsampling_y;

  ChromaSamplePosition chroma_sample_position{ChromaSamplePosition::CSP_UNKNOWN};

  bool separate_uv_delta_q{};
};

} // namespace parser::av1