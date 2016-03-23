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

class videoHandlerDifference : public videoHandler, Ui_videoHandlerDifference
{
  Q_OBJECT

public:

  explicit videoHandlerDifference();
  virtual ~videoHandlerDifference() {};

  virtual void loadFrame(int frameIndex);
  
  // Override from videHandler. We return the range from the child items.
  virtual indexRange getFrameIndexRange() Q_DECL_OVERRIDE;

  // Are both inputs valid and can be used?
  bool inputsValid();

  // Create the yuv controls and return a pointer to the layout. 
  virtual QLayout *createDifferenceHandlerControls(QWidget *parentWidget);

  // Set the two video inputs. This will also update the number frames, the controls and the frame size.
  // The signal signalHandlerChanged will be emitted if a redraw is required.
  void setInputVideos(videoHandler *childVideo0, videoHandler *childVideo1);

  QList<infoItem> differenceInfoList;

  // Draw the pixel values depending on the children type. E.g. if both children are YUV handlers, draw the YUV differences.
  virtual void drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2=NULL) Q_DECL_OVERRIDE;

  // The difference overloads this and returns the difference values alongside the displayed values
  virtual ValuePairList getPixelValues(QPoint pixelPos);

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
  videoHandler *inputVideo[2];

  bool controlsCreated;    ///< Have the controls been created already?

  // Recursively scan the LCU
  bool hierarchicalPosition( int x, int y, int blockSize, int &firstX, int &firstY, int &partIndex, const QImage diffImg );

};

#endif //VIDEOHANDLERDIFFERENCE_H
