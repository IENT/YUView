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

#ifndef PLAYLISTITEMRAWFILE_H
#define PLAYLISTITEMRAWFILE_H

#include <QFuture>
#include <QString>
#include "fileSource.h"
#include "playlistItem.h"
#include "typedef.h"
#include "videoHandlerRGB.h"
#include "videoHandlerYUV.h"

class playlistItemRawFile : public playlistItem
{
  Q_OBJECT

public:
  // Create a new raw file. The format (RGB or YUV) will be gotten from the extension. If the extension is not one of the supported
  // extensions (getSupportedFileExtensions), set fmt to either "rgb" or "yuv". If you already know the frame size and/or 
  // sourcePixelFormat, you can set them as well.
  playlistItemRawFile(const QString &rawFilePath, const QSize &frameSize=QSize(-1,-1), const QString &sourcePixelFormat=QString(), const QString &fmt=QString());

  // Overload from playlistItem. Save the raw file item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;

  // Override from playlistItem. Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return (rawFormat == YUV) ? "YUV File Properties" : "RGB File Properties"; }

  // Create a new playlistItemRawFile from the playlist file entry. Return NULL if parsing failed.
  static playlistItemRawFile *newplaylistItemRawFile(const QDomElementYUView &root, const QString &playlistFilePath);

  // All the functions that we have to overload if we are indexed by frame
  virtual QSize getSize() const Q_DECL_OVERRIDE { return (video) ? video->getFrameSize() : QSize(); }
  
  // A raw file can be used in a difference
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }
  virtual frameHandler *getFrameHandler() Q_DECL_OVERRIDE { return video.data(); }

  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // Draw the item
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback) Q_DECL_OVERRIDE;

  // -- Caching
  // Cache the given frame
  virtual void cacheFrame(int idx) Q_DECL_OVERRIDE { if (!cachingEnabled) return; video->cacheFrame(idx); }
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const Q_DECL_OVERRIDE { return video->getCachedFrames(); }
  // How many bytes will caching one frame use (in bytes)?
  // For a raw file we only cache the output pixmap so it is w*h*PIXMAP_BYTESPERPIXEL bytes. 
  virtual unsigned int getCachingFrameSize() const Q_DECL_OVERRIDE { return getSize().width() * getSize().height() * PIXMAP_BYTESPERPIXEL; }
  // Remove the given frame from the cache (-1: all frames)
  virtual void removeFrameFromCache(int idx) Q_DECL_OVERRIDE { video->removefromCache(idx); }

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { return dataSource.isFileChanged(); }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateFileWatchSetting() Q_DECL_OVERRIDE { dataSource.updateFileWatchSetting(); }

  // Is an image currently being loaded?
  virtual bool isLoading() Q_DECL_OVERRIDE { return backgroundLoadingFuture.isRunning(); }

public slots:
  // Load the raw data for the given frame index from file. This slot is called by the videoHandler if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadRawData(int frameIdx, bool caching);

protected:

  // Try to get and set the format from file name. If after calling this function isFormatValid()
  // returns false then it failed.
  void setFormatFromFileName();

  // Override from playlistItemIndexed. For a raw raw file the index range is 0...numFrames-1. 
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, getNumberFrames()-1); }

private:

  typedef enum
  {
    YUV,
    RGB
  } RawFormat;
  RawFormat rawFormat;

  // Overload from playlistItem. Create a properties widget custom to the RawFile
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  virtual qint64 getNumberFrames() const;
  
  fileSource dataSource;

  QScopedPointer<videoHandler> video;

  videoHandlerYUV *getYUVVideo() { return dynamic_cast<videoHandlerYUV*>(video.data()); }
  videoHandlerRGB *getRGBVideo() { return dynamic_cast<videoHandlerRGB*>(video.data()); }
  const videoHandlerYUV *getYUVVideo() const { return dynamic_cast<const videoHandlerYUV*>(video.data()); }
  const videoHandlerRGB *getRGBVideo() const { return dynamic_cast<const videoHandlerRGB*>(video.data()); }

  qint64 getBytesPerFrame() const;

  // Background loading
  void backgroundLoadImage();
  int backgroundFileIndex;
  bool playbackRunning;
  QFuture<void> backgroundLoadingFuture;
};

#endif // PLAYLISTITEMRAWFILE_H
