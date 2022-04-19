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

#pragma once

#include <QDialog>

#include "ui_statisticsstylecontrol.h"

namespace stats
{
class StatisticsType;
}

class StatisticsStyleControl : public QDialog
{
  Q_OBJECT

public:
  explicit StatisticsStyleControl(QWidget *parent = 0);

  // Set the current statistics item to edit. If this function is called, the controls will be
  // updated to show the current style of the given item. If a control is changed, the style of this
  // given item will be updated.
  void setStatsItem(stats::StatisticsType *item);
signals:
  // The style was changed by the user. Redraw the view.
  void StyleChanged();

private slots:
  // Block data controls
  void on_groupBoxBlockData_clicked(bool check);
  void on_checkBoxScaleValueToBlockSize_stateChanged(int val);
  void on_blockDataTab_currentChanged(int index);
  void on_comboBoxPreset_currentIndexChanged(int index);
  void on_spinBoxPresetRangeMin_valueChanged(int val);
  void on_spinBoxPresetRangeMax_valueChanged(int val);
  void on_frameGradientStartColor_clicked();
  void on_pushButtonGradientEditStartColor_clicked();
  void on_frameGradientEndColor_clicked();
  void on_pushButtonGradientEditEndColor_clicked();
  void on_spinBoxGradientRangeMin_valueChanged(int val);
  void on_spinBoxGradientRangeMax_valueChanged(int val);
  void on_comboBoxCustomMap_currentIndexChanged(int index);
  void on_pushButtonEditMap_clicked();
  void on_pushButtonSaveMap_clicked();
  void on_pushButtonDeleteMap_clicked();

  // Vector data controls
  void on_groupBoxVector_clicked(bool check);
  void on_comboBoxVectorLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxVectorLineWidth_valueChanged(double val);
  void on_checkBoxVectorScaleToZoom_stateChanged(int val);
  void on_comboBoxVectorHeadStyle_currentIndexChanged(int index);
  void on_checkBoxVectorMapToColor_stateChanged(int val);
  void on_colorFrameVectorColor_clicked();
  void on_pushButtonEditVectorColor_clicked() { on_colorFrameVectorColor_clicked(); }

  // Grid slots
  void on_groupBoxGrid_clicked(bool check);
  void on_frameGridColor_clicked();
  void on_pushButtonEditGridColor_clicked() { on_frameGridColor_clicked(); }
  void on_comboBoxGridLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxGridLineWidth_valueChanged(double val);
  void on_checkBoxGridScaleToZoom_stateChanged(int val);

private:
  Ui::StatisticsStyleControl ui;
  stats::StatisticsType *    currentItem;
};
