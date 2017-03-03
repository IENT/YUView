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
  // Caching threads check box
  void on_checkBoxNrThreads_stateChanged(int newState);
  void on_checkBoxEnablePlaybackCaching_stateChanged(int state);

  // Colors buttons
  void on_pushButtonEditBackgroundColor_clicked();
  void on_frameBackgroundColor_clicked() { on_pushButtonEditBackgroundColor_clicked(); }
  void on_pushButtonEditGridColor_clicked();
  void on_frameGridLineColor_clicked() { on_pushButtonEditGridColor_clicked(); }

  // FFMpeg lineEdit and path selection button
  void on_pushButtonFFmpegSelectPath_clicked();

  // Save/Load buttons
  void on_pushButtonSave_clicked();
  void on_pushButtonCancel_clicked() { reject(); }

private:
  // Check if the ffmpeg libraries in the path are available.
  // Update the indicator labelFFMpegFound.
  void checkFFmpegPath();

  Ui::SettingsDialog ui;
};

#endif // SETTINGSDIALOG_H
