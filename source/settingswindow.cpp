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
  QWidget(parent),
  ui(new Ui::SettingsWindow)
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

  ui->setupUi(this);

  // Set the minimum and maximum values for memory
  ui->maxMBLabel->setText(QString("%1 MB").arg(memSizeInMB));
  ui->minMBLabel->setText(QString("%1 MB").arg(memSizeInMB / 100));

  loadSettings();
}

SettingsWindow::~SettingsWindow()
{
  delete ui;
}

bool SettingsWindow::getClearFrameState()
{
  return ui->clearFrameCheckBox->isChecked();
}

unsigned int SettingsWindow::getCacheSizeInMB()
{
  unsigned int useMem = 0;
  // update video cache
  if ( ui->cachingGroupBox->isChecked() ) 
    useMem = memSizeInMB * (ui->cacheThresholdSlider->value()+1) / 100;
  
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
  settings.setValue("Enabled", ui->cachingGroupBox->isChecked());
  settings.setValue("ThresholdValue", ui->cacheThresholdSlider->value());
  settings.setValue("ThresholdValueMB", getCacheSizeInMB());
  settings.endGroup();

  settings.setValue("ClearFrameEnabled",ui->clearFrameCheckBox->isChecked());
  settings.setValue("SplitViewLineStyle", ui->splitLineStyle->currentText());

  emit settingsChanged();

  return true;
}

bool SettingsWindow::loadSettings()
{
  settings.beginGroup("VideoCache");
  ui->cachingGroupBox->setChecked( settings.value("Enabled", true).toBool() );
  ui->cacheThresholdSlider->setValue( settings.value("ThresholdValue", 49).toInt() );
  settings.endGroup();
  ui->clearFrameCheckBox->setChecked(settings.value("ClearFrameEnabled",false).toBool());
  return true;
}

void SettingsWindow::on_cacheThresholdSlider_valueChanged(int value)
{
  ui->cacheThresholdLabel->setText(QString("Threshold (%1 MB)").arg(memSizeInMB * (value+1) / 100));
}

void SettingsWindow::on_cachingGroupBox_toggled(bool enable)
{
  ui->cacheThresholdLabel->setEnabled( enable );
  ui->cacheThresholdSlider->setEnabled( enable );
  ui->maxMBLabel->setEnabled( enable );
  ui->minMBLabel->setEnabled( enable );
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
