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

#include "playlistitemtext.h"

PlaylistItemText::PlaylistItemText(const QString &text, QTreeWidget* parent) : PlaylistItem ( text, parent )
{
    // create new object for this video file
    p_displayObject = new TextObject(text);

    QString oneLineString = text;
    setText(0, oneLineString.replace("\n", " "));

    // update icon
    setIcon(0, QIcon(":images/img_text.png"));

    // disable dropping on text items
    setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

PlaylistItemText::~PlaylistItemText()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemText::itemType()
{
    return TextItemType;
}
