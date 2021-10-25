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

#include "dec_ref_pic_marking.h"

#include <common/Typedef.h>

namespace parser::avc

{
using namespace reader;

void dec_ref_pic_marking::parse(reader::SubByteReaderLogging &reader, bool IdrPicFlag)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "dec_ref_pic_marking()");

  if (IdrPicFlag)
  {
    this->no_output_of_prior_pics_flag = reader.readFlag("no_output_of_prior_pics_flag");
    this->long_term_reference_flag     = reader.readFlag("long_term_reference_flag");
  }
  else
  {
    this->adaptive_ref_pic_marking_mode_flag =
        reader.readFlag("adaptive_ref_pic_marking_mode_flag");
    if (this->adaptive_ref_pic_marking_mode_flag)
    {
      unsigned memory_management_control_operation;
      do
      {
        memory_management_control_operation = reader.readUEV("memory_management_control_operation");
        this->memory_management_control_operation_list.push_back(
            memory_management_control_operation);

        if (memory_management_control_operation == 1 || memory_management_control_operation == 3)
          this->difference_of_pic_nums_minus1.push_back(
              reader.readUEV("difference_of_pic_nums_minus1"));
        if (memory_management_control_operation == 2)
          this->long_term_pic_num.push_back(reader.readUEV("long_term_pic_num"));
        if (memory_management_control_operation == 3 || memory_management_control_operation == 6)
          this->long_term_frame_idx.push_back(reader.readUEV("long_term_frame_idx"));
        if (memory_management_control_operation == 4)
          this->max_long_term_frame_idx_plus1.push_back(
              reader.readUEV("max_long_term_frame_idx_plus1"));
      } while (memory_management_control_operation != 0);
    }
  }
}

} // namespace parser::avc