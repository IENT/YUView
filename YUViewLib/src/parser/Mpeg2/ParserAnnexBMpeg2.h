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

#include "parser/common/ParserAnnexB.h"
#include "parser/common/NalUnitBase.h"

#include "Extensions.h"
#include "PictureHeader.h"
#include "SequenceHeader.h"

class ParserAnnexBMpeg2 : public ParserAnnexB
{
  Q_OBJECT
  
public:
  ParserAnnexBMpeg2(QObject *parent = nullptr) : ParserAnnexB(parent) {};
  ~ParserAnnexBMpeg2() {};

  // Get properties
  double getFramerate() const Q_DECL_OVERRIDE;
  QSize getSequenceSizeSamples() const Q_DECL_OVERRIDE;
  yuvPixelFormat getPixelFormat() const Q_DECL_OVERRIDE;

  ParseResult parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile={}, TreeItem *parent=nullptr) Q_DECL_OVERRIDE;

  // TODO: Reading from raw mpeg2 streams not supported (yet? Is this even defined / possible?)
  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) Q_DECL_OVERRIDE { Q_UNUSED(iFrameNr); Q_UNUSED(filePos); return QList<QByteArray>(); }
  QByteArray getExtradata() Q_DECL_OVERRIDE { return QByteArray(); }
  QPair<int,int> getProfileLevel() Q_DECL_OVERRIDE;
  QPair<int,int> getSampleAspectRatio() Q_DECL_OVERRIDE;

private:

  // We will keep a pointer to the first sequence extension to be able to retrive some data
  QSharedPointer<MPEG2::SequenceExtension> firstSequenceExtension;
  QSharedPointer<MPEG2::SequenceHeader> firstSequenceHeader;

  unsigned int sizeCurrentAU{ 0 };
  int pocOffset{ 0 };
  int curFramePOC{ -1 };
  int lastFramePOC{ -1 };
  unsigned int counterAU{ 0 };
  bool lastAUStartBySequenceHeader{ false };
  bool currentAUAllSlicesIntra {true};
  QMap<QString, unsigned int> currentAUSliceTypes;
  QSharedPointer<MPEG2::PictureHeader> lastPictureHeader;
};
