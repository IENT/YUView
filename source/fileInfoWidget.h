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

#ifndef FILEINFOWIDGET_H
#define FILEINFOWIDGET_H

#include <QGridLayout>
#include <QPixmap>
#include <QWidget>

// This is the text that will be shown in the dockWidgets title if no playlistitem is selected
#define FILEINFOWIDGET_DEFAULT_WINDOW_TITLE "Info"

// An info item has a name, a text and an optional toolTip. These are used to show them in the fileInfoWidget.
// For example: ["File Name", "file.yuv"] or ["Number Frames", "123"]
// Another option is to show a button. If the user clicks on it, the callback function infoListButtonPressed() for the 
// playlist item is called.
struct infoItem
{
  infoItem(const QString &name, const QString &text, const QString &toolTip=QString(), bool button=false, int buttonID=-1) : name(name), text(text), button(button), buttonID(buttonID), toolTip(toolTip) {}
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

class FileInfoWidget : public QWidget
{
  Q_OBJECT

public:
  FileInfoWidget(QWidget *parent = 0);

  // Set the file info. The title of the dock widget will be set to fileInfoTitle and
  // the given list of infoItems (Qpai<QString,QString>) will be added as labels into
  // the QGridLayout infoLayout.
  Q_SLOT void setInfo(const infoData &info1 = infoData(), const infoData &info2 = infoData());

  // One at a given row of a given infoIndex (0 or 1) was clicked.
  // infoIndex 0 refers to info1 above, infoIndex 1 refers to info2 above
  Q_SIGNAL void infoButtonClicked(int infoIndex, int row);

private:
  // Clear widgets starting at given row.
  void clear(int startRow);

  // Add information at the given grid row, for a given infoData index (0 or 1).
  int addInfo(const infoData &data, int row, int infoIndex);

  // One of the buttons in the info panel was clicked.
  void infoButtonClickedSlot();

  // The grid layout that contains all the infoItems
  QGridLayout grid;
    
  // The warning icon. This is shown instead of a text if the name of the infoItem is "Warning"
  QPixmap warningIcon;
};

#endif
