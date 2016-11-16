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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "ui_settingsDialog.h"

class SettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = 0);
  
  // Get settings
  unsigned int getCacheSizeInMB() const;

private slots:

  // Caching slider
  void on_sliderThreshold_valueChanged(int value);

  // Colors buttons
  void on_pushButtonEditBackgroundColor_clicked();
  void on_frameBackgroundColor_clicked() { on_pushButtonEditBackgroundColor_clicked(); }
  void on_pushButtonEditGridColor_clicked();
  void on_frameGridLineColor_clicked() { on_pushButtonEditGridColor_clicked(); }

  // Save/Load buttons
  void on_pushButtonSave_clicked();
  void on_pushButtonCancel_clicked() { reject(); }

private:

  // The installed memory size. The constructor sets this.
  unsigned int memSizeInMB;

  Ui::SettingsDialog ui;
};

#endif // SETTINGSDIALOG_H
