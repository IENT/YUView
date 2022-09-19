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

#include "NalUnitAVC.h"

namespace parser::avc
{

bool AUDelimiterDetector::isStartOfNewAU(std::shared_ptr<NalUnitAVC> nal,
                                         std::optional<int>          curFramePOC)
{
  const auto isSlice = (nal->header.nal_unit_type == NalType::CODED_SLICE_NON_IDR ||
                        nal->header.nal_unit_type == NalType::CODED_SLICE_IDR);

  // 7.4.1.2.3 Order of NAL units and coded pictures and association to access units
  auto isStart = false;
  if (this->primaryCodedPictureInAuEncountered)
  {
    // The first of any of the following NAL units after the last VCL NAL unit of a primary coded
    // picture specifies the start of a new access unit:
    if (nal->header.nal_unit_type == NalType::AUD)
      isStart = true;

    if (nal->header.nal_unit_type == NalType::SPS || nal->header.nal_unit_type == NalType::PPS)
      isStart = true;

    if (nal->header.nal_unit_type == NalType::SEI)
      isStart = true;

    const bool fourteenToEigtheen = (nal->header.nal_unit_type == NalType::PREFIX_NAL ||
                                     nal->header.nal_unit_type == NalType::SUBSET_SPS ||
                                     nal->header.nal_unit_type == NalType::DEPTH_PARAMETER_SET ||
                                     nal->header.nal_unit_type == NalType::RESERVED_17 ||
                                     nal->header.nal_unit_type == NalType::RESERVED_18);
    if (fourteenToEigtheen)
      isStart = true;

    if (isSlice && curFramePOC && curFramePOC.value() != this->lastSlicePoc)
    {
      isStart            = true;
      this->lastSlicePoc = curFramePOC.value();
    }
  }

  if (isSlice)
    this->primaryCodedPictureInAuEncountered = true;

  if (isStart && !isSlice)
    this->primaryCodedPictureInAuEncountered = false;

  return isStart;
}

} // namespace parser::avc
