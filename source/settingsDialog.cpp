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

#include "settingsDialog.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QSettings>
#include "typedef.h"
#include "FFmpegDecoder.h"

#define MIN_CACHE_SIZE_IN_MB (20u)

SettingsDialog::SettingsDialog(QWidget *parent) :
  QDialog(parent)
{
  ui.setupUi(this);

  // Set the minimum and maximum values for memory
  ui.labelMaxMb->setText(QString("%1 MB").arg(systemMemorySizeInMB()));
  ui.labelMinMB->setText(QString("%1 MB").arg(systemMemorySizeInMB() / 100));

  // --- Load the current settings from the QSettings ---
  QSettings settings;

  // Video cache settings
  settings.beginGroup("VideoCache");
  ui.groupBoxCaching->setChecked(settings.value("Enabled", true).toBool());
  ui.sliderThreshold->setValue(settings.value("ThresholdValue", 49).toInt());
  ui.checkBoxNrThreads->setChecked(settings.value("SetNrThreads", false).toBool());
  if (ui.checkBoxNrThreads->isChecked())
    ui.spinBoxNrThreads->setValue(settings.value("NrThreads", getOptimalThreadCount()).toInt());
  else
    ui.spinBoxNrThreads->setValue(getOptimalThreadCount());
  ui.spinBoxNrThreads->setEnabled(ui.checkBoxNrThreads->isChecked());

  // Caching
  ui.checkBoxPausPlaybackForCaching->setChecked(settings.value("PlaybackPauseCaching", true).toBool());
  bool playbackCaching = settings.value("PlaybackCachingEnabled", false).toBool();
  ui.checkBoxEnablePlaybackCaching->setChecked(playbackCaching);
  ui.spinBoxThreadLimit->setValue(settings.value("PlaybackCachingThreadLimit", 1).toInt());
  ui.spinBoxThreadLimit->setEnabled(playbackCaching);
  settings.endGroup();

  // Central view settings
  QString splittingStyleString = settings.value("SplitViewLineStyle", "Solid Line").toString();
  if (splittingStyleString == "Handlers")
    ui.comboBoxSplitLineStyle->setCurrentIndex(1);
  else
    ui.comboBoxSplitLineStyle->setCurrentIndex(0);
  QString mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    ui.comboBoxMouseMode->setCurrentIndex(0);
  else
    ui.comboBoxMouseMode->setCurrentIndex(1);
  QColor backgroundColor = settings.value("Background/Color").value<QColor>();
  QColor gridLineColor = settings.value("OverlayGrid/Color").value<QColor>();
  ui.frameBackgroundColor->setPlainColor(backgroundColor);
  ui.frameGridLineColor->setPlainColor(gridLineColor);
  ui.pushButtonEditBackgroundColor->setIcon(convertIcon(":img_edit.png"));
  ui.pushButtonEditGridColor->setIcon(convertIcon(":img_edit.png"));
  
  // Updates settings
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  ui.groupBoxUpdates->setChecked(checkForUpdates);
  if (UPDATE_FEATURE_ENABLE)
  {
    QString updateBehavior = settings.value("updateBehavior", "ask").toString();
    if (updateBehavior == "ask")
      ui.comboBoxUpdateSettings->setCurrentIndex(1);
    else if (updateBehavior == "auto")
      ui.comboBoxUpdateSettings->setCurrentIndex(0);
  }
  else
    // Updating is not supported. Disable the update strategy combo box.
    ui.groupBoxUpdates->setEnabled(false);
  settings.endGroup();

  // General settings
  ui.checkBoxWatchFiles->setChecked(settings.value("WatchFiles",true).toBool());
  ui.checkBoxContinuePlaybackNewSelection->setChecked(settings.value("ContinuePlaybackOnSequenceSelection",false).toBool());
  QString theme = settings.value("Theme", "Default").toString();
  int themeIdx = getThemeNameList().indexOf(theme);
  if (themeIdx < 0)
    themeIdx = 0;
  ui.comboBoxTheme->addItems(getThemeNameList());
  ui.comboBoxTheme->setCurrentIndex(themeIdx);

  // FFmpeg settings
  ui.lineEditFFmpegPath->setText(settings.value("FFmpegPath","").toString());
  ui.pushButtonFFmpegSelectPath->setIcon(convertIcon(":img_folder.png"));
  checkFFmpegPath();
}

unsigned int SettingsDialog::getCacheSizeInMB() const
{
  unsigned int useMem = 0;
  // update video cache
  if ( ui.groupBoxCaching->isChecked() )
    useMem = systemMemorySizeInMB() * (ui.sliderThreshold->value()+1) / 100;

  return std::max(useMem, MIN_CACHE_SIZE_IN_MB);
}

void SettingsDialog::on_checkBoxNrThreads_stateChanged(int newState)
{
  ui.spinBoxNrThreads->setEnabled(newState);
  if (newState == Qt::Unchecked)
    ui.spinBoxNrThreads->setValue(getOptimalThreadCount());
}

void SettingsDialog::on_checkBoxEnablePlaybackCaching_stateChanged(int state)
{
  // Enable/disable the spinBoxThreadLimit
  ui.spinBoxThreadLimit->setEnabled(state != Qt::Unchecked);
}

void SettingsDialog::on_pushButtonEditBackgroundColor_clicked()
{
  QColor currentColor = ui.frameBackgroundColor->getPlainColor();
  QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentColor != newColor)
    ui.frameBackgroundColor->setPlainColor(newColor);
}

void SettingsDialog::on_pushButtonEditGridColor_clicked()
{
  QColor currentColor = ui.frameGridLineColor->getPlainColor();
  QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentColor != newColor)
    ui.frameGridLineColor->setPlainColor(newColor);
}

void SettingsDialog::on_pushButtonFFmpegSelectPath_clicked()
{
  // Open a path selection dialog
  QSettings settings;

  QFileDialog pathDialog(this);
  pathDialog.setDirectory(settings.value("lastFilePath").toString());
  pathDialog.setFileMode(QFileDialog::Directory);
  pathDialog.setOption(QFileDialog::ShowDirsOnly);

  if (pathDialog.exec())
  {
    QString path = pathDialog.selectedFiles()[0];
    ui.lineEditFFmpegPath->setText(path);

    checkFFmpegPath();
  }
}

void SettingsDialog::checkFFmpegPath()
{
  QString path = ui.lineEditFFmpegPath->text();
  if (!path.isEmpty())
  {
    // Check if the path exists
    QString path = ui.lineEditFFmpegPath->text();
    QFileInfo fi = QFileInfo(path);
    if (fi.exists() && fi.isDir())
    {
      // Check if this directory contains the ffmpeg libraries that we can use.
      // Set the indicator accordingly
      if (FFmpegDecoder::checkForLibraries(path))
      {
        // The directory contains ffmpeg libraries that we can open and use
        ui.labelFFmpegFound->setPixmap(convertPixmap(":img_check.png"));
        ui.labelFFmpegFound->setToolTip("Usable FFmpeg libraries were found in the provided path.");
      }
      else
      {
        ui.labelFFmpegFound->setPixmap(convertPixmap(":img_x.png"));
        ui.labelFFmpegFound->setToolTip("No usable FFmpeg libraries were found in the provided path.");
      }
    }
  }
}

void SettingsDialog::on_pushButtonSave_clicked()
{
  // --- Save the settings ---
  QSettings settings;
  settings.beginGroup("VideoCache");
  settings.setValue("Enabled", ui.groupBoxCaching->isChecked());
  settings.setValue("ThresholdValue", ui.sliderThreshold->value());
  settings.setValue("ThresholdValueMB", getCacheSizeInMB());
  settings.setValue("SetNrThreads", ui.checkBoxNrThreads->isChecked());
  settings.setValue("NrThreads", ui.spinBoxNrThreads->value());
  settings.setValue("PlaybackPauseCaching", ui.checkBoxPausPlaybackForCaching->isChecked());
  settings.setValue("PlaybackCachingEnabled", ui.checkBoxEnablePlaybackCaching->isChecked());
  settings.setValue("PlaybackCachingThreadLimit", ui.spinBoxThreadLimit->value());
  settings.endGroup();

  // Central View Widget
  settings.setValue("SplitViewLineStyle", ui.comboBoxSplitLineStyle->currentText());
  settings.setValue("MouseMode", ui.comboBoxMouseMode->currentText());
  settings.setValue("Background/Color", ui.frameBackgroundColor->getPlainColor());
  settings.setValue("OverlayGrid/Color", ui.frameGridLineColor->getPlainColor());

  // Update settings
  settings.beginGroup("updates");
  settings.setValue("checkForUpdates", ui.groupBoxUpdates->isChecked());
  if (UPDATE_FEATURE_ENABLE)
  {
    QString updateBehavior = "ask";
    if (ui.comboBoxUpdateSettings->currentIndex() == 0)
      updateBehavior = "auto";
    settings.setValue("updateBehavior", updateBehavior);
  }
  settings.endGroup();

  // General settings
  settings.setValue("WatchFiles", ui.checkBoxWatchFiles->isChecked());
  settings.setValue("ContinuePlaybackOnSequenceSelection", ui.checkBoxContinuePlaybackNewSelection->isChecked());
  settings.setValue("Theme", ui.comboBoxTheme->currentText());

  // FFmpeg settings
  settings.setValue("FFmpegPath", ui.lineEditFFmpegPath->text());

  accept();
}

void SettingsDialog::on_sliderThreshold_valueChanged(int value)
{
  ui.labelThreshold->setText(QString("Threshold (%1 MB)").arg(systemMemorySizeInMB() * (value+1) / 100));
}
