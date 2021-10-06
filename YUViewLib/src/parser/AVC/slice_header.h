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

#include "commonMaps.h"
#include "nal_unit_header.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser::avc
{

class ref_pic_list_mvc_modification;
class ref_pic_list_modification;
class pred_weight_table;
class dec_ref_pic_marking;

enum class SliceType
{
  SLICE_P,
  SLICE_B,
  SLICE_I,
  SLICE_SP,
  SLICE_SI
};

std::string to_string(SliceType type);

class slice_header
{
public:
  slice_header();
  ~slice_header();

  void parse(reader::SubByteReaderLogging &reader,
             SPSMap &                      spsMap,
             PPSMap &                      ppsMap,
             NalType                       nal_unit_type,
             unsigned                      nal_ref_idc,
             std::shared_ptr<slice_header> prev_pic);

  unsigned int first_mb_in_slice{};
  unsigned int slice_type_id{};
  SliceType    slice_type{};
  bool         slice_type_fixed{};
  bool         IdrPicFlag{};

  unsigned int pic_parameter_set_id{};
  unsigned int colour_plane_id{};
  unsigned int frame_num{};
  bool         field_pic_flag{};
  bool         bottom_field_flag{};
  unsigned int idr_pic_id{};
  unsigned int pic_order_cnt_lsb{};
  int          delta_pic_order_cnt_bottom{};
  int          delta_pic_order_cnt[2]{};
  unsigned int redundant_pic_cnt{};
  bool         direct_spatial_mv_pred_flag{};
  bool         num_ref_idx_active_override_flag{};
  unsigned int num_ref_idx_l0_active_minus1{};
  unsigned int num_ref_idx_l1_active_minus1{};

  std::unique_ptr<ref_pic_list_mvc_modification> refPicListMvcModification;
  std::unique_ptr<ref_pic_list_modification>     refPicListModification;
  std::unique_ptr<pred_weight_table>             predWeightTable;
  std::unique_ptr<dec_ref_pic_marking>           decRefPicMarking;

  unsigned int cabac_init_idc{};
  int          slice_qp_delta{};
  bool         sp_for_switch_flag{};
  int          slice_qs_delta{};
  unsigned int disable_deblocking_filter_idc{};
  int          slice_alpha_c0_offset_div2{};
  int          slice_beta_offset_div2{};
  unsigned int slice_group_change_cycle{};

  // These values are not parsed from the slice header but are calculated
  // from the parsed values.
  int firstMacroblockAddressInSlice{};
  int prevPicOrderCntMsb{-1};
  int prevPicOrderCntLsb{-1};
  int PicOrderCntMsb{-1};
  // For pic_order_cnt_type == 1
  int FrameNumOffset{-1};

  int TopFieldOrderCnt{-1};
  int BottomFieldOrderCnt{-1};

  // This value is not defined in the standard. We just keep on counting the
  // POC up from where we started parsing the bitstream to get a "global" POC.
  int globalPOC{};
  int globalPOC_highestGlobalPOCLastGOP{-1};
  int globalPOC_lastIDR{};
};

} // namespace parser::avc
