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

#include "settingswindow.h"
#include <QMessageBox>
#include <QColorDialog>

#include "typedef.h"

#ifdef Q_OS_MAC
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN32)
#include <windows.h>
#endif

#define MIN_CACHE_SIZE_IN_MB    20
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

SettingsWindow::SettingsWindow(QWidget *parent) :
  QWidget(parent)
{
  // Fet size of main memory - assume 2 GB first.
  // Unfortunately there is no Qt ways of doing this so this is platform dependent.
  memSizeInMB = 2 << 10;
#ifdef Q_OS_MAC
  int mib[2] = { CTL_HW, HW_MEMSIZE };
  u_int namelen = sizeof(mib) / sizeof(mib[0]);
  uint64_t size;
  size_t len = sizeof(size);

  if (sysctl(mib, namelen, &size, &len, NULL, 0) == 0)
    memSizeInMB = size >> 20;
#elif defined Q_OS_UNIX
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  memSizeInMB = (pages * page_size) >> 20;
#elif defined Q_OS_WIN32
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  memSizeInMB = status.ullTotalPhys >> 20;
#endif

  ui.setupUi(this);

  // Set the minimum and maximum values for memory
  ui.maxMBLabel->setText(QString("%1 MB").arg(memSizeInMB));
  ui.minMBLabel->setText(QString("%1 MB").arg(memSizeInMB / 100));

#if !UPDATE_FEATURE_ENABLE
  // Updating is not supported. Disable the update strategy combo box.
  ui.updateSettingComboBox->setEnabled(false);
#endif

  loadSettings();
}

SettingsWindow::~SettingsWindow()
{
}

unsigned int SettingsWindow::getCacheSizeInMB()
{
  unsigned int useMem = 0;
  // update video cache
  if ( ui.cachingGroupBox->isChecked() )
    useMem = memSizeInMB * (ui.cacheThresholdSlider->value()+1) / 100;

  return MAX(useMem, MIN_CACHE_SIZE_IN_MB);
}

void SettingsWindow::on_saveButton_clicked()
{
  if (!saveSettings())
  {
    QMessageBox::warning(this, tr("Settings could not be saved"), tr("Sorry, but some settings could not be saved."), QMessageBox::Ok);
  }
  close();
}

bool SettingsWindow::saveSettings()
{
  settings.beginGroup("VideoCache");
  settings.setValue("Enabled", ui.cachingGroupBox->isChecked());
  settings.setValue("ThresholdValue", ui.cacheThresholdSlider->value());
  settings.setValue("ThresholdValueMB", getCacheSizeInMB());
  settings.endGroup();

  settings.setValue("SplitViewLineStyle", ui.splitLineStyle->currentText());
  settings.setValue("MouseMode", ui.mouseMode->currentText());
  settings.setValue("MapVectorToColor", ui.MapVectorColorCheckBox->isChecked());
  settings.setValue("ClearFrameEnabled", ui.clearFrameCheckBox->isChecked());
  settings.setValue("WatchFiles", ui.watchFilesCheckBox->isChecked());
  settings.setValue("ContinuePlaybackOnSequenceSelection", ui.playbackContinueNewSequenceCheckBox->isChecked());
  
  // Update settings
  settings.beginGroup("updates");
  settings.setValue("checkForUpdates", ui.checkUpdatesGroupBox->isChecked());
#if UPDATE_FEATURE_ENABLE
  QString updateBehavior = "ask";
  if (ui.updateSettingComboBox->currentIndex() == 0)
    updateBehavior = "auto";
  settings.setValue("updateBehavior", updateBehavior);
#endif
  settings.endGroup();

  emit settingsChanged();

  return true;
}

bool SettingsWindow::loadSettings()
{
  settings.beginGroup("VideoCache");
  ui.cachingGroupBox->setChecked( settings.value("Enabled", true).toBool() );
  ui.cacheThresholdSlider->setValue( settings.value("ThresholdValue", 49).toInt() );
  settings.endGroup();

  QString splittingStyleString = settings.value("SplitViewLineStyle", "Solid Line").toString();
  if (splittingStyleString == "Handlers")
    ui.splitLineStyle->setCurrentIndex(1);
  else
    ui.splitLineStyle->setCurrentIndex(0);
  QString mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    ui.mouseMode->setCurrentIndex(0);
  else
    ui.mouseMode->setCurrentIndex(1);
  ui.MapVectorColorCheckBox->setChecked(settings.value("MapVectorToColor",false).toBool());
  ui.clearFrameCheckBox->setChecked(settings.value("ClearFrameEnabled",false).toBool());
  ui.watchFilesCheckBox->setChecked(settings.value("WatchFiles",true).toBool());
  ui.playbackContinueNewSequenceCheckBox->setChecked(settings.value("ContinuePlaybackOnSequenceSelection",false).toBool());

  // Updates settings
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  ui.checkUpdatesGroupBox->setChecked(checkForUpdates);
#if UPDATE_FEATURE_ENABLE
  QString updateBehavior = settings.value("updateBehavior", "ask").toString();
  if (updateBehavior == "ask")
    ui.updateSettingComboBox->setCurrentIndex(1);
  else if (updateBehavior == "auto")
    ui.updateSettingComboBox->setCurrentIndex(0);
#endif
  settings.endGroup();

  return true;
}

void SettingsWindow::on_cacheThresholdSlider_valueChanged(int value)
{
  ui.cacheThresholdLabel->setText(QString("Threshold (%1 MB)").arg(memSizeInMB * (value+1) / 100));
}

void SettingsWindow::on_cachingGroupBox_toggled(bool enable)
{
  ui.cacheThresholdLabel->setEnabled( enable );
  ui.cacheThresholdSlider->setEnabled( enable );
  ui.maxMBLabel->setEnabled( enable );
  ui.minMBLabel->setEnabled( enable );
}

void SettingsWindow::on_gridColorButton_clicked()
{
  QColor curColor = settings.value("OverlayGrid/Color").value<QColor>();
  QColor newColor = QColorDialog::getColor(curColor, this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
  settings.setValue("OverlayGrid/Color", newColor);
}

void SettingsWindow::on_bgColorButton_clicked()
{
  QColor curColor = settings.value("Background/Color").value<QColor>();
  QColor newColor = QColorDialog::getColor(curColor, this, tr("Select background color"));
  settings.setValue("Background/Color", newColor);
}

void SettingsWindow::on_cancelButton_clicked()
{
  loadSettings();
  close();
}

void SettingsWindow::on_differenceColorButton_clicked()
{
  QColor curColor = settings.value("Difference/Color").value<QColor>();
  QColor newColor = QColorDialog::getColor(curColor, this, tr("Select difference color"));
  settings.setValue("Difference/Color", newColor);
}
