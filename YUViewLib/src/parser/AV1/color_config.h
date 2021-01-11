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

unsigned to_int(ColorPrimaries colorPrimaries);

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

unsigned to_int(TransferCharacteristics transferCharacteristics);

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

unsigned to_int(MatrixCoefficients matrixCoefficients);

enum class ChromaSamplePosition
{
  CSP_UNKNOWN,
  CSP_VERTICAL,
  CSP_COLOCATED,
  CSP_RESERVED
};

unsigned to_int(ChromaSamplePosition chromaSamplePosition);

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