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

#ifndef STATISTICSSTYLECONTROL_H
#define STATISTICSSTYLECONTROL_H

#include <QDialog>
#include <QPen>

#include "statisticsExtensions.h"

namespace Ui {
  class StatisticsStyleControl;
}

class StatisticsStyleControl : public QDialog
{
  Q_OBJECT

public:
  explicit StatisticsStyleControl(QWidget *parent = 0);
  ~StatisticsStyleControl();

  // Set the current statistics item to edit. If this function is called, the controls will be updated to show
  // the current style of the given item. If a control is changed, the style of this given item will be updated.
  void setStatsItem(StatisticsType *item);
signals:
  // The style was changed by the user. Redraw the view.
  void StyleChanged();

private slots:
  // Block data controls
  void on_groupBoxBlockData_clicked(bool check);
  void on_comboBoxDataColorMap_currentIndexChanged(int index);
  void on_frameMinColor_clicked();
  void on_pushButtonEditMinColor_clicked() { on_frameMinColor_clicked(); }
  void on_frameMaxColor_clicked();
  void on_pushButtonEditMaxColor_clicked() { on_frameMaxColor_clicked(); }
  void on_pushButtonEditColorMap_clicked();
  void on_spinBoxRangeMin_valueChanged(int arg1);
  void on_spinBoxRangeMax_valueChanged(int arg1);
  void on_checkBoxScaleValueToBlockSize_stateChanged(int arg1);

  // Vector data controls
  void on_groupBoxVector_clicked(bool check);
  void on_comboBoxVectorLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxVectorLineWidth_valueChanged(double arg1);
  void on_checkBoxVectorScaleToZoom_stateChanged(int arg1);
  void on_comboBoxVectorHeadStyle_currentIndexChanged(int index);
  void on_checkBoxVectorMapToColor_stateChanged(int arg1);
  void on_colorFrameVectorColor_clicked();
  void on_pushButtonEditVectorColor_clicked() { on_colorFrameVectorColor_clicked(); }

  // Grid slots
  void on_groupBoxGrid_clicked(bool check);
  void on_frameGridColor_clicked();
  void on_pushButtonEditGridColor_clicked() { on_frameGridColor_clicked(); }
  void on_comboBoxGridLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxGridLineWidth_valueChanged(double arg1);
  void on_checkBoxGridScaleToZoom_stateChanged(int arg1);

private:
  Ui::StatisticsStyleControl *ui;
  StatisticsType *currentItem;

  // Static list of pen stlyes to convert from Qt::PenStyle to an index and back.
  static const QList<Qt::PenStyle> penStyleList;
};

#endif // STATISTICSSTYLECONTROL_H
