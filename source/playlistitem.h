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
#include "FileInfoGroupBox.h"
#include "frameobject.h"
#include "textobject.h"
#include "statisticsobject.h"
#include "differenceobject.h"

enum PlaylistItemType {
  PlaylistItem_Video,
  PlaylistItem_Text,
  PlaylistItem_Statistics,
  PlaylistItem_Difference
};

class PlaylistItem : public QTreeWidgetItem
{
public:
  PlaylistItem(const PlaylistItemType type, QString itemNameOrFileName, QTreeWidget* parent = 0);
  PlaylistItem(const PlaylistItemType type, QString itemNameOrFileName, QTreeWidgetItem* parent = 0);
  PlaylistItem(QSharedPointer<statisticSource> statSrc, QTreeWidgetItem* parent = 0);	/// Init using an existing statistic source pointer
  ~PlaylistItem();

  // Get item type and display object
  PlaylistItemType itemType() { return p_playlistItemType; };
  QSharedPointer<DisplayObject> displayObject() { return p_displayObject; }

  // Return the display object cast to the four sub classes (or NULL if not valid)
  QSharedPointer<FrameObject>      getFrameObject() { return qSharedPointerDynamicCast<FrameObject>(p_displayObject); }
  QSharedPointer<TextObject>       getTextObject()  { return qSharedPointerDynamicCast<TextObject>(p_displayObject);  }
  QSharedPointer<StatisticsObject> getStatisticsObject() { return qSharedPointerDynamicCast<StatisticsObject>(p_displayObject); }
  QSharedPointer<DifferenceObject> getDifferenceObject() { return qSharedPointerDynamicCast<DifferenceObject>(p_displayObject); }

  // Return the info title and info list to be shown in the fileInfo groupBox.
  // Get these from the display object.
  QString getInfoTitel() { return p_displayObject->getInfoTitle(); };
  QList<fileInfoItem> getInfoList() { return p_displayObject->getInfoList();  }

  // Updated the DifferenceObject (if it is one) and call the QTreeWidgetItem::takeChild function
  QTreeWidgetItem *takeChild(int index);

protected:
  QSharedPointer<DisplayObject> p_displayObject;

  PlaylistItemType p_playlistItemType;

  void initClass(const PlaylistItemType type, QString itemNameOrFileName);
};

#endif // PLAYLISTITEM_H
