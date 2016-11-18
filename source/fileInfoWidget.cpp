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

#include <QPointer>
#include <QPushButton>
#include <QVariant>
#include "labelElided.h"

static const char kInfoIndex[] = "fi_infoIndex";
static const char kInfoRow[] = "fi_infoRow";

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

// Returns a possibly new widget at given row and column, having a set column span.
// Any existing widgets of other types or other span will be removed.
template <typename W> static W * widgetAt(QGridLayout *grid, int row, int column, int columnSpan = 1)
{
  Q_ASSERT(grid->columnCount() <= 2);
  QPointer<QWidget> widgets[2];
  for (int j = 0; j < grid->columnCount(); ++j)
  {
    auto item = grid->itemAtPosition(row, j);
    if (item) widgets[j] = item->widget();
  }

  if (columnSpan == 1 && widgets[0] == widgets[1])
    delete widgets[0];
  if (columnSpan == 2 && widgets[0] != widgets[2])
    for (auto & w : widgets)
      delete w;

  auto widget = qobject_cast<W*>(widgets[column]);
  if (!widget)
  {
    // There may be an incompatible widget there.
    delete widgets[column];
    widget = new W;
    grid->addWidget(widget, row, column, 1, columnSpan, Qt::AlignTop);
  }
  return widget;
}

int FileInfoWidget::addInfo(const infoData &data, int row, int infoIndex)
{
  int const firstRow = row;
  for (auto &info : data.items)
  {
    auto name = widgetAt<QLabel>(&grid, row, 0);
    if (info.name != "Warning")
      name->setText(info.name);
    else
      name->setPixmap(warningIcon);

    if (!info.button)
    {
      auto text = widgetAt<labelElided>(&grid, row, 1);
      text->setText(info.text);
      text->setToolTip(info.toolTip);
    }
    else
    {
      auto button = widgetAt<QPushButton>(&grid, row, 1);
      button->setText(info.text);
      button->setToolTip(info.toolTip);
      button->setProperty(kInfoIndex, infoIndex);
      button->setProperty(kInfoRow, row - firstRow);
      connect(button, &QPushButton::clicked, this, &FileInfoWidget::infoButtonClickedSlot, Qt::UniqueConnection);
    }
    grid.setRowStretch(row, 0);
    ++ row;
  }
  return row;
}

void FileInfoWidget::setInfo(const infoData &info1, const infoData &info2)
{
  QString topTitle = FILEINFOWIDGET_DEFAULT_WINDOW_TITLE;
  int row = 0;

  if (!info2.isEmpty())
    widgetAt<QLabel>(&grid, row++, 0, 2)->setText(QStringLiteral("1: %1").arg(info1.title));
  else
    if (!info1.title.isEmpty()) topTitle = info1.title;

  row = addInfo(info1, row, 0);

  if (!info2.isEmpty())
  {
    widgetAt<QFrame>(&grid, row++, 0, 2)->setFrameStyle(QFrame::HLine);
    widgetAt<QLabel>(&grid, row++, 0, 2)->setText(QStringLiteral("2: %2").arg(info2.title));
  }

  row = addInfo(info2, row, 1);

  clear(row);

  if (row)
  {
    grid.setColumnStretch(1, 1); // Last column should stretch
    grid.setRowStretch(row-1, 1); // Last row should stretch
  }

  if (parentWidget())
    parentWidget()->setWindowTitle(topTitle);
}

void FileInfoWidget::infoButtonClickedSlot()
{
  auto button = qobject_cast<QPushButton*>(QObject::sender());
  if (button)
    emit infoButtonClicked(button->property(kInfoIndex).toInt(),
                           button->property(kInfoRow).toInt());
}
