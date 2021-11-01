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

#include "ref_pic_list_mvc_modification.h"

#include <common/Typedef.h>

namespace parser::avc

{
using namespace reader;

void ref_pic_list_mvc_modification::parse(reader::SubByteReaderLogging &reader,
                                          SliceType                     slice_type)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "ref_pic_list_mvc_modification()");

  if (slice_type != SliceType::SLICE_I && slice_type != SliceType::SLICE_SI)
  {
    this->ref_pic_list_modification_flag_l0 = reader.readFlag("ref_pic_list_modification_flag_l0");
    unsigned modification_of_pic_nums_idc;
    if (ref_pic_list_modification_flag_l0)
    {
      do
      {
        modification_of_pic_nums_idc = reader.readUEV("modification_of_pic_nums_idc");
        modification_of_pic_nums_idc_l0.push_back(modification_of_pic_nums_idc);
        if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
        {
          unsigned int abs_diff_pic_num_minus1;
          abs_diff_pic_num_minus1 = reader.readUEV("abs_diff_pic_num_minus1");
          abs_diff_pic_num_minus1_l0.push_back(abs_diff_pic_num_minus1);
        }
        else if (modification_of_pic_nums_idc == 2)
        {
          unsigned int long_term_pic_num;
          long_term_pic_num = reader.readUEV("long_term_pic_num");
          long_term_pic_num_l0.push_back(long_term_pic_num);
        }
        else if (modification_of_pic_nums_idc == 4 || modification_of_pic_nums_idc == 5)
        {
          unsigned int abs_diff_view_idx_minus1;
          abs_diff_view_idx_minus1 = reader.readUEV("abs_diff_view_idx_minus1");
          abs_diff_view_idx_minus1_l0.push_back(abs_diff_view_idx_minus1);
        }
      } while (modification_of_pic_nums_idc != 3);
    }
  }
  if (slice_type == SliceType::SLICE_B)
  {
    this->ref_pic_list_modification_flag_l1 = reader.readFlag("ref_pic_list_modification_flag_l1");
    unsigned int modification_of_pic_nums_idc;
    if (ref_pic_list_modification_flag_l1)
    {
      do
      {
        modification_of_pic_nums_idc = reader.readUEV("modification_of_pic_nums_idc");
        modification_of_pic_nums_idc_l1.push_back(modification_of_pic_nums_idc);
        if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
        {
          unsigned int abs_diff_pic_num_minus1;
          abs_diff_pic_num_minus1 = reader.readUEV("abs_diff_pic_num_minus1");
          abs_diff_pic_num_minus1_l1.push_back(abs_diff_pic_num_minus1);
        }
        else if (modification_of_pic_nums_idc == 2)
        {
          unsigned int long_term_pic_num;
          long_term_pic_num = reader.readUEV("long_term_pic_num");
          long_term_pic_num_l1.push_back(long_term_pic_num);
        }
        else if (modification_of_pic_nums_idc == 4 || modification_of_pic_nums_idc == 5)
        {
          unsigned int abs_diff_view_idx_minus1;
          abs_diff_view_idx_minus1 = reader.readUEV("abs_diff_view_idx_minus1");
          abs_diff_view_idx_minus1_l1.push_back(abs_diff_view_idx_minus1);
        }
      } while (modification_of_pic_nums_idc != 3);
    }
  }
}

} // namespace parser::avc