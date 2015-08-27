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

#include "playlistitem.h"

#include "playlistitemvid.h"

PlaylistItem::PlaylistItem(const QString &itemName, QTreeWidget * parent) : QTreeWidgetItem ( parent, 1001 )
{
    p_displayObject = NULL;

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon());

    // enable dragging
    setFlags(flags() | Qt::ItemIsDragEnabled);
}

PlaylistItem::PlaylistItem(const QString &itemName, QTreeWidgetItem* parentItem) : QTreeWidgetItem( parentItem, 1001 )
{
    p_displayObject = NULL;

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon());

    // enable dragging
    setFlags(flags() | Qt::ItemIsDragEnabled);
}

PlaylistItem::~PlaylistItem() {


}
