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

#ifndef FILEINFOFROUPBOX_H
#define FILEINFOFROUPBOX_H

#include <QDockWidget>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include "playlistitem.h"
#include "typedef.h"

class FileInfoDock : public QDockWidget
{
  Q_OBJECT

public:
  FileInfoDock(QWidget *parent);
  ~FileInfoDock();

  void setFileInfo();
  void setFileInfo(QString fileInfoTitle, QList<infoItem> fileInfoList);

  // This is kind of a hack. If using the QDesigner it is not possible to remove the dock widget associated with the
  // QDockWidget. This means that the designer will generate code that will always try to set a widget to a
  // QDockWidget. Also to a custom QDockWidget which might not need this. The UI code adds this widget after the 
  // contructor of this class has been called, so if you do setWidget() in the constructor your widget will be overwritten
  // by the UI. This function will catch the UI's call to setWidget and ignore it.
  // Don't forget to call QDockWidget::setWidget(w) in the constructor explicitly.
  void setWidget(QWidget *w) {};

public slots:
  // Accept the signal from the playlisttreewidget that signals if a new (or two) item was selected.
  // This function will get and show the info from the given item1.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2);

protected:
  QWidget     *contentWidget;
  QGridLayout *infoLayout;

  // The list containing pointers to all labels in the grid layout.
  QList<QLabel*> labelList;

  int nrLabelPairs;		///< The number of label pairs currently in the groupBox
};

#endif