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

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QTreeWidgetItem>
#include "displayobject.h"
#include "statisticsobject.h"

enum PlaylistItemType {
    VideoItemType,
    TextItemType,
    StatisticsItemType,
    DifferenceItemType
};

class PlaylistItem : public QTreeWidgetItem
{
public:
    PlaylistItem(const QString &itemName, QTreeWidget* parent = 0);
    PlaylistItem(const QString &itemName, QTreeWidgetItem* parentItem);

    ~PlaylistItem();

    virtual DisplayObject *displayObject() { return p_displayObject; }

    virtual PlaylistItemType itemType() = 0;

public slots:

private:

protected:
    DisplayObject* p_displayObject;
};

#endif // PLAYLISTITEM_H
