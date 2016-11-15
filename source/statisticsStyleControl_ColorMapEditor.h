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

#ifndef STATISTICSSTYLECONTROL_COLORMAPEDITOR_H
#define STATISTICSSTYLECONTROL_COLORMAPEDITOR_H

#include <QDialog>
#include <QMap>
#include <QColor>
#include <QTableWidgetItem>

#include "statisticsExtensions.h"
#include "ui_statisticsStyleControl_ColorMapEditor.h"

class StatisticsStyleControl_ColorMapEditor : public QDialog
{
  Q_OBJECT

public:
  explicit StatisticsStyleControl_ColorMapEditor(const QMap<int, QColor> &colorMap, const QColor &other, QWidget *parent = 0);

  QMap<int, QColor> getColorMap();
  QColor getOtherColor();

public slots:
  // Override from QDialog. Check for dublicate entries.
  virtual void done(int r) Q_DECL_OVERRIDE;

private slots:
  void slotItemClicked(QTableWidgetItem *item);
  void slotItemChanged(QTableWidgetItem *item);
  void on_pushButtonAdd_clicked();
  void on_pushButtonDelete_clicked();

protected:
  Ui::statisticStyleControl_ColorMapEditor ui;

  virtual void keyPressEvent(QKeyEvent *keyEvent) Q_DECL_OVERRIDE;
};

#endif // STATISTICSSTYLECONTROL_COLORMAPEDITOR_H
