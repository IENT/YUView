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

#include <common/Typedef.h>
#include <parser/common/SubByteReaderLogging.h>

#include "commonMaps.h"
#include "nal_unit_header.h"
#include "pred_weight_table.h"
#include "ref_pic_lists_modification.h"
#include "st_ref_pic_set.h"

namespace parser::hevc
{

enum class SliceType
{
  I,
  B,
  P
};

class slice_segment_layer_rbsp;

std::string to_string(SliceType sliceType);

// T-REC-H.265-201410 - 7.3.6.1 slice_segment_header()
class slice_segment_header
{
public:
  slice_segment_header() {}

  void parse(reader::SubByteReaderLogging &            reader,
             bool                                      firstAUInDecodingOrder,
             int                                       prevTid0PicSlicePicOrderCntLsb,
             int                                       prevTid0PicPicOrderCntMsb,
             const nal_unit_header &                   nalUnitHeader,
             SPSMap &                                  spsMap,
             PPSMap &                                  ppsMap,
             std::shared_ptr<slice_segment_layer_rbsp> firstSliceInSegment);

  bool              first_slice_segment_in_pic_flag{};
  bool              no_output_of_prior_pics_flag{};
  bool              dependent_slice_segment_flag{};
  unsigned          slice_pic_parameter_set_id{};
  unsigned          slice_segment_address{};
  bool              discardable_flag{};
  bool              cross_layer_bla_flag{};
  vector<bool>      slice_reserved_flag;
  SliceType         slice_type{};
  bool              pic_output_flag{true};
  unsigned          colour_plane_id{};
  unsigned          slice_pic_order_cnt_lsb{};
  bool              short_term_ref_pic_set_sps_flag{};
  st_ref_pic_set    stRefPicSet{};
  unsigned          short_term_ref_pic_set_idx{};
  unsigned          num_long_term_sps{};
  unsigned          num_long_term_pics{};
  umap_1d<unsigned> lt_idx_sps;
  umap_1d<unsigned> poc_lsb_lt;
  umap_1d<bool>     used_by_curr_pic_lt_flag;
  umap_1d<bool>     delta_poc_msb_present_flag;
  umap_1d<unsigned> delta_poc_msb_cycle_lt;
  bool              slice_temporal_mvp_enabled_flag{};
  bool              slice_sao_luma_flag{};
  bool              slice_sao_chroma_flag{};
  bool              num_ref_idx_active_override_flag{};
  unsigned          num_ref_idx_l0_active_minus1{};
  unsigned          num_ref_idx_l1_active_minus1{};

  ref_pic_lists_modification refPicListsModification;

  bool              mvd_l1_zero_flag{};
  bool              cabac_init_flag{};
  bool              collocated_from_l0_flag{true};
  unsigned          collocated_ref_idx{};
  pred_weight_table predWeightTable;
  unsigned          five_minus_max_num_merge_cand{};
  int               slice_qp_delta{};
  int               slice_cb_qp_offset{};
  int               slice_cr_qp_offset{};
  bool              cu_chroma_qp_offset_enabled_flag{};
  bool              deblocking_filter_override_flag{};
  bool              slice_deblocking_filter_disabled_flag{};
  int               slice_beta_offset_div2{};
  int               slice_tc_offset_div2{};
  bool              slice_loop_filter_across_slices_enabled_flag{};

  unsigned         num_entry_point_offsets{};
  unsigned         offset_len_minus1{};
  vector<unsigned> entry_point_offset_minus1;

  unsigned         slice_segment_header_extension_length{};
  vector<unsigned> slice_segment_header_extension_data_byte;

  // Calculated values
  int         PicOrderCntVal{-1}; // The slice POC
  int         PicOrderCntMsb{-1};
  vector<int> UsedByCurrPicLt;
  bool        NoRaslOutputFlag{};

  int globalPOC{-1};
};

} // namespace parser::hevc