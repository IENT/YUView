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

#include "slice_segment_header.h"

#include "parser/common/CodingEnum.h"
#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_segment_layer_rbsp.h"
#include <parser/common/Functions.h>

#include <cmath>

namespace parser::hevc
{

namespace
{

static parser::CodingEnum<SliceType> sliceTypeCoding({{0, SliceType::B, "B", "B-Slice"},
                                                      {1, SliceType::P, "P", "P-Slice"},
                                                      {2, SliceType::I, "I", "I-Slice"}},
                                                     SliceType::B);

}

using namespace reader;

std::string to_string(SliceType sliceType)
{
  return sliceTypeCoding.getMeaning(sliceType);
}

void slice_segment_header::parse(SubByteReaderLogging & reader,
                                 bool                   firstAUInDecodingOrder,
                                 int                    prevTid0PicSlicePicOrderCntLsb,
                                 int                    prevTid0PicPicOrderCntMsb,
                                 const nal_unit_header &nalUnitHeader,
                                 SPSMap &               spsMap,
                                 PPSMap &               ppsMap,
                                 std::shared_ptr<slice_segment_layer_rbsp> firstSliceInSegment)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_segment_header()");

  this->first_slice_segment_in_pic_flag = reader.readFlag("first_slice_segment_in_pic_flag");

  if (nalUnitHeader.isIRAP())
    this->no_output_of_prior_pics_flag = reader.readFlag("no_output_of_prior_pics_flag");

  this->slice_pic_parameter_set_id =
      reader.readUEV("slice_pic_parameter_set_id", Options().withCheckRange({0, 63}));

  if (ppsMap.count(this->slice_pic_parameter_set_id) == 0)
    throw std::logic_error("PPS with given slice_pic_parameter_set_id not found.");
  auto pps = ppsMap[this->slice_pic_parameter_set_id];

  if (spsMap.count(pps->pps_seq_parameter_set_id) == 0)
    throw std::logic_error("SPS with given pps_seq_parameter_set_id not found.");
  auto sps = spsMap[pps->pps_seq_parameter_set_id];

  if (!this->first_slice_segment_in_pic_flag)
  {
    if (pps->dependent_slice_segments_enabled_flag)
      this->dependent_slice_segment_flag = reader.readFlag("dependent_slice_segment_flag");
    auto nrBits                 = std::ceil(std::log2(sps->PicSizeInCtbsY)); // 7.4.7.1
    this->slice_segment_address = reader.readBits("slice_segment_address", nrBits);
  }

  if (!dependent_slice_segment_flag)
  {
    unsigned i = 0;
    if (pps->num_extra_slice_header_bits > i)
    {
      i++;
      this->discardable_flag = reader.readFlag("discardable_flag");
    }
    if (pps->num_extra_slice_header_bits > i)
    {
      i++;
      this->cross_layer_bla_flag = reader.readFlag("cross_layer_bla_flag");
    }
    for (; i < pps->num_extra_slice_header_bits; i++)
      this->slice_reserved_flag.push_back(reader.readFlag(formatArray("slice_reserved_flag", i)));

    auto sliceTypeIdx = reader.readUEV(
        "slice_type",
        Options().withMeaningMap(sliceTypeCoding.getMeaningMap()).withCheckRange({0, 2}));
    this->slice_type = sliceTypeCoding.getValue(sliceTypeIdx);
    if (pps->output_flag_present_flag)
      this->pic_output_flag = reader.readFlag("pic_output_flag");

    if (sps->separate_colour_plane_flag)
      this->colour_plane_id = reader.readBits("colour_plane_id", 2);

    if (nalUnitHeader.nal_unit_type != NalType::IDR_W_RADL &&
        nalUnitHeader.nal_unit_type != NalType::IDR_N_LP)
    {
      this->slice_pic_order_cnt_lsb =
          reader.readBits("slice_pic_order_cnt_lsb",
                          sps->log2_max_pic_order_cnt_lsb_minus4 + 4); // Max 16 bits read
      this->short_term_ref_pic_set_sps_flag = reader.readFlag("short_term_ref_pic_set_sps_flag");

      if (!this->short_term_ref_pic_set_sps_flag)
      {
        this->stRefPicSet.parse(
            reader, sps->num_short_term_ref_pic_sets, sps->num_short_term_ref_pic_sets);
      }
      else if (sps->num_short_term_ref_pic_sets > 1)
      {
        auto nrBits                      = std::ceil(std::log2(sps->num_short_term_ref_pic_sets));
        this->short_term_ref_pic_set_idx = reader.readBits("short_term_ref_pic_set_idx", nrBits);

        // The short term ref pic set is the one with the given index from the SPS
        if (this->short_term_ref_pic_set_idx >= sps->stRefPicSets.size())
          throw std::logic_error(
              "Error parsing slice header. The specified short term ref pic list could not be "
              "found in the SPS.");
        this->stRefPicSet = sps->stRefPicSets[this->short_term_ref_pic_set_idx];
      }
      if (sps->long_term_ref_pics_present_flag)
      {
        if (sps->num_long_term_ref_pics_sps > 0)
          this->num_long_term_sps = reader.readUEV("num_long_term_sps");
        this->num_long_term_pics = reader.readUEV("num_long_term_pics");
        for (unsigned i = 0; i < this->num_long_term_sps + this->num_long_term_pics; i++)
        {
          if (i < this->num_long_term_sps)
          {
            if (sps->num_long_term_ref_pics_sps > 1)
            {
              auto nrBits         = std::ceil(std::log2(sps->num_long_term_ref_pics_sps));
              this->lt_idx_sps[i] = reader.readBits(formatArray("lt_idx_sps", i), nrBits);
            }

            this->UsedByCurrPicLt.push_back(
                sps->used_by_curr_pic_lt_sps_flag.at(this->lt_idx_sps[i]));
          }
          else
          {
            auto nrBits         = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
            this->poc_lsb_lt[i] = reader.readBits(formatArray("poc_lsb_lt", i), nrBits);
            this->used_by_curr_pic_lt_flag[i] =
                reader.readFlag(formatArray("used_by_curr_pic_lt_flag", i));

            this->UsedByCurrPicLt.push_back(this->used_by_curr_pic_lt_flag[i]);
          }

          this->delta_poc_msb_present_flag[i] =
              reader.readFlag(formatArray("delta_poc_msb_present_flag", i));
          if (delta_poc_msb_present_flag[i])
            this->delta_poc_msb_cycle_lt[i] =
                reader.readUEV(formatArray("delta_poc_msb_cycle_lt", i));
        }
      }
      if (sps->sps_temporal_mvp_enabled_flag)
        this->slice_temporal_mvp_enabled_flag = reader.readFlag("slice_temporal_mvp_enabled_flag");
    }

    // This is not 100% done yet.
    // if (nalUnitHeader.nuh_layer_id > 0 && !default_ref_layers_active_flag &&
    //     NumDirectRefLayers[nuh_layer_id] > 0)
    // {
    // }

    if (sps->sample_adaptive_offset_enabled_flag)
    {
      this->slice_sao_luma_flag = reader.readFlag("slice_sao_luma_flag");
      if (sps->ChromaArrayType != 0)
        this->slice_sao_chroma_flag = reader.readFlag("slice_sao_chroma_flag");
    }

    if (this->slice_type == SliceType::P || this->slice_type == SliceType::B)
    {
      // Infer if not present
      this->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
      this->num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;

      this->num_ref_idx_active_override_flag = reader.readFlag("num_ref_idx_active_override_flag");
      if (this->num_ref_idx_active_override_flag)
      {
        this->num_ref_idx_l0_active_minus1 = reader.readUEV("num_ref_idx_l0_active_minus1");
        if (this->slice_type == SliceType::B)
          this->num_ref_idx_l1_active_minus1 = reader.readUEV("num_ref_idx_l1_active_minus1");
      }

      auto CurrRpsIdx = (this->short_term_ref_pic_set_sps_flag) ? this->short_term_ref_pic_set_idx
                                                                : sps->num_short_term_ref_pic_sets;
      auto NumPicTotalCurr = this->stRefPicSet.NumPicTotalCurr(CurrRpsIdx, this);
      if (pps->lists_modification_present_flag && NumPicTotalCurr > 1)
        this->refPicListsModification.parse(reader, NumPicTotalCurr, this);

      if (this->slice_type == SliceType::B)
        this->mvd_l1_zero_flag = reader.readFlag("mvd_l1_zero_flag");
      if (pps->cabac_init_present_flag)
        this->cabac_init_flag = reader.readFlag("cabac_init_flag");
      if (this->slice_temporal_mvp_enabled_flag)
      {
        if (this->slice_type == SliceType::B)
          this->collocated_from_l0_flag = reader.readFlag("collocated_from_l0_flag");
        if ((this->collocated_from_l0_flag && this->num_ref_idx_l0_active_minus1 > 0) ||
            (!this->collocated_from_l0_flag && this->num_ref_idx_l1_active_minus1 > 0))
          this->collocated_ref_idx = reader.readUEV("collocated_ref_idx");
      }
      if ((pps->weighted_pred_flag && this->slice_type == SliceType::P) ||
          (pps->weighted_bipred_flag && this->slice_type == SliceType::B))
        this->predWeightTable.parse(reader, sps, this);

      this->five_minus_max_num_merge_cand = reader.readUEV("five_minus_max_num_merge_cand");
    }
    this->slice_qp_delta = reader.readSEV("slice_qp_delta");
    if (pps->pps_slice_chroma_qp_offsets_present_flag)
    {
      this->slice_cb_qp_offset = reader.readSEV("slice_cb_qp_offset");
      this->slice_cr_qp_offset = reader.readSEV("slice_cr_qp_offset");
    }
    if (pps->ppsRangeExtension.chroma_qp_offset_list_enabled_flag)
      this->cu_chroma_qp_offset_enabled_flag = reader.readFlag("cu_chroma_qp_offset_enabled_flag");
    if (pps->deblocking_filter_override_enabled_flag)
      this->deblocking_filter_override_flag = reader.readFlag("deblocking_filter_override_flag");
    if (this->deblocking_filter_override_flag)
    {
      this->slice_deblocking_filter_disabled_flag =
          reader.readFlag("slice_deblocking_filter_disabled_flag");
      if (!this->slice_deblocking_filter_disabled_flag)
      {
        this->slice_beta_offset_div2 = reader.readSEV("slice_beta_offset_div2");
        this->slice_tc_offset_div2   = reader.readSEV("slice_tc_offset_div2");
      }
    }
    else
      this->slice_deblocking_filter_disabled_flag = pps->pps_deblocking_filter_disabled_flag;

    if (pps->pps_loop_filter_across_slices_enabled_flag &&
        (this->slice_sao_luma_flag || this->slice_sao_chroma_flag ||
         !this->slice_deblocking_filter_disabled_flag))
      this->slice_loop_filter_across_slices_enabled_flag =
          reader.readFlag("slice_loop_filter_across_slices_enabled_flag");
  }
  else // dependent_slice_segment_flag is true -- inferr values from firstSliceInSegment
  {
    if (!firstSliceInSegment)
      throw std::logic_error("Dependent slice without a first slice in the segment.");

    slice_pic_order_cnt_lsb = firstSliceInSegment->sliceSegmentHeader.slice_pic_order_cnt_lsb;
  }

  if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag)
  {
    this->num_entry_point_offsets = reader.readUEV("num_entry_point_offsets");
    if (this->num_entry_point_offsets > 0)
    {
      this->offset_len_minus1 =
          reader.readUEV("offset_len_minus1", Options().withCheckRange({0, 31}));

      for (unsigned i = 0; i < this->num_entry_point_offsets; i++)
      {
        auto nrBits = offset_len_minus1 + 1;
        this->entry_point_offset_minus1.push_back(
            reader.readBits(formatArray("entry_point_offset_minus1", i), nrBits));
      }
    }
  }

  if (pps->slice_segment_header_extension_present_flag)
  {
    this->slice_segment_header_extension_length =
        reader.readUEV("slice_segment_header_extension_length");
    for (unsigned i = 0; i < this->slice_segment_header_extension_length; i++)
      this->slice_segment_header_extension_data_byte.push_back(
          reader.readBits(formatArray("slice_segment_header_extension_data_byte", i), 8));
  }

  // End of the slice header - byte_alignment()

  // Calculate the picture order count
  auto MaxPicOrderCntLsb = 1u << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  reader.logCalculatedValue("MaxPicOrderCntLsb", MaxPicOrderCntLsb);

  // If the current picture is an IDR picture, a BLA picture, the first picture in the bitstream in
  // decoding order, or the first picture that follows an end of sequence NAL unit in decoding
  // order, the variable NoRaslOutputFlag is set equal to 1.
  this->NoRaslOutputFlag = false;
  if (nalUnitHeader.nal_unit_type == NalType::IDR_W_RADL ||
      nalUnitHeader.nal_unit_type == NalType::IDR_N_LP ||
      nalUnitHeader.nal_unit_type == NalType::BLA_W_LP)
    this->NoRaslOutputFlag = true;
  else if (firstAUInDecodingOrder)
  {
    this->NoRaslOutputFlag = true;
  }

  // T-REC-H.265-201410 - 8.3.1 Decoding process for picture order count

  int prevPicOrderCntLsb = 0;
  int prevPicOrderCntMsb = 0;
  // When the current picture is not an IRAP picture with NoRaslOutputFlag equal to 1, ...
  if (!(nalUnitHeader.isIRAP() && this->NoRaslOutputFlag))
  {
    // the variables prevPicOrderCntLsb and prevPicOrderCntMsb are derived as follows:
    prevPicOrderCntLsb = prevTid0PicSlicePicOrderCntLsb;
    prevPicOrderCntMsb = prevTid0PicPicOrderCntMsb;
  }
  reader.logCalculatedValue("prevPicOrderCntLsb", prevPicOrderCntLsb);
  reader.logCalculatedValue("prevPicOrderCntMsb", prevPicOrderCntMsb);

  // The variable PicOrderCntMsb of the current picture is derived as follows:
  if (nalUnitHeader.isIRAP() && this->NoRaslOutputFlag)
  {
    // If the current picture is an IRAP picture with NoRaslOutputFlag equal to 1, PicOrderCntMsb is
    // set equal to 0.
    PicOrderCntMsb = 0;
  }
  else
  {
    // Otherwise, PicOrderCntMsb is derived as follows: (8-1)
    if (((int)slice_pic_order_cnt_lsb < prevPicOrderCntLsb) &&
        (((int)prevPicOrderCntLsb - (int)slice_pic_order_cnt_lsb) >= ((int)MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if (((int)slice_pic_order_cnt_lsb > prevPicOrderCntLsb) &&
             (((int)slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > ((int)MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else
      PicOrderCntMsb = prevPicOrderCntMsb;
  }
  reader.logCalculatedValue("PicOrderCntMsb", PicOrderCntMsb);

  // PicOrderCntVal is derived as follows: (8-2)
  PicOrderCntVal = PicOrderCntMsb + slice_pic_order_cnt_lsb;
  reader.logCalculatedValue("PicOrderCntVal", PicOrderCntVal);
}

} // namespace parser::hevc