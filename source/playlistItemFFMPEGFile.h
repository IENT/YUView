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

#ifndef PLAYLISTITEMFFMPEGFILE_H
#define PLAYLISTITEMFFMPEGFILE_H

#include "FFMpegDecoder.h"
#include "playlistItemWithVideo.h"
#include "statisticHandler.h"
#include "videoHandlerYUV.h"

class videoHandler;

class playlistItemFFMPEGFile : public playlistItemWithVideo
{
  Q_OBJECT

public:

  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
   * provide a pointer to the widget stack for the properties panels. The constructor will then call
   * addPropertiesWidget to add the custom properties panel.
  */
  playlistItemFFMPEGFile(const QString &fileName);

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "FFMpeg File Properties"; }

  // Save the HEVC file element to the given XML structure.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;

  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  // Override from playlistItemIndexed. The FFMpeg decoder can tell us how many POSs there are.
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, decoder.getNumberPOCs()-1); }

public slots:
  // Load the YUV data for the given frame index from file. This slot is called by the videoHandlerYUV if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadYUVData(int frameIdx, bool forceDecodingNow);

private:

  FFMpegDecoder decoder;
  bool decoderReady;
};

#endif // PLAYLISTITEMFFMPEGFILE_H
