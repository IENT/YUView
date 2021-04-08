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

#include "../AnnexB.h"
#include "AUDelimiterDetector.h"
#include "SEI/sei_message.h"
#include "commonMaps.h"
#include "nal_unit_header.h"
#include "video/videoHandlerYUV.h"

#include <queue>

using namespace YUV_Internals;

namespace parser
{

namespace hevc
{
class slice_segment_layer_rbsp;
class seq_parameter_set_rbsp;
class buffering_period;
class pic_timing;
} // namespace hevc

// This class knows how to parse the bitrstream of HEVC annexB files
class AnnexBHEVC : public AnnexB
{
  Q_OBJECT

public:
  AnnexBHEVC(QObject *parent = nullptr) : AnnexB(parent)
  {
    curFrameFileStartEndPos = pairUint64(-1, -1);
  }
  ~AnnexBHEVC(){};

  // Get some properties
  double         getFramerate() const override;
  Size           getSequenceSizeSamples() const override;
  YUVPixelFormat getPixelFormat() const override;

  std::optional<SeekData> getSeekData(int iFrameNr) override;
  QByteArray              getExtradata() override;
  IntPair                 getProfileLevel() override;
  Ratio                   getSampleAspectRatio() override;

  ParseResult parseAndAddNALUnit(int                                           nalID,
                                 const ByteVector &                            data,
                                 std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                                 std::optional<pairUint64> nalStartEndPosFile = {},
                                 TreeItem *                parent             = nullptr) override;

protected:
  // ----- Some nested classes that are only used in the scope of this file handler class

  // The PicOrderCntMsb may be reset to zero for IDR frames. In order to count the global POC, we
  // store the maximum POC.
  int maxPOCCount{-1};
  int pocCounterOffset{0};

  bool firstAUInDecodingOrder{true};
  int  prevTid0PicSlicePicOrderCntLsb{};
  int  prevTid0PicPicOrderCntMsb{};

  // Is this NAL skipped because it is RASL (can not be decoded because of slicing/merging of the
  // bitstream)
  bool isRandomAccessSkip;

  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess{INT_MAX};

  struct ActiveParameterSets
  {
    hevc::VPSMap vpsMap;
    hevc::SPSMap spsMap;
    hevc::PPSMap ppsMap;
  };
  ActiveParameterSets activeParameterSets;

  std::vector<std::shared_ptr<hevc::NalUnitHEVC>> nalUnitsForSeeking;

  // We keept a pointer to the last slice with first_slice_segment_in_pic_flag set.
  // All following slices with dependent_slice_segment_flag set need this slice to infer some
  // values.
  std::shared_ptr<hevc::slice_segment_layer_rbsp> lastFirstSliceSegmentInPic;
  // It is allowed that units (like SEI messages) sent before the parameter sets but still refer to
  // the parameter sets. Here we keep a list of seis that need to be parsed after the parameter sets
  // were recieved.
  std::queue<hevc::sei_message> reparse_sei;

  std::shared_ptr<hevc::seq_parameter_set_rbsp> currentAUAssociatedSPS;

  std::shared_ptr<hevc::buffering_period> lastBufferingPeriodSEI;
  std::shared_ptr<hevc::pic_timing>       lastPicTimingSEI;
  std::shared_ptr<hevc::buffering_period> newBufferingPeriodSEI;
  std::shared_ptr<hevc::pic_timing>       newPicTimingSEI;
  bool                                    nextAUIsFirstAUInBufferingPeriod{false};

  // For every frame, we save the file position where the NAL unit of the first slice starts and
  // where the NAL of the last slice ends. This is used by getNextFrameNALUnits to return all
  // information (NAL units) for a specific frame.
  std::optional<pairUint64>
      curFrameFileStartEndPos; //< Save the file start/end position of the current frame (if known)
                               // in case the frame has multiple NAL units
  // The POC of the current frame. We save this we encounter a NAL from the next POC; then we add
  // it.
  int  curFramePOC{-1};
  bool curFrameIsRandomAccess{false};

  hevc::AUDelimiterDetector auDelimiterDetector;

  size_t                              sizeCurrentAU{0};
  int                                 lastFramePOC{-1};
  size_t                              counterAU{0};
  bool                                currentAUAllSlicesIntra{true};
  std::map<std::string, unsigned int> currentAUSliceTypes;
};

} // namespace parser