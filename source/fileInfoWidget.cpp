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

#include "fileInfoWidget.h"

#include <QPushButton>
#include <QVariant>
#include "labelElided.h"

static const char kRow[] = "gridRow";

/* The file info group box can display information on a file (or any other displayobject).
 * If you provide a list of QString tuples, this class will fill a grid layout with the 
 * corresponding labels.
 */
FileInfoWidget::FileInfoWidget(QWidget *parent) :
  QWidget(parent),
  grid(this)
{
  // Load the warning icon
  warningIcon = QPixmap(":img_warning.png");

  // Clear the file info dock
  setInfo();
}

void FileInfoWidget::clear(int startRow)
{
  for (int i = startRow; i < grid.rowCount(); ++i)
    for (int j = 0; j < grid.columnCount(); ++j)
    {
      auto item = grid.itemAtPosition(i, j);
      if (item) delete item->widget();
    }
}

template <typename W> static W * widgetAt(QGridLayout *grid, int row, int column)
{
  auto item = grid->itemAtPosition(row, column);
  if (item && item->widget() && !qobject_cast<W*>(item->widget()))
    delete item->widget();
  if (!item || !item->widget()) {
    auto widget = new W;
    grid->addWidget(widget, row, column, Qt::AlignTop);
    return widget;
  }
  return qobject_cast<W*>(item->widget());
}

void FileInfoWidget::setInfo(const infoData &info1, const infoData &info2)
{
  // TODO Handle the information of the other item
  Q_UNUSED(info2);

  // Set the title of the dock widget (our parent)
  if (parentWidget())
    parentWidget()->setWindowTitle(!info1.title.isEmpty() ? info1.title : FILEINFOWIDGET_DEFAULT_WINDOW_TITLE);

  int i = 0;
  for (auto &info : info1.items)
  {
    auto name = widgetAt<QLabel>(&grid, i, 0);
    if (info.name != "Warning")
      name->setText(info.name);
    else
      name->setPixmap(warningIcon);

    if (!info.button)
    {
      auto text = widgetAt<labelElided>(&grid, i, 1);
      text->setText(info.text);
      text->setToolTip(info.toolTip);
    }
    else
    {
      auto button = widgetAt<QPushButton>(&grid, i, 1);
      button->setText(info.text);
      button->setToolTip(info.toolTip);
      button->setProperty(kRow, i);
      connect(button, &QPushButton::clicked, this, &FileInfoWidget::infoButtonClickedSlot, Qt::UniqueConnection);
    }
    grid.setRowStretch(i, 0);
    ++ i;
  }
  clear(i);

  if (i) {
    grid.setColumnStretch(1, 1); // Last column should stretch
    grid.setRowStretch(i-1, 1); // Last row should stretch
  }
}

void FileInfoWidget::infoButtonClickedSlot()
{
  auto button = qobject_cast<QPushButton*>(QObject::sender());
  if (button)
    emit infoButtonClicked(button->property(kRow).toInt());
}
