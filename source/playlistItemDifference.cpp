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

#include "playlistItemRawFile.h"
#include "playlistitemStatic.h"

#define DIFFERENCE_TEXT "Please drop two video item's onto this difference item to calculate the difference."

playlistItemDifference::playlistItemDifference() 
  : playlistItemIndexed("Difference Item")
{
  setIcon(0, QIcon(":difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);

  connect(&difference, SIGNAL(signalHandlerChanged(bool,bool)), this, SLOT(slotEmitSignalItemChanged(bool,bool)));
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

  playlistItem *child0 = (childCount() > 0) ? dynamic_cast<playlistItem*>(child(0)) : NULL;
  playlistItem *child1 = (childCount() > 1) ? dynamic_cast<playlistItem*>(child(1)) : NULL;
  infoList.append(infoItem(QString("File 1"), (child0) ? child0->getName() : "-"));
  infoList.append(infoItem(QString("File 1"), (child1) ? child1->getName() : "-"));

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
  {
    // Draw an error text in the view instead of showing an empty image
    QString text = DIFFERENCE_TEXT;

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

QSize playlistItemDifference::getSize() 
{ 
  if (!difference.inputsValid())
  {
    // Return the size of the text that is drawn on screen.
    // This is needed for overlays and for zoomToFit.
    QPainter painter;
    QFont displayFont = painter.font();
    return painter.fontMetrics().size(0, QString(DIFFERENCE_TEXT));
  }
  
  return difference.getFrameSize(); 
}

void playlistItemDifference::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert(!propertiesWidget);

  preparePropertiesWidget(QStringLiteral("playlistItemDifference"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame(propertiesWidget.data());
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then yuv controls (format,...)
  vAllLaout->addLayout( difference.createFrameHandlerControls(true) );
  vAllLaout->addWidget( line );
  vAllLaout->addLayout( difference.createDifferenceHandlerControls() );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);
}

void playlistItemDifference::updateChildItems()
{
  // Let's find out if our child item's changed.
  playlistItem *child0 = (childCount() > 0) ? dynamic_cast<playlistItem*>(child(0)) : NULL;
  playlistItem *child1 = (childCount() > 1) ? dynamic_cast<playlistItem*>(child(1)) : NULL;

  frameHandler *childVideo0 = (child0) ? child0->getFrameHandler() : NULL;
  frameHandler *childVideo1 = (child1) ? child1->getFrameHandler() : NULL;

  difference.setInputVideos(childVideo0, childVideo1);

  // Update the frame range
  startEndFrame = getstartEndFrameLimits();
}

void playlistItemDifference::savePlaylist(QDomElement &root, const QDir &playlistDir)
{
  QDomElementYUView d = root.ownerDocument().createElement("playlistItemDifference");

  // Append the indexed item's properties
  playlistItemIndexed::appendPropertiesToPlaylist(d);

  playlistItem *childVideo0 = (childCount() > 0) ? dynamic_cast<playlistItem*>(child(0)) : NULL;
  playlistItem *childVideo1 = (childCount() > 1) ? dynamic_cast<playlistItem*>(child(1)) : NULL;
  
  // Apppend the two child items
  if (childVideo0)
    childVideo0->savePlaylist(d, playlistDir);
  if (childVideo1)
    childVideo1->savePlaylist(d, playlistDir);

  root.appendChild(d);
}

playlistItemDifference *playlistItemDifference::newPlaylistItemDifference(const QDomElementYUView &root)
{
  playlistItemDifference *newDiff = new playlistItemDifference();

  // Load properties from the parent classes
  playlistItemIndexed::loadPropertiesFromPlaylist(root, newDiff);

  // The difference might just have children that have to be added. After adding the children don't forget
  // to call updateChildItems().
    
  return newDiff;
}

indexRange playlistItemDifference::getstartEndFrameLimits()
{
  playlistItemStatic *childStatic0 = (childCount() > 0) ? dynamic_cast<playlistItemStatic*>(child(0)) : NULL;
  playlistItemStatic *childStatic1 = (childCount() > 1) ? dynamic_cast<playlistItemStatic*>(child(1)) : NULL;
  
  playlistItemIndexed *childVideo0 = (childCount() > 0) ? dynamic_cast<playlistItemIndexed*>(child(0)) : NULL;
  playlistItemIndexed *childVideo1 = (childCount() > 1) ? dynamic_cast<playlistItemIndexed*>(child(1)) : NULL;

  if (childCount() == 1)
  {
    if (childVideo0)
      // Just one item. Return it's limits
      return childVideo0->getstartEndFrameLimits();
    else if (childStatic0)
      // Just one item and it is static
      return indexRange(0,1);
  }
  else if (childCount() >= 2)
  {
    if (childVideo0 && childVideo1)
    {
      // Two items. Return the overlapping region.
      indexRange limit0 = childVideo0->getstartEndFrameLimits();
      indexRange limit1 = childVideo1->getstartEndFrameLimits();

      int start = std::max(limit0.first, limit1.first);
      int end   = std::min(limit0.second, limit1.second);

      indexRange limits = indexRange( start, end );
      return limits;
    }
    else if (childStatic0 && childStatic1)
    {
      // Two items which are static
      return indexRange(0,1);
    }
  }

  return indexRange(-1,-1);
}

ValuePairListSets playlistItemDifference::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  playlistItem *child0 = (childCount() > 0) ? dynamic_cast<playlistItem*>(child(0)) : NULL;
  playlistItem *child1 = (childCount() > 1) ? dynamic_cast<playlistItem*>(child(1)) : NULL;

  frameHandler *childVideo0 = (child0) ? child0->getFrameHandler() : NULL;
  frameHandler *childVideo1 = (child1) ? child1->getFrameHandler() : NULL;

  if (child0)
    newSet.append("Item A", childVideo0->getPixelValues(pixelPos, frameIdx));

  if (child1)
  {
    newSet.append("Item B", childVideo1->getPixelValues(pixelPos, frameIdx));
    newSet.append("Diff (A-B)", difference.getPixelValues(pixelPos, frameIdx));
  }

  return newSet;
}

bool playlistItemDifference::isSourceChanged()
{
  // Check the children. Always call isSourceChanged() on all children because this function
  // also resets the flag.
  bool changed = false;
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem->isSourceChanged())
      changed = true;
  }

  return changed;
}

void playlistItemDifference::reloadItemSource()
{
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    childItem->reloadItemSource();
  }

  // Invalidate the buffers of the difference so that the difference image is recalculated.
  difference.invalidateAllBuffers();
}

void playlistItemDifference::updateFileWatchSetting()
{
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    childItem->updateFileWatchSetting();
  }
}