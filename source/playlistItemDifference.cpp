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
  : playlistItemContainer("Difference Item")
{
  setIcon(0, QIcon(":img_difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // For a difference item, only 2 items are allowed.
  maxItemCount = 2;

  // The text that is shown when no difference can be drawn
  emptyText = "Please drop two video item's onto this difference item to calculate the difference.";

  connect(&difference, SIGNAL(signalHandlerChanged(bool,bool)), this, SLOT(slotEmitSignalItemChanged(bool,bool)));
}

/* For a difference item, the info list is just a list of the names of the
 * child elemnts.
 */
QList<infoItem> playlistItemDifference::getInfoList()
{
  QList<infoItem> infoList;

  if (childList.count() >= 1)
    infoList.append(infoItem(QString("File 1"), childList[0]->getName()));
  if (childList.count() >= 2)
    infoList.append(infoItem(QString("File 2"), childList[1]->getName()));

  // Report the position of the first difference in coding order
  difference.reportFirstDifferencePosition(infoList);

  // Report MSE
  for (int i = 0; i < difference.differenceInfoList.length(); i++)
  {
    infoItem p = difference.differenceInfoList[i];
    infoList.append( p );
  }
    
  return infoList;
}

void playlistItemDifference::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  Q_UNUSED(playback);

  if (!difference.inputsValid())
    // Draw the emptyText
    playlistItemContainer::drawEmptyContainerText(painter, zoomFactor);
  else
    // draw the videoHandler
    difference.drawFrame(painter, frameIdx, zoomFactor);
}

QSize playlistItemDifference::getSize() const
{ 
  if (!difference.inputsValid())
  {
    // Return the size of the empty text.
    return playlistItemContainer::getSize();
  }
  
  return difference.getFrameSize(); 
}

void playlistItemDifference::createPropertiesWidget()
{
  // Absolutely always only call this once
  assert(!propertiesWidget);

  preparePropertiesWidget(QStringLiteral("playlistItemDifference"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame;
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then yuv controls (format,...)
  vAllLaout->addLayout(difference.createFrameHandlerControls(true));
  vAllLaout->addWidget(line);
  vAllLaout->addLayout(difference.createDifferenceHandlerControls());

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);
}

void playlistItemDifference::updateChildItems()
{
  // Let's find out if our child item's changed.
  frameHandler *childVideo0 = NULL;
  frameHandler *childVideo1 = NULL;

  if (childList.count() >= 1)
    childVideo0 = childList[0]->getFrameHandler();
  if (childList.count() >= 2)
    childVideo1 = childList[1]->getFrameHandler();

  difference.setInputVideos(childVideo0, childVideo1);

  // Update the frame range
  startEndFrame = getStartEndFrameLimits();
}

void playlistItemDifference::savePlaylist(QDomElement &root, const QDir &playlistDir)
{
  QDomElementYUView d = root.ownerDocument().createElement("playlistItemDifference");

  // Append the indexed item's properties
  playlistItem::appendPropertiesToPlaylist(d);

  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemDifference *playlistItemDifference::newPlaylistItemDifference(const QDomElementYUView &root)
{
  playlistItemDifference *newDiff = new playlistItemDifference();

  // Load properties from the parent classes
  playlistItem::loadPropertiesFromPlaylist(root, newDiff);

  // The difference might just have children that have to be added. After adding the children don't forget
  // to call updateChildItems().
    
  return newDiff;
}

ValuePairListSets playlistItemDifference::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  if (childList.count() >= 1)
    newSet.append("Item A", childList[0]->getFrameHandler()->getPixelValues(pixelPos, frameIdx));

  if (childList.count() >= 2)
  {
    newSet.append("Item B", childList[0]->getFrameHandler()->getPixelValues(pixelPos, frameIdx));
    newSet.append("Diff (A-B)", difference.getPixelValues(pixelPos, frameIdx));
  }

  return newSet;
}
