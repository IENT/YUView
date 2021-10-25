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

#include "playlistItemDifference.h"

#include <QPainter>

#include <common/FunctionsGui.h>

// Activate this if you want to know when which difference is loaded
#define PLAYLISTITEMDIFFERENCE_DEBUG_LOADING 0
#if PLAYLISTITEMDIFFERENCE_DEBUG_LOADING && !NDEBUG
#define DEBUG_DIFF qDebug
#else
#define DEBUG_DIFF(fmt, ...) ((void)0)
#endif

#define DIFFERENCE_INFO_TEXT                                                                       \
  "Please drop two video item's onto this difference item to calculate the difference."

playlistItemDifference::playlistItemDifference() : playlistItemContainer("Difference Item")
{
  setIcon(0, functionsGui::convertIcon(":img_difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the
  // difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  this->prop.propertiesWidgetTitle = "Difference Properties";

  // For a difference item, only 2 items are allowed.
  this->maxItemCount   = 2;
  this->frameLimitsMax = false;
  this->infoText       = DIFFERENCE_INFO_TEXT;

  connect(&difference,
          &video::videoHandlerDifference::signalHandlerChanged,
          this,
          &playlistItemDifference::signalItemChanged);
}

/* For a difference item, the info list is just a list of the names of the
 * child elements.
 */
InfoData playlistItemDifference::getInfo() const
{
  InfoData info("Difference Info");

  if (childCount() >= 1)
    info.items.append(InfoItem(QString("File 1"), getChildPlaylistItem(0)->properties().name));
  if (childCount() >= 2)
    info.items.append(InfoItem(QString("File 2"), getChildPlaylistItem(1)->properties().name));

  // Report the position of the first difference in coding order
  difference.reportFirstDifferencePosition(info.items);

  // Report MSE
  for (int i = 0; i < difference.differenceInfoList.length(); i++)
  {
    InfoItem p = difference.differenceInfoList[i];
    info.items.append(p);
  }

  return info;
}

void playlistItemDifference::drawItem(QPainter *painter,
                                      int       frameIdx,
                                      double    zoomFactor,
                                      bool      drawRawData)
{
  DEBUG_DIFF("playlistItemDifference::drawItem frameIdx %d %s",
             frameIdx,
             childLlistUpdateRequired ? "childLlistUpdateRequired" : "");
  if (childLlistUpdateRequired)
  {
    // Update the 'childList' and connect the signals/slots
    updateChildList();

    // Update the items in the difference item
    video::FrameHandler *childVideo0 = nullptr;
    video::FrameHandler *childVideo1 = nullptr;
    if (childCount() >= 1)
      childVideo0 = getChildPlaylistItem(0)->getFrameHandler();
    if (childCount() >= 2)
      childVideo1 = getChildPlaylistItem(1)->getFrameHandler();

    difference.setInputVideos(childVideo0, childVideo1);

    if (childCount() > 2)
      infoText = "More than two items are not supported.\n" DIFFERENCE_INFO_TEXT;
    else
      infoText = DIFFERENCE_INFO_TEXT;
  }

  if (childCount() != 2 || !difference.inputsValid())
    // Draw the emptyText
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  else
  {
    // draw the videoHandler
    difference.drawDifferenceFrame(painter, frameIdx, zoomFactor, drawRawData);
  }
}

ItemLoadingState playlistItemDifference::needsLoading(int frameIdx, bool loadRawData)
{
  return this->difference.needsLoading(frameIdx, loadRawData);
}

QSize playlistItemDifference::getSize() const
{
  if (!difference.inputsValid())
  {
    // Return the size of the empty text.
    return playlistItemContainer::getSize();
  }

  auto s = difference.getFrameSize();
  return QSize(s.width, s.height);
}

void playlistItemDifference::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  preparePropertiesWidget(QStringLiteral("playlistItemDifference"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame;
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then YUV controls
  // (format,...)
  vAllLaout->addLayout(difference.createFrameHandlerControls(true));
  vAllLaout->addWidget(line);
  vAllLaout->addLayout(difference.createDifferenceHandlerControls());

  vAllLaout->insertStretch(-1, 1); // Push controls up
}

void playlistItemDifference::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  YUViewDomElement d = root.ownerDocument().createElement("playlistItemDifference");

  // Append the indexed item's properties
  playlistItem::appendPropertiesToPlaylist(d);

  this->difference.savePlaylist(d);
  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemDifference *
playlistItemDifference::newPlaylistItemDifference(const YUViewDomElement &root)
{
  playlistItemDifference *newDiff = new playlistItemDifference();

  // Load properties from the parent classes
  newDiff->difference.loadPlaylist(root);
  playlistItem::loadPropertiesFromPlaylist(root, newDiff);

  // The difference might just have children that have to be added. After adding the children don't
  // forget to call updateChildItems().

  return newDiff;
}

ValuePairListSets playlistItemDifference::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  if (childCount() >= 1)
    newSet.append("Item A",
                  getChildPlaylistItem(0)->getFrameHandler()->getPixelValues(pixelPos, frameIdx));

  if (childCount() >= 2)
  {
    newSet.append("Item B",
                  getChildPlaylistItem(1)->getFrameHandler()->getPixelValues(pixelPos, frameIdx));
    newSet.append("Diff (A-B)", difference.getPixelValues(pixelPos, frameIdx, nullptr));
  }

  return newSet;
}

void playlistItemDifference::loadFrame(int  frameIdx,
                                       bool playing,
                                       bool loadRawData,
                                       bool emitSignals)
{
  if (childCount() != 2 || !difference.inputsValid())
    return;

  auto state = difference.needsLoading(frameIdx, loadRawData);
  if (state == ItemLoadingState::LoadingNeeded)
  {
    // Load the requested current frame
    DEBUG_DIFF("playlistItemDifference::loadFrame loading difference for frame %d", frameIdx);
    isDifferenceLoading = true;
    // Since every playlist item can have it's own relative indexing, we need two frame indices
    difference.loadFrameDifference(frameIdx);
    isDifferenceLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }

  if (playing && (state == ItemLoadingState::LoadingNeeded ||
                  state == ItemLoadingState::LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdx + 1;
    if (nextFrameIdx <= this->properties().startEndRange.second)
    {
      DEBUG_DIFF("playlistItemDifference::loadFrame loading difference into double buffer %d %s",
                 nextFrameIdx,
                 playing ? "(playing)" : "");
      isDifferenceLoadingToDoubleBuffer = true;
      difference.loadFrameDifference(frameIdx, true);
      isDifferenceLoadingToDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

bool playlistItemDifference::isLoading() const { return this->isDifferenceLoading; }

bool playlistItemDifference::isLoadingDoubleBuffer() const
{
  return this->isDifferenceLoadingToDoubleBuffer;
}

void playlistItemDifference::childChanged(bool redraw, recacheIndicator recache)
{
  // One of the child items changed and needs to redraw. This means that the difference is out of
  // date and has to be recalculated.
  difference.invalidateAllBuffers();
  playlistItemContainer::childChanged(redraw, recache);
}