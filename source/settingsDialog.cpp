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

#include "settingsDialog.h"

#include <QColorDialog>
#include <QSettings>
#include "typedef.h"

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

  // Get the caching strategy (0: Pause caching, 1: Continue caching, 2: Limit nr threads to N)
  ui.checkBoxPausPlaybackForCaching->setChecked(settings.value("PlaybackPauseCaching").toBool());
  bool playbackCaching = settings.value("PlaybackCachingEnabled", 0).toBool();
  ui.checkBoxEnablePlaybackCaching->setChecked(playbackCaching);
  ui.spinBoxThreadLimit->setValue(settings.value("PlaybackCachingThreadLimit", 1).toInt());
  ui.spinBoxThreadLimit->setEnabled(playbackCaching);
  settings.endGroup();

  // Colors settings
  QColor backgroundColor = settings.value("Background/Color").value<QColor>();
  QColor gridLineColor = settings.value("OverlayGrid/Color").value<QColor>();
  ui.frameBackgroundColor->setPlainColor(backgroundColor);
  ui.frameGridLineColor->setPlainColor(gridLineColor);

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

  // General settings
  ui.checkBoxWatchFiles->setChecked(settings.value("WatchFiles",true).toBool());
  ui.checkBoxContinuePlaybackNewSelection->setChecked(settings.value("ContinuePlaybackOnSequenceSelection",false).toBool());

  settings.endGroup();
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

  settings.setValue("Background/Color", ui.frameBackgroundColor->getPlainColor());
  settings.setValue("OverlayGrid/Color", ui.frameGridLineColor->getPlainColor());
  settings.setValue("SplitViewLineStyle", ui.comboBoxSplitLineStyle->currentText());
  settings.setValue("MouseMode", ui.comboBoxMouseMode->currentText());
  settings.setValue("WatchFiles", ui.checkBoxWatchFiles->isChecked());
  settings.setValue("ContinuePlaybackOnSequenceSelection", ui.checkBoxContinuePlaybackNewSelection->isChecked());

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

  accept();
}

void SettingsDialog::on_sliderThreshold_valueChanged(int value)
{
  ui.labelThreshold->setText(QString("Threshold (%1 MB)").arg(systemMemorySizeInMB() * (value+1) / 100));
}
