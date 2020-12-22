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

#include "../ParserAnnexB.h"
#include "video/videoHandlerYUV.h"

using namespace YUV_Internals;

namespace parser
{

// This class knows how to parse the bitrstream of VVC annexB files
class ParserAnnexBVVC : public ParserAnnexB
{
  Q_OBJECT
  
public:
  ParserAnnexBVVC(QObject *parent = nullptr) : ParserAnnexB(parent) { curFrameFileStartEndPos = pairUint64(-1, -1); }
  ~ParserAnnexBVVC() {};

  // Get some properties
  double getFramerate() const override;
  QSize getSequenceSizeSamples() const override;
  yuvPixelFormat getPixelFormat() const override;

  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) override;
  QByteArray getExtradata() override;
  QPair<int,int> getProfileLevel() override;
  Ratio getSampleAspectRatio() override;

  ParseResult parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile={}, TreeItem *parent=nullptr) override;

protected:
  // ----- Some nested classes that are only used in the scope of this file handler class

  /* The basic VVC NAL unit. Additionally to the basic NAL unit, it knows the HEVC nal unit types.
  */
  struct nal_unit_vvc : NalUnit
  {
    nal_unit_vvc(int nalIdx, std::optional<pairUint64> filePosStartEnd) : NalUnit(nalIdx, filePosStartEnd) {}
    nal_unit_vvc(QSharedPointer<nal_unit_vvc> nal_src) : NalUnit(nal_src->nalIdx, nal_src->filePosStartEnd) { nalUnitTypeID = nal_src->nalUnitTypeID; nuh_layer_id = nal_src->nuh_layer_id; nuh_temporal_id_plus1 = nal_src->nuh_temporal_id_plus1; }
    virtual ~nal_unit_vvc() {}

    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return false; }  // We don't know yet
    bool parseNalUnitHeader(const QByteArray &parameterSetData, TreeItem *root) override;

    bool isAUDelimiter() { return nalUnitTypeID == 19; }

    // The information of the NAL unit header
    unsigned int nuh_layer_id;
    unsigned int nuh_temporal_id_plus1;
  };

  // Since full parsing is not implemented yet, we will just look for AU delimiters (they must be enabled in the bitstream and are by default).
  // This is used by getNextFrameNALUnits to return all information (NAL units) for a specific frame.
  std::optional<pairUint64> curFrameFileStartEndPos;   //< Save the file start/end position of the current frame (in case the frame has multiple NAL units)

  unsigned int counterAU{ 0 };
  unsigned int sizeCurrentAU{ 0 };
};

} // namespace parser