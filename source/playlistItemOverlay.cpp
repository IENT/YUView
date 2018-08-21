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

#include "playlistItemOverlay.h"

#include <limits>
#include <QPainter>

#define PLAYLISTITEMOVERLAY_DEBUG 0
#if PLAYLISTITEMOVERLAY_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_OVERLAY qDebug
#else
#define DEBUG_OVERLAY(fmt,...) ((void)0)
#endif

playlistItemOverlay::playlistItemOverlay() :
  playlistItemContainer("Overlay Item")
{
  setIcon(0, convertIcon(":img_overlay.png"));
  // Enable dropping for overlay objects. The user can drop items here to draw them as an overlay.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // This text is drawn if there are no child items in the overlay
  infoText = "Please drop some items onto this overlay. All child items will be drawn on top of each other.";

  alignmentMode = 0;  // Top left
  manualAlignment = QPoint(0,0);
  vSpacer = nullptr;
  startEndFrame = indexRange(-1,-1);
}

/* For an overlay item, the info list is just a list of the names of the
 * child elements.
 */
infoData playlistItemOverlay::getInfo() const
{
  infoData info("Overlay Info");

  // Add the size of this playlistItemOverlay
  info.items.append(infoItem("Overlay Size",QString("(%1,%2)").arg(getSize().width()).arg(getSize().height())));

  // Add the sizes of all child items
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      info.items.append(infoItem(QString("Item %1 size").arg(i),QString("(%1,%2)").arg(childSize.width()).arg(childSize.height())));
    }
  }
  return info;
}

ValuePairListSets playlistItemOverlay::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  // The given pixelPos is relative to the bounding rectangle. For every child we have to calculate
  // the relative point within that item.
  QPoint relPoint = boundingRect.topLeft() + pixelPos;

  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      // First check if the point is even within the child bounding rectangle
      if (childItems[i].contains(relPoint))
      {
        // Calculate the relative pixel position within this child item
        QPoint childPixelPos = relPoint - childItems[i].topLeft();

        ValuePairListSets childSets = childItem->getPixelValues(childPixelPos, frameIdx);
        // Append the item id for every set in the child
        for (int j = 0; j < childSets.count(); j++)
        {
          childSets[j].first = QString("Item %1 - %2").arg(i).arg(childSets[j].first);
        }
        newSet.append(childSets);
      }
    }
  }

  return newSet;
}

itemLoadingState playlistItemOverlay::needsLoading(int frameIdx, bool loadRawdata)
{
  // The overlay needs to load if one of the child items needs to load
  for (int i = 0; i < childCount(); i++)
  {
    if (getChildPlaylistItem(i)->needsLoading(frameIdx, loadRawdata) == LoadingNeeded)
    {
      DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNeeded child %s", getChildPlaylistItem(i)->getName().toLatin1().data());
      return LoadingNeeded;
    }
  }
  for (int i = 0; i < childCount(); i++)
  {
    if (getChildPlaylistItem(i)->needsLoading(frameIdx, loadRawdata) == LoadingNeededDoubleBuffer)
    {
      DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNeededDoubleBuffer child %s", getChildPlaylistItem(i)->getName().toLatin1().data());
      return LoadingNeededDoubleBuffer;
    }
  }

  DEBUG_OVERLAY("playlistItemOverlay::needsLoading LoadingNotNeeded");
  return LoadingNotNeeded;
}

void playlistItemOverlay::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  DEBUG_OVERLAY("playlistItemOverlay::drawItem frame %d", frameIdx);

  if (childLlistUpdateRequired)
    updateChildList();

  if (childCount() == 0)
  {
    playlistItem::drawItem(painter, frameIdx, zoomFactor, drawRawData);
    return;
  }

  // Update the layout if the number of items changedupdateLayout
  updateLayout();

  // Translate to the center of this overlay item
  painter->translate(centerRoundTL(boundingRect) * zoomFactor * -1);

  // Draw all child items at their positions
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      QPoint center = centerRoundTL(childItems[i]);
      painter->translate(center * zoomFactor);
      childItem->drawItem(painter, frameIdx, zoomFactor, drawRawData);
      painter->translate(center * zoomFactor * -1);
    }
  }

  // Reverse translation to the center of this overlay item
  painter->translate(centerRoundTL(boundingRect) * zoomFactor);
}

QSize playlistItemOverlay::getSize() const
{
  if (childCount() == 0)
    return playlistItemContainer::getSize();

  return boundingRect.size();
}

void playlistItemOverlay::updateLayout(bool checkNumber)
{
  if (childCount() == 0)
  {
    childItems.clear();
    boundingRect = QRect();
    return;
  }

  if (checkNumber && childCount() == childItems.count())
    return;

  DEBUG_OVERLAY("playlistItemOverlay::updateLayout%s", checkNumber ? " checkNumber" : "");

  if (childItems.count() != childCount())
  {
    // Resize the childItems list
    childItems.clear();
    for (int i = 0; i < childCount(); i++)
    {
      childItems.append(QRect());
    }
  }

  // Update the layout in all children which are also playlistItemOverlays
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    playlistItemOverlay *childOverlay = dynamic_cast<playlistItemOverlay*>(childItem);
    if (childOverlay)
      childOverlay->updateLayout();
  }

  playlistItem *firstItem = getChildPlaylistItem(0);
  boundingRect.setSize(firstItem->getSize());
  boundingRect.moveCenter(QPoint(0,0));

  QRect firstItemRect;
  firstItemRect.setSize(firstItem->getSize());
  firstItemRect.moveCenter(QPoint(0,0));
  childItems[0] = firstItemRect;
  DEBUG_OVERLAY("playlistItemOverlay::updateLayout item 0 size (%d,%d) firstItemRect (%d,%d)", firstItem->getSize().width(), firstItem->getSize().height(), firstItemRect.left(), firstItemRect.top());

  // Align the rest of the items
  int alignmentMode = 0;
  if (propertiesWidget != nullptr)
    alignmentMode = ui.alignmentMode->currentIndex();

  DEBUG_OVERLAY("playlistItemOverlay::updateLayout childCount %d", childCount());
  for (int i = 1; i < childCount(); i++)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      QRect targetRect;
      targetRect.setSize(childSize);
      targetRect.moveCenter(QPoint(0,0));

      // Align based on alignment mode (must be between 0 and 8)
      if (alignmentMode == 0)
        targetRect.moveTopLeft(firstItemRect.topLeft());
      else if (alignmentMode == 1)
        targetRect.moveTop(firstItemRect.top());
      else if (alignmentMode == 2)
        targetRect.moveTopRight(firstItemRect.topRight());
      else if (alignmentMode == 3)
        targetRect.moveLeft(firstItemRect.left());
      else if (alignmentMode == 5)
        targetRect.moveRight(firstItemRect.right());
      else if (alignmentMode == 6)
        targetRect.moveBottomLeft(firstItemRect.bottomLeft());
      else if (alignmentMode == 7)
        targetRect.moveBottom(firstItemRect.bottom());
      else if (alignmentMode == 8)
        targetRect.moveBottomRight(firstItemRect.bottomRight());
      else
        assert(alignmentMode == 4);

      // Add the offset
      targetRect.translate(manualAlignment);

      // Set item bounding rectangle
      childItems[i] = targetRect;

      DEBUG_OVERLAY("playlistItemOverlay::updateLayout item %d size (%d,%d) alignmentMode %d targetRect (%d,%d)", i, childSize.width(), childSize.height(), alignmentMode, targetRect.left(), targetRect.top());

      // Expand the bounding rectangle
      boundingRect = boundingRect.united(targetRect);
    }
  }
}

void playlistItemOverlay::createPropertiesWidget()
{
  // Absolutely always only call this once
  Q_ASSERT_X(!propertiesWidget, "playlistItemOverlay::createPropertiesWidget", "Always create the properties only once!");

  // Create a new widget and populate it with controls
  propertiesWidget.reset(new QWidget);
  ui.setupUi(propertiesWidget.data());

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  ui.verticalLayout->insertLayout(0, createPlaylistItemControls());
  ui.verticalLayout->insertStretch(3, 1);

  // Alignment mode
  ui.alignmentMode->addItems(QStringList() << "Top Left" << "Top Center" << "Top Right");
  ui.alignmentMode->addItems(QStringList() << "Center Left" << "Center" << "Center Right");
  ui.alignmentMode->addItems(QStringList() << "Bottom Left" << "Bottom Center" << "Bottom Right");
  ui.alignmentMode->setCurrentIndex(alignmentMode);

  // Offset
  ui.alignmentHozizontal->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  ui.alignmentVertical->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  ui.alignmentHozizontal->setValue(manualAlignment.x());
  ui.alignmentVertical->setValue(manualAlignment.y());

  // Add the Container Layout
  ui.verticalLayout->insertLayout(3,createContainerItemControls());

  // Connect signals/slots
  connect(ui.alignmentMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &playlistItemOverlay::controlChanged);
  connect(ui.alignmentHozizontal, QOverload<int>::of(&QSpinBox::valueChanged), this, &playlistItemOverlay::controlChanged);
  connect(ui.alignmentVertical, QOverload<int>::of(&QSpinBox::valueChanged), this, &playlistItemOverlay::controlChanged);
}

void playlistItemOverlay::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  QDomElementYUView d = root.ownerDocument().createElement("playlistItemOverlay");

  // Append the playlist item properties
  playlistItem::appendPropertiesToPlaylist(d);

  // Append the overlay properties
  d.appendProperiteChild("alignmentMode", QString::number(alignmentMode));
  d.appendProperiteChild("manualAlignmentX", QString::number(manualAlignment.x()));
  d.appendProperiteChild("manualAlignmentY", QString::number(manualAlignment.y()));

  // Append all children
  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemOverlay *playlistItemOverlay::newPlaylistItemOverlay(const QDomElementYUView &root, const QString &filePath)
{
  Q_UNUSED(filePath);

  playlistItemOverlay *newOverlay = new playlistItemOverlay();

  int alignment = root.findChildValue("alignmentMode").toInt();
  int manualAlignmentX = root.findChildValue("manualAlignmentX").toInt();
  int manualAlignmentY = root.findChildValue("manualAlignmentY").toInt();

  newOverlay->alignmentMode = alignment;
  newOverlay->manualAlignment = QPoint(manualAlignmentX, manualAlignmentY);

  DEBUG_OVERLAY("playlistItemOverlay::newPlaylistItemOverlay alignmentMode %d manualAlignment (%d,%d)", alignment, manualAlignmentX, manualAlignmentY);
  playlistItem::loadPropertiesFromPlaylist(root, newOverlay);

  return newOverlay;
}

void playlistItemOverlay::controlChanged(int idx)
{
  Q_UNUSED(idx);

  // One of the controls changed. Update values and emit the redraw signal
  alignmentMode = ui.alignmentMode->currentIndex();
  manualAlignment.setX(ui.alignmentHozizontal->value());
  manualAlignment.setY(ui.alignmentVertical->value());

  // No new item was added but update the layout of the items
  updateLayout(false);

  emit signalItemChanged(true, RECACHE_NONE);
}

void playlistItemOverlay::childChanged(bool redraw, recacheIndicator recache)
{
  if (redraw)
    updateLayout(false);

  playlistItemContainer::childChanged(redraw, recache);
}

void playlistItemOverlay::loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals)
{
  // Does one of the items need loading?
  bool itemLoadedDoubleBuffer = false;
  bool itemLoaded = false;

  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *item = getChildPlaylistItem(i);
    auto state = item->needsLoading(frameIdx, loadRawData);
    if (state != LoadingNotNeeded)
    {
      // Load the requested current frame (or the double buffer) without emitting any signals.
      // We will emit the signal that loading is complete when all overlay items have loaded.
      DEBUG_OVERLAY("playlistItemWithVideo::loadFrame loading frame %d%s%s", frameIdx, playing ? " playing" : "", loadRawData ? " raw" : "");
      item->loadFrame(frameIdx, playing, loadRawData, false);
    }

    if (state == LoadingNeeded)
      itemLoaded = true;
    if (playing && (state == LoadingNeeded || state == LoadingNeededDoubleBuffer))
      itemLoadedDoubleBuffer = true;
  }

  if (emitSignals && itemLoaded)
    emit signalItemChanged(true, RECACHE_NONE);
  if (emitSignals && itemLoadedDoubleBuffer)
    emit signalItemDoubleBufferLoaded();
}

bool playlistItemOverlay::isLoading() const
{
  // We are loading if one of the child items is loading
  for (int i = 0; i < childCount(); i++)
    if (getChildPlaylistItem(i)->isLoading())
      return true;
  return false;
}

bool playlistItemOverlay::isLoadingDoubleBuffer() const
{
  // We are loading to the double buffer if one of the child items is loading to the double buffer
  for (int i = 0; i < childCount(); i++)
    if (getChildPlaylistItem(i)->isLoadingDoubleBuffer())
      return true;
  return false;
}