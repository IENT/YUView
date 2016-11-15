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

#ifndef PLAYLISTITEMHEVCFILE_H
#define PLAYLISTITEMHEVCFILE_H

#include <QFuture>
#include "de265wrapper.h"
#include "fileSourceHEVCAnnexBFile.h"
#include "playlistItem.h"
#include "statisticHandler.h"
#include "videoHandlerYUV.h"

class videoHandler;

class playlistItemHEVCFile :
  public playlistItem, private de265Wrapper
{
  Q_OBJECT

public:

  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
   * provide a pointer to the widget stack for the properties panels. The constructor will then call
   * addPropertiesWidget to add the custom properties panel.
  */
  playlistItemHEVCFile(const QString &fileName);

  // Save the HEVC file element to the given xml structure.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemHEVCFile from the playlist file entry. Return NULL if parsing failed.
  static playlistItemHEVCFile *newplaylistItemHEVCFile(const QDomElementYUView &root, const QString &playlistFilePath);

  // Return the info title and info list to be shown in the fileInfo groupBox.
  // The default implementations will return empty strings/list.
  virtual QString getInfoTitle() const Q_DECL_OVERRIDE { return "HEVC File Info"; }
  virtual QList<infoItem> getInfoList() const Q_DECL_OVERRIDE;
  virtual void infoListButtonPressed(int buttonID);

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "HEVC File Properties"; };
  virtual QSize getSize() const Q_DECL_OVERRIDE { return yuvVideo.getFrameSize(); }
  
  // Draw the item using the given painter and zoom factor. If the item is indexed by frame, the given frame index will be drawn. If the
  // item is not indexed by frame, the parameter frameIdx is ignored.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback) Q_DECL_OVERRIDE;

  // Return the source (YUV and statistics) values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

  // If you want your item to be droppable onto a difference object, return true here and return a valid video handler.
  virtual bool canBeUsedInDifference() const Q_DECL_OVERRIDE { return true; }
  virtual frameHandler *getFrameHandler() Q_DECL_OVERRIDE { return &yuvVideo; }

  // Override from playlistItemIndexed. The annexBFile handler can tell us how many POSs there are.
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, annexBFile.getNumberPOCs()-1); }

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE { return annexBFile.isFileChanged(); }
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;
  virtual void updateFileWatchSetting() Q_DECL_OVERRIDE { annexBFile.updateFileWatchSetting(); }

  // Return true if decoding the background is running.
  virtual bool isLoading() Q_DECL_OVERRIDE { return drawDecodingMessage; }

  // -- Caching
  // Cache the given frame
  virtual void cacheFrame(int idx) Q_DECL_OVERRIDE;
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const Q_DECL_OVERRIDE { return yuvVideo.getCachedFrames(); }
  // How many bytes will caching one frame use (in bytes)?
  // For a raw file we only cache the output pixmap so it is w*h*PIXMAP_BYTESPERPIXEL bytes. 
  virtual unsigned int getCachingFrameSize() const Q_DECL_OVERRIDE { return getSize().width() * getSize().height() * PIXMAP_BYTESPERPIXEL; }
  // Remove the given frame from the cache (-1: all frames)
  virtual void removeFrameFromCache(int idx) Q_DECL_OVERRIDE { yuvVideo.removefromCache(idx); }

public slots:
  // Load the YUV data for the given frame index from file. This slot is called by the videoHandlerYUV if the frame that is
  // requested to be drawn has not been loaded yet.
  virtual void loadYUVData(int frameIdx, bool forceDecodingNow);
  
  // The statistic with the given frameIdx/typeIdx could not be found in the cache. Load it.
  virtual void loadStatisticToCache(int frameIdx, int typeIdx);

protected:
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

private:

  // The Annex B source file
  fileSourceHEVCAnnexBFile annexBFile;

  videoHandlerYUV yuvVideo;

  void setDe265ChromaMode(const de265_image *img);

  // ------------ Everything we need to plug into the libde265 library ------------

  de265_decoder_context* decoder;

  // Was there an error? If everything is allright it will be DE265_OK.
  de265_error decError;

  void allocateNewDecoder();

  /// ===== Buffering
#if SSE_CONVERSION
  byteArrayAligned  currentOutputBuffer;      ///< The buffer that was requested in the last call to getOneFrame
#else
  QByteArray  currentOutputBuffer;            ///< The buffer that was requested in the last call to getOneFrame
#endif
  int         currentOutputBufferFrameIndex;	///< The frame index of the buffer in currentOutputBuffer

  // Decode the next picture into the buffer. Return true on success.
#if SSE_CONVERSION
  bool decodeOnePicture(byteArrayAligned &buffer);
#else
  bool decodeOnePicture(QByteArray &buffer);
#endif
  // Copy the raw data from the de265_image src to the byte array
#if SSE_CONVERSION
  void copyImgToByteArray(const de265_image *src, byteArrayAligned &dst);
#else
  void copyImgToByteArray(const de265_image *src, QByteArray &dst);
#endif

  // --------------- Statistics ----------------

  // The statistics source
  statisticHandler statSource;

  // fill the list of statistic types that we can provide
  void fillStatisticList();
  
  // Get the statistics from the frame and put them into the local cache for the current frame
  void cacheStatistics(const de265_image *img, int iPOC);
  QHash<int, statisticsData> curPOCStats;  // cache of the statistics for the current POC [statsTypeID]
  int statsCacheCurPOC;                    // the POC of the statistics that are in the curPOCStats

  // With the given partitioning mode, the size of the CU and the prediction block index, calculate the
  // sub-position and size of the prediction block
  void getPBSubPosition(int partMode, int CUSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const;
  //
  void cacheStatistics_TUTree_recursive(uint8_t *tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int log2TUSize, int trDepth);

  bool retrieveStatistics;    ///< if set to true the decoder will also get statistics from each decoded frame and put them into the local cache

  // Convert intra direction mode into vector
  static const int vectorTable[35][2];

  // ------------- Background decoding -----------------
  void backgroundProcessDecode();
  QFuture<void> backgroundDecodingFuture;
  int  backgroundDecodingFrameIndex;        //< The background process is complete if this frame has been decoded
  QList<int> backgroundStatisticsToLoad;    //< The type indices of the statistics that are requested from the backgroundDecodingFrameIndex
  bool cancelBackgroundDecoding;            //< Abort the background process as soon as possible if this is set
  bool drawDecodingMessage;
  bool playbackRunning;

  QMutex cachingMutex;

private slots:
  void updateStatSource(bool bRedraw) { emit signalItemChanged(bRedraw, false); }

};

#endif // PLAYLISTITEMHEVCFILE_H
