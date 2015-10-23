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
#include "differenceobject.h"
#include "textobject.h"

PlaylistItem::PlaylistItem(const PlaylistItemType type, QString itemNameOrFileName, QTreeWidget* parent)
	: QTreeWidgetItem(parent, 1001)
{
	initClass(type, itemNameOrFileName);
}

PlaylistItem::PlaylistItem(const PlaylistItemType type, QString itemNameOrFileName, QTreeWidgetItem* parent)
	: QTreeWidgetItem(parent, 1001)
{
	initClass(type, itemNameOrFileName);
}

/* This constructor accepts a statisticSource pointer and will create a new statistics
 * playlist item. This function is helpfull if you already created another playlist item
 * but this item also supports statistics.
 */
PlaylistItem::PlaylistItem(statisticSource *stat_src, QTreeWidgetItem* parent)
	: QTreeWidgetItem(parent, 1001)
{
	p_playlistItemType = PlaylistItem_Statistics;
	setIcon(0, QIcon(":stats.png"));
	// Disable dragging/dropping for this statistics items
	setFlags(flags() & ~Qt::ItemIsDropEnabled & ~Qt::ItemIsDragEnabled);
	// Create statistics display object
	p_displayObject = new StatisticsObject(stat_src);
	// Get text to show from the new statistics object
	setText(0, p_displayObject->name());
}

void PlaylistItem::initClass(const PlaylistItemType type, QString itemNameOrFileName)
{
	p_playlistItemType = type;
	
    // update icon, set drop rules, create display object and set the text
	// to show in the list view depending on the selected type.
	switch (type)
	{
	case PlaylistItem_Video:
		setIcon(0, QIcon(":img_television.png"));
		// Enable dropping. The user can drop statistics files on video items
		setFlags(flags() | Qt::ItemIsDropEnabled);
		// Create frame object
		p_displayObject = new FrameObject(itemNameOrFileName);
		// Get text to show from the new frame object
		setText(0, p_displayObject->name());
		break;
	case PlaylistItem_Text:
		setIcon(0, QIcon(":img_text.png"));
		// Disable dropping for text items
		setFlags(flags() & ~Qt::ItemIsDropEnabled);
		// Create new text item
		p_displayObject = new TextObject(itemNameOrFileName);
		// Remove new lines and set that as text
		setText(0, itemNameOrFileName.replace("\n", " "));
		break;
	case PlaylistItem_Statistics:
		setIcon(0, QIcon(":stats.png"));
		// Disable dropping for statistics items
		setFlags(flags() & ~Qt::ItemIsDropEnabled);
		// Create statistics display object
		p_displayObject = new StatisticsObject(itemNameOrFileName);
		// Get text to show from the new statistics object
		setText(0, p_displayObject->name());
		break;
	case PlaylistItem_Difference:
		setIcon(0, QIcon(":difference.png"));
		// Enable dropping for difference objects. The user can drop the two items to calculate the difference from.
		setFlags(flags() | Qt::ItemIsDropEnabled);
		// Create difference display object
		p_displayObject = new DifferenceObject();
		break;
	default:
		setIcon(0, QIcon());
		break;
	}
}

PlaylistItem::~PlaylistItem()
{
	// If we have children delete them first
	for (int i = 0; i < childCount(); i++)
	{
		PlaylistItem *plItem = dynamic_cast<PlaylistItem*>(QTreeWidgetItem::takeChild(0));
		delete plItem;
	}

	delete p_displayObject;
}

QTreeWidgetItem *PlaylistItem::takeChild(int index)
{
	if (p_playlistItemType == PlaylistItem_Difference) {
		// Update the associated difference object
		DifferenceObject *diff = dynamic_cast<DifferenceObject*>(p_displayObject);
		if (diff)
			diff->removeFrameObject(index);
	}
	return QTreeWidgetItem::takeChild(index);
}
