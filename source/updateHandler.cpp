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

#include "updateHandler.h"
#include <QSettings>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCheckBox>
#include <QFile>
#include <QFileInfo>

#include "typedef.h"

#if _WIN32 && UPDATE_FEATURE_ENABLE
#include <windows.h>
#endif

updateHandler::updateHandler(QWidget *mainWindow) :
  mainWidget(mainWindow)
{
  updaterStatus = updaterIdle;
  elevatedRights = false;

  connect(&networkManager, SIGNAL(finished(QNetworkReply*)),this, SLOT(replyFinished(QNetworkReply*)));
}

// Start the asynchronous checking for an update.
void updateHandler::startCheckForNewVersion(bool userRequest, bool forceUpdate)
{
  QSettings settings;
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  settings.endGroup();
  if (!userRequest && !checkForUpdates && !forceUpdate)
    // The user did not request this, we are not automatocally checking for updates and it is not a forced check. Abort.
    return;

  if (updaterStatus != updaterIdle)
    // The updater is busy. Do not start another check for updates.
    return;

#if UPDATE_FEATURE_ENABLE && _WIN32
  // We are on windows and the update feature is available.
  // Check the IENT websize if there is a new version of the YUView executable available.
  updaterStatus = forceUpdate ? updaterCheckingForce : updaterChecking;
  userCheckRequest = userRequest;
  networkManager.get(QNetworkRequest(QUrl("http://www.ient.rwth-aachen.de/~blaeser/YUViewWinRelease/gitver.txt")));
#else
#if VERSION_CHECK
  updaterStatus = forceUpdate ? updaterCheckingForce : updaterChecking;
  userCheckRequest = userRequest;
  networkManager.get(QNetworkRequest(QUrl("https://api.github.com/repos/IENT/YUView/commits")));  
#else
  // We don't know the current version. We cannot check if there is a newer one.
  if (userRequest)
    QMessageBox::information(mainWidget, "Can not check for updates", "Unfortunately no version information has been compiled into this YUView version. Because of this we cannot check for updates.");
#endif
#endif
}

// There is an answer from the server.
void updateHandler::replyFinished(QNetworkReply *reply)
{
  bool error = (reply->error() != QNetworkReply::NoError);

#if UPDATE_FEATURE_ENABLE && _WIN32
  // We requested the update.txt file. See what is contains.
  if (!error)
  {
    QString serverHash = (QString)reply->readAll().simplified();
    QString buildHash = QString::fromUtf8(YUVIEW_HASH).simplified();
    if (serverHash != buildHash)
    {
      // There is a new YUView version available. Do we ask the user first or do we just install?
      QSettings settings;
      settings.beginGroup("updates");
      QString updateBehavior = settings.value("updateBehavior", "ask").toString();
      if (updateBehavior == "auto" || updaterStatus == updaterCheckingForce)
      {
        // Don't ask. Just update.
        downloadAndInstallUpdate();
      }
      else
      {
        // Ask the user if he wants to update.
        UpdateDialog update(mainWidget);
        if (update.exec() == QDialog::Accepted)
        {
          // The user pressed 'update'
          downloadAndInstallUpdate();
        }
      }
      
      reply->deleteLater();
      return;
    }
  }
#else
#if VERSION_CHECK
  // We can check the github master branch to see if there is a new version
  // However, we cannot automatically update
  if (!error)
  {
    QString strReply = (QString)reply->readAll();
    //parse json
    QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());
    QJsonArray jsonArray = jsonResponse.array();
    QJsonObject jsonObject = jsonArray[0].toObject();
    QString serverHash = jsonObject["sha"].toString();
    QString buildHash = QString::fromUtf8(YUVIEW_HASH);
    if (serverHash != buildHash)
    {
      QMessageBox msgBox;
      msgBox.setTextFormat(Qt::RichText);
      msgBox.setInformativeText("Unfortunately your version of YUView does not support automatic updating. If you compiled YUView yourself, use GIT to pull the changes and rebuild YUView. Precompiled versions of YUView are also available on Github in the releases section: <a href='https://github.com/IENT/YUView/releases'>https://github.com/IENT/YUView/releases</a>");
      msgBox.setText("A newer YUView version than the one you are currently using is available on Github.");
      msgBox.exec();

      updaterStatus = updaterIdle;
      reply->deleteLater();
      return;
    }
  }
#endif // VERSION_CHECK
#endif

  // If the check worked and it was determined that there is a new version available, the function
  // should already have returned.

  if (userCheckRequest)
  {
    // Inform the user about the outcome of the check because he requested the check.
    if (error)
    {
      QMessageBox::critical(mainWidget, "Error checking for updates", "An error occured while checking for updates. Are you connected to the internet?");
    }
    else
    {
      // The software is up to date but the user requested this check so tell him that no update is required.

      // Get if the user activated automatic checking for a new version
      QSettings settings;
      settings.beginGroup("updates");
      bool checkForUpdates = settings.value("checkForUpdates", true).toBool();

      if (checkForUpdates)
      {
        QMessageBox msgBox;
        msgBox.setText("Your YUView version is up to date.");
        msgBox.setInformativeText("YUView will check for updates every time you start the application.");
        msgBox.exec();
      }
      else
      {
        // Suggest to activate automatic update checking
        QMessageBox msgBox;
        msgBox.setText("Your YUView version is up to date.");
        msgBox.setInformativeText("Currently, automatic checking for updates is disabled. If you want to obtain the latest bugfixes and enhancements, we recomend to activate automatic update checks.");
        msgBox.setCheckBox(new QCheckBox("Check for updates"));
        msgBox.exec();

        // Get if the user now selected automatic update checks.
        if (msgBox.checkBox()->isChecked())
        {
          // Save the new setting
          settings.setValue("checkForUpdates", true);
        }
      }

      settings.endGroup();
    }
  }

  reply->deleteLater();
  updaterStatus = updaterIdle;
}

void updateHandler::downloadAndInstallUpdate()
{
#if UPDATE_FEATURE_ENABLE
  assert(updaterStatus == updaterChecking || updaterStatus == updaterCheckingForce);

#if _WIN32
  // We are updating on windows. Check if we will need elevated rights to perform the update.
  bool elevatedRightsNeeded = false;

  // First try to delete the old executable if it still exists.
  QString executable = QCoreApplication::applicationFilePath(); // The current running executable with path
  QString oldFilePath = QFileInfo(executable).absolutePath() + "/YUView_old.exe";
  QFile oldFile(oldFilePath);
  if (oldFile.exists())
  {
    if (!oldFile.remove())
    {
      // Removing failed. This probably has to do with the user rights in the Programs folder. 
      // By default the normal users (the program is by default started as a normal user) has the rights to rename and create
      // files but not to delete them. We don't want to spam the user with a lot of YUView_oldxxxx.exe files so let's ask
      // for admin rights to delete the old file.

      if (elevatedRights)
      {
        // This is the instance of the executable with elevated rights but we could not delete the file anyways. 
        // That is bad. Abort the update.
        QMessageBox::critical(mainWidget, "Update Error", QString("We were unable to delete the YUView_old.exe file although we should have elevated rights. Maybe you can try running YUView using 'run as administrator' or you could try to delete the YUView_old.exe file yourself. Error code %1.").arg(oldFile.error()));
        updaterStatus = updaterIdle;
        return;
      }

      elevatedRightsNeeded = true;
    }
  }

  // In the second test, we will try to rename the current executable. If this fails, we will also need elevated rights.
  if (!elevatedRightsNeeded)
  {
    // Rename the old file
    QFile current(executable);
    QString newFilePath = QFileInfo(executable).absolutePath() + "/YUView_test.exe";
    if (!current.rename(newFilePath))
    {
      // We can not rename the file. Elevated rights needed.
      elevatedRightsNeeded = true;
    }
    else
    {
      // We can rename the file. Good. Rename it back.
      QFile renamedFile(newFilePath);
      renamedFile.rename(executable);
    }
  }

  if (elevatedRightsNeeded)
  {
    LPCWSTR fullPathToExe = (const wchar_t*) executable.utf16();
    // This should trigger the UAC dialog to start the application with elevated rights.
    // The "updateElevated" parameter tells the new instance of YUView that it should have elevated rights now
    // and it should retry to update.
    HINSTANCE h = ShellExecute(NULL, L"runas", fullPathToExe, L"updateElevated", NULL, SW_SHOWNORMAL);
    INT_PTR retVal = (INT_PTR)h;
    if (retVal > 32)  // From MSDN: If the function succeeds, it returns a value greater than 32.
    {
      // The user allowed restarting YUView as admin. Quit this one. The other one will take over.
      QApplication::quit();
    }
    else
    {
      DWORD err = GetLastError();
      if (err == ERROR_CANCELLED)
      {
        // The user did not allow YUView to restart with higher rights.
        QMessageBox::critical(mainWidget, "Update Error", "YUView could not be started with admin rights. These are needed in order to update the application.");
        // Abort the update process.
        updaterStatus = updaterIdle;
        return;
      }
    }
  }
#endif

  updaterStatus = updaterDownloading;

  // Connect the network manager to our download functions
  disconnect(&networkManager, SIGNAL(finished(QNetworkReply*)), NULL, NULL);
  connect(&networkManager, SIGNAL(finished(QNetworkReply*)),this, SLOT(downloadFinished(QNetworkReply*)));

  // Create a progress dialog.
  // downloadProgress is a weak pointer since the dialog's lifetime is managed by the mainWidget.
  assert(downloadProgress.isNull());
  assert(! mainWidget.isNull()); // dialog would leak otherwise
  downloadProgress = new QProgressDialog("Downloading YUView...", "Cancel", 0, 100, mainWidget);

  QNetworkReply *reply = networkManager.get(QNetworkRequest(QUrl("http://www.ient.rwth-aachen.de/~blaeser/YUViewWinRelease/YUView.exe")));
  connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(updateDownloadProgress(qint64, qint64)));
#endif
}

void updateHandler::updateDownloadProgress(qint64 val, qint64 max)
{
  downloadProgress->setMaximum(max);
  downloadProgress->setValue(val);
}

void updateHandler::downloadFinished(QNetworkReply *reply)
{
#if UPDATE_FEATURE_ENABLE && _WIN32
  bool error = (reply->error() != QNetworkReply::NoError);
  if (error)
  {
    QMessageBox::critical(mainWidget, "Error downloading update.", "An error occured while downloading YUView.");
  }
  else
  {
    // Download successfull. Get the data.
    QByteArray data = reply->readAll();

    // TODO: Here we could check the MD5 sum. Or maybe more advanced/secure check a certificate of some sort.
    // ...

    // Install the update
    QString executable = QCoreApplication::applicationFilePath();

    // Rename the old file
    QFile current(executable);
    QString newFilePath = QFileInfo(executable).absolutePath() + "/YUView_old.exe";
    if (!current.rename(newFilePath))
    {
      QMessageBox::critical(mainWidget, "Error installing update.", QString("Could not rename the old executable. Error %1").arg(current.error()));
    }
    else
    {
      // Save the download as the new executable
      QFile file(executable);
      if (!file.open(QIODevice::WriteOnly))
      {
        QMessageBox::critical(mainWidget, "Error installing update.", "Could not open the new executable for writing.");
      }
      else
      {
        file.write(data);
        file.close();
        // Update successfull.
        QMessageBox::information(mainWidget, "Update successfull.", "The update was successfull. Please restart YUView in order to start the new version.");
      }
    }
  }

  // Disconnect/delete the update progress dialog
  delete downloadProgress;

  // reconnect the network signal to the reply function
  disconnect(&networkManager, SIGNAL(finished(QNetworkReply*)),NULL, NULL);
  connect(&networkManager, SIGNAL(finished(QNetworkReply*)),this, SLOT(replyFinished(QNetworkReply*)));

  updaterStatus = updaterIdle;
#else
  Q_UNUSED(reply);
#endif
}

UpdateDialog::UpdateDialog(QWidget *parent) : 
  QDialog(parent)
{
  ui.setupUi(this);

  // Load the update settings from the QSettings
  QSettings settings;
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  QString updateBehavior = settings.value("updateBehavior", "ask").toString();
  settings.endGroup();

  ui.checkUpdatesGroupBox->setChecked(checkForUpdates);
  if (updateBehavior == "ask")
    ui.updateSettingComboBox->setCurrentIndex(1);
  else if (updateBehavior == "auto")
    ui.updateSettingComboBox->setCurrentIndex(0);

  // Connect signals/slots
  connect(ui.updateButton, SIGNAL(clicked()), this, SLOT(onButtonUpdateClicked()));
  connect(ui.cancelButton, SIGNAL(clicked()), this, SLOT(onButtonCancelClicked()));

#if !UPDATE_FEATURE_ENABLE
  // If the update feature is not available, we will grey this out.
  ui.updateSettingComboBox->setEnabled(false);
#endif
}

void UpdateDialog::onButtonUpdateClicked()
{
  // The user wants to download/install the update.
  
  // First save the settings
  QSettings settings;
  settings.beginGroup("updates");
  settings.setValue("checkForUpdates", ui.checkUpdatesGroupBox->isChecked());
  QString updateBehavior = "ask";
  if (ui.updateSettingComboBox->currentIndex() == 0)
    updateBehavior = "auto";
  settings.setValue("updateBehavior", updateBehavior);

  // The update request was accepted by the user
  accept();
}

void UpdateDialog::onButtonCancelClicked()
{
  // Cancel. Do not save the current settings and do not update.
  reject();
}


