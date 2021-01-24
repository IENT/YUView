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

using namespace reader;

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
    auto color_primaries_index =
        reader.readBits("color_primaries",
                        8,
                        Options()
                            .withMeaningMap(colorPrimariesCoding.getMeaningMap())
                            .withMeaning("Unused"));
    this->color_primaries = colorPrimariesCoding.getValue(color_primaries_index);

    auto transfer_characteristics_index =
        reader.readBits("transfer_characteristics",
                        8,
                        Options()
                            .withMeaningMap(transferCharacteristicsCoding.getMeaningMap())
                            .withMeaning("Reserved"));
    this->transfer_characteristics =
        transferCharacteristicsCoding.getValue(transfer_characteristics_index);

    auto matrix_coefficients_index = reader.readBits(
        "matrix_coefficients",
        8,
        Options().withMeaningMap(matrixCoefficientsCoding.getMeaningMap()).withMeaning("Reserved"));
    this->matrix_coefficients = matrixCoefficientsCoding.getValue(matrix_coefficients_index);
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

      this->chroma_sample_position = chromaSamplePositionCoding.getValue(chromaPositionIndex);
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