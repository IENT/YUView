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

#include <parser/ParserAnnexB.h>
#include <video/yuv/videoHandlerYUV.h>

#include "NalUnitVVC.h"
#include "commonMaps.h"

#include <memory>

namespace parser
{

namespace vvc
{

class slice_layer_rbsp;
class picture_header_structure;
class buffering_period;

struct ParsingState
{
  using sharedPictureHeader = std::shared_ptr<vvc::picture_header_structure>;
  sharedPictureHeader currentPictureHeaderStructure;
  using Nuh_Layer_Id = unsigned;
  std::map<Nuh_Layer_Id, sharedPictureHeader> prevTid0Pic;

  std::shared_ptr<vvc::slice_layer_rbsp> currentSlice;
  std::shared_ptr<vvc::buffering_period> lastBufferingPeriod;

  struct CurrentAU
  {
    size_t                    counter{};
    size_t                    sizeBytes{};
    int                       poc{-1};
    bool                      isKeyframe{};
    std::optional<pairUint64> fileStartEndPos;
  };
  CurrentAU currentAU{};

  using LayerID = unsigned;
  std::map<LayerID, bool> NoOutputBeforeRecoveryFlag;
};

} // namespace vvc

// This class knows how to parse the bitrstream of VVC annexB files
class ParserAnnexBVVC : public ParserAnnexB
{
  Q_OBJECT

public:
  ParserAnnexBVVC(QObject *parent = nullptr) : ParserAnnexB(parent){};
  ~ParserAnnexBVVC() = default;

  // Get some properties
  double                     getFramerate() const override;
  Size                       getSequenceSizeSamples() const override;
  video::yuv::PixelFormatYUV getPixelFormat() const override;

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
  // The PicOrderCntMsb may be reset to zero for IDR frames. In order to count the global POC, we
  // store the maximum POC.
  uint64_t maxPOCCount{0};
  uint64_t pocCounterOffset{0};
  int      calculateAndUpdateGlobalPOC(bool isIRAP, unsigned PicOrderCntVal);

  struct ActiveParameterSets
  {
    vvc::VPSMap vpsMap;
    vvc::SPSMap spsMap;
    vvc::PPSMap ppsMap;
    vvc::APSMap apsMap;
  };
  ActiveParameterSets activeParameterSets;

  std::vector<std::shared_ptr<vvc::NalUnitVVC>> nalUnitsForSeeking;

  vvc::ParsingState parsingState;
  bool              handleNewAU(vvc::ParsingState &updatedParsingState);

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