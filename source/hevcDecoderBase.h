/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef HEVCDECODERBASE_H
#define HEVCDECODERBASE_H

#include <QLibrary>
#include <QString>
#include "decoderBase.h"
#include "fileInfoWidget.h"
#include "fileSourceHEVCAnnexBFile.h"
#include "statisticHandler.h"
#include "statisticsExtensions.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

/* This class is the abstract base class for the two decoder wrappers de265Decoder
 * and hmDecoder. They both have the same interface and can thusly be used by the
 * playlistItemHEVCFile.
*/
class hevcDecoderBase : public decoderBase
{
public:
  hevcDecoderBase(bool cachingDecoder=false);
  virtual ~hevcDecoderBase() {};
  
  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  // If another decoder is given, don't parse the annex B bitstream again.
  bool openFile(QString fileName, decoderBase *otherDecoder = nullptr) Q_DECL_OVERRIDE;

  // Get some infos on the file
  QList<infoItem> getFileInfoList() const Q_DECL_OVERRIDE { return annexBFile.getFileInfoList(); }
  int getNumberPOCs() const Q_DECL_OVERRIDE { return annexBFile.getNumberPOCs(); }
  bool isFileChanged() Q_DECL_OVERRIDE { return annexBFile.isFileChanged(); }
  void updateFileWatchSetting() Q_DECL_OVERRIDE { annexBFile.updateFileWatchSetting(); }

protected:
  
  // The Annex B source file
  fileSourceHEVCAnnexBFile annexBFile;

  // Convert HEVC intra direction mode into vector
  static const int vectorTable[35][2];
};

#endif // HEVCDECODERBASE_H