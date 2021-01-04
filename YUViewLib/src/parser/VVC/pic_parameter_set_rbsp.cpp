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

#include "pic_parameter_set_rbsp.h"

#include <cmath>

#include "seq_parameter_set_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void pic_parameter_set_rbsp::parse(SubByteReaderLogging &reader, SPSMap &spsMap)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pic_parameter_set_rbsp");

  this->pps_pic_parameter_set_id = reader.readBits("pps_pic_parameter_set_id", 6);
  this->pps_seq_parameter_set_id =
      reader.readBits("pps_seq_parameter_set_id", 4, Options().withCheckRange({0, 15}));

  if (spsMap.count(this->pps_seq_parameter_set_id) == 0)
    throw std::logic_error("SPS with given pps_seq_parameter_set_id not found.");
  auto sps = spsMap[this->pps_seq_parameter_set_id];

  this->pps_mixed_nalu_types_in_pic_flag = reader.readFlag("pps_mixed_nalu_types_in_pic_flag");
  this->pps_pic_width_in_luma_samples =
      reader.readUEV("pps_pic_width_in_luma_samples",
                     Options().withCheckEqualTo(sps->sps_pic_width_max_in_luma_samples));
  this->pps_pic_height_in_luma_samples =
      reader.readUEV("pps_pic_height_in_luma_samples",
                     Options().withCheckEqualTo(sps->sps_pic_height_max_in_luma_samples));

  // (64) -> (72)
  this->PicWidthInCtbsY    = std::ceil(this->pps_pic_width_in_luma_samples / sps->CtbSizeY);
  this->PicHeightInCtbsY   = std::ceil(this->pps_pic_height_in_luma_samples / sps->CtbSizeY);
  this->PicSizeInCtbsY     = this->PicWidthInCtbsY * this->PicHeightInCtbsY;
  this->PicWidthInMinCbsY  = this->pps_pic_width_in_luma_samples / sps->MinCbSizeY;
  this->PicHeightInMinCbsY = this->pps_pic_height_in_luma_samples / sps->MinCbSizeY;
  this->PicSizeInMinCbsY   = this->PicWidthInMinCbsY * this->PicHeightInMinCbsY;
  this->PicSizeInSamplesY =
      this->pps_pic_width_in_luma_samples * this->pps_pic_height_in_luma_samples;
  this->PicWidthInSamplesC  = this->pps_pic_width_in_luma_samples / sps->SubWidthC;
  this->PicHeightInSamplesC = this->pps_pic_height_in_luma_samples / sps->SubHeightC;

  {
    Options opt;
    if (this->pps_pic_width_in_luma_samples == sps->sps_pic_width_max_in_luma_samples &&
        this->pps_pic_height_in_luma_samples == sps->sps_pic_height_max_in_luma_samples)
      opt = Options().withCheckEqualTo(0);
    this->pps_conformance_window_flag = reader.readFlag("pps_conformance_window_flag", opt);
  }
  if (this->pps_conformance_window_flag)
  {
    this->pps_conf_win_left_offset   = reader.readUEV("pps_conf_win_left_offset");
    this->pps_conf_win_right_offset  = reader.readUEV("pps_conf_win_right_offset");
    this->pps_conf_win_top_offset    = reader.readUEV("pps_conf_win_top_offset");
    this->pps_conf_win_bottom_offset = reader.readUEV("pps_conf_win_bottom_offset");
  }
  {
    Options opt;
    if (!sps->sps_ref_pic_resampling_enabled_flag)
      opt = Options().withCheckEqualTo(0);
    this->pps_scaling_window_explicit_signalling_flag =
        reader.readFlag("pps_scaling_window_explicit_signalling_flag", opt);
  }
  if (this->pps_scaling_window_explicit_signalling_flag)
  {
    this->pps_scaling_win_left_offset   = reader.readSEV("pps_scaling_win_left_offset");
    this->pps_scaling_win_right_offset  = reader.readSEV("pps_scaling_win_right_offset");
    this->pps_scaling_win_top_offset    = reader.readSEV("pps_scaling_win_top_offset");
    this->pps_scaling_win_bottom_offset = reader.readSEV("pps_scaling_win_bottom_offset");
  }
  this->pps_output_flag_present_flag = reader.readFlag("pps_output_flag_present_flag");
  {
    Options opt;
    if (sps->sps_num_subpics_minus1 > 0 || this->pps_mixed_nalu_types_in_pic_flag)
      opt = Options().withCheckEqualTo(0);
    this->pps_no_pic_partition_flag = reader.readFlag("pps_no_pic_partition_flag", opt);
  }
  {
    Options opt;
    if (!sps->sps_subpic_id_mapping_explicitly_signalled_flag ||
        sps->sps_subpic_id_mapping_present_flag)
      opt = Options().withCheckEqualTo(0);
    else if (sps->sps_subpic_id_mapping_explicitly_signalled_flag &&
             !sps->sps_subpic_id_mapping_present_flag)
      opt = Options().withCheckEqualTo(1);
    this->pps_subpic_id_mapping_present_flag =
        reader.readFlag("pps_subpic_id_mapping_present_flag", opt);
  }
  if (this->pps_subpic_id_mapping_present_flag)
  {
    if (!this->pps_no_pic_partition_flag)
    {
      this->pps_num_subpics_minus1 = reader.readUEV("pps_num_subpics_minus1");
    }
    this->pps_subpic_id_len_minus1 = reader.readUEV("pps_subpic_id_len_minus1");
    for (unsigned i = 0; i <= this->pps_num_subpics_minus1; i++)
    {
      auto nrBits = this->pps_subpic_id_len_minus1 + 1;
      this->pps_subpic_id.push_back(reader.readBits("pps_subpic_id", nrBits));

      // (75)
      if (sps->sps_subpic_id_mapping_explicitly_signalled_flag)
        this->SubpicIdVal.push_back(this->pps_subpic_id_mapping_present_flag
                                        ? this->pps_subpic_id[i]
                                        : sps->sps_subpic_id[i]);
      else
        this->SubpicIdVal.push_back(i);
    }
  }
  if (!this->pps_no_pic_partition_flag)
  {
    this->pps_log2_ctu_size_minus5 = reader.readBits(
        "pps_log2_ctu_size_minus5", 2, Options().withCheckEqualTo(sps->sps_log2_ctu_size_minus5));
    this->pps_num_exp_tile_columns_minus1 = reader.readUEV(
        "pps_num_exp_tile_columns_minus1", Options().withCheckRange({0, PicWidthInCtbsY}));
    this->pps_num_exp_tile_rows_minus1 = reader.readUEV(
        "pps_num_exp_tile_rows_minus1", Options().withCheckRange({0, PicHeightInCtbsY}));
    for (unsigned i = 0; i <= this->pps_num_exp_tile_columns_minus1; i++)
    {
      this->pps_tile_column_width_minus1.push_back(reader.readUEV("pps_tile_column_width_minus1"));
    }
    for (unsigned i = 0; i <= this->pps_num_exp_tile_rows_minus1; i++)
    {
      this->pps_tile_row_height_minus1.push_back(reader.readUEV("pps_tile_row_height_minus1"));
    }

    this->calculateTileRowsAndColumns();

    if (this->NumTilesInPic > 1)
    {
      this->pps_loop_filter_across_tiles_enabled_flag =
          reader.readFlag("pps_loop_filter_across_tiles_enabled_flag");
      {
        Options opt;
        if (sps->sps_subpic_info_present_flag || this->pps_mixed_nalu_types_in_pic_flag)
          opt = Options().withCheckEqualTo(1);
        this->pps_rect_slice_flag = reader.readFlag("pps_rect_slice_flag", opt);
      }
    }
    if (this->pps_rect_slice_flag)
    {
      this->pps_single_slice_per_subpic_flag = reader.readFlag("pps_single_slice_per_subpic_flag");
    }

    if (this->pps_rect_slice_flag && !this->pps_single_slice_per_subpic_flag)
    {
      this->pps_num_slices_in_pic_minus1 = reader.readUEV(
          "pps_num_slices_in_pic_minus1"); // Options().withCheckRange({0, MaxSlicesPerAu}
      if (this->pps_num_slices_in_pic_minus1 > 1)
      {
        this->pps_tile_idx_delta_present_flag = reader.readFlag("pps_tile_idx_delta_present_flag");
      }

      this->calculateTilesInSlices(sps);

      for (unsigned i = 0; i < this->pps_num_slices_in_pic_minus1; i++)
      {
        if (this->SliceTopLeftTileIdx[i] % this->NumTileColumns != this->NumTileColumns - 1)
        {
          this->pps_slice_width_in_tiles_minus1.push_back(
              reader.readUEV("pps_slice_width_in_tiles_minus1"));
        }
        if (this->SliceTopLeftTileIdx[i] / this->NumTileColumns != this->NumTileRows - 1 &&
            (this->pps_tile_idx_delta_present_flag ||
             this->SliceTopLeftTileIdx[i] % this->NumTileColumns == 0))
        {
          this->pps_slice_height_in_tiles_minus1.push_back(
              reader.readUEV("pps_slice_height_in_tiles_minus1"));
        }
        else // Inferr
        {
          if (this->SliceTopLeftTileIdx[i] / this->NumTileColumns == this->NumTileRows - 1)
          {
            this->pps_slice_height_in_tiles_minus1.push_back(0);
          }
          else
          {
            this->pps_slice_height_in_tiles_minus1.push_back(
                this->pps_slice_height_in_tiles_minus1[i - 1]);
          }
        }
        if (this->pps_slice_width_in_tiles_minus1[i] == 0 &&
            this->pps_slice_height_in_tiles_minus1[i] == 0 &&
            this->RowHeightVal[SliceTopLeftTileIdx[i] / this->NumTileColumns] > 1)
        {
          this->pps_num_exp_slices_in_tile[i] = reader.readUEV("pps_num_exp_slices_in_tile");
          this->pps_exp_slice_height_in_ctus_minus1.push_back({});
          for (unsigned j = 0; j < this->pps_num_exp_slices_in_tile[i]; j++)
          {
            this->pps_exp_slice_height_in_ctus_minus1[i].push_back(
                reader.readUEV("pps_exp_slice_height_in_ctus_minus1"));
          }
          i += NumSlicesInTile[i] - 1;
        }
        if (this->pps_tile_idx_delta_present_flag && i < this->pps_num_slices_in_pic_minus1)
        {
          this->pps_tile_idx_delta_val.push_back(reader.readSEV("pps_tile_idx_delta_val"));
        }
      }
    }
    else
    {
      this->calculateTilesInSlices(sps);
    }

    if (this->pps_single_slice_per_subpic_flag)
    {
      this->pps_num_slices_in_pic_minus1 = sps->sps_num_subpics_minus1;
    }

    if (!this->pps_rect_slice_flag || this->pps_single_slice_per_subpic_flag ||
        this->pps_num_slices_in_pic_minus1 > 0)
    {
      this->pps_loop_filter_across_slices_enabled_flag =
          reader.readFlag("pps_loop_filter_across_slices_enabled_flag");
    }
  }
  else // pps_no_pic_partition_flag == true
  {
    this->pps_single_slice_per_subpic_flag = true;
    this->pps_tile_column_width_minus1.push_back(this->PicWidthInCtbsY - 1);
    this->pps_tile_row_height_minus1.push_back(this->PicHeightInCtbsY - 1);
    this->calculateTileRowsAndColumns();
    this->calculateTilesInSlices(sps);
  }

  this->pps_cabac_init_present_flag = reader.readFlag("pps_cabac_init_present_flag");
  for (unsigned i = 0; i < 2; i++)
  {
    this->pps_num_ref_idx_default_active_minus1.push_back(
        reader.readUEV("pps_num_ref_idx_default_active_minus1"));
  }
  this->pps_rpl1_idx_present_flag = reader.readFlag("pps_rpl1_idx_present_flag");
  this->pps_weighted_pred_flag =
      reader.readFlag("pps_weighted_pred_flag",
                      sps->sps_weighted_pred_flag ? Options() : Options().withCheckEqualTo(0));
  this->pps_weighted_bipred_flag =
      reader.readFlag("pps_weighted_bipred_flag",
                      sps->sps_weighted_bipred_flag ? Options() : Options().withCheckEqualTo(0));
  {
    Options opt;
    if (!sps->sps_ref_wraparound_enabled_flag ||
        (sps->CtbSizeY / sps->MinCbSizeY + 1) >
            this->pps_pic_width_in_luma_samples / sps->MinCbSizeY - 1)
      opt = Options().withCheckEqualTo(0);
    this->pps_ref_wraparound_enabled_flag = reader.readFlag("pps_ref_wraparound_enabled_flag", opt);
  }
  if (this->pps_ref_wraparound_enabled_flag)
  {
    this->pps_pic_width_minus_wraparound_offset = reader.readUEV(
        "pps_pic_width_minus_wraparound_offset",
        Options().withCheckSmaller((this->pps_pic_width_in_luma_samples / sps->MinCbSizeY) -
                                   (sps->CtbSizeY / sps->MinCbSizeY) - 2));
  }
  this->pps_init_qp_minus26 = reader.readSEV(
      "pps_init_qp_minus26", Options().withCheckRange({-(26 + int(sps->QpBdOffset)), 37}));
  this->pps_cu_qp_delta_enabled_flag = reader.readFlag("pps_cu_qp_delta_enabled_flag");
  {
    Options opt;
    if (sps->sps_chroma_format_idc == 0)
      opt = Options().withCheckEqualTo(0);
    this->pps_chroma_tool_offsets_present_flag =
        reader.readFlag("pps_chroma_tool_offsets_present_flag", opt);
  }
  if (this->pps_chroma_tool_offsets_present_flag)
  {
    this->pps_cb_qp_offset = reader.readSEV("pps_cb_qp_offset");
    this->pps_cr_qp_offset = reader.readSEV("pps_cr_qp_offset");
    this->pps_joint_cbcr_qp_offset_present_flag =
        reader.readFlag("pps_joint_cbcr_qp_offset_present_flag");
    if (this->pps_joint_cbcr_qp_offset_present_flag)
    {
      this->pps_joint_cbcr_qp_offset_value =
          reader.readSEV("pps_joint_cbcr_qp_offset_value", Options().withCheckRange({-12, +12}));
    }
    this->pps_slice_chroma_qp_offsets_present_flag =
        reader.readFlag("pps_slice_chroma_qp_offsets_present_flag");
    this->pps_cu_chroma_qp_offset_list_enabled_flag =
        reader.readFlag("pps_cu_chroma_qp_offset_list_enabled_flag");
    if (this->pps_cu_chroma_qp_offset_list_enabled_flag)
    {
      this->pps_chroma_qp_offset_list_len_minus1 =
          reader.readUEV("pps_chroma_qp_offset_list_len_minus1", Options().withCheckRange({0, 5}));
      for (unsigned i = 0; i <= this->pps_chroma_qp_offset_list_len_minus1; i++)
      {
        this->pps_cb_qp_offset_list.push_back(reader.readSEV("pps_cb_qp_offset_list"));
        this->pps_cr_qp_offset_list.push_back(reader.readSEV("pps_cr_qp_offset_list"));
        if (this->pps_joint_cbcr_qp_offset_present_flag)
        {
          this->pps_joint_cbcr_qp_offset_list.push_back(
              reader.readSEV("pps_joint_cbcr_qp_offset_list"));
        }
      }
    }
  }
  this->pps_deblocking_filter_control_present_flag =
      reader.readFlag("pps_deblocking_filter_control_present_flag");
  if (this->pps_deblocking_filter_control_present_flag)
  {
    this->pps_deblocking_filter_override_enabled_flag =
        reader.readFlag("pps_deblocking_filter_override_enabled_flag");
    this->pps_deblocking_filter_disabled_flag =
        reader.readFlag("pps_deblocking_filter_disabled_flag");
    if (!this->pps_no_pic_partition_flag && this->pps_deblocking_filter_override_enabled_flag)
    {
      this->pps_dbf_info_in_ph_flag = reader.readFlag("pps_dbf_info_in_ph_flag");
    }
    if (!this->pps_deblocking_filter_disabled_flag)
    {
      this->pps_luma_beta_offset_div2 = reader.readSEV("pps_luma_beta_offset_div2");
      this->pps_luma_tc_offset_div2   = reader.readSEV("pps_luma_tc_offset_div2");
      if (this->pps_chroma_tool_offsets_present_flag)
      {
        this->pps_cb_beta_offset_div2 = reader.readSEV("pps_cb_beta_offset_div2");
        this->pps_cb_tc_offset_div2   = reader.readSEV("pps_cb_tc_offset_div2");
        this->pps_cr_beta_offset_div2 = reader.readSEV("pps_cr_beta_offset_div2");
        this->pps_cr_tc_offset_div2   = reader.readSEV("pps_cr_tc_offset_div2");
      }
    }
  }
  if (!this->pps_no_pic_partition_flag)
  {
    this->pps_rpl_info_in_ph_flag = reader.readFlag("pps_rpl_info_in_ph_flag");
    this->pps_sao_info_in_ph_flag = reader.readFlag("pps_sao_info_in_ph_flag");
    this->pps_alf_info_in_ph_flag = reader.readFlag("pps_alf_info_in_ph_flag");
    if ((this->pps_weighted_pred_flag || this->pps_weighted_bipred_flag) &&
        this->pps_rpl_info_in_ph_flag)
    {
      this->pps_wp_info_in_ph_flag = reader.readFlag("pps_wp_info_in_ph_flag");
    }
    this->pps_qp_delta_info_in_ph_flag = reader.readFlag("pps_qp_delta_info_in_ph_flag");
  }
  this->pps_picture_header_extension_present_flag =
      reader.readFlag("pps_picture_header_extension_present_flag", Options().withCheckEqualTo(0));
  this->pps_slice_header_extension_present_flag =
      reader.readFlag("pps_slice_header_extension_present_flag", Options().withCheckEqualTo(0));
  this->pps_extension_flag = reader.readFlag("pps_extension_flag", Options().withCheckEqualTo(0));
  if (this->pps_extension_flag)
  {
    while (reader.more_rbsp_data())
    {
      this->pps_extension_data_flag = reader.readFlag("pps_extension_data_flag");
    }
  }
  this->rbsp_trailing_bits_instance.parse(reader);
}

void pic_parameter_set_rbsp::calculateTileRowsAndColumns()
{
  {
    // 6.5.1 (14)
    auto remainingWidthInCtbsY = this->PicWidthInCtbsY;
    for (unsigned i = 0; i <= this->pps_num_exp_tile_columns_minus1; i++)
    {
      this->ColWidthVal.push_back(this->pps_tile_column_width_minus1[i] + 1);
      remainingWidthInCtbsY -= this->ColWidthVal[i];
    }
    auto uniformTileColWidth =
        this->pps_tile_column_width_minus1[pps_num_exp_tile_columns_minus1] + 1;
    while (remainingWidthInCtbsY >= uniformTileColWidth)
    {
      this->ColWidthVal.push_back(uniformTileColWidth);
      remainingWidthInCtbsY -= uniformTileColWidth;
    }
    if (remainingWidthInCtbsY > 0)
      ColWidthVal.push_back(remainingWidthInCtbsY);
    this->NumTileColumns = unsigned(this->ColWidthVal.size());
  }
  {
    // 6.5.1 (15)
    auto remainingHeightInCtbsY = PicHeightInCtbsY;
    for (unsigned j = 0; j <= this->pps_num_exp_tile_rows_minus1; j++)
    {
      this->RowHeightVal.push_back(this->pps_tile_row_height_minus1[j] + 1);
      remainingHeightInCtbsY -= RowHeightVal[j];
    }
    auto uniformTileRowHeight =
        this->pps_tile_row_height_minus1[this->pps_num_exp_tile_rows_minus1] + 1;
    while (remainingHeightInCtbsY >= uniformTileRowHeight)
    {
      this->RowHeightVal.push_back(uniformTileRowHeight);
      remainingHeightInCtbsY -= uniformTileRowHeight;
    }
    if (remainingHeightInCtbsY > 0)
      this->RowHeightVal.push_back(remainingHeightInCtbsY);
    this->NumTileRows = unsigned(this->RowHeightVal.size());
  }
  this->NumTilesInPic = this->NumTileColumns * this->NumTileRows;
}

void pic_parameter_set_rbsp::calculateTilesInSlices(std::shared_ptr<seq_parameter_set_rbsp> sps)
{
  // 6.5.1 (16)
  this->TileColBdVal.push_back(0);
  for (unsigned i = 0; i < this->NumTileColumns; i++)
    this->TileColBdVal.push_back(this->TileColBdVal[i] + this->ColWidthVal[i]);

  this->TileRowBdVal.push_back(0);
  for (unsigned j = 0; j < this->NumTileRows; j++)
    this->TileRowBdVal.push_back(this->TileRowBdVal[j] + this->RowHeightVal[j]);

  // 6.5.1 (18)
  {
    auto tileX = 0;
    for (unsigned ctbAddrX = 0; ctbAddrX <= this->PicWidthInCtbsY; ctbAddrX++)
    {
      if (ctbAddrX == this->TileColBdVal[tileX + 1])
        tileX++;
      this->CtbToTileColBd.push_back(this->TileColBdVal[tileX]);
      this->ctbToTileColIdx.push_back(tileX);
    }
  }

  // 6.5.1 (19)
  {
    auto tileY = 0;
    for (unsigned ctbAddrY = 0; ctbAddrY <= this->PicHeightInCtbsY; ctbAddrY++)
    {
      if (ctbAddrY == this->TileRowBdVal[tileY + 1])
        tileY++;
      this->CtbToTileRowBd.push_back(this->TileRowBdVal[tileY]);
      this->ctbToTileRowIdx.push_back(tileY);
    }
  }

  // 6.5.1 (20)
  for (unsigned i = 0; i <= sps->sps_num_subpics_minus1; i++)
  {
    auto leftX  = sps->sps_subpic_ctu_top_left_x[i];
    auto rightX = leftX + sps->sps_subpic_width_minus1[i];
    this->SubpicWidthInTiles.push_back(ctbToTileColIdx[rightX] + 1 - ctbToTileColIdx[leftX]);
    auto topY    = sps->sps_subpic_ctu_top_left_y[i];
    auto bottomY = topY + sps->sps_subpic_height_minus1[i];
    this->SubpicHeightInTiles.push_back(ctbToTileRowIdx[bottomY] + 1 - ctbToTileRowIdx[topY]);
    if (this->SubpicHeightInTiles[i] == 1 &&
        sps->sps_subpic_height_minus1[i] + 1 < this->RowHeightVal[ctbToTileRowIdx[topY]])
      subpicHeightLessThanOneTileFlag.push_back(true);
    else
      subpicHeightLessThanOneTileFlag.push_back(false);
  }

  // 6.5.1 (22)
  auto AddCtbsToSlice =
      [this](unsigned sliceIdx, unsigned startX, unsigned stopX, unsigned startY, unsigned stopY) {
        for (auto ctbY = startY; ctbY < stopY; ctbY++)
        {
          for (auto ctbX = startX; ctbX < stopX; ctbX++)
          {
            this->CtbAddrInSlice[sliceIdx][this->NumCtusInSlice[sliceIdx]] =
                ctbY * this->PicWidthInCtbsY + ctbX;
            this->NumCtusInSlice[sliceIdx]++;
          }
        }
      };

  // 6.5.1 (21)
  if (this->pps_single_slice_per_subpic_flag)
  {
    if (!sps->sps_subpic_info_present_flag)
    {
      /* There is no subpicture info and only one slice in a
                                                       picture. */
      for (unsigned j = 0; j < this->NumTileRows; j++)
      {
        for (unsigned i = 0; i < this->NumTileColumns; i++)
        {
          AddCtbsToSlice(0,
                         this->TileColBdVal[i],
                         this->TileColBdVal[i + 1],
                         this->TileRowBdVal[j],
                         this->TileRowBdVal[j + 1]);
        }
      }
    }
    else
    {
      for (unsigned i = 0; i <= sps->sps_num_subpics_minus1; i++)
      {
        if (this->subpicHeightLessThanOneTileFlag[i])
        {
          // The slice consists of a set of CTU rows in a tile.
          AddCtbsToSlice(i,
                         sps->sps_subpic_ctu_top_left_x[i],
                         sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1,
                         sps->sps_subpic_ctu_top_left_y[i],
                         sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1);
        }
        else
        {
          // The slice consists of a number of complete tiles covering a rectangular region.
          auto tileX = this->ctbToTileColIdx[sps->sps_subpic_ctu_top_left_x[i]];
          auto tileY = this->ctbToTileRowIdx[sps->sps_subpic_ctu_top_left_y[i]];
          for (unsigned j = 0; j < this->SubpicHeightInTiles[i]; j++)
          {
            for (unsigned k = 0; k < this->SubpicWidthInTiles[i]; k++)
              AddCtbsToSlice(i,
                             this->TileColBdVal[tileX + k],
                             this->TileColBdVal[tileX + k + 1],
                             this->TileRowBdVal[tileY + j],
                             this->TileRowBdVal[tileY + j + 1]);
          }
        }
      }
    }
  }
  else
  {
    auto tileIdx = 0;
    for (unsigned i = 0; i <= this->pps_num_slices_in_pic_minus1; i++)
    {
      SliceTopLeftTileIdx.push_back(tileIdx);
      auto tileX = tileIdx % NumTileColumns;
      auto tileY = tileIdx / NumTileColumns;
      if (i < this->pps_num_slices_in_pic_minus1)
      {
        this->sliceWidthInTiles[i]  = this->pps_slice_width_in_tiles_minus1[i] + 1;
        this->sliceHeightInTiles[i] = this->pps_slice_height_in_tiles_minus1[i] + 1;
      }
      else
      {
        this->sliceWidthInTiles[i]  = this->NumTileColumns - tileX;
        this->sliceHeightInTiles[i] = this->NumTileRows - tileY;
        this->NumSlicesInTile[i]    = 1;
      }
      if (sliceWidthInTiles[i] == 1 && sliceHeightInTiles[i] == 1)
      {
        if (this->pps_num_exp_slices_in_tile[i] == 0)
        {
          this->NumSlicesInTile[i] = 1;
          this->sliceHeightInCtus[i] =
              this->RowHeightVal[this->SliceTopLeftTileIdx[i] / this->NumTileColumns];
        }
        else
        {
          auto remainingHeightInCtbsY =
              this->RowHeightVal[this->SliceTopLeftTileIdx[i] / this->NumTileColumns];
          unsigned j = 0;
          for (; j < this->pps_num_exp_slices_in_tile[i]; j++)
          {
            this->sliceHeightInCtus[i + j] = this->pps_exp_slice_height_in_ctus_minus1[i][j] + 1;
            remainingHeightInCtbsY -= this->sliceHeightInCtus[i + j];
          }
          auto uniformSliceHeight = sliceHeightInCtus[i + j - 1];
          while (remainingHeightInCtbsY >= uniformSliceHeight)
          {
            this->sliceHeightInCtus[i + j] = uniformSliceHeight;
            remainingHeightInCtbsY -= uniformSliceHeight;
            j++;
          }
          if (remainingHeightInCtbsY > 0)
          {
            this->sliceHeightInCtus[i + j] = remainingHeightInCtbsY;
            j++;
          }
          this->NumSlicesInTile[i] = j;
        }
        auto ctbY = this->TileRowBdVal[tileY];
        for (unsigned j = 0; j < this->NumSlicesInTile[i]; j++)
        {
          AddCtbsToSlice(i + j,
                         this->TileColBdVal[tileX],
                         this->TileColBdVal[tileX + 1],
                         ctbY,
                         ctbY + this->sliceHeightInCtus[i + j]);
          ctbY += this->sliceHeightInCtus[i + j];
          this->sliceWidthInTiles[i + j]  = 1;
          this->sliceHeightInTiles[i + j] = 1;
        }
        i += this->NumSlicesInTile[i] - 1;
      }
      else
      {
        for (unsigned j = 0; j < this->sliceHeightInTiles[i]; j++)
        {
          for (unsigned k = 0; k < this->sliceWidthInTiles[i]; k++)
            AddCtbsToSlice(i,
                           this->TileColBdVal[tileX + k],
                           this->TileColBdVal[tileX + k + 1],
                           this->TileRowBdVal[tileY + j],
                           this->TileRowBdVal[tileY + j + 1]);
        }
      }
      if (i < this->pps_num_slices_in_pic_minus1)
      {
        if (this->pps_tile_idx_delta_present_flag)
          tileIdx += this->pps_tile_idx_delta_val[i];
        else
        {
          tileIdx += this->sliceWidthInTiles[i];
          if (tileIdx % this->NumTileColumns == 0)
            tileIdx += (this->sliceHeightInTiles[i] - 1) * this->NumTileColumns;
        }
      }
    }
  }

  // 6.5.1 (23)
  for (unsigned i = 0; i <= sps->sps_num_subpics_minus1; i++)
  {
    this->NumSlicesInSubpic.push_back(0);
    for (unsigned j = 0; j <= this->pps_num_slices_in_pic_minus1; j++)
    {
      auto posX = CtbAddrInSlice[j][0] % PicWidthInCtbsY;
      auto posY = CtbAddrInSlice[j][0] / PicWidthInCtbsY;
      if ((posX >= sps->sps_subpic_ctu_top_left_x[i]) &&
          (posX < sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1) &&
          (posY >= sps->sps_subpic_ctu_top_left_y[i]) &&
          (posY < sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1))
      {
        this->SubpicIdxForSlice[j]   = i;
        this->SubpicLevelSliceIdx[j] = NumSlicesInSubpic[i];
        this->NumSlicesInSubpic[i]++;
      }
    }
  }
}

} // namespace parser::vvc
