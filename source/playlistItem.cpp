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

#include "playlistItem.h"

unsigned int playlistItem::idCounter = 0;

playlistItem::playlistItem(const QString &itemNameOrFileName)
{
  setName(itemNameOrFileName);
  cachingEnabled = false;

  // Whenever a playlistItem is created, we give it an ID (which is unique for this instance of YUView)
  id = idCounter++;
  playlistID = -1;
}

playlistItem::~playlistItem()
{
  // If we have children delete them first
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *plItem = dynamic_cast<playlistItem*>(QTreeWidgetItem::takeChild(0));
    delete plItem;
  }
}

// Delete the item later but disable caching of this item before, so that the video cache ignores it
// until it is really gone.
void playlistItem::disableCaching()
{
  // This will block until all background caching processes are done.
  cachingMutex.lock();
  cachingEnabled = false;
  cachingMutex.unlock();
}

void playlistItem::appendPropertiesToPlaylist(QDomElementYUView &d)
{
  d.appendProperiteChild("id", QString::number(id));
}

void playlistItem::loadPropertiesFromPlaylist(const QDomElementYUView &root, playlistItem *newItem)
{
  newItem->playlistID = root.findChildValue("id").toInt();
}

QList<playlistItem*> playlistItem::getItemAndAllChildren()
{
  QList<playlistItem*> returnList;
  returnList.append(this);
  for (int i = 0; i < childCount(); i++)
  {
    playlistItem *childItem = dynamic_cast<playlistItem*>(child(i));
    if (childItem)
      returnList.append(childItem->getItemAndAllChildren());
  }
  return returnList;
}

void playlistItem::preparePropertiesWidget(const QString &name) {
  Q_ASSERT(!propertiesWidget);
  propertiesWidget.reset(new QWidget);
  propertiesWidget->setObjectName(name);
}
