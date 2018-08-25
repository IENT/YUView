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

#ifndef PLAYLISTITEMHEVCFILE_H
#define PLAYLISTITEMHEVCFILE_H

#include "decoderBase.h"
#include "playlistItemWithVideo.h"
#include "statisticHandler.h"
#include "ui_playlistItemHEVCFile.h"
#include "videoHandlerYUV.h"

class videoHandler;

/* This playlist item encapsulates all raw coded video sequences that require a decoder like
 * raw HEVC files and raw HEVC next gen (JEM) files.
 */
class playlistItemRawCodedVideo : public playlistItemWithVideo
{
  Q_OBJECT

public:

  typedef enum
  {
    decoderInvalid = -1,  // invalid value
    decoderLibde265,      // The libde265 decoder
    decoderHM,            // The HM reference software decoder
    decoderJEM,           // The JEM reference software decoder
    decoder_NUM
  } decoderEngine;

  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
   * provide a pointer to the widget stack for the properties panels. The constructor will then call
   * addPropertiesWidget to add the custom properties panel.
   * 'displayComponent' initializes the component to display (reconstruction/prediction/residual/trCoeff).
  */
  playlistItemRawCodedVideo(const QString &fileName, int displayComponent=0, decoderEngine e=decoderLibde265);

  // Save the HEVC file element to the given XML structure.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemHEVCFile from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemRawCodedVideo *newplaylistItemRawCodedVideo(const QDomElementYUView &root, const QString &playlistFilePath);

  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;
  virtual void infoListButtonPressed(int buttonID) Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "HEVC File Properties"; }

  // Draw the HEVC item using the given painter and zoom factor.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // Return the source (YUV and statistics) values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // If you want your item to be droppable onto a difference object, return true here and return a valid video handler.
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }
  
  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { return loadingDecoder->isFileChanged(); }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateSettings()         Q_DECL_OVERRIDE { loadingDecoder->updateFileWatchSetting(); statSource.updateSettings(); }

  // Do we need to load the given frame first?
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawData) Q_DECL_OVERRIDE;
  // Load the frame in the video item. Emit signalItemChanged(true,false) when done. Always called from a thread.
  virtual void loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals=true) Q_DECL_OVERRIDE;
  // Is an image currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return isFrameLoading; }
  virtual bool isLoadingDoubleBuffer() const Q_DECL_OVERRIDE { return isFrameLoadingDoubleBuffer; }

  // Cache the frame with the given index.
  // For HEVC items, a mutex must be locked when caching a frame (only one frame can be cached at a time).
  void cacheFrame(int idx, bool testMode) Q_DECL_OVERRIDE;

  // We only have one caching decoder so it is better if only one thread caches frames from this item.
  // This way, the frames will always be cached in the right order and no unnecessary decoding is performed.
  virtual int cachingThreadLimit() Q_DECL_OVERRIDE { return 1; }

  // Ask the user which decoder engine to use
  static decoderEngine askForDecoderEngine(QWidget *parent);

public slots:
  // Load the YUV data for the given frame index from file. This slot is called by the videoHandlerYUV if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadYUVData(int frameIdxInternal, bool forceDecodingNow);

  // The statistic with the given frameIdx/typeIdx could not be found in the cache. Load it.
  virtual void loadStatisticToCache(int frameIdx, int typeIdx);

protected:
  // Override from playlistItemIndexed. The annexBFile handler can tell us how many POSs there are.
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, loadingDecoder->getNumberPOCs() - 1); }

  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  typedef enum
  {
    noError,     // There was no error. Parsing the bitstream worked and frames can be decoded.
    onlyParsing, // Loading of the decoder failed. We can only parse the bitstream.
    error        // The bitstream looks invalid. Error.
  } hevcFileState;
  hevcFileState fileState;

  // We allocate two decoder: One for loading images in the foreground and one for caching in the background.
  // This is better if random access and linear decoding (caching) is performed at the same time.
  QScopedPointer<decoderBase> loadingDecoder;
  QScopedPointer<decoderBase> cachingDecoder;

  // Which type of decoder do we use?
  decoderEngine decoderEngineType;

  // Is the loadFrame function currently loading?
  bool isFrameLoading;
  bool isFrameLoadingDoubleBuffer;

  // Only cache one frame at a time. Caching should also always be done in display order of the frames.
  // TODO: Could we somehow make shure that caching is always performed in display order?
  QMutex cachingMutex;

  // The statistics source
  statisticHandler statSource;

  // fill the list of statistic types that we can provide
  void fillStatisticList();

  // Which of the signals is being displayed? Reconstruction(0), Prediction(1) or Residual(2)
  int displaySignal;

  SafeUi<Ui::playlistItemHEVCFile_Widget> ui;

  static QStringList decoderEngineNames;

private slots:
  void updateStatSource(bool bRedraw) { emit signalItemChanged(bRedraw, RECACHE_NONE); }
  void displaySignalComboBoxChanged(int idx);
};

#endif // PLAYLISTITEMHEVCFILE_H
