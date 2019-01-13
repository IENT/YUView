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

#ifndef PLAYLISTITEMRAWFILE_H
#define PLAYLISTITEMRAWFILE_H

#include <QFuture>
#include <QString>
#include "fileSource.h"
#include "playlistItemWithVideo.h"
#include "typedef.h"

class playlistItemRawFile : public playlistItemWithVideo
{
  Q_OBJECT

public:
  // Create a new raw file. The format (RGB or YUV) will be gotten from the extension. If the extension is not one of the supported
  // extensions (getSupportedFileExtensions), set the format "fmt" to either "rgb" or "yuv". If you already know the frame size and/or 
  // sourcePixelFormat, you can set them as well.
  playlistItemRawFile(const QString &rawFilePath, const QSize &frameSize=QSize(-1,-1), const QString &sourcePixelFormat=QString(), const QString &fmt=QString());

  // Overload from playlistItem. Save the raw file item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;

  // Override from playlistItem. Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return (rawFormat == raw_YUV) ? "YUV File Properties" : "RGB File Properties"; }

  // Create a new playlistItemRawFile from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemRawFile *newplaylistItemRawFile(const QDomElementYUView &root, const QString &playlistFilePath);

  // A raw file can be used in a difference
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }

  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()  Q_DECL_OVERRIDE { return dataSource.isFileChanged(); }
  virtual void reloadItemSource() Q_DECL_OVERRIDE;
  virtual void updateSettings()   Q_DECL_OVERRIDE { dataSource.updateFileWatchSetting(); }

  // Cache the given frame
  virtual void cacheFrame(int idx, bool testMode) Q_DECL_OVERRIDE { if (testMode) dataSource.clearFileCache(); playlistItemWithVideo::cacheFrame(idx, testMode); }

public slots:
  // Load the raw data for the given frame index from file. This slot is called by the videoHandler if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadRawData(int frameIdxInternal);

protected:
  // Override from playlistItemIndexed. For a raw file the index range is 0...numFrames-1. 
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, getNumberFrames() - 1); }

  // Try to get and set the format from file name. If after calling this function isFormatValid()
  // returns false then it failed.
  void setFormatFromFileName();
  
private:

  // Overload from playlistItem. Create a properties widget custom to the RawFile
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  virtual int64_t getNumberFrames() const;
  
  fileSource dataSource;

  int64_t getBytesPerFrame() const { return video->getBytesPerFrame(); }

  // A y4m file is a raw YUV file but it adds a header (which has information about the YUV format)
  // and start indicators for every frame. This file will parse the header and save all the byte
  // offsets for each raw YUV frame.
  bool parseY4MFile();
  bool isY4MFile;
  QList<uint64_t> y4mFrameIndices;
};

#endif // PLAYLISTITEMRAWFILE_H
