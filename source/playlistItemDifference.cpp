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

playlistItemDifference::playlistItemDifference() 
  : playlistItemVideo("Difference Item")
{
  setIcon(0, QIcon(":difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  inputVideo[0] = NULL;
  inputVideo[1] = NULL;
}

playlistItemDifference::~playlistItemDifference()
{
}

// This item accepts dropping of two items that provide video
bool playlistItemDifference::acceptDrops(playlistItem *draggingItem)
{
  return (childCount() < 2 && draggingItem->providesVideo());
}

/* For a difference item, the info list is just a list of the names of the
 * child elemnts.
 */
QList<infoItem> playlistItemDifference::getInfoList()
{
  QList<infoItem> infoList;

  infoList.append(infoItem(QString("File 1"), (inputVideo[0]) ? inputVideo[0]->getName() : "-"));
  infoList.append(infoItem(QString("File 2"), (inputVideo[1]) ? inputVideo[1]->getName() : "-"));

  infoList.append( conversionInfoList );
  
  return infoList;
}

void playlistItemDifference::drawFrame(QPainter *painter, int frameIdx, double zoomFactor)
{
  if (inputVideo[0] == NULL || inputVideo[1] == NULL)
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

  // Call the base class (playlistItemVide) drawing function.
  playlistItemVideo::drawFrame(painter, frameIdx, zoomFactor);
}

void playlistItemDifference::loadFrame(int frameIdx)
{
  if (inputVideo[0] == NULL || inputVideo[1] == NULL)
  {
    currentFrameIdx = -1;
    return;
  }

  conversionInfoList.clear();
  currentFrame = inputVideo[0]->calculateDifference(inputVideo[1], frameIdx, conversionInfoList);
  currentFrameIdx = frameIdx;

  emit signalItemChanged(false);
}

// Get the number of frames from the child items
qint64 playlistItemDifference::getNumberFrames()
{
  if (inputVideo[0] == NULL || inputVideo[1] == NULL)
    return 0;

  return qMin(inputVideo[0]->getNumberFrames(), inputVideo[1]->getNumberFrames());
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

  // First add the parents controls (first video controls (width/height...) then yuv controls (format,...)
  vAllLaout->addLayout( playlistItemVideo::createVideoControls(true) );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(1, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemDifference::updateChildren()
{
  // Let's find out if our child item's changed.
  playlistItemVideo *childVideo0 = (childCount() > 0) ? dynamic_cast<playlistItemVideo*>(child(0)) : NULL;
  playlistItemVideo *childVideo1 = (childCount() > 1) ? dynamic_cast<playlistItemVideo*>(child(1)) : NULL;

  if (inputVideo[0] != childVideo0 || inputVideo[1] != childVideo1)
  {
    // Something changed
    inputVideo[0] = childVideo0;
    inputVideo[1] = childVideo1;

    if (inputVideo[0] != NULL && inputVideo[1] != NULL)
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

      // Now the item needs to be redrawn
      emit signalItemChanged(true);
    }
  }
}

ValuePairList playlistItemDifference::getPixelValues(QPoint pixelPos)
{
  if (inputVideo[0] == NULL || inputVideo[1] == NULL)
    return ValuePairList();

  return inputVideo[0]->getPixelValuesDifference(inputVideo[1], pixelPos);
}