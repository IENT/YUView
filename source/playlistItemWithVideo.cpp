/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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
  unresolvableError = false;
};

void playlistItemWithVideo::connectVideo()
{
  // Forward these signals from the video source up
  connect(video.data(), &videoHandler::signalHandlerChanged, this, &playlistItem::signalItemChanged);
}

void playlistItemWithVideo::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues)
{
  if (unresolvableError)
  {
    playlistItem::drawItem(painter, frameIdx, zoomFactor, drawRawValues);
    return;
  }

  indexRange range = getStartEndFrameLimits();
  if (frameIdx >= range.first && frameIdx <= range.second)
    video->drawFrame(painter, frameIdx, zoomFactor, drawRawValues);
}

void playlistItemWithVideo::loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals)
{
  auto state = video->needsLoading(frameIdx, loadRawData);

  if (state == LoadingNeeded)
  {
    // Load the requested current frame
    DEBUG_PLVIDEO("playlistItemWithVideo::loadFrame loading frame %d%s%s", frameIdx, playing ? " playing" : "", loadRawData ? " raw" : "");
    isFrameLoading = true;
    video->loadFrame(frameIdx);
    isFrameLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, false);
  }
  
  if (playing && (state == LoadingNeeded || state == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdx + 1;
    if (nextFrameIdx <= startEndFrame.second)
    {
      DEBUG_PLVIDEO("playlistItemWithVideo::loadFrame loading frame into double buffer %d%s%s", nextFrameIdx, playing ? " playing" : "", loadRawData ? " raw" : "");
      isFrameLoadingDoubleBuffer = true;
      video->loadFrame(nextFrameIdx, true);
      isFrameLoadingDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

itemLoadingState playlistItemWithVideo::needsLoading(int frameIdx, bool loadRawValues)
{
  // See if the item has so many frames
  indexRange range = getStartEndFrameLimits();
  if (frameIdx < range.first || frameIdx > range.second)
    return LoadingNotNeeded;

  if (video)
    return video->needsLoading(frameIdx, loadRawValues);
  return LoadingNotNeeded;
}