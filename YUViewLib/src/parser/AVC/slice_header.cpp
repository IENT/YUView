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

#include "slice_header.h"

#include <common/Typedef.h>

#include "dec_ref_pic_marking.h"
#include "pic_parameter_set_rbsp.h"
#include "pred_weight_table.h"
#include "ref_pic_list_modification.h"
#include "ref_pic_list_mvc_modification.h"
#include "seq_parameter_set_rbsp.h"

#include <cmath>

#define PARSER_AVC_SLICEHEADER_DEBUG_OUTPUT 0
#if PARSER_AVC_SLICEHEADER_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_AVC(msg) qDebug() << msg
#else
#define DEBUG_AVC(fmt) ((void)0)
#endif

namespace parser::avc
{

namespace
{

parser::CodingEnum<SliceType>
    sliceTypeCoding({{0, SliceType::SLICE_P, "P (P slice)"},
                     {1, SliceType::SLICE_B, "B (B slice)"},
                     {2, SliceType::SLICE_I, "I (I slice)"},
                     {3, SliceType::SLICE_SP, "SP (SP slice)"},
                     {4, SliceType::SLICE_SI, "SI (SI slice)"},
                     {5, SliceType::SLICE_P, "P (P slice) all slices"},
                     {6, SliceType::SLICE_B, "B (B slice) all slices"},
                     {7, SliceType::SLICE_I, "I (I slice) all slices"},
                     {8, SliceType::SLICE_SP, "SP (SP slice) all slices"},
                     {9, SliceType::SLICE_SI, "SI (SI slice) all slices"}},
                    SliceType::SLICE_P);

}

std::string to_string(SliceType type) { return sliceTypeCoding.getMeaning(type); }

using namespace reader;

slice_header::slice_header() {}
slice_header::~slice_header() {}

void slice_header::parse(reader::SubByteReaderLogging &reader,
                         SPSMap &                      spsMap,
                         PPSMap &                      ppsMap,
                         NalType                       nal_unit_type,
                         unsigned                      nal_ref_idc,
                         std::shared_ptr<slice_header> prev_pic)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_header()");

  this->first_mb_in_slice = reader.readUEV("first_mb_in_slice");
  this->slice_type_id =
      reader.readUEV("slice_type_id", Options().withMeaningMap(sliceTypeCoding.getMeaningMap()));
  this->slice_type       = sliceTypeCoding.getValue(this->slice_type_id % 5);
  this->slice_type_fixed = this->slice_type_id > 4;
  this->IdrPicFlag       = (nal_unit_type == NalType::CODED_SLICE_IDR);

  this->pic_parameter_set_id = reader.readUEV("pic_parameter_set_id");

  if (ppsMap.count(this->pic_parameter_set_id) == 0)
    throw std::logic_error("The signaled PPS was not found in the bitstream.");
  auto refPPS = ppsMap.at(this->pic_parameter_set_id);

  if (spsMap.count(refPPS->seq_parameter_set_id) == 0)
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  auto refSPS = spsMap.at(refPPS->seq_parameter_set_id);

  if (refSPS->seqParameterSetData.separate_colour_plane_flag)
  {
    this->colour_plane_id = reader.readBits("colour_plane_id", 2);
  }
  int nrBits      = refSPS->seqParameterSetData.log2_max_frame_num_minus4 + 4;
  this->frame_num = reader.readBits("frame_num", nrBits);
  if (!refSPS->seqParameterSetData.frame_mbs_only_flag)
  {
    this->field_pic_flag = reader.readFlag("field_pic_flag");
    if (this->field_pic_flag)
    {
      this->bottom_field_flag = reader.readFlag("bottom_field_flag");
      // Update
      refSPS->seqParameterSetData.MbaffFrameFlag =
          (refSPS->seqParameterSetData.mb_adaptive_frame_field_flag && !this->field_pic_flag);
      refSPS->seqParameterSetData.PicHeightInMbs = refSPS->seqParameterSetData.FrameHeightInMbs / 2;
      refSPS->seqParameterSetData.PicSizeInMbs =
          refSPS->seqParameterSetData.PicWidthInMbs * refSPS->seqParameterSetData.PicHeightInMbs;
    }
  }

  // Since the MbaffFrameFlag flag is now finally known, we can check the range of first_mb_in_slice
  if (refSPS->seqParameterSetData.MbaffFrameFlag == 0)
  {
    this->firstMacroblockAddressInSlice = this->first_mb_in_slice;
    if (this->first_mb_in_slice > refSPS->seqParameterSetData.PicSizeInMbs - 1)
      throw std::logic_error(
          "first_mb_in_slice shall be in the range of 0 to PicSizeInMbs - 1, inclusive");
  }
  else
  {
    this->firstMacroblockAddressInSlice = this->first_mb_in_slice * 2;
    if (this->first_mb_in_slice > refSPS->seqParameterSetData.PicSizeInMbs / 2 - 1)
      throw std::logic_error(
          "first_mb_in_slice shall be in the range of 0 to PicSizeInMbs / 2 - 1, inclusive.");
  }
  reader.logCalculatedValue("firstMacroblockAddressInSlice", this->firstMacroblockAddressInSlice);

  if (this->IdrPicFlag)
  {
    this->idr_pic_id = reader.readUEV("idr_pic_id", Options().withCheckRange({0, 65535}));
  }
  if (refSPS->seqParameterSetData.pic_order_cnt_type == 0)
  {
    int nrBits              = refSPS->seqParameterSetData.log2_max_pic_order_cnt_lsb_minus4 + 4;
    this->pic_order_cnt_lsb = reader.readBits(
        "pic_order_cnt_lsb",
        nrBits,
        Options().withCheckRange({0, refSPS->seqParameterSetData.MaxPicOrderCntLsb - 1}));
    if (refPPS->bottom_field_pic_order_in_frame_present_flag && !this->field_pic_flag)
      this->delta_pic_order_cnt_bottom = reader.readSEV("delta_pic_order_cnt_bottom");
  }
  if (refSPS->seqParameterSetData.pic_order_cnt_type == 1 &&
      !refSPS->seqParameterSetData.delta_pic_order_always_zero_flag)
  {
    this->delta_pic_order_cnt[0] = reader.readSEV("delta_pic_order_cnt[0]");
    if (refPPS->bottom_field_pic_order_in_frame_present_flag && !this->field_pic_flag)
      this->delta_pic_order_cnt[1] = reader.readSEV("delta_pic_order_cnt[1]");
  }
  if (refPPS->redundant_pic_cnt_present_flag)
    this->redundant_pic_cnt = reader.readUEV("redundant_pic_cnt");
  if (slice_type == SliceType::SLICE_B)
    this->direct_spatial_mv_pred_flag = reader.readFlag("direct_spatial_mv_pred_flag");
  if (slice_type == SliceType::SLICE_P || slice_type == SliceType::SLICE_SP ||
      slice_type == SliceType::SLICE_B)
  {
    this->num_ref_idx_active_override_flag = reader.readFlag("num_ref_idx_active_override_flag");
    if (this->num_ref_idx_active_override_flag)
    {
      this->num_ref_idx_l0_active_minus1 = reader.readUEV("num_ref_idx_l0_active_minus1");
      if (this->slice_type == SliceType::SLICE_B)
        this->num_ref_idx_l1_active_minus1 = reader.readUEV("num_ref_idx_l1_active_minus1");
    }
    else
    {
      this->num_ref_idx_l0_active_minus1 = refPPS->num_ref_idx_l0_default_active_minus1;
      if (this->slice_type == SliceType::SLICE_B)
        this->num_ref_idx_l1_active_minus1 = refPPS->num_ref_idx_l1_default_active_minus1;
    }
  }

  if (nal_unit_type == NalType::CODED_SLICE_EXTENSION ||
      nal_unit_type == NalType::CODED_SLICE_EXTENSION_DEPTH_MAP)
  {
    this->refPicListMvcModification = std::make_unique<ref_pic_list_mvc_modification>();
    this->refPicListMvcModification->parse(reader, this->slice_type);
  }
  else
  {
    this->refPicListModification = std::make_unique<ref_pic_list_modification>();
    this->refPicListModification->parse(reader, this->slice_type);
  }

  if ((refPPS->weighted_pred_flag &&
       (this->slice_type == SliceType::SLICE_P || this->slice_type == SliceType::SLICE_SP)) ||
      (refPPS->weighted_bipred_idc == 1 && this->slice_type == SliceType::SLICE_B))
  {
    this->predWeightTable = std::make_unique<pred_weight_table>();
    this->predWeightTable->parse(reader,
                                 this->slice_type,
                                 refSPS->seqParameterSetData.ChromaArrayType,
                                 num_ref_idx_l0_active_minus1,
                                 num_ref_idx_l1_active_minus1);
  }

  if (nal_ref_idc != 0)
  {
    this->decRefPicMarking = std::make_unique<dec_ref_pic_marking>();
    this->decRefPicMarking->parse(reader, this->IdrPicFlag);
  }

  if (refPPS->entropy_coding_mode_flag && this->slice_type != SliceType::SLICE_I &&
      this->slice_type != SliceType::SLICE_SI)
  {
    this->cabac_init_idc = reader.readUEV("cabac_init_idc", Options().withCheckRange({0, 2}));
  }
  this->slice_qp_delta = reader.readSEV("slice_qp_delta");
  if (this->slice_type == SliceType::SLICE_SP || this->slice_type == SliceType::SLICE_SI)
  {
    if (this->slice_type == SliceType::SLICE_SP)
      this->sp_for_switch_flag = reader.readFlag("sp_for_switch_flag");
    this->slice_qs_delta = reader.readSEV("slice_qs_delta");
  }
  if (refPPS->deblocking_filter_control_present_flag)
  {
    this->disable_deblocking_filter_idc = reader.readUEV("disable_deblocking_filter_idc");
    if (this->disable_deblocking_filter_idc != 1)
    {
      this->slice_alpha_c0_offset_div2 = reader.readSEV("slice_alpha_c0_offset_div2");
      this->slice_beta_offset_div2     = reader.readSEV("slice_beta_offset_div2");
    }
  }
  if (refPPS->num_slice_groups_minus1 > 0 && refPPS->slice_group_map_type >= 3 &&
      refPPS->slice_group_map_type <= 5)
  {
    auto nrBits                    = std::ceil(std::log2(
        refSPS->seqParameterSetData.PicSizeInMapUnits % refPPS->SliceGroupChangeRate + 1));
    this->slice_group_change_cycle = reader.readBits("slice_group_change_cycle", nrBits);
  }

  // Calculate the POC
  auto memoryManagement5InPrevPic =
      (prev_pic && prev_pic->decRefPicMarking &&
       std::find(prev_pic->decRefPicMarking->memory_management_control_operation_list.begin(),
                 prev_pic->decRefPicMarking->memory_management_control_operation_list.end(),
                 5) != prev_pic->decRefPicMarking->memory_management_control_operation_list.end());
  if (refSPS->seqParameterSetData.pic_order_cnt_type == 0)
  {
    // 8.2.1.1 Decoding process for picture order count type 0
    // In: PicOrderCntMsb (of previous pic)
    // Out: TopFieldOrderCnt, BottomFieldOrderCnt
    if (this->IdrPicFlag || (this->slice_type == SliceType::SLICE_I && !prev_pic))
    {
      this->prevPicOrderCntMsb = 0;
      this->prevPicOrderCntLsb = 0;
    }
    else
    {
      if (!prev_pic)
      {
        reader.logArbitrary("This is not an IDR picture (IdrPicFlag not set) and not an I frame "
                            "but there is no previous picture available. Can not calculate POC.");
        return;
      }

      if (this->first_mb_in_slice == 0)
      {
        // If the previous reference picture in decoding order included a
        // memory_management_control_operation equal to 5, the following applies:
        if (memoryManagement5InPrevPic)
        {
          if (!prev_pic->bottom_field_flag)
          {
            this->prevPicOrderCntMsb = 0;
            this->prevPicOrderCntLsb = prev_pic->TopFieldOrderCnt;
          }
          else
          {
            this->prevPicOrderCntMsb = 0;
            this->prevPicOrderCntLsb = 0;
          }
        }
        else
        {
          this->prevPicOrderCntMsb = prev_pic->PicOrderCntMsb;
          this->prevPicOrderCntLsb = prev_pic->pic_order_cnt_lsb;
        }
      }
      else
      {
        // Just copy the values from the previous slice
        this->prevPicOrderCntMsb = prev_pic->prevPicOrderCntMsb;
        this->prevPicOrderCntLsb = prev_pic->prevPicOrderCntLsb;
      }
    }

    if (((int)this->pic_order_cnt_lsb < this->prevPicOrderCntLsb) &&
        ((this->prevPicOrderCntLsb - this->pic_order_cnt_lsb) >=
         (refSPS->seqParameterSetData.MaxPicOrderCntLsb / 2)))
      this->PicOrderCntMsb =
          this->prevPicOrderCntMsb + refSPS->seqParameterSetData.MaxPicOrderCntLsb;
    else if (((int)pic_order_cnt_lsb > prevPicOrderCntLsb) &&
             ((this->pic_order_cnt_lsb - this->prevPicOrderCntLsb) >
              (refSPS->seqParameterSetData.MaxPicOrderCntLsb / 2)))
      this->PicOrderCntMsb =
          this->prevPicOrderCntMsb - refSPS->seqParameterSetData.MaxPicOrderCntLsb;
    else
      this->PicOrderCntMsb = this->prevPicOrderCntMsb;

    if (!this->bottom_field_flag)
      this->TopFieldOrderCnt = this->PicOrderCntMsb + this->pic_order_cnt_lsb;
    else
    {
      if (!this->field_pic_flag)
        this->BottomFieldOrderCnt = this->TopFieldOrderCnt + this->delta_pic_order_cnt_bottom;
      else
        this->BottomFieldOrderCnt = this->PicOrderCntMsb + this->pic_order_cnt_lsb;
    }
  }
  else
  {
    if (!this->IdrPicFlag && this->slice_type != SliceType::SLICE_I && !prev_pic)
    {
      reader.logArbitrary("This is not an IDR picture (IdrPicFlag not set) or an I frame but there "
                          "is no previous picture available. Can not calculate POC.");
      return;
    }

    int prevFrameNum = -1;
    if (prev_pic)
      prevFrameNum = prev_pic->frame_num;

    int prevFrameNumOffset = 0;
    if (!this->IdrPicFlag)
    {
      // If the previous reference picture in decoding order included a
      // memory_management_control_operation equal to 5, the following applies:
      if (memoryManagement5InPrevPic)
        prevFrameNumOffset = 0;
      else
        prevFrameNumOffset = prev_pic->FrameNumOffset;
    }

    // (8-6), (8-11)
    if (IdrPicFlag)
      this->FrameNumOffset = 0;
    else if (prevFrameNum > (int)frame_num)
      this->FrameNumOffset = prevFrameNumOffset + refSPS->seqParameterSetData.MaxFrameNum;
    else
      this->FrameNumOffset = prevFrameNumOffset;

    if (refSPS->seqParameterSetData.pic_order_cnt_type == 1)
    {
      // 8.2.1.2 Decoding process for picture order count type 1
      // in : FrameNumOffset (of previous pic)
      // out: TopFieldOrderCnt, BottomFieldOrderCnt

      // (8-7)
      int absFrameNum;
      if (refSPS->seqParameterSetData.num_ref_frames_in_pic_order_cnt_cycle != 0)
        absFrameNum = FrameNumOffset + frame_num;
      else
        absFrameNum = 0;
      if (nal_ref_idc == 0 && absFrameNum > 0)
        absFrameNum = absFrameNum - 1;
      int expectedPicOrderCnt = 0;
      if (absFrameNum > 0)
      {
        // (8-8)
        int picOrderCntCycleCnt =
            (absFrameNum - 1) / refSPS->seqParameterSetData.num_ref_frames_in_pic_order_cnt_cycle;
        int frameNumInPicOrderCntCycle =
            (absFrameNum - 1) % refSPS->seqParameterSetData.num_ref_frames_in_pic_order_cnt_cycle;
        expectedPicOrderCnt =
            picOrderCntCycleCnt * refSPS->seqParameterSetData.ExpectedDeltaPerPicOrderCntCycle;
        for (int i = 0; i <= frameNumInPicOrderCntCycle; i++)
          expectedPicOrderCnt =
              expectedPicOrderCnt + refSPS->seqParameterSetData.offset_for_ref_frame[i];
      }
      if (nal_ref_idc == 0)
        expectedPicOrderCnt =
            expectedPicOrderCnt + refSPS->seqParameterSetData.offset_for_non_ref_pic;
      // (8-19)
      if (!this->field_pic_flag)
      {
        this->TopFieldOrderCnt    = expectedPicOrderCnt + this->delta_pic_order_cnt[0];
        this->BottomFieldOrderCnt = this->TopFieldOrderCnt +
                                    refSPS->seqParameterSetData.offset_for_top_to_bottom_field +
                                    this->delta_pic_order_cnt[1];
      }
      else if (!this->bottom_field_flag)
        this->TopFieldOrderCnt = expectedPicOrderCnt + this->delta_pic_order_cnt[0];
      else
        this->BottomFieldOrderCnt = expectedPicOrderCnt +
                                    refSPS->seqParameterSetData.offset_for_top_to_bottom_field +
                                    this->delta_pic_order_cnt[0];
    }
    else if (refSPS->seqParameterSetData.pic_order_cnt_type == 2)
    {
      // 8.2.1.3 Decoding process for picture order count type 2
      // out: TopFieldOrderCnt, BottomFieldOrderCnt

      // (8-12)
      int tempPicOrderCnt = 0;
      if (!this->IdrPicFlag)
      {
        if (nal_ref_idc == 0)
          tempPicOrderCnt = 2 * (this->FrameNumOffset + this->frame_num) - 1;
        else
          tempPicOrderCnt = 2 * (this->FrameNumOffset + this->frame_num);
      }
      // (8-13)
      if (!this->field_pic_flag)
      {
        this->TopFieldOrderCnt    = tempPicOrderCnt;
        this->BottomFieldOrderCnt = tempPicOrderCnt;
      }
      else if (this->bottom_field_flag)
        this->BottomFieldOrderCnt = tempPicOrderCnt;
      else
        this->TopFieldOrderCnt = tempPicOrderCnt;
    }
  }

  if (!prev_pic)
  {
    if (slice_type == SliceType::SLICE_I)
    {
      if (this->field_pic_flag && this->bottom_field_flag)
        this->globalPOC = this->BottomFieldOrderCnt;
      else
        this->globalPOC = this->TopFieldOrderCnt;

      this->globalPOC_highestGlobalPOCLastGOP = this->globalPOC;
      this->globalPOC_lastIDR                 = 0;
      DEBUG_AVC("slice_header::parse_slice_header POC - First pic non IDR but I - global POC "
                << this->globalPOC);
    }
    else
    {
      this->globalPOC                         = 0;
      this->globalPOC_lastIDR                 = 0;
      this->globalPOC_highestGlobalPOCLastGOP = 0;
      DEBUG_AVC("slice_header::parse_slice_header POC - First pic - global POC "
                << this->globalPOC);
    }
  }
  else
  {
    if (first_mb_in_slice != 0)
    {
      this->globalPOC                         = prev_pic->globalPOC;
      this->globalPOC_lastIDR                 = prev_pic->globalPOC_lastIDR;
      this->globalPOC_highestGlobalPOCLastGOP = prev_pic->globalPOC_highestGlobalPOCLastGOP;
      DEBUG_AVC("slice_header::parse_slice_header POC - additional slice - global POC "
                << this->globalPOC << " - last IDR " << this->globalPOC_lastIDR << " - highest POC "
                << this->globalPOC_highestGlobalPOCLastGOP);
    }
    else if (IdrPicFlag)
    {
      this->globalPOC                         = prev_pic->globalPOC_highestGlobalPOCLastGOP + 2;
      this->globalPOC_lastIDR                 = this->globalPOC;
      this->globalPOC_highestGlobalPOCLastGOP = this->globalPOC;
      DEBUG_AVC("slice_header::parse_slice_header POC - IDR - global POC "
                << this->globalPOC << " - last IDR " << this->globalPOC_lastIDR << " - highest POC "
                << this->globalPOC_highestGlobalPOCLastGOP);
    }
    else
    {
      if (this->field_pic_flag && this->bottom_field_flag)
        this->globalPOC = prev_pic->globalPOC_lastIDR + BottomFieldOrderCnt;
      else
        this->globalPOC = prev_pic->globalPOC_lastIDR + TopFieldOrderCnt;

      this->globalPOC_highestGlobalPOCLastGOP = prev_pic->globalPOC_highestGlobalPOCLastGOP;
      if (this->globalPOC > this->globalPOC_highestGlobalPOCLastGOP)
        this->globalPOC_highestGlobalPOCLastGOP = this->globalPOC;
      this->globalPOC_lastIDR = prev_pic->globalPOC_lastIDR;
      DEBUG_AVC("slice_header::parse_slice_header POC - first slice - global POC "
                << globalPOC << " - last IDR " << this->globalPOC_lastIDR << " - highest POC "
                << this->globalPOC_highestGlobalPOCLastGOP);
    }
  }
}

} // namespace parser::avc