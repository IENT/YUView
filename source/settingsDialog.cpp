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
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>
#include "typedef.h"
#include "decoderDav1d.h"
#include "decoderHM.h"
#include "decoderLibde265.h"
#include "FFMpegLibrariesHandling.h"

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

  // "Generals" tab
  ui.checkBoxWatchFiles->setChecked(settings.value("WatchFiles", true).toBool());
  ui.checkBoxAskToSave->setChecked(settings.value("AskToSaveOnExit", true).toBool());
  ui.checkBoxContinuePlaybackNewSelection->setChecked(settings.value("ContinuePlaybackOnSequenceSelection", false).toBool());
  ui.checkBoxSavePositionPerItem->setChecked(settings.value("SavePositionAndZoomPerItem", false).toBool());
  // UI
  QString theme = settings.value("Theme", "Default").toString();
  int themeIdx = getThemeNameList().indexOf(theme);
  if (themeIdx < 0)
    themeIdx = 0;
  ui.comboBoxTheme->addItems(getThemeNameList());
  ui.comboBoxTheme->setCurrentIndex(themeIdx);
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
  ui.checkBoxPlaybackControlFullScreen->setChecked(settings.value("ShowPlaybackControlFullScreen", false).toBool());
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
  {
    // Updating is not supported. Disable the update strategy combo box.
    ui.comboBoxUpdateSettings->setEnabled(false);
    ui.labelUpdateSettings->setEnabled(false);
  }
  settings.endGroup();

  // "Caching" tab
  settings.beginGroup("VideoCache");
  ui.groupBoxCaching->setChecked(settings.value("Enabled", true).toBool());
  ui.sliderThreshold->setValue(settings.value("ThresholdValue", 49).toInt());
  ui.checkBoxNrThreads->setChecked(settings.value("SetNrThreads", false).toBool());
  if (ui.checkBoxNrThreads->isChecked())
    ui.spinBoxNrThreads->setValue(settings.value("NrThreads", getOptimalThreadCount()).toInt());
  else
    ui.spinBoxNrThreads->setValue(getOptimalThreadCount());
  ui.spinBoxNrThreads->setEnabled(ui.checkBoxNrThreads->isChecked());
  // Playback
  ui.checkBoxPausPlaybackForCaching->setChecked(settings.value("PlaybackPauseCaching", true).toBool());
  bool playbackCaching = settings.value("PlaybackCachingEnabled", false).toBool();
  ui.checkBoxEnablePlaybackCaching->setChecked(playbackCaching);
  ui.spinBoxThreadLimit->setValue(settings.value("PlaybackCachingThreadLimit", 1).toInt());
  ui.spinBoxThreadLimit->setEnabled(playbackCaching);
  settings.endGroup();

  // "Decoders" tab
  settings.beginGroup("Decoders");
  ui.lineEditDecoderPath->setText(settings.value("SearchPath", "").toString());

  for (int i=0; i<decoderEngineNum; i++)
    ui.comboBoxDefaultDecoder->addItem(getDecoderEngineName((decoderEngine)i));
  ui.comboBoxDefaultDecoder->setCurrentIndex(settings.value("DefaultDecoder", 0).toInt());

  ui.lineEditLibde265File->setText(settings.value("libde265File", "").toString());
  ui.lineEditLibHMFile->setText(settings.value("libHMFile", "").toString());
  ui.lineEditLibDav1d->setText(settings.value("libDav1dFile", "").toString());
  ui.lineEditLibJEMFile->setText(settings.value("libJEMFile", "").toString());
  ui.lineEditAVFormat->setText(settings.value("FFmpeg.avformat", "").toString());
  ui.lineEditAVCodec->setText(settings.value("FFmpeg.avcodec", "").toString());
  ui.lineEditAVUtil->setText(settings.value("FFmpeg.avutil", "").toString());
  ui.lineEditSWResample->setText(settings.value("FFmpeg.swresample", "").toString());
  settings.endGroup();
}

unsigned int SettingsDialog::getCacheSizeInMB() const
{
  unsigned int useMem = 0;
  // update video cache
  if (ui.groupBoxCaching->isChecked())
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

void SettingsDialog::on_pushButtonDecoderSelectPath_clicked()
{
  // Open a path selection dialog
  QSettings settings;

  // Use the currently selected dir or the dir to YUView if this one does not exist.
  QDir curDir = QDir(settings.value("SearchPath", "").toString());
  if (!curDir.exists())
    curDir = QDir::currentPath();

  QFileDialog pathDialog(this);
  pathDialog.setDirectory(curDir);
  pathDialog.setFileMode(QFileDialog::Directory);
  pathDialog.setOption(QFileDialog::ShowDirsOnly);

  if (pathDialog.exec())
  {
    QString path = pathDialog.selectedFiles()[0];
    ui.lineEditDecoderPath->setText(path);
  }
}

QStringList SettingsDialog::getLibraryPath(QString currentFile, QString caption, bool multipleFiles)
{
  // Open a file selection dialog
  
  // Use the currently selected dir or the dir to YUView if this one does not exist.
  QFileInfo curFile(currentFile);
  QDir curDir = curFile.absolutePath();
  if (!curDir.exists())
    curDir = QDir::currentPath();

  QFileDialog fileDialog(this, caption);
  fileDialog.setDirectory(curDir);
  fileDialog.setFileMode(multipleFiles ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
  if (is_Q_OS_LINUX)
    fileDialog.setNameFilter("*.so.*");
  if (is_Q_OS_MAC)
    fileDialog.setNameFilter("*.dylib");
  if (is_Q_OS_WIN)
    fileDialog.setNameFilter("*.dll");

  if (fileDialog.exec())
    return fileDialog.selectedFiles();
  
  return QStringList();
}

void SettingsDialog::on_pushButtonLibde265SelectFile_clicked()
{
  QStringList newFiles = getLibraryPath(ui.lineEditLibde265File->text(), "Please select the libde265 library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoderLibde265::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(this, "Error testing the library", "The selected file does not appear to be a usable libde265 library. Error: " + error);
  else
    ui.lineEditLibde265File->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonlibHMSelectFile_clicked()
{
  QStringList newFiles = getLibraryPath(ui.lineEditLibHMFile->text(), "Please select the libHMDecoder library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoderHM::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(this, "Error testing the library", "The selected file does not appear to be a usable libHMDecoder library. Error: " + error);
  else
    ui.lineEditLibHMFile->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonLibJEMSelectFile_clicked()
{
  QStringList newFiles = getLibraryPath(ui.lineEditLibJEMFile->text(), "Please select the libJEMDecoder library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  // TODO
  /*if (!hevcNextGenDecoderJEM::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(this, "Error testing the library", "The selected file does not appear to be a usable libJEMDecoder library. Error: " + error);
  else
    ui.lineEditLibJEMFile->setText(newFiles[0]);*/
}

void SettingsDialog::on_pushButtonLibDav1dSelectFile_clicked()
{
  QStringList newFiles = getLibraryPath(ui.lineEditLibDav1d->text(), "Please select the libDav1d library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoderDav1d::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(this, "Error testing the library", "The selected file does not appear to be a usable libDav1d library. Error: " + error);
  else
    ui.lineEditLibDav1d->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonFFMpegSelectFile_clicked()
{
  QStringList newFiles = getLibraryPath(ui.lineEditAVFormat->text(), "Please select the 4 FFmpeg libraries AVCodec, AVFormat, AVUtil and SWResample.", true);
  if (newFiles.empty())
    return;
  
  // Get the 4 libraries from the list
  QString avCodecLib, avFormatLib, avUtilLib, swResampleLib;
  if (newFiles.count() == 4)
  {
    for (QString file : newFiles)
    {
      QFileInfo fileInfo(file);
      if (fileInfo.baseName().contains("avcodec", Qt::CaseInsensitive))
        avCodecLib = file;
      if (fileInfo.baseName().contains("avformat", Qt::CaseInsensitive))
        avFormatLib = file;
      if (fileInfo.baseName().contains("avutil", Qt::CaseInsensitive))
        avUtilLib = file;
      if (fileInfo.baseName().contains("swresample", Qt::CaseInsensitive))
        swResampleLib = file;
    }
  }
  if (avCodecLib.isEmpty() || avFormatLib.isEmpty() || avUtilLib.isEmpty() || swResampleLib.isEmpty())
  {
    QMessageBox::critical(this, "Error in file selection", "Please select the four FFmpeg files AVCodec, AVFormat, AVUtil and SWresample.");
    return;
  }

  // Try to open ffmpeg using the four libraries
  QStringList logList;
  if (!FFmpegVersionHandler::checkLibraryFiles(avCodecLib, avFormatLib, avUtilLib, swResampleLib, logList))
  {
    QMessageBox::StandardButton b = QMessageBox::question(this, "Error opening the library", "The selected file does not appear to be a usable ffmpeg avFormat library. \nWe have collected a more detailed log. Do you want to save it to disk?");
    if (b == QMessageBox::Yes)
    {
      QString filePath = QFileDialog::getSaveFileName(this, "Select a destination for the log file.");
      QFile logFile(filePath);
      logFile.open(QIODevice::WriteOnly);
      if (logFile.isOpen())
      {
        QTextStream outputStream(&logFile);
        for (auto l : logList)
          outputStream << l << "\n";
      }
      else
        QMessageBox::information(this, "Error opening file", "There was an error opening the log file " + filePath);
    }
  }
  else
  {
    ui.lineEditAVCodec->setText(avCodecLib);
    ui.lineEditAVFormat->setText(avFormatLib);
    ui.lineEditAVUtil->setText(avUtilLib);
    ui.lineEditSWResample->setText(swResampleLib);
  }
}

void SettingsDialog::on_pushButtonSave_clicked()
{
  // --- Save the settings ---
  QSettings settings;

  // "General" tab
  settings.setValue("WatchFiles", ui.checkBoxWatchFiles->isChecked());
  settings.setValue("AskToSaveOnExit", ui.checkBoxAskToSave->isChecked());
  settings.setValue("ContinuePlaybackOnSequenceSelection", ui.checkBoxContinuePlaybackNewSelection->isChecked());
  settings.setValue("SavePositionAndZoomPerItem", ui.checkBoxSavePositionPerItem->isChecked());
  // UI
  settings.setValue("Theme", ui.comboBoxTheme->currentText());
  settings.setValue("SplitViewLineStyle", ui.comboBoxSplitLineStyle->currentText());
  settings.setValue("MouseMode", ui.comboBoxMouseMode->currentText());
  settings.setValue("Background/Color", ui.frameBackgroundColor->getPlainColor());
  settings.setValue("OverlayGrid/Color", ui.frameGridLineColor->getPlainColor());
  settings.setValue("ShowPlaybackControlFullScreen", ui.checkBoxPlaybackControlFullScreen->isChecked());
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

  // "Caching" tab
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

  // "Decoders" tab
  settings.beginGroup("Decoders");
  settings.setValue("SearchPath", ui.lineEditDecoderPath->text());
  settings.setValue("DefaultDecoder", ui.comboBoxDefaultDecoder->currentIndex());
  // Raw coded video files
  settings.setValue("libde265File", ui.lineEditLibde265File->text());
  settings.setValue("libHMFile", ui.lineEditLibHMFile->text());
  settings.setValue("libJEMFile", ui.lineEditLibJEMFile->text());
  settings.setValue("libDav1dFile", ui.lineEditLibDav1d->text());
  // FFMpeg files
  settings.setValue("FFmpeg.avformat", ui.lineEditAVFormat->text());
  settings.setValue("FFmpeg.avcodec", ui.lineEditAVCodec->text());
  settings.setValue("FFmpeg.avutil", ui.lineEditAVUtil->text());
  settings.setValue("FFmpeg.swresample", ui.lineEditSWResample->text());
  settings.endGroup();
  
  accept();
}

void SettingsDialog::on_sliderThreshold_valueChanged(int value)
{
  ui.labelThreshold->setText(QString("Threshold (%1 MB)").arg(systemMemorySizeInMB() * (value+1) / 100));
}
