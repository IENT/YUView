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

#include <QSharedPointer>

#include <parser/ParserAnnexB.h>
#include <video/yuv/videoHandlerYUV.h>

#include "AUDelimiterDetector.h"
#include "HRD.h"
#include "SEI/sei_message.h"
#include "commonMaps.h"

#include <queue>
#include <vector>

namespace parser
{

namespace avc
{
class buffering_period;
class pic_timing;
class slice_header;
class NalUnitAVC;
class seq_parameter_set_rbsp;

struct FrameParsingData
{
  // For every frame (AU), we save the file position where the NAL unit of the first slice starts
  // and where the NAL of the last slice ends (if known). This is used by getNextFrameNALUnits to
  // return all information (NAL units) for a specific frame (AU). This includes SPS/PPS.
  std::optional<pairUint64> fileStartEndPos{};
  std::optional<int>        poc{};
  bool                      isRandomAccess{};
};
} // namespace avc

// This class knows how to parse the bitrstream of HEVC annexB files
class ParserAnnexBAVC : public ParserAnnexB
{
  Q_OBJECT

public:
  ParserAnnexBAVC(QObject *parent = nullptr) : ParserAnnexB(parent){};
  ~ParserAnnexBAVC() = default;

  // Get properties
  double                     getFramerate() const override;
  Size                       getSequenceSizeSamples() const override;
  video::yuv::PixelFormatYUV getPixelFormat() const override;

  ParseResult parseAndAddNALUnit(int                                           nalID,
                                 const ByteVector &                            data,
                                 std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                                 std::optional<pairUint64> nalStartEndPosFile = {},
                                 std::shared_ptr<TreeItem> parent             = nullptr) override;

  std::optional<SeekData> getSeekData(int iFrameNr) override;
  QByteArray              getExtradata() override;
  IntPair                 getProfileLevel() override;
  Ratio                   getSampleAspectRatio() override;

protected:
  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess{INT_MAX};

  struct ActiveParameterSets
  {
    avc::SPSMap spsMap;
    avc::PPSMap ppsMap;
  };
  ActiveParameterSets activeParameterSets;

  // In order to calculate POCs we need the first slice of the last reference picture
  std::shared_ptr<avc::slice_header> last_picture_first_slice;
  // It is allowed that units (like SEI messages) sent before the parameter sets but still refer to
  // the parameter sets. Here we keep a list of seis that need to be parsed after the parameter sets
  // were recieved.
  std::queue<avc::sei_message> reparse_sei;

  // When new SEIs come in and they don't initialize the HRD, they are not accessed until the
  // current AU is processed by the HRD.
  std::shared_ptr<avc::buffering_period>       lastBufferingPeriodSEI;
  std::shared_ptr<avc::pic_timing>             lastPicTimingSEI;
  std::shared_ptr<avc::buffering_period>       newBufferingPeriodSEI;
  std::shared_ptr<avc::pic_timing>             newPicTimingSEI;
  std::shared_ptr<avc::seq_parameter_set_rbsp> currentAUAssociatedSPS;
  std::shared_ptr<avc::seq_parameter_set_rbsp> currentAUPartitionASPS;
  bool                                         nextAUIsFirstAUInBufferingPeriod{false};

  bool CpbDpbDelaysPresentFlag{false};

  std::optional<avc::FrameParsingData> curFrameData;

  std::vector<std::shared_ptr<avc::NalUnitAVC>> nalUnitsForSeeking;

  avc::AUDelimiterDetector auDelimiterDetector;

  size_t                              sizeCurrentAU{0};
  int                                 lastFramePOC{-1};
  int                                 counterAU{0};
  bool                                currentAUAllSlicesIntra{true};
  std::map<std::string, unsigned int> currentAUSliceTypes;

  avc::HRD hrd;
};

} // namespace parser