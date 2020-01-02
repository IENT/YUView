/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FILEINFO_H
#define FILEINFO_H

#include <QList>
#include <QString>

/*
 * An info item has a name, a text and an optional toolTip. These are used to show them in the fileInfoWidget.
 * For example: ["File Name", "file.yuv"] or ["Number Frames", "123"]
 * Another option is to show a button. If the user clicks on it, the callback function infoListButtonPressed() for the 
 * corresponding playlist item is called.
 */
struct infoItem
{
  infoItem(const QString &name, const QString &text, const QString &toolTip=QString(), bool button=false, int buttonID=-1) 
    : name(name), text(text), button(button), buttonID(buttonID), toolTip(toolTip) 
  {}
  QString name;
  QString text;
  bool button;
  int buttonID;
  QString toolTip;
};

struct infoData
{
  explicit infoData(const QString &title = QString()) : title(title) {}
  bool isEmpty() const { return title.isEmpty() && items.isEmpty(); }
  QString title;
  QList<infoItem> items;
};
Q_DECLARE_METATYPE(infoData)

#endif // FILEINFO_H
