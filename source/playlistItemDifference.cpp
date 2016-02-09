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

playlistItemDifference::playlistItemDifference() 
  : playlistItem("Difference Item")
{
  setIcon(0, QIcon(":difference.png"));
  // Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
  setFlags(flags() | Qt::ItemIsDropEnabled);
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

  for (int i=0; i < childCount(); i++)
  {
    playlistItem* item = dynamic_cast<playlistItem*>(child(i));
    infoList.append(infoItem(QString("File %1").arg(i), item->getName()));
  }

  return infoList;
}