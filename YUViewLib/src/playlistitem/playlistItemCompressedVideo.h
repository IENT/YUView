/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef PLAYLISTITEMCOMPRESSEDVIDEO_H
#define PLAYLISTITEMCOMPRESSEDVIDEO_H

#include "decoder/decoderBase.h"
#include "filesource/fileSourceFFmpegFile.h"
#include "parser/parserAnnexB.h"
#include "playlistItemWithVideo.h"
#include "statistics/statisticHandler.h"
#include "ui_playlistItemCompressedFile.h"

class videoHandler;

/* This playlist item encapsulates all compressed video sequences. 
 * We can read the data from a plain binary file (we support various raw annexB formats) or we can use
 * ffmpeg to read from a container.
 * For decoding, a range of different decoders is available.
*/
class playlistItemCompressedVideo : public playlistItemWithVideo
{
  Q_OBJECT

public:

  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
  * provide a pointer to the widget stack for the properties panels. The constructor will then call
  * addPropertiesWidget to add the custom properties panel.
  * 'displayComponent' initializes the component to display (reconstruction/prediction/residual/trCoeff).
  */
  playlistItemCompressedVideo(const QString &fileName, int displayComponent=0, YUView::inputFormat input = YUView::inputInvalid, YUView::decoderEngine decoder = YUView::decoderEngineInvalid);

  // Save the compressed file element to the given XML structure.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemHEVCFile from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemCompressedVideo *newPlaylistItemCompressedVideo(const YUViewDomElement &root, const QString &playlistFilePath);

  bool isFileSource() const Q_DECL_OVERRIDE { return true; };

  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;
  virtual void infoListButtonPressed(int buttonID) Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "Compressed File Properties"; }

  // Draw the compressed item using the given painter and zoom factor.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // Return the source (YUV and statistics) values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // If you want your item to be droppable onto a difference object, return true here and return a valid video handler.
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { /* TODO */ return false; }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateSettings()         Q_DECL_OVERRIDE { /* TODO loadingDecoder->updateFileWatchSetting(); statSource.updateSettings(); */ }

  // Do we need to load the given frame first?
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawData) Q_DECL_OVERRIDE;
  // Load the frame in the video item. Emit signalItemChanged(true,false) when done.
  virtual void loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals=true) Q_DECL_OVERRIDE;
  // Is an image currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return isFrameLoading; }
  virtual bool isLoadingDoubleBuffer() const Q_DECL_OVERRIDE { return isFrameLoadingDoubleBuffer; }

  // Cache the frame with the given index.
  // For all compressed items, a mutex must be locked when caching a frame (only one frame can be cached at a time because we only have one decoder).
  void cacheFrame(int idx, bool testMode) Q_DECL_OVERRIDE;

  // We only have one caching decoder so it is better if only one thread caches frames from this item.
  // This way, the frames will always be cached in the right order and no unnecessary decoding is performed.
  virtual int cachingThreadLimit() Q_DECL_OVERRIDE { return 1; }

  YUView::inputFormat getInputFormat() const { return inputFormatType; }
  
protected:
  // Override from playlistItemIndexed. The readerEngine can tell us how many frames there are in the sequence.
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE;

  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // We allocate two decoder: One for loading images in the foreground and one for caching in the background.
  // This is better if random access and linear decoding (caching) is performed at the same time.
  QScopedPointer<decoderBase> loadingDecoder;
  QScopedPointer<decoderBase> cachingDecoder;

  // When opening the file, we will fill this list with the possible decoders
  QList<YUView::decoderEngine> possibleDecoders;
  // The actual type of the decoder
  YUView::decoderEngine decoderEngineType;
  // Delete existing decoders and allocate decoders for the type "decoderEngineType"
  bool allocateDecoder(int displayComponent = 0);

  // In order to parse raw annexB files, we need a file reader (that can read NAL units)
  // and a parser that can understand what the NAL units mean. We open the file source twice (once for interactive loading,
  // once for the background caching). The parser is only needed once and can be used for both loading and caching tasks.
  QScopedPointer<fileSourceAnnexBFile> inputFileAnnexBLoading;
  QScopedPointer<fileSourceAnnexBFile> inputFileAnnexBCaching;
  QScopedPointer<parserAnnexB> inputFileAnnexBParser;
  // When reading annex B data using the fileSourceAnnexBFile::getFrameData function, we need to count how many frames we already read.
  int readAnnexBFrameCounterCodingOrder { -1 };
  
  // Which type is the input?
  YUView::inputFormat inputFormatType;
  AVCodecIDWrapper ffmpegCodec;

  // For FFMpeg files we don't need a reader to parse them. But if the container contains a supported format, we can
  // read the NAL units from the compressed file.
  QScopedPointer<fileSourceFFmpegFile> inputFileFFmpegLoading;
  QScopedPointer<fileSourceFFmpegFile> inputFileFFmpegCaching;
  
  // Is the loadFrame function currently loading?
  bool isFrameLoading { false };
  bool isFrameLoadingDoubleBuffer { false };

  // Only cache one frame at a time. Caching should also always be done in display order of the frames.
  // TODO: Could we somehow make shure that caching is always performed in display order?
  QMutex cachingMutex;

  statisticHandler statSource;

  // Fill the list of statistic types that we can provide
  void fillStatisticList();

  SafeUi<Ui::playlistItemCompressedFile_Widget> ui;

  // The current frame index of the decoders (interactive/caching)
  int currentFrameIdx[2] {-1, -1};

  // Seek the input file to the given position, reset the decoder and prepare it to start decoding from the given position.
  void seekToPosition(int seekToFrame, int seekToDTS, bool caching);

  // For certain decoders (FFmpeg or HM), pushing data may fail. The decoder may or may not switch to retrieveing mode.
  // In this case, we must re-push the packet for which pushing failed.
  bool repushData {false};

  // Besides the normal stats (error / no error) this item might be able to parse the file but not to decode it.
  void setDecodingError(QString err) { infoText = err; decodingEnabled = false; }
  bool decodingEnabled {false};

  // If the bitstream is invalid (for example it was cut at a position that it should not be cut at), we
  // might be unable to decode some of the frames at the end of the sequence.
  int decodingNotPossibleAfter { -1 };

private slots:
  // Load the raw (YUV or RGN) data for the given frame index from file. This slot is called by the videoHandler if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadRawData(int frameIdxInternal, bool forceDecodingNow);

  // The statistic with the given frameIdx/typeIdx could not be found in the cache. Load it.
  virtual void loadStatisticToCache(int frameIdx, int typeIdx);

  void updateStatSource(bool bRedraw) { emit signalItemChanged(bRedraw, RECACHE_NONE); }
  void displaySignalComboBoxChanged(int idx);
  void decoderComboxBoxChanged(int idx);
};

#endif // PLAYLISTITEMCOMPRESSEDVIDEO_H
