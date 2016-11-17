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

#ifndef FILEINFOWIDGET_H
#define FILEINFOWIDGET_H

#include <QGridLayout>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

class playlistItem;

// This is the text that will be shown in the dockWidgets title if no playlistitem is selected
#define FILEINFOWIDGET_DEFAULT_WINDOW_TITLE "Info"

// An info item has a name, a text and an optional toolTip. These are used to show them in the fileInfoWidget.
// For example: ["File Name", "file.yuv"] or ["Number Frames", "123"]
// Another option is to show a button. If the user clicks on it, the callback function infoListButtonPressed() for the 
// playlist item is called.
class infoItem
{
public:
  infoItem(const QString &infoName, const QString &infoText, const QString &infoToolTip=QString(), bool button=false) : name(infoName), text(infoText), button(button), toolTip(infoToolTip) {}
  QString name;
  QString text;
  bool button;
  QString toolTip;
};

class FileInfoWidget : public QWidget
{
  Q_OBJECT

public:
  FileInfoWidget(QWidget *parent = 0);

public slots:
  // Accept the signal from the playlisttreewidget that signals if a new (or two) item was selected.
  // This function will get and show the info from the given item1.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2);

  // Update the file info for the currently selected items (the ones las set with currentSelectedItemsChanged)
  void updateFileInfo(bool redraw=false);

private:

  /* Set the file info. The title of the dock widget will be set to fileInfoTitle and
   * the given list of infoItems (Qpai<QString,QString>) will be added as labels into 
   * the QGridLayout infoLayout.
  */
  void setFileInfo(const QString &fileInfoTitle, const QList<infoItem> &fileInfoList);

  // Clear the contents and set the default title.
  void setFileInfo();

  // The grid layout that contains all the infoItems
  QGridLayout grid;

  // Clear all widgets.
  void clear(int startRow = 0);
    
  // Pointers to the currently selected items
  QPointer<playlistItem> currentItem1, currentItem2;

  // The warning icon. This is shown instead of a text if the name of the infoItem is "Warning"
  QPixmap warningIcon;

private slots:
  // One of the buttons in the info panel was clicked.
  void fileInfoButtonClicked();
};

#endif
