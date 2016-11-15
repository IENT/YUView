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

#include "playlistItemOverlay.h"
#include <QPainter>
#include <limits>
#include "statisticHandler.h"

#include "playlistItemRawFile.h"

playlistItemOverlay::playlistItemOverlay() :
  playlistItemContainer("Overlay Item")
{
  setIcon(0, QIcon(":img_overlay.png"));
  // Enable dropping for overlay objects. The user can drop items here to draw them as an overlay.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  emptyText = "Please drop some items onto this overlay. All child items will be drawn on top of each other.";

  alignmentMode = 0;  // Top left
  manualAlignment = QPoint(0,0);
  vSpacer = NULL;
  startEndFrame = indexRange(-1,-1);
}

/* For an overlay item, the info list is just a list of the names of the
 * child elemnts.
 */
QList<infoItem> playlistItemOverlay::getInfoList() const
{
  QList<infoItem> infoList;

  // Add the size of this playlistItemOverlay
  infoList.append( infoItem("Overlay Size",QString("(%1,%2)").arg(getSize().width()).arg(getSize().height())) );

  // Add the sizes of all child items
  for (int i = 0; i < childList.count(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      infoList.append( infoItem(QString("Item %1 size").arg(i),QString("(%1,%2)").arg(childSize.width()).arg(childSize.height())) );
    }
  }
  return infoList;
}

ValuePairListSets playlistItemOverlay::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  // The given pixelPos is relative to the bounding rect. For every child we have to calculate
  // the relative point within that item.
  QPoint relPoint = boundingRect.topLeft() + pixelPos;

  for (int i = 0; i < childList.count(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem) 
    {
      // First check if the point is even within the child rect
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

void playlistItemOverlay::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  if (childLlistUpdateRequired)
    updateChildList();

  if (childList.empty())
  {
    playlistItemContainer::drawEmptyContainerText(painter, zoomFactor);
    return;
  }

  // Update the layout if the number of items changed
  updateLayout();

  // Translate to the center of this overlay item
  painter->translate(boundingRect.centerRoundTL() * zoomFactor * -1);

  // Draw all child items at their positions
  for (int i = 0; i < childList.count(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      QPoint center = childItems[i].centerRoundTL();
      painter->translate( center * zoomFactor );
      childItem->drawItem(painter, frameIdx, zoomFactor, playback);
      painter->translate( center * zoomFactor * -1 );
    }
  }
  
  // Reverse translation to the center of this overlay item
  painter->translate( boundingRect.centerRoundTL() * zoomFactor );
}

QSize playlistItemOverlay::getSize() const
{ 
  if (childList.empty())
    return playlistItemContainer::getSize();

  return boundingRect.size();
}

void playlistItemOverlay::updateLayout(bool checkNumber)
{ 
  if (childList.empty())
  {
    childItems.clear();
    boundingRect = Rect();
    return;
  }

  if (checkNumber && childList.count() == childItems.count())
    return;
  
  if (childItems.count() != childList.count())
  {
    // Resize the childItems list
    childItems.clear();
    for (int i = 0; i < childList.count(); i++)
    {
      childItems.append(Rect());
    }
  }

  playlistItem *firstItem = getFirstChildPlaylistItem();
  boundingRect.setSize(firstItem->getSize());
  boundingRect.moveCenter(QPoint(0,0));

  Rect firstItemRect;
  firstItemRect.setSize(firstItem->getSize());
  firstItemRect.moveCenter(QPoint(0,0));
  childItems[0] = firstItemRect;
  
  // Align the rest of the items
  int alignmentMode = ui.alignmentMode->currentIndex();
  for (int i = 1; i < childList.count(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      Rect targetRect;
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

      // Set item rect
      childItems[i] = targetRect;

      // Expand the bounding rect
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

  // Conncet signals/slots
  connect(ui.alignmentMode, SIGNAL(currentIndexChanged(int)), this, SLOT(controlChanged(int)));
  connect(ui.alignmentHozizontal, SIGNAL(valueChanged(int)), this, SLOT(controlChanged(int)));
  connect(ui.alignmentVertical, SIGNAL(valueChanged(int)), this, SLOT(controlChanged(int)));
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

  playlistItem::loadPropertiesFromPlaylist(root, newOverlay);

  return newOverlay;
}

void playlistItemOverlay::controlChanged(int idx)
{
  Q_UNUSED(idx);

  // One of the controls changed. Update values and emit the redraw signal
  alignmentMode = ui.alignmentMode->currentIndex();
  manualAlignment.setX( ui.alignmentHozizontal->value() );
  manualAlignment.setY( ui.alignmentVertical->value() );

  // No new item was added but update the layout of the items
  updateLayout(false);

  emit signalItemChanged(true, false);
}

void playlistItemOverlay::childChanged(bool redraw, bool cacheChanged)
{
  playlistItemContainer::childChanged(redraw, cacheChanged);

  if (redraw)
  {
    // A child item changed and it needs redrawing, so we need to re-layout everything and also redraw
    updateLayout(false);
    emit signalItemChanged(true, false);
  }
}