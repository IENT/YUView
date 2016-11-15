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

#ifndef FRAMEHANDLER_H
#define FRAMEHANDLER_H

#include "typedef.h"
#include "playlistItem.h"

#include "ui_frameHandler.h"

#include <assert.h>

/* TODO
*/
class frameHandler : public QObject
{
  Q_OBJECT

public:

  /*
  */
  frameHandler();

  // Get the size of the (current) frame
  QSize getFrameSize() const { return frameSize; }
  
  // Draw the (current) frame with the given zoom factor
  void drawFrame(QPainter *painter, double zoomFactor);

  // Set the values and update the controls. Only emit an event if emitSignal is set.
  virtual void setFrameSize(const QSize &size, bool emitSignal = false);

  // Return the RGB values of the given pixel. If a second item is provided, return the difference values to that item.
  virtual ValuePairList getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2=NULL);
  // Is the pixel under the cursor brighter or darker than the middle brightness level?
  virtual bool isPixelDark(const QPoint &pixelPos);

  // Is the current format of the frameHandler valid? The default implementation will check if the frameSize is
  // valid but more specialized implementations may also check other thigs: For example the videoHandlerYUV also
  // checks if a valid YUV format is set.
  virtual bool isFormatValid() const { return frameSize.width() > 0 && frameSize.height() > 0; }

  // Calculate the difference of this frameHandler to another frameHandler. This
  // function can be overloaded by more specialized video items. For example the videoHandlerYUV
  // overloads this and calculates the difference directly on the YUV values (if possible).
  virtual QPixmap calculateDifference(frameHandler *item2, const int frame, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference);
  
  // Create the frame controls and return a pointer to the layout. This can be used by
  // inherited classes to create a properties widget.
  // isSizeFixed: For example a YUV file does not have a fixed size (the user can change this),
  // other sources might provide a fixed size which the user cannot change (HEVC file, png image sequences ...)
  // If the size is fixed, do not add the controls for the size.
  virtual QLayout *createFrameHandlerControls(bool isSizeFixed=false);

  // Draw the pixel values of the visible pixels in the center of each pixel.
  // Only draw values for the given range of pixels and frame index.
  // The playlistItemVideo implememntation of this function will draw the RGB vales. However, if a derived class knows other
  // source values to show it can overload this function (like the playlistItemYUVSource).
  // If a second frameHandler item is provided, the difference values will be drawn (set markDifference if only the difference is marked).
  virtual void drawPixelValues(QPainter *painter, const int frameIdx, const QRect &videoRect, const double zoomFactor, frameHandler *item2=NULL, const bool markDifference=false);
  
  QImage getCurrentFrameAsImage() const { return currentImage; }

  // Load the current image from file and set the correct size.
  bool loadCurrentImageFromFile(const QString &filePath);
 
signals:
  void signalHandlerChanged(bool redrawNeeded, bool cacheChanged);

protected:

  QImage currentImage;
  QSize  frameSize;

  // Get the pixel value from currentImage. Make sure that currentImage is the correct image.
  virtual QRgb getPixelVal(const QPoint &pixelPos) { return currentImage.pixel(pixelPos); }
  virtual QRgb getPixelVal(int x, int y)           { return currentImage.pixel(x, y);     }
  
private:

  // A list of all frame size presets. Only used privately in this class. Defined in the .cpp file.
  class frameSizePresetList;

  // The (static) list of frame size presets (like CIF, QCIF, 4k ...)
  static frameSizePresetList presetFrameSizes;
 
  SafeUi<Ui::frameHandler> ui;

protected slots:

  // All the valueChanged() signals from the controls are connected here.
  virtual void slotVideoControlChanged();
};

#endif // FRAMEHANDLER_H
