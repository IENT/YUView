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

#ifndef PLAYLISTITEMHANDLER_H
#define PLAYLISTITEMHANDLER_H

#include "playlistItemCompressedVideo.h"
#include "playlistItemDifference.h"
#include "playlistItemStatisticsCSVFile.h"
#include "playlistItemStatisticsVTMBMSFile.h"
#include "playlistItemImageFile.h"
#include "playlistItemImageFileSequence.h"
#include "playlistItemOverlay.h"
#include "playlistItemRawFile.h"
#include "playlistItemText.h"

/* This namespace contains all functions that are needed for creation of playlist Items. This way, no other
   function must know, what types of item's there are. If you implement a new playlistItem, it only has to
   be added here (and in the functions).
*/
namespace playlistItems
{
  // Get a list of all supported file format filets and the extensions. This can be used in a file open dialog.
  QStringList getSupportedFormatsFilters();
  
  // Get a list of all supported file extensions (["*.csv", "*.yuv" ...])
  QStringList getSupportedNameFilters();

  // When given a file, this function will create the correct playlist item (depending on the file extension)
  playlistItem *createPlaylistItemFromFile(QWidget *parent, const QString &fileName);

  // Load a playlist item (and all of it's children) from the playlist
  // Append all loaded playlist items to the list plItemAndIDList (alongside the IDs that were saved in the playlist file)
  playlistItem *loadPlaylistItem(const QDomElement &elem, const QString &filePath);
}

#endif // PLAYLISTITEMHANDLER_H
