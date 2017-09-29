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

#ifndef PLAYLISTITEMIMAGEFILESEQUENCE_H
#define PLAYLISTITEMIMAGEFILESEQUENCE_H

#include <QFileSystemWatcher>
#include <QFuture>
#include "playlistItemWithVideo.h"
#include "videoHandler.h"
#include "playlistItemRawFile.h"

class playlistItemImageFileSequence : public playlistItemWithVideo
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

  // A raw file can be used in a difference
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }

  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE { return ValuePairListSets("RGB", video->getPixelValues(pixelPos, getFrameIdxInternal(frameIdx))); }

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // Check if this is just one image, or if there is a pattern in the file name. E.g:
  // image000.png, image001.png ...
  static bool isImageSequence(const QString &filePath);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { bool b = fileChanged; fileChanged = false; return b; }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateSettings()         Q_DECL_OVERRIDE;

  // Is an image currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return isFrameLoading; }

private slots:
  // Load the given frame from file. This slot is called by the videoHandler if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void slotFrameRequest(int frameIdxInternal, bool caching);

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
  
  // This is true if the sequence was loaded from playlist and a frame is missing
  bool loadPlaylistFrameMissing;

  // Watch the loaded file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  // Is a frame currently being loaded?
  bool isFrameLoading;
};

#endif // PLAYLISTITEMIMAGEFILESEQUENCE_H
