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

#include "../AnnexB.h"
#include "NalUnitVVC.h"
#include "commonMaps.h"
#include "video/videoHandlerYUV.h"

#include <memory>

using namespace YUV_Internals;

namespace parser
{

namespace vvc
{
class slice_layer_rbsp;
class picture_header_structure;
class buffering_period;
} // namespace vvc

// This class knows how to parse the bitrstream of VVC annexB files
class AnnexBVVC : public AnnexB
{
  Q_OBJECT

public:
  AnnexBVVC(QObject *parent = nullptr) : AnnexB(parent){};
  ~AnnexBVVC() = default;

  // Get some properties
  double         getFramerate() const override;
  Size           getSequenceSizeSamples() const override;
  YUVPixelFormat getPixelFormat() const override;

  virtual std::optional<SeekData> getSeekData(int iFrameNr) override;
  QByteArray                      getExtradata() override;
  IntPair                         getProfileLevel() override;
  Ratio                           getSampleAspectRatio() override;

  ParseResult parseAndAddNALUnit(int                                           nalID,
                                 const ByteVector &                            data,
                                 std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                                 std::optional<pairUint64> nalStartEndPosFile = {},
                                 std::shared_ptr<TreeItem> parent             = {}) override;

protected:
  struct ActiveParameterSets
  {
    vvc::VPSMap vpsMap;
    vvc::SPSMap spsMap;
    vvc::PPSMap ppsMap;
    vvc::APSMap apsMap;
  };
  ActiveParameterSets activeParameterSets;

  std::vector<std::shared_ptr<vvc::NalUnitVVC>> nalUnitsForSeeking;

  struct ParsingState
  {
    std::shared_ptr<vvc::picture_header_structure> currentPictureHeaderStructure;
    std::shared_ptr<vvc::slice_layer_rbsp>         currentSlice;
    std::shared_ptr<vvc::buffering_period>         lastBufferingPeriod;

    size_t                    counterAU{};
    size_t                    sizeCurrentAU{};
    int                       lastFramePOC{-1};
    bool                      lastFrameIsKeyframe{};
    std::optional<pairUint64> curFrameFileStartEndPos;
  };
  ParsingState parsingState;

  bool handleNewAU(ParsingState &                                updatedParsingState,
                   AnnexB::ParseResult &                         parseResult,
                   std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                   std::optional<pairUint64>                     nalStartEndPosFile);

  struct auDelimiterDetector_t
  {
    bool     isStartOfNewAU(std::shared_ptr<vvc::NalUnitVVC>               nal,
                            std::shared_ptr<vvc::picture_header_structure> ph);
    bool     lastNalWasVcl{false};
    unsigned lastVcl_PicOrderCntVal{};
    unsigned lastVcl_ph_pic_order_cnt_lsb{};
    unsigned lastVcl_nuh_layer_id;
  };
  auDelimiterDetector_t auDelimiterDetector;
};

} // namespace parser