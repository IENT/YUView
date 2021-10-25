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
#include <parser/HEVC/AnnexBHEVC.h>
#include <parser/common/BitratePlotModel.h>
#include <parser/common/SubByteReaderLogging.h>
#include <parser/common/TreeItem.h>

namespace parser::avformat
{

class HVCCNalUnit
{
public:
  HVCCNalUnit() = default;

  void parse(unsigned                      unitID,
             reader::SubByteReaderLogging &reader,
             AnnexBHEVC *                  hevcParser,
             BitratePlotModel *            bitrateModel);

  unsigned nalUnitLength{};
};

class HVCCNalArray
{
public:
  HVCCNalArray() = default;

  void parse(unsigned                      arrayID,
             reader::SubByteReaderLogging &reader,
             AnnexBHEVC *                  hevcParser,
             BitratePlotModel *            bitrateModel);

  bool                array_completeness{};
  unsigned            nal_unit_type{};
  unsigned            numNalus{};
  vector<HVCCNalUnit> nalList;
};

class HVCC
{
public:
  HVCC() = default;

  void parse(ByteVector &              data,
             std::shared_ptr<TreeItem> root,
             AnnexBHEVC *              hevcParser,
             BitratePlotModel *        bitrateModel);

  unsigned configurationVersion{};
  unsigned general_profile_space{};
  bool     general_tier_flag{};
  unsigned general_profile_idc{};
  unsigned general_profile_compatibility_flags{};
  uint64_t general_constraint_indicator_flags{};
  unsigned general_level_idc{};
  unsigned min_spatial_segmentation_idc{};
  unsigned parallelismType{};
  unsigned chromaFormat{};
  unsigned bitDepthLumaMinus8{};
  unsigned bitDepthChromaMinus8{};
  unsigned avgFrameRate{};
  unsigned constantFrameRate{};
  unsigned numTemporalLayers{};
  bool     temporalIdNested{};
  unsigned lengthSizeMinusOne{};
  unsigned numOfArrays{};

  vector<HVCCNalArray> naluArrays;
};

} // namespace parser::avformat