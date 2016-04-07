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
  manualAlignment[0] = 0;
  manualAlignment[1] = 0;
}

/* For a difference item, the info list is just a list of the names of the
 * child elemnts.
 */
QList<infoItem> playlistItemOverlay::getInfoList()
{
  QList<infoItem> infoList;
  // Just return all info lists from the children.
  // TODO: Do something more intelligent.
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
      infoList.append( childItem->getInfoList() );
  }
  return infoList;
}

ValuePairList playlistItemOverlay::getPixelValues(QPoint pixelPos)
{
  ValuePairList pixelValues;
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
      pixelValues.append( childItem->getPixelValues(pixelPos) );
  }
  return pixelValues;
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

  // Just draw all child items on top of each other. Align relative to the first item

  // Draw the first item first
  playlistItem *firstItem = getFirstChildPlaylistItem();
  firstItem->drawItem(painter, frameIdx, zoomFactor);
  QRect firstItemRect;
  firstItemRect.setSize(firstItem->getSize() * zoomFactor);
  firstItemRect.moveCenter( QPoint(0,0) );
  
  // Draw the rest of the items
  int alignmentMode = ui.alignmentMode->currentIndex();
  for (int i = 1; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
    {
      QSize childSize = childItem->getSize() * zoomFactor;
      QRect targetRect;
      targetRect.setSize( childSize );

      if (alignmentMode == 0)
        targetRect.moveTopLeft( firstItemRect.topLeft() );
      else if (alignmentMode == 1)
        targetRect.moveTop( firstItemRect.top() );
      else if (alignmentMode == 2)
        targetRect.moveTopRight( firstItemRect.topRight() );
      else if (alignmentMode == 3)
        targetRect.moveLeft( firstItemRect.left() );
      else if (alignmentMode == 4)
        targetRect.moveCenter( QPoint(0,0) );
      else if (alignmentMode == 5)
        targetRect.moveRight( firstItemRect.right() );
      else if (alignmentMode == 6)
        targetRect.moveBottomLeft( firstItemRect.bottomLeft() );
      else if (alignmentMode == 7)
        targetRect.moveBottom( firstItemRect.bottom() );
      else if (alignmentMode == 8)
        targetRect.moveBottomRight( firstItemRect.bottomRight() );
      else
        // Manual
        targetRect.moveCenter( QPoint(ui.alignmentHozizontal->value() * zoomFactor, ui.alignmentVertical->value() * zoomFactor) );

      // Move the painter to the target center, draw the item and undo translation
      painter->translate( targetRect.center() );
      childItem->drawItem(painter, frameIdx, zoomFactor);
      painter->translate( targetRect.center() * -1 );
    }
  }
}

void playlistItemOverlay::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemDifference"));

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  ui.setupUi( propertiesWidget );
  propertiesWidget->setLayout( ui.topVBoxLayout );

  ui.alignmentMode->addItems( QStringList() << "Top Left" << "Top Center" << "Top Right" );
  ui.alignmentMode->addItems( QStringList() << "Center Left" << "Center" << "Center Right" );
  ui.alignmentMode->addItems( QStringList() << "Bottom Left" << "Bottom Center" << "Bottom Right" );
  ui.alignmentMode->addItem( "Manual" );
  ui.alignmentMode->setCurrentIndex(alignmentMode);

  ui.alignmentHozizontal->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  ui.alignmentVertical->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  ui.alignmentHozizontal->setEnabled(alignmentMode == 9);
  ui.alignmentVertical->setEnabled(alignmentMode == 9);

  // Conncet signals/slots
  connect(ui.alignmentMode, SIGNAL(currentIndexChanged(int)), this, SLOT(alignmentModeChanged(int)));
  connect(ui.alignmentHozizontal, SIGNAL(	valueChanged(int)), this, SLOT(manualAlignmentHorChanged(int)));
  connect(ui.alignmentVertical, SIGNAL(	valueChanged(int)), this, SLOT(manualAlignmentVerChanged(int)));
}

void playlistItemOverlay::savePlaylist(QDomElement &root, QDir playlistDir)
{
  QDomElement d = root.ownerDocument().createElement("playlistItemDifference");

  playlistItem *childVideo0 = (childCount() > 0) ? dynamic_cast<playlistItem*>(child(0)) : NULL;
  playlistItem *childVideo1 = (childCount() > 1) ? dynamic_cast<playlistItem*>(child(1)) : NULL;
  
  // Apppend the two child items
  if (childVideo0)
    childVideo0->savePlaylist(d, playlistDir);
  if (childVideo1)
    childVideo1->savePlaylist(d, playlistDir);

  root.appendChild(d);
}

playlistItemOverlay *playlistItemOverlay::newPlaylistItemDifference(QDomElementYUV root, QString filePath)
{
  playlistItemOverlay *newOverlay = new playlistItemOverlay();

  QDomNodeList children = root.childNodes();
  
  for (int i = 0; i < children.length(); i++)
  {
    // Parse the child items
    if (children.item(i).toElement().tagName() == "playlistItemYUVFile")
    {
      // This is a playlistItemYUVFile. Create a new one and add it to the playlist
      playlistItemYUVFile *newYUVFile = playlistItemYUVFile::newplaylistItemYUVFile(children.item(i).toElement(), filePath);
      if (newYUVFile)
        newOverlay->addChild(newYUVFile);
    }
  }

  return newOverlay;
}

playlistItem *playlistItemOverlay::getFirstChildPlaylistItem()
{
  if (childCount() == 0)
    return NULL;

  playlistItem *childItem = dynamic_cast<playlistItem*>(child(0));
  return childItem;
}

void playlistItemOverlay::alignmentModeChanged(int idx)
{
  if (idx != alignmentMode)
  {
    alignmentMode = idx;

    // Activate controls if manual is selected
    ui.alignmentHozizontal->setEnabled(idx == 9);
    ui.alignmentVertical->setEnabled(idx == 9);

    emit signalItemChanged(true);
  }
}

void playlistItemOverlay::manualAlignmentVerChanged(int val)
{
  if (val != manualAlignment[1])
  {
    manualAlignment[1] = val;
    emit signalItemChanged(true);
  }
}

void playlistItemOverlay::manualAlignmentHorChanged(int val)
{
  if (val != manualAlignment[0])
  {
    manualAlignment[0] = val;
    emit signalItemChanged(true);
  }
}