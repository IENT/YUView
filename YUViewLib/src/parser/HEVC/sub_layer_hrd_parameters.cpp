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

#include "sub_layer_hrd_parameters.h"

#include <parser/common/functions.h>

namespace parser::hevc
{

using namespace reader;

void sub_layer_hrd_parameters::parse(SubByteReaderLogging &reader,
                                     int                   CpbCnt,
                                     bool                  sub_pic_hrd_params_present_flag,
                                     bool                  SubPicHrdFlag,
                                     unsigned int          bit_rate_scale,
                                     unsigned int          cpb_size_scale,
                                     unsigned int          cpb_size_du_scale)
{
  SubByteReaderLoggingSubLevel subevel(reader, "sub_layer_hrd_parameters()");

  if (CpbCnt >= 32)
    throw std::logic_error("The value of CpbCnt must be in the range of 0 to 31");

  for (int i = 0; i <= CpbCnt; i++)
  {
    this->bit_rate_value_minus1.push_back(reader.readUEV(
        "bit_rate_value_minus1",
        Options()
            .withMeaning("(together with bit_rate_scale) specifies the maximum) input bit rate for "
                         "the i-th CPB when the CPB operates at the access unit level.")
            .withCheckRange({0, 4294967294})));
    if (i > 0 && this->bit_rate_value_minus1[i] <= this->bit_rate_value_minus1[i - 1])
      throw std::logic_error("For any i > 0, bit_rate_value_minus1[i] shall be greater than "
                             "bit_rate_value_minus1[i-1].");
    if (!SubPicHrdFlag)
    {
      auto value = (this->bit_rate_value_minus1[i] + 1) * (1 << (6 + bit_rate_scale));
      this->BitRate.push_back(value);
      reader.logCalculatedValue(formatArray("BitRate", i), value);
    }

    this->cpb_size_value_minus1.push_back(
        reader.readUEV("cpb_size_value_minus1",
                       Options()
                           .withMeaning("Is used together with cpb_size_scale to specify) the i-th "
                                        "CPB size when the CPB operates at the access unit level.")
                           .withCheckRange({0, 4294967294})));
    if (i > 0 && this->cpb_size_value_minus1[i] > this->cpb_size_value_minus1[i - 1])
      throw std::logic_error("For any i greater than 0, cpb_size_value_minus1[i] shall be less "
                             "than or equal to cpb_size_value_minus1[i-1].");
    if (!SubPicHrdFlag)
    {
      auto value = (this->cpb_size_value_minus1[i] + 1) * (1 << (4 + cpb_size_scale));
      CpbSize.push_back(value);
      reader.logCalculatedValue(formatArray("CpbSize", i), value);
    }

    if (sub_pic_hrd_params_present_flag)
    {
      this->cpb_size_du_value_minus1.push_back(reader.readUEV(
          "cpb_size_du_value_minus1",
          Options()
              .withMeaning("Is used together with cpb_size_du_scale to specify) the i-th CPB size "
                           "when the CPB operates at sub-picture level.")
              .withCheckRange({0, 4294967294})));
      if (i > 0 && cpb_size_du_value_minus1[i] > cpb_size_du_value_minus1[i - 1])
        throw std::logic_error("For any i greater than 0, cpb_size_du_value_minus1[i] shall be "
                               "less than or equal to cpb_size_du_value_minus1[i-1].");
      if (SubPicHrdFlag)
      {
        auto value = (this->cpb_size_du_value_minus1[i] + 1) * (1 << (4 + cpb_size_du_scale));
        CpbSize.push_back(value);
        reader.logCalculatedValue(formatArray("CpbSize", i), value);
      }

      this->bit_rate_du_value_minus1.push_back(reader.readUEV(
          "bit_rate_du_value_minus1",
          Options()
              .withMeaning("(together with bit_rate_scale) specifies the maximum) input bit rate "
                           "for the i-th CPB when the CPB operates at the sub-picture level.")
              .withCheckRange({0, 4294967294})));
      if (i > 0 && bit_rate_du_value_minus1[i] <= bit_rate_du_value_minus1[i - 1])
        throw std::logic_error("For any i > 0, bit_rate_du_value_minus1[i] shall be greater than "
                               "bit_rate_du_value_minus1[i-1].");
      if (SubPicHrdFlag)
      {
        auto value = (this->bit_rate_du_value_minus1[i] + 1) * (1 << (6 + bit_rate_scale));
        BitRate.push_back(value);
        reader.logCalculatedValue(formatArray("BitRate", i), value);
      }
    }
    this->cbr_flag.push_back(reader.readFlag(
        formatArray("cbr_flag", i),
        Options().withMeaning(
            "False specifies that to decode this CVS by the HRD using the i-th CPB specification, "
            "the hypothetical stream scheduler (HSS) operates in an intermittent bit rate mode. "
            "True specifies that the HSS operates in a constant bit rate (CBR) mode.")));
  }
}

} // namespace parser::hevc