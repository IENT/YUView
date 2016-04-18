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

#ifndef VIDEOHANDLER_H
#define VIDEOHANDLER_H

#include <QTreeWidgetItem>
#include <QDomElement>
#include <QDir>
#include <QString>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGridLayout>
#include <QTimer>
#include <QMutex>

#include "typedef.h"
#include "playlistItem.h"

#include "ui_videoHandler.h"

#include <assert.h>

/* TODO
*/
class videoHandler : public QObject, private Ui_videoHandler
{
  Q_OBJECT

public:

  /*
  */
  videoHandler();
  virtual ~videoHandler();

  virtual double getFrameRate() { return frameRate; }
  virtual int    getSampling()  { return sampling;  }
  virtual QSize  getSize()      { return frameSize; }
  virtual indexRange getFrameIndexRange() { return startEndFrame; }

  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor);

  // Return the RGB values of the given pixel
  virtual ValuePairList getPixelValues(QPoint pixelPos);
  // For the difference item: Return values of this item, the other item and the difference at
  // the given pixel position
  virtual ValuePairList getPixelValuesDifference(QPoint pixelPos, videoHandler *item2);

  // Calculate the difference of this videoHandler to another videoHandler. This
  // function can be overloaded by more specialized video items. For example the videoHandlerYUV
  // overloads this and calculates the difference directly on the YUV values (if possible).
  virtual QPixmap calculateDifference(videoHandler *item2, int frame, QList<infoItem> &differenceInfoList, int amplificationFactor, bool markDifference);

  // Set the current frameLimits (from, to). Set to (-1,-1) if you don't know.
  // Calling this might change some controls but will not trigger any signals to be emitted.
  // You should only call this function if this class asks for it (signalGetFrameLimits).
  void setFrameLimits( indexRange limits );

  // Create the video controls and return a pointer to the layout. This can be used by
  // inherited classes to create a properties widget.
  // isSizeFixed: For example a YUV file does not have a fixed size (the user can change this),
  // other sources might provide a fixed size which the user cannot change (HEVC file, png image sequences ...)
  virtual QLayout *createVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed=false);

  // Draw the pixel values of the visible pixels in the center of each pixel.
  // Only draw values for the given range of pixels.
  // The playlistItemVideo implememntation of this function will draw the RGB vales. However, if a derived class knows other
  // source values to show it can overload this function (like the playlistItemYUVSource).
  // If a second videoHandler item is provided, the difference values will be drawn.
  virtual void drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2=NULL);

  // Set the values and update the controls. Only emit an event if emitSignal is set.
  virtual void setFrameSize(QSize size, bool emitSignal = false);
  void setStartEndFrame(indexRange range, bool emitSignal = false);

  virtual int getNrFramesCached() { return pixmapCache.size(); }
  // Caching: Load the frame with the given index into the cache
  virtual void cacheFrame(int frameIdx);
  virtual QList<int> getCachedFrames() { return pixmapCache.keys(); }

public slots:
  // Caching: Remove the frame with the given index from the cache
  virtual void removeFrameFromCache(int frameIdx);

signals:
  void signalHandlerChanged(bool redrawNeeded, bool cacheChanged);

  // This video handler want's to know the current number of frames. Whatever the source for the data
  // is, it has to provide it. The handler of this signal has to use the setFrameLimits() function to set
  // the new values.
  void signalGetFrameLimits();

protected:

  // --- Drawing: We keep a buffer of the current frame as RGB image so wen don't have to ´convert
  // it from the source every time a draw event is triggered. But if currentFrameIdx is not identical to
  // the requested frame in the draw event, we will have to update currentFrame.
  QPixmap    currentFrame;
  int        currentFrameIdx;

  // Compute the MSE between the given char sources for numPixels bytes
  float computeMSE( unsigned char *ptr, unsigned char *ptr2, int numPixels ) const;

  // The video handler want's to draw a frame but it's not cached yet and has to be loaded.
  // The actual loading/conversion has to be performed by a specific video format handler implementation.
  // After this function was called, currentFrame should contain the requested frame and currentFrameIdx should
  // be equal to frameIndex.
  virtual void loadFrame(int frameIndex) = 0;
  // The video handler wants to cache a frame. After the operation the frameToCache should contain
  // the requested frame. No other internal state of the specific video format handler should be changed.
  // currentFrame/currentFrameIdx is still the frame on screen. This is called from a background thread.
  virtual void loadFrameForCaching(int frameIndex, QPixmap &frameToCache) = 0;

  // Every video item has a frameRate, start frame, end frame a sampling, a size and a number of frames
  indexRange startEndFrame;
  indexRange startEndFrameLimit;
  bool startEndFrameChanged; // True if the user changed the start/end frame. In this case we don't update the spin boxes if updateStartEndFrameLimit is called
  double frameRate;
  int sampling;
  QSize frameSize;

  // --- Caching
  QMap<int, QPixmap> pixmapCache;
  QTimer             cachingTimer;
  QMutex             cachingFrameSizeMutex; // Do not change the frameSize while this mutex is locked (by the caching process)

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

  // We also keep a QImage version of the same frame for fast lookup of pixel values. If there is a look up and this
  // is not up to date, we update it.
  QRgb getPixelVal(QPoint pixelPos);
  QRgb getPixelVal(int x, int y);
  QImage     currentFrame_Image;
  int        currentFrame_Image_FrameIdx;

  bool controlsCreated;    ///< Have the video controls been created already?

signals:
  // Start the caching timer (connected to cachingTimer::start())
  void cachingTimerStart();
private slots:
  void cachingTimerEvent();

private slots:
  // All the valueChanged() signals from the controls are connected here.
  void slotVideoControlChanged();
};

#endif // VIDEOHANDLER_H
