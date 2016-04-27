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

#include "playlistItemYUVFile.h"

playlistItemOverlay::playlistItemOverlay() 
  : playlistItem("Overlay Item")
{
  // TODO: Create new symbol for this
  setIcon(0, QIcon(":difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  alignmentMode = 0;  // Top left
  manualAlignment = QPoint(0,0);
  childLlistUpdateRequired = true;
}

/* For a difference item, the info list is just a list of the names of the
 * child elemnts.
 */
QList<infoItem> playlistItemOverlay::getInfoList()
{
  QList<infoItem> infoList;

  // Add the size of this playlistItemOverlay
  infoList.append( infoItem("Overlay Size",QString("(%1,%2)").arg(getSize().width()).arg(getSize().height())) );

  // Add the sizes of all child items
  for (int i = 0; i < childCount(); i++)
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

ValuePairListSets playlistItemOverlay::getPixelValues(QPoint pixelPos)
{
  ValuePairListSets newSet;

  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem) 
    {
      ValuePairListSets childSets = childItem->getPixelValues(pixelPos);
      // Append the item id for every set in the child
      for (int j = 0; j < childSets.count(); j++)
      {
        childSets[j].first = QString("Item %1 - %2").arg(i).arg(childSets[j].first);
      }
      newSet.append(childSets);
    }
  }

  return newSet;
}

void playlistItemOverlay::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  if (childCount() == 0)
  {
    // Draw an error text in the view instead of showing an empty image
    QString text = "Please drop some items onto this overlay. All child items will be drawn on top of each other.";

    // Get the size of the text and create a rect of that size which is centered at (0,0)
    QFont displayFont = painter->font();
    displayFont.setPointSizeF( painter->font().pointSizeF() * zoomFactor );
    painter->setFont( displayFont );
    QSize textSize = painter->fontMetrics().size(0, text);
    QRect textRect;
    textRect.setSize( textSize );
    textRect.moveCenter( QPoint(0,0) );

    // Draw the text
    painter->drawText( textRect, text );

    return;
  }

  if (childLlistUpdateRequired)
    updateChildList();

  // Update the layout if the number of items changed
  updateLayout();

  // Translate to the center of this overlay item
  QPoint boundingRectCenter = boundingRect.centerRoundTL();
  painter->translate( boundingRect.centerRoundTL() * zoomFactor * -1 );

  // Draw all child items at their positions
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      QPoint center = childItems[i].centerRoundTL();

      // Debug
      int x1 = childItems[i].left();
      int x2 = childItems[i].right();

      painter->translate( center * zoomFactor );
      childItem->drawItem(painter, frameIdx, zoomFactor);
      painter->translate( center * zoomFactor * -1 );
    }
  }
  
  // Reverse translation to the center of this overlay item
  painter->translate( boundingRect.centerRoundTL() * zoomFactor );
}

void playlistItemOverlay::updateLayout(bool checkNumber)
{ 
  if (childCount() == 0)
  {
    childItems.clear();
    boundingRect = Rect();
    return;
  }

  if (checkNumber && childCount() == childItems.count())
    return;
  
  if (childItems.count() != childCount())
  {
    // Resize the childItems list
    childItems.clear();
    for (int i = 0; i < childCount(); i++)
    {
      childItems.append( Rect() );
    }
  }

  playlistItem *firstItem = getFirstChildPlaylistItem();
  boundingRect.setSize(firstItem->getSize());
  boundingRect.moveCenter( QPoint(0,0) );

  Rect firstItemRect;
  firstItemRect.setSize(firstItem->getSize());
  firstItemRect.moveCenter( QPoint(0,0) );
  childItems[0] = firstItemRect;
  
  // Align the rest of the items
  int alignmentMode = ui.alignmentMode->currentIndex();
  for (int i = 1; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      QSize childSize = childItem->getSize();
      Rect targetRect;
      targetRect.setSize( childSize );
      targetRect.moveCenter( QPoint(0,0) );

      // Align based on alignment mode (must be between 0 and 8)
      if (alignmentMode == 0)
        targetRect.moveTopLeft( firstItemRect.topLeft() );
      else if (alignmentMode == 1)
        targetRect.moveTop( firstItemRect.top() );
      else if (alignmentMode == 2)
        targetRect.moveTopRight( firstItemRect.topRight() );
      else if (alignmentMode == 3)
        targetRect.moveLeft( firstItemRect.left() );
      else if (alignmentMode == 5)
        targetRect.moveRight( firstItemRect.right() );
      else if (alignmentMode == 6)
        targetRect.moveBottomLeft( firstItemRect.bottomLeft() );
      else if (alignmentMode == 7)
        targetRect.moveBottom( firstItemRect.bottom() );
      else if (alignmentMode == 8)
        targetRect.moveBottomRight( firstItemRect.bottomRight() );
      else
        assert(alignmentMode == 4);

      //if (alignmentMode == 0 || alignmentMode == 1 || alignmentMode == 2)
      //  // Top alignment
      //  targetRect.translate(0, -1);
      //if (alignmentMode == 0 || alignmentMode == 3 || alignmentMode == 6)
      //  // Left alignment
      //  targetRect.translate(-1, 0);

      // Add the offset
      targetRect.translate( manualAlignment );

      // Set item rect
      childItems[i] = targetRect;

      // Expand the bounding rect
      boundingRect = boundingRect.united( targetRect );
    }
  }
}

void playlistItemOverlay::updateChildList()
{
  // Disconnect all signalItemChanged event from the children
  for (int i = 0; i < childList.count(); i++)
  {
    disconnect(childList[i], SIGNAL(signalItemChanged(bool,bool)));
  }

  // Connect all child items
  childList.clear();
  int count = childCount();
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      connect(childItem, SIGNAL(signalItemChanged(bool,bool)), this, SLOT(childChanged(bool,bool)));
      childList.append(childItem);
    }
  }

  childLlistUpdateRequired = false;
}

void playlistItemOverlay::itemAboutToBeDeleter(playlistItem *item)
{
  // Remove the item from childList and disconnect signlas/slots
  for (int i = 0; i < childList.count(); i++)
  {
    if (childList[i] == item)
    {
      disconnect(childList[i], SIGNAL(signalItemChanged(bool,bool)));
      childList.removeAt(i);
    }
  }
}

void playlistItemOverlay::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  ui.setupUi( propertiesWidget );
  propertiesWidget->setLayout( ui.topVBoxLayout );

  // Alignment mode
  ui.alignmentMode->addItems( QStringList() << "Top Left" << "Top Center" << "Top Right" );
  ui.alignmentMode->addItems( QStringList() << "Center Left" << "Center" << "Center Right" );
  ui.alignmentMode->addItems( QStringList() << "Bottom Left" << "Bottom Center" << "Bottom Right" );
  ui.alignmentMode->setCurrentIndex(alignmentMode);

  // Offset
  ui.alignmentHozizontal->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  ui.alignmentVertical->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  ui.alignmentHozizontal->setValue( manualAlignment.x() );
  ui.alignmentVertical->setValue( manualAlignment.y() );

  // Conncet signals/slots
  connect(ui.alignmentMode, SIGNAL(currentIndexChanged(int)), this, SLOT(controlChanged(int)));
  connect(ui.alignmentHozizontal, SIGNAL(valueChanged(int)), this, SLOT(controlChanged(int)));
  connect(ui.alignmentVertical, SIGNAL(valueChanged(int)), this, SLOT(controlChanged(int)));
}

void playlistItemOverlay::savePlaylist(QDomElement &root, QDir playlistDir)
{
  QDomElementYUV d = root.ownerDocument().createElement("playlistItemOverlay");

  // Append the overlay properties
  d.appendProperiteChild( "alignmentMode", QString::number(alignmentMode) );
  d.appendProperiteChild( "manualAlignmentX", QString::number(manualAlignment.x()) );
  d.appendProperiteChild( "manualAlignmentY", QString::number(manualAlignment.y()) );
  
  // Append all children
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
      childItem->savePlaylist(d, playlistDir);
  }

  root.appendChild(d);
}

playlistItemOverlay *playlistItemOverlay::newPlaylistItemOverlay(QDomElementYUV root, QString filePath)
{
  playlistItemOverlay *newOverlay = new playlistItemOverlay();

  int alignment = root.findChildValue("alignmentMode").toInt();
  int manualAlignmentX = root.findChildValue("manualAlignmentX").toInt();
  int manualAlignmentY = root.findChildValue("manualAlignmentY").toInt();
  
  newOverlay->alignmentMode = alignment;
  newOverlay->manualAlignment = QPoint(manualAlignmentX, manualAlignmentY);

  return newOverlay;
}

playlistItem *playlistItemOverlay::getFirstChildPlaylistItem()
{
  if (childCount() == 0)
    return NULL;

  playlistItem *childItem = dynamic_cast<playlistItem*>(child(0));
  return childItem;
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

void playlistItemOverlay::childChanged(bool redraw)
{
  if (redraw)
  {
    // A child item changed and it needs redrawing, so we need to re-layout everything and also redraw
    updateLayout(false);
    emit signalItemChanged(true, false);
  }
}