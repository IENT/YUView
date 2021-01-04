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

#include "NalUnitVVC.h"
#include "commonMaps.h"
#include "parser/common/SubByteReaderLogging.h"
#include "rbsp_trailing_bits.h"

namespace parser::vvc
{

class pic_parameter_set_rbsp : public NalRBSP
{
public:
  pic_parameter_set_rbsp()  = default;
  ~pic_parameter_set_rbsp() = default;
  void parse(reader::SubByteReaderLogging &reader, SPSMap &spsMap);

  unsigned           pps_pic_parameter_set_id{};
  unsigned           pps_seq_parameter_set_id{};
  bool               pps_mixed_nalu_types_in_pic_flag{};
  unsigned           pps_pic_width_in_luma_samples{};
  unsigned           pps_pic_height_in_luma_samples{};
  bool               pps_conformance_window_flag{};
  unsigned           pps_conf_win_left_offset{};
  unsigned           pps_conf_win_right_offset{};
  unsigned           pps_conf_win_top_offset{};
  unsigned           pps_conf_win_bottom_offset{};
  bool               pps_scaling_window_explicit_signalling_flag{};
  int                pps_scaling_win_left_offset{};
  int                pps_scaling_win_right_offset{};
  int                pps_scaling_win_top_offset{};
  int                pps_scaling_win_bottom_offset{};
  bool               pps_output_flag_present_flag{};
  bool               pps_no_pic_partition_flag{};
  bool               pps_subpic_id_mapping_present_flag{};
  unsigned           pps_num_subpics_minus1{};
  unsigned           pps_subpic_id_len_minus1{};
  vector<unsigned>   pps_subpic_id{};
  unsigned           pps_log2_ctu_size_minus5{};
  unsigned           pps_num_exp_tile_columns_minus1{};
  unsigned           pps_num_exp_tile_rows_minus1{};
  vector<unsigned>   pps_tile_column_width_minus1{};
  vector<unsigned>   pps_tile_row_height_minus1{};
  bool               pps_loop_filter_across_tiles_enabled_flag{};
  bool               pps_rect_slice_flag{true};
  bool               pps_single_slice_per_subpic_flag{};
  unsigned           pps_num_slices_in_pic_minus1{};
  bool               pps_tile_idx_delta_present_flag{};
  vector<unsigned>   pps_slice_width_in_tiles_minus1{};
  vector<unsigned>   pps_slice_height_in_tiles_minus1{};
  umap_1d<unsigned>  pps_num_exp_slices_in_tile{};
  vector2d<unsigned> pps_exp_slice_height_in_ctus_minus1{};
  vector<int>        pps_tile_idx_delta_val{};
  bool               pps_loop_filter_across_slices_enabled_flag{};
  bool               pps_cabac_init_present_flag{};
  vector<unsigned>   pps_num_ref_idx_default_active_minus1{};
  bool               pps_rpl1_idx_present_flag{};
  bool               pps_weighted_pred_flag{};
  bool               pps_weighted_bipred_flag{};
  bool               pps_ref_wraparound_enabled_flag{};
  unsigned           pps_pic_width_minus_wraparound_offset{};
  int                pps_init_qp_minus26{};
  bool               pps_cu_qp_delta_enabled_flag{};
  bool               pps_chroma_tool_offsets_present_flag{};
  int                pps_cb_qp_offset{};
  int                pps_cr_qp_offset{};
  bool               pps_joint_cbcr_qp_offset_present_flag{};
  int                pps_joint_cbcr_qp_offset_value{};
  bool               pps_slice_chroma_qp_offsets_present_flag{};
  bool               pps_cu_chroma_qp_offset_list_enabled_flag{};
  unsigned           pps_chroma_qp_offset_list_len_minus1{};
  vector<int>        pps_cb_qp_offset_list{};
  vector<int>        pps_cr_qp_offset_list{};
  vector<int>        pps_joint_cbcr_qp_offset_list{};
  bool               pps_deblocking_filter_control_present_flag{};
  bool               pps_deblocking_filter_override_enabled_flag{};
  bool               pps_deblocking_filter_disabled_flag{};
  bool               pps_dbf_info_in_ph_flag{};
  int                pps_luma_beta_offset_div2{};
  int                pps_luma_tc_offset_div2{};
  int                pps_cb_beta_offset_div2{};
  int                pps_cb_tc_offset_div2{};
  int                pps_cr_beta_offset_div2{};
  int                pps_cr_tc_offset_div2{};
  bool               pps_rpl_info_in_ph_flag{};
  bool               pps_sao_info_in_ph_flag{};
  bool               pps_alf_info_in_ph_flag{};
  bool               pps_wp_info_in_ph_flag{};
  bool               pps_qp_delta_info_in_ph_flag{};
  bool               pps_picture_header_extension_present_flag{};
  bool               pps_slice_header_extension_present_flag{};
  bool               pps_extension_flag{};
  bool               pps_extension_data_flag{};
  rbsp_trailing_bits rbsp_trailing_bits_instance;

  vector<unsigned> ColWidthVal;
  unsigned         NumTileColumns; // Size of ColWidthVal
  vector<unsigned> RowHeightVal;
  unsigned         NumTileRows; // Size of RowHeightVal
  unsigned         NumTilesInPic;

  vector<unsigned>  TileColBdVal;
  vector<unsigned>  TileRowBdVal;
  vector<unsigned>  CtbToTileColBd;
  vector<unsigned>  ctbToTileColIdx;
  vector<unsigned>  CtbToTileRowBd;
  vector<unsigned>  ctbToTileRowIdx;
  vector<unsigned>  SubpicWidthInTiles;
  vector<unsigned>  SubpicHeightInTiles;
  vector<bool>      subpicHeightLessThanOneTileFlag;
  vector<unsigned>  SliceTopLeftTileIdx;
  umap_1d<unsigned> sliceWidthInTiles;
  umap_1d<unsigned> sliceHeightInTiles;
  umap_1d<unsigned> NumSlicesInTile;
  umap_1d<unsigned> sliceHeightInCtus;
  umap_2d<unsigned> CtbAddrInSlice;
  umap_1d<unsigned> NumCtusInSlice;

  vector<unsigned>  NumSlicesInSubpic;
  umap_1d<unsigned> SubpicIdxForSlice;
  umap_1d<unsigned> SubpicLevelSliceIdx;

  vector<unsigned> SubpicIdVal;

  unsigned PicWidthInCtbsY{};
  unsigned PicHeightInCtbsY{};
  unsigned PicSizeInCtbsY{};
  unsigned PicWidthInMinCbsY{};
  unsigned PicHeightInMinCbsY{};
  unsigned PicSizeInMinCbsY{};
  unsigned PicSizeInSamplesY{};
  unsigned PicWidthInSamplesC{};
  unsigned PicHeightInSamplesC{};

private:
  void calculateTileRowsAndColumns();
  void calculateTilesInSlices(std::shared_ptr<seq_parameter_set_rbsp> sps);
};

} // namespace parser::vvc
