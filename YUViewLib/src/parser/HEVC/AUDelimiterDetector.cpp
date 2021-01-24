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

#include "AUDelimiterDetector.h"

#include "NalUnitHEVC.h"

namespace parser::hevc
{

bool AUDelimiterDetector::isStartOfNewAU(std::shared_ptr<NalUnitHEVC> nal,
                                         bool first_slice_segment_in_pic_flag)
{
  bool isStart = false;
  if (this->primaryCodedPictureInAuEncountered)
  {
    if (nal->header.nal_unit_type == NalType::AUD_NUT && nal->header.nuh_layer_id == 0)
      isStart = true;

    if (nal->header.nuh_layer_id == 0 && (nal->header.nal_unit_type == NalType::VPS_NUT ||
                                          nal->header.nal_unit_type == NalType::SPS_NUT ||
                                          nal->header.nal_unit_type == NalType::PPS_NUT ||
                                          nal->header.nal_unit_type == NalType::PREFIX_SEI_NUT))
      isStart = true;

    if (nal->header.nal_unit_type == NalType::PREFIX_SEI_NUT && nal->header.nuh_layer_id == 0)
      isStart = true;

    if (nal->header.nuh_layer_id == 0 && (nal->header.nal_unit_type == NalType::RSV_NVCL41 ||
                                          nal->header.nal_unit_type == NalType::RSV_NVCL42 ||
                                          nal->header.nal_unit_type == NalType::RSV_NVCL43 ||
                                          nal->header.nal_unit_type == NalType::RSV_NVCL44))
      isStart = true;

    if (nal->header.nuh_layer_id == 0 && (nal->header.nal_unit_type >= NalType::UNSPEC48 &&
                                          nal->header.nal_unit_type <= NalType::UNSPEC55))
      isStart = true;

    if (nal->header.isSlice() && first_slice_segment_in_pic_flag)
      isStart = true;
  }

  if (nal->header.isSlice())
    this->primaryCodedPictureInAuEncountered = true;

  if (isStart && !nal->header.isSlice())
    this->primaryCodedPictureInAuEncountered = false;

  return isStart;
}

} // namespace parser::hevc
