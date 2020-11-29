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

#include "playlistItemResample.h"

#include <QPainter>

#include "common/functions.h"

// Activate this if you want to know when which difference is loaded
#define PLAYLISTITEMRESAMPLE_DEBUG_LOADING 0
#if PLAYLISTITEMRESAMPLE_DEBUG_LOADING && !NDEBUG
#define DEBUG_RESAMPLE qDebug
#else
#define DEBUG_RESAMPLE(fmt,...) ((void)0)
#endif

#define RESAMPLE_INFO_TEXT "Please drop an item onto this item to show a resampled version of it."

playlistItemResample::playlistItemResample()
  : playlistItemContainer("Resample Item")
{
  setIcon(0, functions::convertIcon(":img_resample.png"));

  // The user can drop one item
  setFlags(flags() | Qt::ItemIsDropEnabled);

  this->prop.propertiesWidgetTitle = "Resample Properties";

  this->maxItemCount = 1;
  this->frameLimitsMax = false;
  this->infoText = RESAMPLE_INFO_TEXT;

  connect(&this->video, &frameHandler::signalHandlerChanged, this, &playlistItemResample::signalItemChanged);
}

/* For a resample item, the info list is just the name of the child item
 */
infoData playlistItemResample::getInfo() const
{
  infoData info("Resample Info");

  if (childCount() >= 1)
    info.items.append(infoItem(QString("File 1"), getChildPlaylistItem(0)->properties().name));
  
  return info;
}

void playlistItemResample::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);
  DEBUG_RESAMPLE("playlistItemResample::drawItem frameIdx %d %s", frameIdxInternal, childLlistUpdateRequired ? "childLlistUpdateRequired" : "");
  if (childLlistUpdateRequired)
  {
    // Update the 'childList' and connect the signals/slots
    updateChildList();
    
    if (childCount() == 1)
    {
      auto frameHandler = getChildPlaylistItem(0)->getFrameHandler();
      this->video.setInputVideo(frameHandler);
    }

    // Update the frame range
    this->startEndFrame = this->getStartEndFrameLimits();
  }

  if (childCount() != 1 || !this->video.inputValid())
    // Draw the emptyText
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  else
  {
    // draw the videoHandler
    int idx0 = getChildPlaylistItem(0)->getFrameIdxInternal(frameIdxInternal);
    this->video.drawFrame(painter, idx0, zoomFactor, drawRawData);
  }
}

QSize playlistItemResample::getSize() const
{ 
  if (!this->video.inputValid())
  {
    // Return the size of the empty text.
    return playlistItemContainer::getSize();
  }
  
  return this->video.getFrameSize(); 
}

void playlistItemResample::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  this->preparePropertiesWidget(QStringLiteral("playlistItemResample"));

  // On the top level everything is layout vertically
  auto vAllLaout = new QVBoxLayout(propertiesWidget.data());

  // First add the parents controls (first video controls (width/height...) then YUV controls (format,...)
  vAllLaout->addLayout(this->video.createResampleHandlerControls());

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);
}

void playlistItemResample::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  YUViewDomElement d = root.ownerDocument().createElement("playlistItemResample");

  // Append the indexed item's properties
  playlistItem::appendPropertiesToPlaylist(d);

  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemResample *playlistItemResample::newPlaylistItemResample(const YUViewDomElement &root)
{
  auto newItemResample = new playlistItemResample();

  // Load properties from the parent classes
  playlistItem::loadPropertiesFromPlaylist(root, newItemResample);

  // The resample might have one child item that has to be added. After adding the child don't forget
  // to call updateChildItems().
    
  return newItemResample;
}

ValuePairListSets playlistItemResample::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  return ValuePairListSets("RGB", this->video.getPixelValues(pixelPos, frameIdx));
}

itemLoadingState playlistItemResample::needsLoading(int frameIdx, bool loadRawData)
{
  return this->video.needsLoading(getFrameIdxInternal(frameIdx), loadRawData); 
}

void playlistItemResample::loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals) 
{
  Q_UNUSED(playing);
  if (childCount() != 1 || !this->video.inputValid())
    return;
  const auto frameIdxInternal = this->getFrameIdxInternal(frameIdx);
  
  auto state = this->video.needsLoading(frameIdxInternal, loadRawData);
  if (state == LoadingNeeded)
  {
    // Load the requested current frame
    DEBUG_RESAMPLE("playlistItemResample::loadFrame loading resampled frame %d", frameIdxInternal);
    this->isFrameLoading = true;
    // Since every playlist item can have it's own relative indexing, we need two frame indices
    auto idx = getChildPlaylistItem(0)->getFrameIdxInternal(frameIdxInternal);
    this->video.loadResampledFrame(frameIdx, idx);
    this->isFrameLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }
  
  if (playing && (state == LoadingNeeded || state == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdxInternal + 1;
    if (nextFrameIdx <= startEndFrame.second)
    {
      DEBUG_RESAMPLE("playlistItemResample::loadFrame loading resampled frame into double buffer %d %s", nextFrameIdx, playing ? "(playing)" : "");
      this->isFrameLoadingDoubleBuffer = true;
      // Since every playlist item can have it's own relative indexing, we need two frame indices
      auto idx = getChildPlaylistItem(0)->getFrameIdxInternal(nextFrameIdx);
      this->video.loadResampledFrame(frameIdx, idx, true);
      this->isFrameLoadingDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

void playlistItemResample::childChanged(bool redraw, recacheIndicator recache)
{
  // The child item changed and needs to redraw. This means that the resampled frame is out of date
  // and has to be recalculated.
  this->video.invalidateAllBuffers();
  playlistItemContainer::childChanged(redraw, recache);
}
