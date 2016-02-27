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

#ifndef PLAYLISTITEMVIDEO_H
#define PLAYLISTITEMVIDEO_H

#include <QTreeWidgetItem>
#include <QDomElement>
#include <QDir>
#include <QString>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGridLayout>
#include <QThread>

#include "typedef.h"
#include "playlistItem.h"

#include "ui_playlistItemVideo.h"

#include "videoCache.h"

#include <assert.h>

class videoCache;

/* The playlistItemVideo is a playlistItem and is the abstract base class for everything that provides a video, so a fixed
 * number of frames that can be drawn and indexed by frame number. This class implements the painting and some controls that
 * are common to all items that provide a video.
 *
 * TODO: more info!
*/
class playlistItemVideo : public playlistItem, private Ui_playlistItemVideo
{
  Q_OBJECT

public:

  /*
  */
  playlistItemVideo(QString itemNameOrFileName);
  virtual ~playlistItemVideo();

  // ------ Override from playlistItem

  // Every playlistItemVideo is indexed by frame
  virtual bool isIndexedByFrame() Q_DECL_OVERRIDE { return true; }

  // Every playlistItemVideo provides a video
  virtual bool providesVideo() Q_DECL_OVERRIDE { return true; }

  virtual double getFrameRate() Q_DECL_OVERRIDE { return frameRate; }
  virtual QSize  getVideoSize() Q_DECL_OVERRIDE { return frameSize; }
  virtual indexRange getFrameIndexRange() Q_DECL_OVERRIDE { return startEndFrame; }

  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

  // different loading functions, depending on the type
  virtual bool loadIntoCache(int frameIdx) { return true; }

  virtual bool isCaching() Q_DECL_OVERRIDE;

  // Calculate the difference of this playlistItemVideo to another playlistItemVideo. This
  // function can be overloaded by more specialized video items. For example the playlistItemYuvSource 
  // overloads this and calculates the difference directly on the YUV values (if possible).
  virtual QPixmap calculateDifference(playlistItemVideo *item2, int frame, QList<infoItem> &conversionInfoList);
  // For the difference item: Return values of this item, the other item and the difference at
  // the given pixel position
  virtual ValuePairList getPixelValuesDifference(playlistItemVideo *item2, QPoint pixelPos);

  // Each video playlistItem should be able to tell us how many frames it contains
  virtual qint64 getNumberFrames() = 0;
  
  // an item can add receive caching instructions from the controller
public slots:
  virtual void startCaching(indexRange range) Q_DECL_OVERRIDE;
  virtual void stopCaching() Q_DECL_OVERRIDE;
  void updateFrameCached() { emit signalItemChanged(false); }


protected:

  // Create the video controls and return a pointer to the layout. This can be used by
  // inherited classes to create a properties widget.
  // isSizeFixed: For example a YUV file does not have a fixed size (the user can change this),
  // other sources might provide a fixed size which the user cannot change (HEVC file, png image sequences ...)
  virtual QLayout *createVideoControls(bool isSizeFixed=false);

  // The child class has to take care of actually loading a frame. The implementation of the child class has to load the frame
  // with the given index into currentFrame. It also has to set the currentFrameIdx.
  virtual void loadFrame(int frameIdx) = 0;

  // Set the values and update the controls. Only emit an event if emitSignal is set.
  void setFrameSize(QSize size, bool emitSignal = false);
  void setStartEndFrame(indexRange range, bool emitSignal = false);


  // Every video item has a frameRate, start frame, end frame a sampling and a size
  double frameRate;
  indexRange startEndFrame;
  int sampling;
  QSize frameSize;

  // --- Drawing: We keep a buffer of the current frame as RGB image so wen don't have to ´convert
  // it from the source every time a draw event is triggered. But if currentFrameIdx is not identical to
  // the requested frame in the draw event, we will have to update currentFrame.
  QPixmap    currentFrame;
  int        currentFrameIdx;

  // --- Caching: We have a cache object and a thread, where the cache object runs on
  videoCache *cache;
  QThread* cacheThread;

  // Saving to/loading from playlist
  void appendItemProperties(QDomDocument &doc, QDomElement &root);
  void parseProperties(QDomElementYUV root);

private:

  // A list of all frame size presets. Only used privately in this class. Defined in the .cpp file.
  class frameSizePresetList
  {
  public:
    // Constructor. Fill the names and sizes lists
    frameSizePresetList();
    // Get all presets in a displayable format ("Name (xxx,yyy)")
    QStringList getFormatedNames();
    // Return the index of a certain size (0 (Custom Size) if not found)
    int findSize(QSize size) { int idx = sizes.indexOf( size ); return (idx == -1) ? 0 : idx; }
    // Get the size with the given index.
    QSize getSize(int index) { return sizes[index]; }
  private:
    QList<QString> names;
    QList<QSize>   sizes;
  };

  // The (static) list of frame size presets (like CIF, QCIF, 4k ...)
  static frameSizePresetList presetFrameSizes;
  QStringList getFrameSizePresetNames();

private slots:
  // All the valueChanged() signals from the controls are connected here.
  void slotVideoControlChanged();
};

#endif // PLAYLISTITEMVIDEO_H
