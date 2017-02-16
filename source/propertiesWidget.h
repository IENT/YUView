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

#ifndef PROPERTIESWIDGET_H
#define PROPERTIESWIDGET_H

#include <QStackedWidget>
#include <QVBoxLayout>

class playlistItem;

// This is the text that will be shown in the dockWidgets title if no playlistitem is selected
#define PROPERTIESWIDGET_DEFAULT_WINDOW_TITLE "Properties"

class PropertiesWidget : public QWidget
{
  Q_OBJECT

public:
  PropertiesWidget(QWidget *parent = 0);

public slots:
  // Accept the signal from the playlisttreewidget that signals if a new (or two) item was selected.
  // This function will get and show the properties from the given item1.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2);

  // The item is about to be delete. If we keep the properties panel of this item, delete the properties panel from 
  // the stack widget.
  void itemAboutToBeDeleted(playlistItem *item);

private:
  QVBoxLayout topLayout;
  QStackedWidget stack;
  QWidget emptyWidget;
};

#endif // PROPERTIESWIDGET_H
