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

#include "FFmpegDecoder.h"
#include "playlistItemWithVideo.h"
#include "statisticHandler.h"
#include "videoHandlerYUV.h"

class videoHandler;

class playlistItemFFmpegFile : public playlistItemWithVideo
{
  Q_OBJECT

public:

  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
   * provide a pointer to the widget stack for the properties panels. The constructor will then call
   * addPropertiesWidget to add the custom properties panel.
  */
  playlistItemFFmpegFile(const QString &fileName);

  // Draw the FFmpeg item using the given painter and zoom factor.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // Save the FFMpeg file element to the given XML structure.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemFFmpegFile from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemFFmpegFile *newplaylistItemFFmpegFile(const QDomElementYUView & root, const QString & playlistFilePath);

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "FFMpeg File Properties"; }

  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;
  virtual void infoListButtonPressed(int buttonID) Q_DECL_OVERRIDE;

  // Do we need to load the frame first?
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawValues) Q_DECL_OVERRIDE;

  // Return the source (YUV and statistics) values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { return loadingDecoder.isFileChanged(); }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateSettings()         Q_DECL_OVERRIDE { loadingDecoder.updateFileWatchSetting(); statSource.updateSettings(); }

  // Cache the frame with the given index.
  // For FFMpeg items, a mutex must be locked when caching a frame (only one frame can be cached at a time).
  void cacheFrame(int idx, bool testMode) Q_DECL_OVERRIDE;

  // Load the frame in the video item. Emit signalItemChanged(true,false) when done.
  virtual void loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals=true) Q_DECL_OVERRIDE;

public slots:
  // Load the YUV data for the given frame index from file. This slot is called by the videoHandlerYUV if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadYUVData(int frameIdxInternal, bool forceDecodingNow);

  // The statistic with the given frameIdx/typeIdx could not be found in the cache. Load it.
  virtual void loadStatisticToCache(int frameIdx, int typeIdx);

protected:
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

private:
  // Override from playlistItemIndexed. The FFMpeg decoder can tell us how many POSs there are.
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, loadingDecoder.getNumberPOCs() - 1); }

  // We allocate two decoder: One for loading images in the foreground and one for caching in the background.
  // This is better if random access and linear decoding (caching) is performed at the same time.
  FFmpegDecoder loadingDecoder;
  FFmpegDecoder cachingDecoder;

  // The statistics source
  statisticHandler statSource;

  // fill the list of statistic types that we can provide
  void fillStatisticList();

  // Only cache one frame at a time. Caching should also always be done in display order of the frames.
  // TODO: Could we somehow make shure that caching is always performed in display order?
  QMutex cachingMutex;

  bool decoderReady;

private slots:
  void updateStatSource(bool bRedraw) { emit signalItemChanged(bRedraw, RECACHE_NONE); }
};

#endif // PLAYLISTITEMFFMPEGFILE_H
