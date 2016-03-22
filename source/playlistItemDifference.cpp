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

#include "playlistItemDifference.h"
#include <QPainter>

#include "playlistItemYUVFile.h"

playlistItemDifference::playlistItemDifference() 
  : playlistItem("Difference Item")
{
  setIcon(0, QIcon(":difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  connect(&difference, SIGNAL(signalHandlerChanged(bool)), this, SLOT(slotEmitSignalItemChanged(bool)));
}

// This item accepts dropping of two items that provide video
bool playlistItemDifference::acceptDrops(playlistItem *draggingItem)
{
  return (childCount() < 2 && draggingItem->canBeUsedInDifference());
}

/* For a difference item, the info list is just a list of the names of the
 * child elemnts.
 */
QList<infoItem> playlistItemDifference::getInfoList()
{
  QList<infoItem> infoList;

  /*infoList.append(infoItem(QString("File 1"), (inputVideo[0]) ? inputVideo[0]->getName() : "-"));
  infoList.append(infoItem(QString("File 2"), (inputVideo[1]) ? inputVideo[1]->getName() : "-"));*/

  // Report the position of the first difference in coding order

  difference.reportFirstDifferencePosition(infoList);
  
  return infoList;
}

void playlistItemDifference::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  if (!difference.inputsValid())
  {
    // Draw an error text in the view instead of showing an empty image
    QString text = "Please drop two video item's onto this difference item to calculate the difference.";

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

  // draw the videoHandler
  difference.drawFrame(painter, frameIdx, zoomFactor);
}

void playlistItemDifference::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemDifference"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);
  vAllLaout->setContentsMargins( 0, 0, 0, 0 );

  QFrame *line = new QFrame(propertiesWidget);
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then yuv controls (format,...)
  vAllLaout->addLayout( difference.createVideoHandlerControls(propertiesWidget, true) );
  vAllLaout->addWidget( line );
  vAllLaout->addLayout( difference.createDifferenceHandlerControls(propertiesWidget) );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(1, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemDifference::updateChildren()
{
  // Let's find out if our child item's changed.
  playlistItem *child0 = (childCount() > 0) ? dynamic_cast<playlistItem*>(child(0)) : NULL;
  playlistItem *child1 = (childCount() > 1) ? dynamic_cast<playlistItem*>(child(1)) : NULL;

  videoHandler *childVideo0 = (child0) ? child0->getVideoHandler() : NULL;
  videoHandler *childVideo1 = (child1) ? child1->getVideoHandler() : NULL;

  difference.setInputVideos(childVideo0, childVideo1);
}

void playlistItemDifference::savePlaylist(QDomElement &root, QDir playlistDir)
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

playlistItemDifference *playlistItemDifference::newPlaylistItemDifference(QDomElementYUV root, QString filePath)
{
  playlistItemDifference *newDiff = new playlistItemDifference();

  QDomNodeList children = root.childNodes();
  
  for (int i = 0; i < children.length(); i++)
  {
    // Parse the child items
    if (children.item(i).toElement().tagName() == "playlistItemYUVFile")
    {
      // This is a playlistItemYUVFile. Create a new one and add it to the playlist
      playlistItemYUVFile *newYUVFile = playlistItemYUVFile::newplaylistItemYUVFile(children.item(i).toElement(), filePath);
      if (newYUVFile)
        newDiff->addChild(newYUVFile);
    }
  }

  newDiff->updateChildren();

  return newDiff;
}

