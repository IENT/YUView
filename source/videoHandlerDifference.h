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

class videoHandlerDifference : public videoHandler
{
  Q_OBJECT

public:

  explicit videoHandlerDifference();
  virtual ~videoHandlerDifference() {};

  virtual void loadFrame(int frameIndex);

  virtual unsigned int getNumberFrames();

  // Are both inputs valid and can be used?
  bool inputsValid();

  // Set the two video inputs. This will also update the number frames, the controls and the frame size.
  // The signal signalHandlerChanged will be emitted if a redraw is required.
  void setInputVideos(videoHandler *childVideo0, videoHandler *childVideo1);

  QList<infoItem> differenceInfoList;

  // The difference overloads this and returns the difference values alongside the displayed values
  virtual ValuePairList getPixelValues(QPoint pixelPos);

private:

  // The two videos that the difference will be calculated from
  videoHandler *inputVideo[2];

};

#endif //VIDEOHANDLERDIFFERENCE_H
