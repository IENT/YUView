/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLAYLISTITEMIMAGE_H
#define PLAYLISTITEMIMAGE_H

#include <QFileSystemWatcher>
#include <QFuture>

#include "playlistitem.h"
#include "frameHandler.h"
#include "typedef.h"

class playlistItemImageFile :
  public playlistItem
{
  Q_OBJECT

public:
  playlistItemImageFile(const QString &imagePath);
  ~playlistItemImageFile() {};

  // ------ Overload from playlistItem

  virtual QString getInfoTitle() const Q_DECL_OVERRIDE { return "Image Info"; }
  virtual QList<infoItem> getInfoList() const Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "Image Properties"; }

  // Get the text size (using the current text, font/text size ...)
  virtual QSize getSize() const Q_DECL_OVERRIDE { return frame.getFrameSize(); }

  // Overload from playlistItem. Save the text item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemText from the playlist file entry. Return NULL if parsing failed.
  static playlistItemImageFile *newplaylistItemImageFile(const QDomElementYUView &root, const QString &playlistFilePath);
    
  // Return the RGB values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // Draw the text item. Since isIndexedByFrame() returned false, this item is not indexed by frames
  // and the given value of frameIdx will be ignored.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback) Q_DECL_OVERRIDE;

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // Get the frame handler
  virtual frameHandler *getFrameHandler() Q_DECL_OVERRIDE { return &frame; }

  // An image can be used in a difference.
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { bool b = fileChanged; fileChanged = false; return b; }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateFileWatchSetting() Q_DECL_OVERRIDE;

  // Is the image currently being loaded?
  virtual bool isLoading() Q_DECL_OVERRIDE { return backgroundLoadingFuture.isRunning(); }
  
private slots:
  // The image file that we loaded was changed.
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

private:
  // The frame handler that draws the frame
  frameHandler frame;

  // Watch the loaded file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  // Background loading
  void backgroundLoadImage();
  QFuture<void> backgroundLoadingFuture;
};

#endif // PLAYLISTITEMIMAGE_H
