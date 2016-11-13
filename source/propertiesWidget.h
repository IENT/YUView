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

#ifndef PROPERTIESWIDGET_H
#define PROPERTIESWIDGET_H

#include "playlistItem.h"
#include "typedef.h"
#include <QStackedWidget>
#include <QVBoxLayout>

// This is the text that will be shown in the dockWidgets title if no playlistitem is selected
#define PROPERTIESWIDGET_DEFAULT_WINDOW_TITEL "Properties"

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
