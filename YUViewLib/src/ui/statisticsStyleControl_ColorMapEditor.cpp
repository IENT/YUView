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

#include <QColorDialog>
#include <QKeyEvent>
#include <QMessageBox>

#include "common/functions.h"
#include "common/typedef.h"

StatisticsStyleControl_ColorMapEditor::StatisticsStyleControl_ColorMapEditor(const QMap<int, QColor> &colorMap, const QColor &other, QWidget *parent) :
  QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint)
{
  ui.setupUi(this);

  ui.colorMapTable->setRowCount(colorMap.count() + 1);
  ui.pushButtonAdd->setIcon(functions::convertIcon(":img_add.png"));
  ui.pushButtonDelete->setIcon(functions::convertIcon(":img_delete.png"));

  // Put all the colors from the colorMap into the table widget
  int count = 0;
  for (auto i = colorMap.begin(); i != colorMap.end(); ++i)
  {
    QTableWidgetItem *newItem = new QTableWidgetItem();
    newItem->setData(Qt::EditRole, i.key());
    ui.colorMapTable->setItem(count, 0, newItem);
    
    newItem = new QTableWidgetItem();
    newItem->setBackgroundColor(i.value());
    ui.colorMapTable->setItem(count, 1, newItem);

    count++;
  }

  // Into the last row, put the item for "other"
  QTableWidgetItem *newItem = new QTableWidgetItem("Other");
  newItem->setFlags((~newItem->flags()) & Qt::ItemIsEditable);
  ui.colorMapTable->setItem(count, 0, newItem);
  // with a white color value.
  newItem = new QTableWidgetItem();
  newItem->setBackgroundColor(other);
  ui.colorMapTable->setItem(count, 1, newItem);
  
  // Connect the signals for editing
  connect(ui.colorMapTable, &QTableWidget::itemClicked, this, &StatisticsStyleControl_ColorMapEditor::slotItemClicked);
  connect(ui.colorMapTable, &QTableWidget::itemChanged, this, &StatisticsStyleControl_ColorMapEditor::slotItemChanged);
}

QMap<int, QColor> StatisticsStyleControl_ColorMapEditor::getColorMap()
{
  // Get all value/color combos and return them as a color map list
  QMap<int, QColor> colorMap;

  for (int row = 0; row < ui.colorMapTable->rowCount(); row++)
  {
    QTableWidgetItem *item0 = ui.colorMapTable->item(row, 0);
    QTableWidgetItem *item1 = ui.colorMapTable->item(row, 1);

    if (item0->text() != "Other")
    {
      int val = item0->data(Qt::EditRole).toInt();
      QColor color = item1->backgroundColor();

      colorMap.insert(val, color);
    }
  }

  return colorMap;
}

QColor StatisticsStyleControl_ColorMapEditor::getOtherColor()
{
  // This should be the last entry in the list
  int row = ui.colorMapTable->rowCount() - 1;
  return ui.colorMapTable->item(row, 1)->backgroundColor();
}

void StatisticsStyleControl_ColorMapEditor::slotItemChanged(QTableWidgetItem *item)
{
  if (item->column() != 0)
    return;

  ui.colorMapTable->sortItems(0);
}

void StatisticsStyleControl_ColorMapEditor::on_pushButtonAdd_clicked()
{
  // Add a new entry at the end of the list with the index of the last item plus 1
  // The last item should be the "other" item. So the imte we are looking for is before that item.
  int rowCount = ui.colorMapTable->rowCount();

  // The value of the new item. Get the value of the last item + 1 (if it exists)
  int newValue = 0;
  if (rowCount > 1)
    newValue = ui.colorMapTable->item(rowCount-2, 0)->data(Qt::EditRole).toInt() + 1;

  // Save the color of the "other" entry
  QColor otherColor = ui.colorMapTable->item(rowCount - 1, 1)->backgroundColor();

  // Add a new item
  ui.colorMapTable->insertRow(rowCount);

  QTableWidgetItem *newItem = new QTableWidgetItem();
  newItem->setData(Qt::EditRole, newValue);
  ui.colorMapTable->setItem(rowCount-1, 0, newItem);

  newItem = new QTableWidgetItem();
  newItem->setBackgroundColor(Qt::black);
  ui.colorMapTable->setItem(rowCount-1, 1, newItem);

  // Add the "other" item at the last position again
  newItem = new QTableWidgetItem("Other");
  newItem->setFlags((~newItem->flags()) & Qt::ItemIsEditable);
  ui.colorMapTable->setItem(rowCount, 0, newItem);
  // with the same color as it was before.
  newItem = new QTableWidgetItem();
  newItem->setBackgroundColor(otherColor);
  ui.colorMapTable->setItem(rowCount, 1, newItem);

  ui.colorMapTable->sortItems(0);

  // Otherwise the color item is not initialized correctly ...
  disconnect(ui.colorMapTable, &QTableWidget::itemClicked, nullptr, nullptr);
  connect(ui.colorMapTable, &QTableWidget::itemClicked, this, &StatisticsStyleControl_ColorMapEditor::slotItemClicked);
}

void StatisticsStyleControl_ColorMapEditor::on_pushButtonDelete_clicked()
{
  // Delete the currently selected rows
  QList<QTableWidgetItem*> selection = ui.colorMapTable->selectedItems();

  for (QTableWidgetItem* item : selection)
  {
    if (item->column() == 1 && item->row() != ui.colorMapTable->rowCount()-1)
    {
      ui.colorMapTable->removeRow(item->row());
    }
  }
}

void StatisticsStyleControl_ColorMapEditor::slotItemClicked(QTableWidgetItem *item)
{
  if (item->column() != 1)
    return;

  QColor oldColor = item->backgroundColor();
  QColor newColor = QColorDialog::getColor(oldColor, this, tr("Select color range maximum"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && newColor != oldColor)
  {
    // Set the new color
    item->setBackgroundColor(newColor);
  }
}

void StatisticsStyleControl_ColorMapEditor::keyPressEvent(QKeyEvent *keyEvent)
{
  if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Delete)
    // Same as pressing the delete button
    on_pushButtonDelete_clicked();
  if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Backspace)
    // On the mac, the backspace key is used as the delete key
    on_pushButtonDelete_clicked();
  else if (keyEvent->modifiers() == Qt::NoModifier && keyEvent->key() == Qt::Key_Insert)
    // Same as pushing the add button
    on_pushButtonAdd_clicked();
  else
    QWidget::keyPressEvent(keyEvent);
}

void StatisticsStyleControl_ColorMapEditor::done(int r)
{
  if (r != QDialog::Accepted)
  {
    // The user pressed cancel. The values don't need to be checked.
    QDialog::done(r);
    return;
  }

  bool dublicates = false;
  int previousValue = -1;
  for (int row = 0; row < ui.colorMapTable->rowCount(); row++)
  {
    QTableWidgetItem *item = ui.colorMapTable->item(row, 0);

    if (item->text() != "Other" && row > 0)
    {
      int val = item->data(Qt::EditRole).toInt();
      
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
    if (QMessageBox::question(this, "Dublicate Items", "There are dublicate items in the list. Only the last value of dublicates will be set in the map. Do you want to continue?") == QMessageBox::Yes)
    {
      QDialog::done(r);
      return;
    }
  }
  else
    QDialog::done(r);
}
