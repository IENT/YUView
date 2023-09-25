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

#include "SettingsDialog.h"

#include <common/Functions.h>
#include <common/Typedef.h>
#include <decoder/decoderDav1d.h>
#include <decoder/decoderHM.h>
#include <decoder/decoderLibde265.h>
#include <decoder/decoderVTM.h>
#include <decoder/decoderVVDec.h>
#include <ffmpeg/FFmpegVersionHandler.h>

#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>

#define MIN_CACHE_SIZE_IN_MB (20u)

namespace
{

QStringList getLibraryPath(QWidget *parent, QString currentFile, QString caption)
{
  // Use the currently selected dir or the dir to YUView if this one does not exist.
  QFileInfo curFile(currentFile);
  auto      curDir = curFile.absoluteDir();
  if (!curDir.exists())
    curDir = QDir::current();

  QFileDialog fileDialog(parent, caption);
  fileDialog.setDirectory(curDir);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  if (is_Q_OS_LINUX)
    fileDialog.setNameFilter("Library files (*.so.* *.so)");
  if (is_Q_OS_MAC)
    fileDialog.setNameFilter("Library files (*.dylib)");
  if (is_Q_OS_WIN)
    fileDialog.setNameFilter("Library files (*.dll)");

  if (fileDialog.exec())
    return fileDialog.selectedFiles();

  return {};
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
  ui.setupUi(this);

  // Set the minimum and maximum values for memory
  ui.labelMaxMb->setText(QString("%1 MB").arg(functions::systemMemorySizeInMB()));
  ui.labelMinMB->setText(QString("%1 MB").arg(functions::systemMemorySizeInMB() / 100));

  // --- Load the current settings from the QSettings ---
  QSettings settings;

  // "Generals" tab
  ui.checkBoxWatchFiles->setChecked(settings.value("WatchFiles", true).toBool());
  ui.checkBoxAskToSave->setChecked(settings.value("AskToSaveOnExit", true).toBool());
  ui.checkBoxContinuePlaybackNewSelection->setChecked(
      settings.value("ContinuePlaybackOnSequenceSelection", false).toBool());
  ui.checkBoxSavePositionPerItem->setChecked(
      settings.value("SavePositionAndZoomPerItem", false).toBool());
  // UI
  const auto theme    = settings.value("Theme", "Default").toString();
  int        themeIdx = functions::getThemeNameList().indexOf(theme);
  if (themeIdx < 0)
    themeIdx = 0;
  ui.comboBoxTheme->addItems(functions::getThemeNameList());
  ui.comboBoxTheme->setCurrentIndex(themeIdx);
  // Central view settings
  const auto splittingStyleString = settings.value("SplitViewLineStyle", "Solid Line").toString();
  if (splittingStyleString == "Handlers")
    ui.comboBoxSplitLineStyle->setCurrentIndex(1);
  else
    ui.comboBoxSplitLineStyle->setCurrentIndex(0);
  const auto mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    ui.comboBoxMouseMode->setCurrentIndex(0);
  else
    ui.comboBoxMouseMode->setCurrentIndex(1);
  ui.viewBackgroundColor->setPlainColor(settings.value("View/BackgroundColor").value<QColor>());
  ui.viewGridLineColor->setPlainColor(settings.value("View/GridColor").value<QColor>());
  ui.plotBackgroundColor->setPlainColor(settings.value("Plot/BackgroundColor").value<QColor>());
  ui.checkBoxPlaybackControlFullScreen->setChecked(
      settings.value("ShowPlaybackControlFullScreen", false).toBool());
  ui.checkBoxShowFilePathSplitMode->setChecked(
      settings.value("ShowFilePathInSplitMode", true).toBool());
  ui.checkBoxPixelValuesHex->setChecked(settings.value("ShowPixelValuesHex", false).toBool());
  // Updates settings
  settings.beginGroup("updates");
  const auto checkForUpdates = settings.value("checkForUpdates", true).toBool();
  ui.groupBoxUpdates->setChecked(checkForUpdates);
  if (UPDATE_FEATURE_ENABLE)
  {
    const auto updateBehavior = settings.value("updateBehavior", "ask").toString();
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
    ui.spinBoxNrThreads->setValue(
        settings.value("NrThreads", functions::getOptimalThreadCount()).toInt());
  else
    ui.spinBoxNrThreads->setValue(functions::getOptimalThreadCount());
  ui.spinBoxNrThreads->setEnabled(ui.checkBoxNrThreads->isChecked());
  // Playback
  ui.checkBoxPausPlaybackForCaching->setChecked(
      settings.value("PlaybackPauseCaching", true).toBool());
  const bool playbackCaching = settings.value("PlaybackCachingEnabled", false).toBool();
  ui.checkBoxEnablePlaybackCaching->setChecked(playbackCaching);
  ui.spinBoxThreadLimit->setValue(settings.value("PlaybackCachingThreadLimit", 1).toInt());
  ui.spinBoxThreadLimit->setEnabled(playbackCaching);
  settings.endGroup();

  // "Decoders" tab
  settings.beginGroup("Decoders");
  ui.lineEditDecoderPath->setText(settings.value("SearchPath", "").toString());

  for (const auto &decoder : decoder::DecodersHEVC)
    ui.comboBoxDefaultHEVC->addItem(
        QString::fromStdString(decoder::DecoderEngineMapper.getName(decoder)));
  for (const auto &decoder : decoder::DecodersVVC)
    ui.comboBoxDefaultVVC->addItem(
        QString::fromStdString(decoder::DecoderEngineMapper.getName(decoder)));
  for (const auto &decoder : decoder::DecodersAV1)
    ui.comboBoxDefaultAV1->addItem(
        QString::fromStdString(decoder::DecoderEngineMapper.getName(decoder)));

  ui.comboBoxDefaultHEVC->setCurrentText(settings.value("DefaultDecoderHEVC", 0).toString());
  ui.comboBoxDefaultVVC->setCurrentText(settings.value("DefaultDecoderVVC", 0).toString());
  ui.comboBoxDefaultAV1->setCurrentText(settings.value("DefaultDecoderAV1", 0).toString());

  ui.lineEditLibde265File->setText(settings.value("libde265File", "").toString());
  ui.lineEditLibHMFile->setText(settings.value("libHMFile", "").toString());
  ui.lineEditLibDav1d->setText(settings.value("libDav1dFile", "").toString());
  ui.lineEditLibVTMFile->setText(settings.value("libVTMFile", "").toString());
  ui.lineEditLibVVDecFile->setText(settings.value("libVVDecFile", "").toString());
  ui.lineEditFFmpegPath->setText(settings.value("FFmpeg.path", "").toString());
  settings.endGroup();
}

void SettingsDialog::initializeDefaults()
{
  // All default values are defined here and will be initialized at every startup.
  QSettings settings;
  if (!settings.contains("View/BackgroundColor"))
    settings.setValue("View/BackgroundColor", QColor(128, 128, 128));
  if (!settings.contains("View/GridColor"))
    settings.setValue("View/GridColor", QColor(0, 0, 0));
  if (!settings.contains("Plot/BackgroundColor"))
    settings.setValue("Plot/BackgroundColor", QColor(255, 255, 255));
}

unsigned int SettingsDialog::getCacheSizeInMB() const
{
  if (!ui.groupBoxCaching->isChecked())
    return 0;
  return std::max(functions::systemMemorySizeInMB() * (ui.sliderThreshold->value() + 1) / 100,
                  MIN_CACHE_SIZE_IN_MB);
}

void SettingsDialog::on_checkBoxNrThreads_stateChanged(int newState)
{
  ui.spinBoxNrThreads->setEnabled(newState);
  if (newState == Qt::Unchecked)
    ui.spinBoxNrThreads->setValue(functions::getOptimalThreadCount());
}

void SettingsDialog::on_checkBoxEnablePlaybackCaching_stateChanged(int state)
{
  // Enable/disable the spinBoxThreadLimit
  ui.spinBoxThreadLimit->setEnabled(state != Qt::Unchecked);
}

void SettingsDialog::on_pushButtonEditViewBackgroundColor_clicked()
{
  QColor currentColor = ui.viewBackgroundColor->getPlainColor();
  QColor newColor     = QColorDialog::getColor(
      currentColor, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentColor != newColor)
    ui.viewBackgroundColor->setPlainColor(newColor);
}

void SettingsDialog::on_pushButtonEditViewGridLineColor_clicked()
{
  QColor currentColor = ui.viewGridLineColor->getPlainColor();
  QColor newColor     = QColorDialog::getColor(
      currentColor, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentColor != newColor)
    ui.viewGridLineColor->setPlainColor(newColor);
}

void SettingsDialog::on_pushButtonEditPlotBackgroundColor_clicked()
{
  QColor currentColor = ui.plotBackgroundColor->getPlainColor();
  QColor newColor     = QColorDialog::getColor(
      currentColor, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentColor != newColor)
    ui.plotBackgroundColor->setPlainColor(newColor);
}

void SettingsDialog::on_pushButtonDecoderSelectPath_clicked()
{
  // Use the currently selected dir or the dir to YUView if this one does not exist.
  auto curDir = QDir(QSettings().value("SearchPath", "").toString());
  if (!curDir.exists())
    curDir = QDir::current();

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

void SettingsDialog::on_pushButtonLibde265SelectFile_clicked()
{
  const auto newFiles = getLibraryPath(
      this, ui.lineEditLibde265File->text(), "Please select the libde265 library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoder::decoderLibde265::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(
        this,
        "Error testing the library",
        "The selected file does not appear to be a usable libde265 library. Error: " + error);
  else
    ui.lineEditLibde265File->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonlibHMSelectFile_clicked()
{
  const auto newFiles = getLibraryPath(
      this, ui.lineEditLibHMFile->text(), "Please select the libHMDecoder library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoder::decoderHM::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(
        this,
        "Error testing the library",
        "The selected file does not appear to be a usable libHMDecoder library. Error: " + error);
  else
    ui.lineEditLibHMFile->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonLibDav1dSelectFile_clicked()
{
  const auto newFiles = getLibraryPath(
      this, ui.lineEditLibDav1d->text(), "Please select the libDav1d library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoder::decoderDav1d::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(
        this,
        "Error testing the library",
        "The selected file does not appear to be a usable libDav1d library. Error: " + error);
  else
    ui.lineEditLibDav1d->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonLibVTMSelectFile_clicked()
{
  const auto newFiles = getLibraryPath(
      this, ui.lineEditLibVTMFile->text(), "Please select the libVTMDecoder library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoder::decoderVTM::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(
        this,
        "Error testing the library",
        "The selected file does not appear to be a usable libVTMDecoder library. Error: " + error);
  else
    ui.lineEditLibVTMFile->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonLibVVDecSelectFile_clicked()
{
  const auto newFiles = getLibraryPath(this,
                                       ui.lineEditLibVVDecFile->text(),
                                       "Please select the libVVDec decoder library file to use.");
  if (newFiles.count() != 1)
    return;
  QString error;
  if (!decoder::decoderVVDec::checkLibraryFile(newFiles[0], error))
    QMessageBox::critical(
        this,
        "Error testing the library",
        "The selected file does not appear to be a usable libVVDec decoder library. Error: " +
            error);
  else
    ui.lineEditLibVVDecFile->setText(newFiles[0]);
}

void SettingsDialog::on_pushButtonFFMpegSelectPath_clicked()
{
  auto curDir = QDir(ui.lineEditFFmpegPath->text());
  if (!curDir.exists())
    curDir = QDir::current();

  QFileDialog fileDialog(this,
                         tr("Please select the path to the shared ffmpeg libraries "
                            "(avformat, avcodec, avutil and swscale)"));
  fileDialog.setDirectory(curDir);
  fileDialog.setFileMode(QFileDialog::Directory);

  if (!fileDialog.exec())
    return;

  auto newDirectory = fileDialog.selectedFiles();

  if (newDirectory.empty())
    return;

  const auto path = std::filesystem::path(newDirectory.first().toStdString());

  const auto result = FFmpeg::FFmpegVersionHandler::checkPathForUsableFFmpeg(path);
  if (result)
  {
    ui.lineEditFFmpegPath->setText(newDirectory.first());
  }
  else
  {
    QMessageBox::information(this,
                             "Error opening ffmpeg in directory",
                             "We could not open the ffmpeg libraries in the given folder.");
    // QMessageBox::StandardButton b = QMessageBox::question(
    //     this,
    //     "Error opening the library",
    //     "The selected file does not appear to be a usable ffmpeg avFormat library. \nWe have "
    //     "collected a more detailed log. Do you want to save it to disk?");
    // if (b == QMessageBox::Yes)
    // {
    //   const auto filePath =
    //       QFileDialog::getSaveFileName(this, "Select a destination for the log file.");
    //   QFile logFile(filePath);
    //   logFile.open(QIODevice::WriteOnly);
    //   if (logFile.isOpen())
    //   {
    //     QTextStream outputStream(&logFile);
    //     for (auto l : logList)
    //       outputStream << l << "\n";
    //   }
    //   else
    //     QMessageBox::information(
    //         this, "Error opening file", "There was an error opening the log file " + filePath);
    // }
  }
}

void SettingsDialog::on_pushButtonSave_clicked()
{
  // --- Save the settings ---
  QSettings settings;

  // "General" tab
  settings.setValue("WatchFiles", ui.checkBoxWatchFiles->isChecked());
  settings.setValue("AskToSaveOnExit", ui.checkBoxAskToSave->isChecked());
  settings.setValue("ContinuePlaybackOnSequenceSelection",
                    ui.checkBoxContinuePlaybackNewSelection->isChecked());
  settings.setValue("SavePositionAndZoomPerItem", ui.checkBoxSavePositionPerItem->isChecked());
  // UI
  settings.setValue("Theme", ui.comboBoxTheme->currentText());
  settings.setValue("SplitViewLineStyle", ui.comboBoxSplitLineStyle->currentText());
  settings.setValue("MouseMode", ui.comboBoxMouseMode->currentText());
  settings.setValue("View/BackgroundColor", ui.viewBackgroundColor->getPlainColor());
  settings.setValue("View/GridColor", ui.viewGridLineColor->getPlainColor());
  settings.setValue("Plot/BackgroundColor", ui.plotBackgroundColor->getPlainColor());
  settings.setValue("ShowPlaybackControlFullScreen",
                    ui.checkBoxPlaybackControlFullScreen->isChecked());
  settings.setValue("ShowFilePathInSplitMode", ui.checkBoxShowFilePathSplitMode->isChecked());
  settings.setValue("ShowPixelValuesHex", ui.checkBoxPixelValuesHex->isChecked());
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
  settings.setValue("DefaultDecoderHEVC", ui.comboBoxDefaultHEVC->currentText());
  settings.setValue("DefaultDecoderVVC", ui.comboBoxDefaultVVC->currentText());
  settings.setValue("DefaultDecoderAV1", ui.comboBoxDefaultAV1->currentText());
  // Raw coded video files
  settings.setValue("libde265File", ui.lineEditLibde265File->text());
  settings.setValue("libHMFile", ui.lineEditLibHMFile->text());
  settings.setValue("libDav1dFile", ui.lineEditLibDav1d->text());
  settings.setValue("libVTMFile", ui.lineEditLibVTMFile->text());
  settings.setValue("libVVDecFile", ui.lineEditLibVVDecFile->text());
  settings.setValue("FFmpeg.path", ui.lineEditFFmpegPath->text());
  settings.endGroup();

  accept();
}

void SettingsDialog::on_sliderThreshold_valueChanged(int value)
{
  ui.labelThreshold->setText(
      QString("Threshold (%1 MB)").arg(functions::systemMemorySizeInMB() * (value + 1) / 100));
}
