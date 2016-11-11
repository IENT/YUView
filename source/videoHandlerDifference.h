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

#ifndef VIDEOHANDLERDIFFERENCE_H
#define VIDEOHANDLERDIFFERENCE_H

#include "videoHandler.h"
#include "ui_videoHandlerDifference.h"

class videoHandlerDifference : public videoHandler
{
  Q_OBJECT

public:

  explicit videoHandlerDifference();
  virtual ~videoHandlerDifference();

  virtual void loadFrame(int frameIndex) Q_DECL_OVERRIDE;
  virtual void loadFrameForCaching(int frameIndex, QPixmap &frameToCache) Q_DECL_OVERRIDE { Q_UNUSED(frameIndex); Q_UNUSED(frameToCache); };
  
  // Are both inputs valid and can be used?
  bool inputsValid();

  // Create the yuv controls and return a pointer to the layout. 
  virtual QLayout *createDifferenceHandlerControls(QWidget *parentWidget);

  // Set the two video inputs. This will also update the number frames, the controls and the frame size.
  // The signal signalHandlerChanged will be emitted if a redraw is required.
  void setInputVideos(frameHandler *childVideo0, frameHandler *childVideo1);

  QList<infoItem> differenceInfoList;

  // Draw the pixel values depending on the children type. E.g. if both children are YUV handlers, draw the YUV differences.
  virtual void drawPixelValues(QPainter *painter, int frameIdx, QRect videoRect, double zoomFactor, frameHandler *item2=NULL) Q_DECL_OVERRIDE;

  // The difference overloads this and returns the difference values (A-B)
  virtual ValuePairList getPixelValues(QPoint pixelPos, int frameIdx, frameHandler *item2=NULL) Q_DECL_OVERRIDE;

  // Calculate the position of the first difference and add the info to the list
  void reportFirstDifferencePosition(QList<infoItem> &infoList);
    
private slots:
  void slotDifferenceControlChanged();

protected:
  bool markDifference;  // Mark differences?
  int  amplificationFactor;

private:

  typedef enum
  {
    CodingOrder_HEVC
  } CodingOrder;
  CodingOrder codingOrder;

  // The two videos that the difference will be calculated from
  frameHandler *inputVideo[2];

  bool controlsCreated;    ///< Have the controls been created already?

  // Recursively scan the LCU
  bool hierarchicalPosition( int x, int y, int blockSize, int &firstX, int &firstY, int &partIndex, const QImage diffImg );

  Ui::videoHandlerDifference *ui;

};

#endif //VIDEOHANDLERDIFFERENCE_H
