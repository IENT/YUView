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

#include "statisticsStyleControl_ColorMapEditor.h"

#include <common/FunctionsGui.h>
#include <common/Typedef.h>

#include <QColorDialog>
#include <QKeyEvent>
#include <QMessageBox>

StatisticsStyleControl_ColorMapEditor::StatisticsStyleControl_ColorMapEditor(
    const std::map<int, Color> &colorMap, const Color &other, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint)
{
  ui.setupUi(this);

  ui.colorMapTable->setRowCount(int(colorMap.size()) + 1);

  // Put all the colors from the colorMap into the table widget
  int count = 0;
  for (const auto &entry : colorMap)
  {
    auto *newItem = new QTableWidgetItem();
    newItem->setData(Qt::EditRole, entry.first);
    ui.colorMapTable->setItem(count, 0, newItem);

    newItem = new QTableWidgetItem();
    newItem->setBackground(QBrush(functionsGui::toQColor(entry.second)));
    ui.colorMapTable->setItem(count, 1, newItem);

    count++;
  }

  // Into the last row, put the item for "other"
  auto *newItem = new QTableWidgetItem("Other");
  newItem->setFlags((~newItem->flags()) & Qt::ItemIsEditable);
  ui.colorMapTable->setItem(count, 0, newItem);
  // with a white color value.
  newItem = new QTableWidgetItem();
  newItem->setBackground(QBrush(functionsGui::toQColor(other)));
  this->ui.colorMapTable->setItem(count, 1, newItem);

  // Connect the signals for editing
  this->connect(this->ui.colorMapTable,
                &QTableWidget::itemClicked,
                this,
                &StatisticsStyleControl_ColorMapEditor::slotItemClicked);
  this->connect(this->ui.colorMapTable,
                &QTableWidget::itemChanged,
                this,
                &StatisticsStyleControl_ColorMapEditor::slotItemChanged);
}

std::map<int, Color> StatisticsStyleControl_ColorMapEditor::getColorMap() const
{
  std::map<int, Color> colorMap;
  const auto           table = this->ui.colorMapTable;

  for (int row = 0; row < ui.colorMapTable->rowCount(); row++)
  {
    auto item0 = table->item(row, 0);
    auto item1 = table->item(row, 1);

    if (item0->text() != "Other")
    {
      auto val   = item0->data(Qt::EditRole).toInt();
      auto color = functionsGui::toColor(item1->background().color());

      colorMap[val] = color;
    }
  }

  return colorMap;
}

Color StatisticsStyleControl_ColorMapEditor::getOtherColor() const
{
  const auto table       = this->ui.colorMapTable;
  auto       row         = table->rowCount() - 1;
  auto       otherQColor = table->item(row, 1)->background().color();
  return functionsGui::toColor(otherQColor);
}

void StatisticsStyleControl_ColorMapEditor::slotItemChanged(QTableWidgetItem *item)
{
  if (item->column() != 0)
    return;

  this->ui.colorMapTable->sortItems(0);
}

void StatisticsStyleControl_ColorMapEditor::on_pushButtonAdd_clicked()
{
  const auto table = this->ui.colorMapTable;

  // Add a new entry at the end of the list with the index of the last item plus 1
  // The last item should be the "other" item. So the item we are looking for is before that item.
  auto rowCount = table->rowCount();

  // The value of the new item. Get the value of the last item + 1 (if it exists)
  int newValue = 0;
  if (rowCount > 1)
    newValue = table->item(rowCount - 2, 0)->data(Qt::EditRole).toInt() + 1;

  // Save the color of the "other" entry
  auto otherColor = table->item(rowCount - 1, 1)->background().color();

  table->insertRow(rowCount);

  auto *newItem = new QTableWidgetItem();
  newItem->setData(Qt::EditRole, newValue);
  table->setItem(rowCount - 1, 0, newItem);
  newItem = new QTableWidgetItem();
  newItem->setBackground(QBrush(Qt::black));
  table->setItem(rowCount - 1, 1, newItem);

  // Add the "other" item at the last position again with the same color as it was before.
  newItem = new QTableWidgetItem("Other");
  newItem->setFlags((~newItem->flags()) & Qt::ItemIsEditable);
  table->setItem(rowCount, 0, newItem);
  newItem = new QTableWidgetItem();
  newItem->setBackground(QBrush(otherColor));
  table->setItem(rowCount, 1, newItem);

  table->sortItems(0);

  // Otherwise the color item is not initialized correctly ...
  this->disconnect(table, &QTableWidget::itemClicked, nullptr, nullptr);
  this->connect(table,
                &QTableWidget::itemClicked,
                this,
                &StatisticsStyleControl_ColorMapEditor::slotItemClicked);
}

void StatisticsStyleControl_ColorMapEditor::on_pushButtonDelete_clicked()
{
  const auto table = this->ui.colorMapTable;
  for (auto item : table->selectedItems())
  {
    if (item->column() == 1 && item->row() != table->rowCount() - 1)
      table->removeRow(item->row());
  }
}

void StatisticsStyleControl_ColorMapEditor::slotItemClicked(QTableWidgetItem *item)
{
  if (item->column() != 1)
    return;

  auto oldColor = item->background().color();
  auto newColor = QColorDialog::getColor(
      oldColor, this, tr("Select color range maximum"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && newColor != oldColor)
    item->setBackground(QBrush(newColor));
}

void StatisticsStyleControl_ColorMapEditor::keyPressEvent(QKeyEvent *keyEvent)
{
  if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Delete)
    this->on_pushButtonDelete_clicked();
  if (is_Q_OS_MAC && keyEvent->modifiers() == Qt::NoModifier &&
      keyEvent->key() == Qt::Key_Backspace)
    this->on_pushButtonDelete_clicked();
  else if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Insert)
    this->on_pushButtonAdd_clicked();
  else
    QWidget::keyPressEvent(keyEvent);
}

void StatisticsStyleControl_ColorMapEditor::done(int r)
{
  if (r != QDialog::Accepted)
  {
    QDialog::done(r);
    return;
  }

  bool dublicates    = false;
  int  previousValue = -1;
  for (int row = 0; row < ui.colorMapTable->rowCount(); row++)
  {
    auto item = ui.colorMapTable->item(row, 0);
    if (item->text() != "Other" && row > 0)
    {
      auto val = item->data(Qt::EditRole).toInt();
      if (val == previousValue)
      {
        dublicates = true;
        break;
      }
      previousValue = val;
    }
  }

  if (dublicates)
  {
    if (QMessageBox::question(this,
                              "Dublicate Items",
                              "There are dublicate items in the list. Only the last value of "
                              "dublicates will be set in the map. Do you want to continue?") ==
        QMessageBox::Yes)
    {
      QDialog::done(r);
      return;
    }
  }
  else
    QDialog::done(r);
}
