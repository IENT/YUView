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

#ifndef PLAYLISTITEMIMAGEFILESEQUENCE_H
#define PLAYLISTITEMIMAGEFILESEQUENCE_H

#include <QFileSystemWatcher>
#include <QFuture>
#include "playlistItem.h"
#include "videoHandler.h"

class playlistItemImageFileSequence : public playlistItem
{
  Q_OBJECT

public:
  playlistItemImageFileSequence(const QString &rawFilePath = QString());

  // Overload from playlistItem. Save the raw file item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;

  // Override from playlistItem. Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "Image Sequence Properties"; }

  // Create a new playlistItemImageFileSequence from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemImageFileSequence *newplaylistItemImageFileSequence(const QDomElementYUView &root, const QString &playlistFilePath);

  // All the functions that we have to overload if we are indexed by frame
  virtual QSize getSize() const Q_DECL_OVERRIDE { return video.getFrameSize(); }

  // A raw file can be used in a difference
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }
  virtual frameHandler *getFrameHandler() Q_DECL_OVERRIDE { return &video; }

  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE { return ValuePairListSets("RGB", video.getPixelValues(pixelPos, frameIdx)); }

  // Draw the item
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

  // Do we need to load the given frame first?
  virtual itemLoadingState needsLoading(int frameIdx) Q_DECL_OVERRIDE { return video.needsLoading(frameIdx); }

  // -- Caching
  // Cache the given frame
  virtual void cacheFrame(int idx) Q_DECL_OVERRIDE { if (!cachingEnabled) return; video.cacheFrame(idx); }
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const Q_DECL_OVERRIDE { return video.getCachedFrames(); }
  // How many bytes will caching one frame use (in bytes)?
  virtual unsigned int getCachingFrameSize() const Q_DECL_OVERRIDE { return video.getCachingFrameSize(); }
  // Remove the given frame from the cache (-1: all frames)
  virtual void removeFrameFromCache(int idx) Q_DECL_OVERRIDE { video.removefromCache(idx); }

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // Check if this is just one image, or if there is a pattern in the file name. E.g:
  // image000.png, image001.png ...
  static bool isImageSequence(const QString &filePath);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { bool b = fileChanged; fileChanged = false; return b; }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateFileWatchSetting() Q_DECL_OVERRIDE;

  // Is an image currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return isFrameLoading; }

private slots:
  // Load the given frame from file. This slot is called by the videoHandler if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadFrame(int frameIdx, bool caching);

  // The image file that we loaded was changed.
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

protected:

  // Override from playlistItemIndexed. For a raw file the index range is 0...numFrames-1.
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, getNumberFrames()-1); }

private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemImageFileSequence
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  virtual qint64 getNumberFrames() const { return imageFiles.length(); }

  // Set internal values (frame Size, caching, ...). Call this after the imageFiles list has been filled.
  // Get the internal name and set it as text of the playlistItem.
  // E.g. for "somehting_0001.png" this will set the name "something_xxxx.png"
  void setInternals(const QString &filePath);

  QString internalName;

  // Fill the given imageFiles list with all the files that can be found for the given file.
  static void fillImageFileList(QStringList &imageFiles, const QString &filePath);
  QStringList imageFiles;

  videoHandler video;

  // This is true if the sequence was loaded from playlist and a frame is missing
  bool loadPlaylistFrameMissing;

  // Watch the loaded file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  // Is a frame currently being loaded?
  bool isFrameLoading;
};

#endif // PLAYLISTITEMIMAGEFILESEQUENCE_H
