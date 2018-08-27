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

#ifndef PLAYLISTITEMIMAGE_H
#define PLAYLISTITEMIMAGE_H

#include <QFileSystemWatcher>
#include <QFuture>
#include "frameHandler.h"
#include "playlistItem.h"

class playlistItemImageFile : public playlistItem
{
  Q_OBJECT

public:
  playlistItemImageFile(const QString &imagePath);

  // ------ Overload from playlistItem

  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "Image Properties"; }

  // Get the text size (using the current text, font/text size ...)
  virtual QSize getSize() const Q_DECL_OVERRIDE { return frame.getFrameSize(); }

  // Overload from playlistItem. Save the text item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemText from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemImageFile *newplaylistItemImageFile(const QDomElementYUView &root, const QString &playlistFilePath);
    
  // Return the RGB values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // Draw the text item. Since isIndexedByFrame() returned false, this item is not indexed by frames
  // and the given value of frameIdx will be ignored.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // Do we need to load the given frame first?
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawValues) Q_DECL_OVERRIDE { Q_UNUSED(frameIdx); Q_UNUSED(loadRawValues); return needToLoadImage ? LoadingNeeded : LoadingNotNeeded; }

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // Get the frame handler
  virtual frameHandler *getFrameHandler() Q_DECL_OVERRIDE { return &frame; }

  // An image can be used in a difference.
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { bool b = fileChanged; fileChanged = false; return b; }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE { needToLoadImage = false; }
  virtual void updateSettings()         Q_DECL_OVERRIDE;

  // Load the frame. Emit signalItemChanged(true,false) when done. Always called from a thread.
  virtual void loadFrame(int frameIdx, bool playing, bool loadRawdata, bool emitSignals=true) Q_DECL_OVERRIDE;

  // Is the image currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return imageLoading; }
  
private slots:
  // The image file that we loaded was changed.
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

private:
  // The frame handler that draws the frame
  frameHandler frame;

  // Watch the loaded file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  // Does the image need to be loaded? Is it currently loading?
  bool needToLoadImage, imageLoading;
};

#endif // PLAYLISTITEMIMAGE_H
