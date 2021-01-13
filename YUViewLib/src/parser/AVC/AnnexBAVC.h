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

#include "parser/common/ReaderHelper.h"
#include "video/videoHandlerYUV.h"
#include "parser/AnnexB.h"

using namespace YUV_Internals;

namespace parser
{

// This class knows how to parse the bitrstream of HEVC annexB files
class AnnexBAVC : public AnnexB
{
  Q_OBJECT
  
public:
  AnnexBAVC(QObject *parent = nullptr) : AnnexB(parent) { curFrameFileStartEndPos = pairUint64(-1, -1); };
  ~AnnexBAVC() {};

  // Get properties
  double getFramerate() const override;
  QSize getSequenceSizeSamples() const override;
  yuvPixelFormat getPixelFormat() const override;

  ParseResult parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile={}, TreeItem *parent=nullptr) override;

  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) override;
  QByteArray getExtradata() override;
  IntPair getProfileLevel() override;
  Ratio getSampleAspectRatio() override;

protected:

  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess {INT_MAX};

  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, QSharedPointer<sps>> active_SPS_list;
  QMap<int, QSharedPointer<pps>> active_PPS_list;
  // In order to calculate POCs we need the first slice of the last reference picture
  QSharedPointer<slice_header> last_picture_first_slice;
  // It is allowed that units (like SEI messages) sent before the parameter sets but still refer to the 
  // parameter sets. Here we keep a list of seis that need to be parsed after the parameter sets were recieved.
  QList<QSharedPointer<sei>> reparse_sei;

  // When new SEIs come in and they don't initialize the HRD, they are not accessed until the current AU is processed by the HRD.
  QSharedPointer<buffering_period_sei> lastBufferingPeriodSEI;
  QSharedPointer<pic_timing_sei> lastPicTimingSEI;
  QSharedPointer<buffering_period_sei> newBufferingPeriodSEI;
  QSharedPointer<pic_timing_sei> newPicTimingSEI;
  bool nextAUIsFirstAUInBufferingPeriod {false};

  // In an SEI, the number of bytes indicated do not consider the emulation prevention. This function
  // can determine the real number of bytes that we need to read from the input considering the emulation prevention
  int determineRealNumberOfBytesSEIEmulationPrevention(QByteArray &in, int nrBytes);

  bool CpbDpbDelaysPresentFlag {false};

  // For every frame, we save the file position where the NAL unit of the first slice starts and where the NAL of the last slice ends (if known).
  // This is used by getNextFrameNALUnits to return all information (NAL units) for a specific frame.
  std::optional<pairUint64> curFrameFileStartEndPos;  //< Save the file start/end position of the current frame (in case the frame has multiple NAL units)

  // The POC of the current frame. We save this when we encounter a NAL from the next POC; then we add it.
  int curFramePOC {-1};
  bool curFrameIsRandomAccess {false};
  
  struct auDelimiterDetector_t
  {
    bool isStartOfNewAU(nal_unit_avc &nal_avc, int curFramePOC);
    int lastSlicePoc {-1};
    bool primaryCodedPictureInAuEncountered {false};
  };
  auDelimiterDetector_t auDelimiterDetector;

  unsigned int sizeCurrentAU {0};
  int lastFramePOC{-1};
  int counterAU {0};
  bool currentAUAllSlicesIntra {true};
  QMap<QString, unsigned int> currentAUSliceTypes;

  class HRD
  {
  public:
    HRD() = default;
    void addAU(unsigned auBits, unsigned poc, QSharedPointer<sps> const &sps, QSharedPointer<buffering_period_sei> const &lastBufferingPeriodSEI, QSharedPointer<pic_timing_sei> const &lastPicTimingSEI, HRDPlotModel *plotModel);
    void endOfFile(HRDPlotModel *plotModel);
  
    bool isFirstAUInBufferingPeriod {true};
  private:
    typedef long double time_t;

    // We keep a list of frames which will be removed in the future
    struct HRDFrameToRemove
    {
        HRDFrameToRemove(time_t t_r, int bits, int poc)
            : t_r(t_r)
            , bits(bits)
            , poc(poc)
        {}
        time_t t_r;
        unsigned int bits;
        int poc;
    };
    QList<HRDFrameToRemove> framesToRemove;

    // The access unit count (n) for this HRD. The HRD is initialized with au n=0.
    uint64_t au_n {0};
    // Final arrival time (t_af for n minus 1)
    time_t t_af_nm1 {0};
    // t_r,n(nb) is the nominal removal time of the first access unit of the previous buffering period
    time_t t_r_nominal_n_first;

    QList<HRDFrameToRemove> popRemoveFramesInTimeInterval(time_t from, time_t to);
    void addToBufferAndCheck(unsigned bufferAdd, unsigned bufferSize, int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel);
    void removeFromBufferAndCheck(const HRDFrameToRemove &frame, int poc, time_t removalTime, HRDPlotModel *plotModel);
    void addConstantBufferLine(int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel);

    int64_t decodingBufferLevel {0};
  };
  HRD hrd;
};

} //namespace parser