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

#include "videoHandlerDifference.h"

videoHandlerDifference::videoHandlerDifference() : videoHandler()
{
  // preset internal values
  inputVideo[0] = NULL;
  inputVideo[1] = NULL;


}

void videoHandlerDifference::loadFrame(int frameIndex)
{
  // Calculate the difference between the inputVideos
  if (!inputsValid())
    return;

  differenceInfoList.clear();
  currentFrame = inputVideo[0]->calculateDifference(inputVideo[1], frameIndex, differenceInfoList);
  currentFrameIdx = frameIndex;
}

unsigned int videoHandlerDifference::getNumberFrames()
{
  if (inputVideo[0] && inputVideo[1])
    return std::min( inputVideo[0]->getNumberFrames(), inputVideo[1]->getNumberFrames() );
  return 0;
}

bool videoHandlerDifference::inputsValid()
{
  if (inputVideo[0] == NULL || inputVideo[1] == NULL)
    return false;
  
  if (inputVideo[0]->getNumberFrames() == 0 || inputVideo[1]->getNumberFrames() == 0)
    return false;

  return true;
}

void videoHandlerDifference::setInputVideos(videoHandler *childVideo0, videoHandler *childVideo1)
{
  if (inputVideo[0] != childVideo0 || inputVideo[1] != childVideo1)
  {
    // Something changed
    inputVideo[0] = childVideo0;
    inputVideo[1] = childVideo1;

    if (inputsValid())
    {
      // We have two valid video "children"

      // Get the frame size of the difference (min in x and y direction), and set it.
      QSize size0 = inputVideo[0]->getVideoSize();
      QSize size1 = inputVideo[1]->getVideoSize();
      QSize diffSize = QSize( qMin(size0.width(), size1.width()), qMin(size0.height(), size1.height()) );
      setFrameSize(diffSize);

      // Set the start and end frame (0 to nrFrames).
      indexRange diffRange(0, getNumberFrames());
      setStartEndFrame(diffRange);
    }

    // If something changed, we might need a redraw
    emit signalHandlerChanged(true);
  }
}

ValuePairList videoHandlerDifference::getPixelValues(QPoint pixelPos)
{
  if (!inputsValid())
    return ValuePairList();

  return inputVideo[0]->getPixelValuesDifference(inputVideo[1], pixelPos);
}