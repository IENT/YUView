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

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit SettingsWindow(QWidget *parent = 0);
    ~SettingsWindow();
    unsigned int getCacheSizeInMB();
    bool getClearFrameState();

signals:
    void settingsChanged();

private slots:
    void on_saveButton_clicked();

    void on_cacheThresholdSlider_valueChanged(int value);

    void on_cacheCheckBox_stateChanged(int);

    void on_gridColorButton_clicked();
    void on_bgColorButton_clicked();

    void on_cancelButton_clicked();

    void on_simplifyColorButton_clicked();

    void on_clearFrameCheckBox_stateChanged(int);

private:
    bool saveSettings();
    bool loadSettings();

    unsigned int p_memSizeInMB;

    Ui::SettingsWindow *ui;
    QSettings settings;
};

#endif // SETTINGSWINDOW_H
