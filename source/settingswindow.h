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

#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QWidget>
#include <QSettings>
#include "ui_settingswindow.h"

class SettingsWindow : public QWidget
{
  Q_OBJECT
    
public:
  explicit SettingsWindow(QWidget *parent = 0);
  ~SettingsWindow();

  // Get settings
  bool getClearFrameState();
  unsigned int getCacheSizeInMB();

signals:
  // When the save button is pressed, this is emitted.
  void settingsChanged();

private slots:
  
  // Caching checkBox/Slider
  void on_cachingGroupBox_toggled(bool);
  void on_cacheThresholdSlider_valueChanged(int value);

  // Colors buttons
  void on_gridColorButton_clicked();
  void on_bgColorButton_clicked();
  void on_simplifyColorButton_clicked();

  // Statistics color button
  void on_differenceColorButton_clicked();
    
  // Save/Load buttons
  void on_saveButton_clicked();
  void on_cancelButton_clicked();

private:
  // Save/Load the settings from QSettings
  QSettings settings;
  bool saveSettings();
  bool loadSettings();

  // The installed memory size. The constructor sets this.
  unsigned int memSizeInMB;

  Ui::SettingsWindow *ui;
};

#endif // SETTINGSWINDOW_H
