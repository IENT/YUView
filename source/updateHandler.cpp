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
#include <QDir>
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
#include <QTextStream>
#include <QThread>
#include <QDesktopServices>

#include "typedef.h"

#if _WIN32 && UPDATE_FEATURE_ENABLE
#include <windows.h>
#endif

#define UPDATER_DEBUG_OUTPUT 0
#if UPDATER_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_UPDATE qDebug
#else
#define DEBUG_UPDATE(fmt,...) ((void)0)
#endif

#define UPDATEFILEHANDLER_FILE_NAME "versioninfo.txt"
#define UPDATEFILEHANDLER_URL "http://raw.githubusercontent.com/IENT/YUViewReleases/master/win/autoupdate/"
typedef QPair<QString, int> downloadFile;

class updateFileHandler
{
public:
  updateFileHandler() : loaded(false) {}
  updateFileHandler(QString fileName) : loaded(false) { readFromFile(fileName); }
  updateFileHandler(QByteArray &byteArray) : loaded(false) { readRemoteFromData(byteArray); }
  // Parse the local file list and add all files that exist locally to the list of files
  // which potentially might require an update.
  void readFromFile(QString fileName)
  {
    DEBUG_UPDATE("updateHandler::loadUpdateFileList");

    // Open the file and get all files and their current version (int) from the file.
    QFileInfo updateFileInfo(fileName);
    if (!updateFileInfo.exists() || !updateFileInfo.isFile())
    {
      DEBUG_UPDATE("updateHandler::loadUpdateFileList local update file %s not found", fileName);
      return;
    }

    // Read all lines from the file
    QFile updateFile(fileName);
    if (updateFile.open(QIODevice::ReadOnly))
    {
      QTextStream in(&updateFile);
      while (!in.atEnd())
      {
        // Convert the line into a file/version pair (they should be separated by a space)
        QString line = in.readLine();
        parseOneLine(line, true);
      }
      updateFile.close();
    }
    loaded = true;
  }
  // Parse the remote file from the given QByteArray. Remote files will not be checked for
  // existence on the remote side. We assume that all files listed in the remote file list
  // also exist on the remote side and can be downloaded.
  void readRemoteFromData(QByteArray &arr)
  {
    QString reply = QString(arr);
    QStringList lines = reply.split("\n");
    for (QString line : lines)
      parseOneLine(line);
  }
  // Parse one line from the update file list (remote or local)
  void parseOneLine(QString &line, bool checkExistence=false)
  {
    QStringList lineSplit = line.split(" ");
    if (line.startsWith("Last Commit"))
    {
      if (line.startsWith("Last Commit: "))
        DEBUG_UPDATE("updateHandler::loadUpdateFileList Local file last commit: ", lineSplit[2]);
      return;
    }
    // Ignore all lines that start with %, / or #
    if (line.startsWith("%") || line.startsWith("/") || line.startsWith("#"))
      return;
    if (lineSplit.count() == 4)
    {
      fileListEntry entry(lineSplit);

      if (checkExistence)
      {
        // Check if the file exists locally
        QFileInfo fInfo(entry.filePath);
        if (fInfo.exists() && fInfo.isFile())
          updateFileList.append(entry);
        else
          // The file does not exist locally. That is strange since it is in the update info file.
          // Files that do not exist locally should always be downloaded so we don't put them into the list.
          DEBUG_UPDATE("updateHandler::loadUpdateFileList The local file %s could not be found.", lineSplit[0]);
      }
      else
        // Do not check if the file exists
        updateFileList.append(entry);
    }
  }
  // Just get all files
  QList<downloadFile> getAllFiles()
  {
    QList<downloadFile> updateList;
    for (auto remoteFile : updateFileList)
      updateList.append(downloadFile(remoteFile.filePath, remoteFile.fileSize));
    // No matter what, we will update the "versioninfo.txt" file (assume it to be 10kbyte)
    updateList.append(downloadFile(UPDATEFILEHANDLER_FILE_NAME, 10000));
    return updateList;
  }
private:
  // For every entry in the "versioninfo.txt" file, these entries present:
  class fileListEntry
  {
  public:
    fileListEntry(QStringList lineSplit)
    {
      filePath = lineSplit[0];
      if (filePath.endsWith(","))
        // There is a comma at the end. Remove it.
        filePath.chop(1);
      version = lineSplit[1];
      if (version.endsWith(","))
        version.chop(1);
      hash = lineSplit[2];
      if (hash.endsWith(","))
        hash.chop(1);
      QString sizeString = lineSplit[3];
      if (sizeString.endsWith(","))
        sizeString.chop(1);
      fileSize = sizeString.toInt();
    }
    QString filePath; //< The file name and path
    QString version;  //< The version of the file
    QString hash;     //< A hash of the file (unused so far)
    int fileSize;     //< The size of the file in bytes
  };

  // For every entry, there is the file path/name and a version string
  QList<fileListEntry> updateFileList;
  bool loaded;
};

// ------------------ updateHandler -----------------

updateHandler::updateHandler(QWidget *mainWindow) :
  mainWidget(mainWindow)
{
  updaterStatus = updaterIdle;
  elevatedRights = false;

  // We always perform the update in the path that the current executable is located in
  // and not in the current working directory.
  QFileInfo info(QCoreApplication::applicationFilePath());
  updatePath = info.absolutePath() + "/";

  connect(&networkManager, SIGNAL(finished(QNetworkReply*)),this, SLOT(replyFinished(QNetworkReply*)));
}

// Start the asynchronous checking for an update.
bool updateHandler::startCheckForNewVersion(bool userRequest, bool forceUpdate)
{
  QSettings settings;
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  settings.endGroup();
  if (!userRequest && !checkForUpdates && !forceUpdate)
    // The user did not request this, we are not automatocally checking for updates and it is not a forced check. Abort.
    return false;

  if (updaterStatus != updaterIdle)
    // The updater is busy. Do not start another check for updates.
    return false;

#if UPDATE_FEATURE_ENABLE && _WIN32
  // We are on windows and the update feature is available.
  // Check the IENT websize if there is a new version of the YUView executable available.
  
  // We cannot update from the new GIThub repository because the old YUView version
  // does not support https and raw.github does not offer unencrypted access.
  if (QMessageBox::Yes == QMessageBox::question(mainWidget, "Update YUView to version 2.0", "There is an update to the new YUView 2.0. Unfortunately we can not update this version to the new one. You can obtain the new version from our github page. Do you want to go there now?"))
  {
    QMessageBox::information(mainWidget, "New version download.", "The download page to the new YUView version will now open. Please do not forget to close all instances of YUView before you install the new version.");
    QDesktopServices::openUrl(QUrl("https://github.com/IENT/YUViewReleases/blob/master/win/installers/SetupYUView.exe?raw=true"));
    QApplication::quit();
    return true;
  }
  return false;
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
  if (updaterStatus == updaterDownloading)
  {
    downloadFinished(reply);
    return;
  }

  bool error = (reply->error() != QNetworkReply::NoError);
  DEBUG_UPDATE("updateHandler::replyFinished %s %d", error ? "error" : "", reply->error());

#if UPDATE_FEATURE_ENABLE && _WIN32
  // We requested the versioninfo.txt file. See what is contains.
  if (!error)
  {
    // We recieved the version info file. See what it contains.
    QByteArray updateFileInfo = reply->readAll();
    updateFileHandler remoteFile(updateFileInfo);

    downloadFiles = remoteFile.getAllFiles();

    // No check needed. 
    // There is a new YUView version available. Do we ask the user first or do we just install?
    QSettings settings;
    settings.beginGroup("updates");
    QString updateBehavior = settings.value("updateBehavior", "ask").toString();
    if (updateBehavior == "auto" || updaterStatus == updaterCheckingForce)
      // Don't ask. Just update.
      downloadAndInstallUpdate();
    else
    {
      // Ask the user if he wants to update.
      UpdateDialog update(mainWidget);
      if (update.exec() == QDialog::Accepted)
        // The user pressed 'update'
        downloadAndInstallUpdate();
      else
        updaterStatus = updaterIdle;
    }

    reply->deleteLater();
    return;
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
#if !UPDATE_FEATURE_ENABLE && _WIN32
  return;
#endif

  assert(updaterStatus == updaterChecking || updaterStatus == updaterCheckingForce);

  // On windows: Before we perform the update, we restart YUView with elevated rights.
  // This is most likely necessary because YUView is normally installed in the 'Program Files'
  // directory where we have no write access from the normal user space.
  if (!elevatedRights)
  {
    restartYUView(true);
    return;
  }

  // The next step is to download the update files.
  updaterStatus = updaterDownloading;

  // Create a progress dialog.
  // downloadProgress is a weak pointer since the dialog's lifetime is managed by the mainWidget.
  assert(downloadProgress.isNull());
  assert(!mainWidget.isNull()); // dialog would leak otherwise
  downloadProgress = new QProgressDialog("Downloading YUView Update...", "Cancel", 0, 100, mainWidget);

  // Get the total download size
  totalDownloadSize = 0;
  currentDownloadProgress = 0;
  for (downloadFile f : downloadFiles)
    totalDownloadSize += f.second;
  downloadProgress->setMaximum(totalDownloadSize);

  // Start downloading
  downloadNextFile();
}

void updateHandler::restartYUView(bool elevated)
{
#if _WIN32 && UPDATE_FEATURE_ENABLE
  QString executable = QCoreApplication::applicationFilePath();
  LPCWSTR fullPathToExe = (const wchar_t*) executable.utf16();
  // This should trigger the UAC dialog to start the application with elevated rights.
  // The "updateElevated" parameter tells the new instance of YUView that it should have elevated rights now
  // and it should retry to update.
  HINSTANCE h;
  if (elevated)
    h = ShellExecute(nullptr, L"runas", fullPathToExe, L"updateElevated", nullptr, SW_SHOWNORMAL);
  else
    h = ShellExecute(nullptr, L"open", fullPathToExe, L"updateElevated", nullptr, SW_SHOWNORMAL);
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
      return abortUpdate("YUView could not be started with admin rights. These are needed in order to update the application.");
  }
#else
  Q_UNUSED(elevated);
#endif
}

void updateHandler::abortUpdate(QString errorMsg)
{
  QMessageBox::critical(mainWidget, "Update error", errorMsg);
  updaterStatus = updaterIdle;
}

void updateHandler::updateDownloadProgress(qint64 val, qint64 max)
{
  Q_UNUSED(max);
  downloadProgress->setValue(currentDownloadProgress + val);
}

void updateHandler::downloadNextFile()
{
  if (downloadFiles.isEmpty())
    return;

  currentDownloadFile = downloadFiles.takeFirst();
  // If the file includes a path, it could contain '\' characters.
  // Replace them by forward slashes
  for (int i = 0; i < currentDownloadFile.first.length(); i++)
  {
    if (currentDownloadFile.first[i] == '\\')
      currentDownloadFile.first[i] = '/';
  }

  DEBUG_UPDATE("updateHandler::downloadNextFile %s", currentDownloadFile);
  QString fullURL = UPDATEFILEHANDLER_URL + currentDownloadFile.first;
  QNetworkReply *reply = networkManager.get(QNetworkRequest(QUrl(fullURL)));
  connect(reply, &QNetworkReply::downloadProgress, this, &updateHandler::updateDownloadProgress);
}

void updateHandler::downloadFinished(QNetworkReply *reply)
{
  if (!UPDATE_FEATURE_ENABLE)
    return;

  bool error = (reply->error() != QNetworkReply::NoError);
  auto err = reply->error();
  DEBUG_UPDATE("updateHandler::downloadFinished %s %s %d", error ? "error" : "", downloadEncrypted ? "encrypted" : "not encrypted", reply->error());
  if (error)
    return abortUpdate(QString("An error occured while downloading YUView. Error code %1.").arg(err));
  else
  {
    // A file was downloaded successfully. Get the data.
    QByteArray data = reply->readAll();

    // Save the file locally
    QString fullPath = updatePath + currentDownloadFile.first;

    // First, check if the file exists. If it does, remove the local file first.
    // This should work, even for existing files because we have elevated rights.
    QFileInfo fileInfo(fullPath);
    if (fileInfo.exists())
    {
      QFile oldFile(fullPath);
      if (!oldFile.remove())
      {
        // Deleting the file failed. Let's just rename it to "something_old.ext"
        QString newName = fileInfo.baseName() + "_old." + fileInfo.completeSuffix();
        QString renamedFilePath = updatePath + newName;
        // First, check if this _old file already exists. If yes, delete it first.
        QFileInfo newFileInfo(renamedFilePath);
        if (newFileInfo.isFile() && newFileInfo.exists())
          if (!QFile(renamedFilePath).remove())
            return abortUpdate(QString("YUView was unable to remove the file %1.").arg(renamedFilePath));
        if (!oldFile.rename(newName))
          return abortUpdate(QString("YUView was unable to remove or rename the file %1.").arg(fileInfo.fileName()));
        DEBUG_UPDATE("updateHandler::downloadFinished The old file could not be deleted but was renamed to %s", newName);
      }
      else
        DEBUG_UPDATE("updateHandler::downloadFinished Successfully deleted old file %s", fileInfo.fileName());
    }

    // Second check: Is the file located in a subirectory that does not exist?
    // If so, create that subdirectory.
    if (currentDownloadFile.first.contains("/"))
    {
      int lastIdx = currentDownloadFile.first.lastIndexOf("/");
      QString fullDir = updatePath + currentDownloadFile.first.left(lastIdx);
      if (!QDir().mkpath(fullDir))
        return abortUpdate(QString("Could not create the subdirectory %1").arg(fullDir));
  }

    // The old file does not exist (anymore) and we can write the new file.
    QFile newFile(fullPath);
    if (!newFile.open(QIODevice::WriteOnly))
      return abortUpdate(QString("Could not open the file %1 locally for writing.").arg(currentDownloadFile.first));
    else
    {
      newFile.write(data);
      newFile.close();
      DEBUG_UPDATE("updateHandler::downloadFinished Written downloaded data to %s", currentDownloadFile.first);

      if (downloadFiles.isEmpty())
      {
        // No more files to download. Update successfull.
        QMessageBox::information(mainWidget, "Update successfull.", "Update was successfull. We will now start the new version of YUView.");

        // Disconnect/delete the update progress dialog.
        delete downloadProgress;
        updaterStatus = updaterIdle;

        // Start the new downloaded YUVeiw version.
        restartYUView(true);
      }
      else
      {
        // Update the download progress
        currentDownloadProgress += currentDownloadFile.second;
        downloadProgress->setValue(currentDownloadProgress);

        // Start download of the next file.
        downloadNextFile();
      }
    }
  }
}

#if _WIN32 && UPDATE_FEATURE_ENABLE
void updateHandler::forceUpdateElevated()
{
  //// Wait. Use this code to attach a debugger to the new YUView instance with elevated rights.
  bool wait = true;
  while(wait == true)
  {
    QThread::sleep(1);
  }

  elevatedRights = true;
  startCheckForNewVersion(false, true);
}
#endif

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

// ------------------ UpdateDialog -----------------

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


