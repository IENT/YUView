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

#include "playlistItemWithVideo.h"

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define PLAYLISTITEMWITHVIDEO_DEBUG_LOADING 0
#if PLAYLISTITEMWITHVIDEO_DEBUG_LOADING && !NDEBUG
#define DEBUG_PLVIDEO qDebug
#else
#define DEBUG_PLVIDEO(fmt,...) ((void)0)
#endif

playlistItemWithVideo::playlistItemWithVideo(const QString &itemNameOrFileName, playlistItemType type)
 : playlistItem(itemNameOrFileName, type), video(nullptr) 
{
  // Nothing is currently being loaded
  isFrameLoading = false;
  isFrameLoadingDoubleBuffer = false;
};

void playlistItemWithVideo::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  indexRange range = getStartEndFrameLimits();
  if (frameIdx >= range.first && frameIdx <= range.second)
    video->drawFrame(painter, frameIdx, zoomFactor);
}

void playlistItemWithVideo::loadFrame(int frameIdx, bool playing)
{
  auto state = video->needsLoading(frameIdx);

  if (state == LoadingNeeded)
  {
    // Load the requested current frame
    DEBUG_PLVIDEO("playlistItemWithVideo::loadFrame loading frame %d %s", frameIdx, playing ? "(playing)" : "");
    isFrameLoading = true;
    video->loadFrame(frameIdx);
    isFrameLoading = false;
    emit signalItemChanged(true, false);
  }
  
  if (playing && (state == LoadingNeeded || state == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdx + 1;
    if (nextFrameIdx <= startEndFrame.second)
    {
      DEBUG_PLVIDEO("playlistItemWithVideo::loadFrame loading frame into double buffer %d %s", nextFrameIdx, playing ? "(playing)" : "");
      isFrameLoadingDoubleBuffer = true;
      video->loadFrame(nextFrameIdx, true);
      isFrameLoadingDoubleBuffer = false;
      emit signalItemDoubleBufferLoaded();
    }
  }
}

itemLoadingState playlistItemWithVideo::needsLoading(int frameIdx)
{
  // See if the item has so many frames
  indexRange range = getStartEndFrameLimits();
  if (frameIdx < range.first || frameIdx > range.second)
    return LoadingNotNeeded;

  if (video)
    return video->needsLoading(frameIdx);
  return LoadingNotNeeded;
}