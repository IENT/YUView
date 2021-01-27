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

#include "parser/AnnexB.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser
{

namespace mpeg2
{
class sequence_extension;
class sequence_header;
class picture_header;
} // namespace mpeg2

class AnnexBMpeg2 : public AnnexB
{
  Q_OBJECT

public:
  AnnexBMpeg2(QObject *parent = nullptr) : AnnexB(parent){};
  ~AnnexBMpeg2(){};

  // Get properties
  double         getFramerate() const override;
  QSize          getSequenceSizeSamples() const override;
  yuvPixelFormat getPixelFormat() const override;

  ParseResult parseAndAddNALUnit(int                                           nalID,
                                 const ByteVector &                            data,
                                 std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                                 std::optional<pairUint64> nalStartEndPosFile = {},
                                 TreeItem *                parent             = nullptr) override;

  // TODO: Reading from raw mpeg2 streams not supported (yet? Is this even defined / possible?)
  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) override
  {
    (void)iFrameNr;
    (void)filePos;
    return {};
  }
  QByteArray getExtradata() override { return QByteArray(); }
  IntPair    getProfileLevel() override;
  Ratio      getSampleAspectRatio() override;

private:
  // We will keep a pointer to the first sequence extension to be able to retrive some data
  std::shared_ptr<mpeg2::sequence_extension> firstSequenceExtension;
  std::shared_ptr<mpeg2::sequence_header>    firstSequenceHeader;

  unsigned                               sizeCurrentAU{0};
  int                                    pocOffset{0};
  int                                    curFramePOC{-1};
  int                                    lastFramePOC{-1};
  unsigned                               counterAU{0};
  bool                                   lastAUStartBySequenceHeader{false};
  bool                                   currentAUAllSlicesIntra{true};
  std::map<std::string, unsigned>        currentAUSliceCounts;
  std::shared_ptr<mpeg2::picture_header> lastPictureHeader;
};

} // namespace parser