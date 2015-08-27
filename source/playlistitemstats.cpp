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

#include "playlistitemstats.h"

PlaylistItemStats::PlaylistItemStats(const QString &srcFileName, QTreeWidget* parent) : PlaylistItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = new StatisticsObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/stats.png"));

    // disable dropping on statistic items
    setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

PlaylistItemStats::PlaylistItemStats(const QString &srcFileName, QTreeWidgetItem* parentItem) : PlaylistItem( srcFileName, parentItem )
{
    // create new object for this video file
    p_displayObject = new StatisticsObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/stats.png"));

    // enable dragging
    setFlags(flags() | Qt::ItemIsDragEnabled);

    // disable dropping on statistic items
    setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

PlaylistItemStats::~PlaylistItemStats()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemStats::itemType()
{
    return StatisticsItemType;
}
