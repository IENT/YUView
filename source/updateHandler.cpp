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

#include "updateHandler.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define UPDATER_DEBUG_OUTPUT 0
#if UPDATER_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_UPDATE qDebug
#else
#define DEBUG_UPDATE(fmt,...) ((void)0)
#endif

// ------------------ updateFileHandler helper class -----------------
#define UPDATEFILEHANDLER_FILE_NAME "versioninfo.txt"

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
    if (lineSplit.count() == 3)
    {
      // Get the file name and the version (index [0] and [1])
      QString fileName = lineSplit[0];
      if (fileName.endsWith(","))
        // There is a comma at the end. Remove it.
        fileName.chop(1);
      QString version = lineSplit[1];
      if (version.endsWith(","))
        version.chop(1);
      
      if (checkExistence)
      {
        // Check if the file exists locally
        QFileInfo fInfo(fileName);
        if (fInfo.exists() && fInfo.isFile())
          updateFileList.append(QPair<QString,QString>(fileName, version));
        else
          // The file does not exist locally. That is strange since it is in the update info file.
          // Files that do not exist locally should always be downloaded so we don't put them into the list.
          DEBUG_UPDATE("updateHandler::loadUpdateFileList The local file %s could not be found.", lineSplit[0]);
      }
      else
        // Do not check if the file exists
        updateFileList.append(QPair<QString,QString>(fileName, version));
    }
  }
  // Call this on the remote file list with a reference to the local file list to get a list
  // of files that require an update (that need to be downloaded).
  QStringList getFilesToUpdate(updateFileHandler &localFiles)
  {
    QStringList updateList;
    for (auto remoteFile : updateFileList)
    {
      bool fileFound = false;
      bool updateNeeded = false;
      for(auto localFile : localFiles.updateFileList)
      {
        if (localFile.first.toLower() == remoteFile.first.toLower())
        {
          // File found. Do we need to update it?
          updateNeeded = localFile.second != remoteFile.second;
          fileFound = true;
          break;
        }
      }
      if (!fileFound || updateNeeded)
        updateList.append(remoteFile.first);
    }
    return updateList;
  }
private:
  // For every entry, there is the file path/name and a version string
  QList<QPair<QString,QString>> updateFileList;
  bool loaded;
};

// ------------------ updateHandler -----------------

updateHandler::updateHandler(QWidget *mainWindow) :
  mainWidget(mainWindow)
{
  updaterStatus = updaterIdle;
  elevatedRights = false;
  forceUpdate = false;
  userCheckRequest = false;

  // We always perform the update in the path that the current executable is located in
  // and not in the current working directory.
  QFileInfo info(QCoreApplication::applicationFilePath());
  updatePath = info.absolutePath() + "/";

  connect(&networkManager, &QNetworkAccessManager::finished, this, &updateHandler::replyFinished);
  connect(&networkManager, &QNetworkAccessManager::sslErrors, this, &updateHandler::sslErrors);
}

void updateHandler::sslErrors(QNetworkReply * reply, const QList<QSslError> & errors)
{
  Q_UNUSED(reply);
  Q_UNUSED(errors);
  QMessageBox::information(mainWidget, "SSL Connection error", "An error occured while trying to establish a secure coonection to the server raw.githubusercontent.com.");
#if UPDATER_DEBUG_OUTPUT && !NDEBUG
  DEBUG_UPDATE("updateHandler::sslErrors");
  for (auto s : errors)
  {
    QString errorString = s.errorString();
    qDebug() << s.errorString();

    auto cert = s.certificate();
    QStringList certText = cert.toText().split("\n");
    for (QString s : certText)
      qDebug() << s;

    auto altNames = cert.subjectAlternativeNames();
    QMultiMap<QSsl::AlternativeNameEntryType, QString>::iterator i = altNames.begin();
    while (i != altNames.end()) 
    {
      qDebug() << i.key() << " - " << i.value();
      ++i;
    }
  }
#endif
}

// Start the asynchronous checking for an update.
void updateHandler::startCheckForNewVersion(bool userRequest, bool force)
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

  if (UPDATE_FEATURE_ENABLE && is_Q_OS_WIN)
  {
    // We are on windows and the update feature is available.
    // Check the Github repository branch binariesAutoUpdate if there is a new version of the YUView executable available.
    // First we will try to establish a secure connection to raw.githubusercontent.com
    DEBUG_UPDATE("updateHandler::startCheckForNewVersion connectToHostEncrypted raw.githubusercontent.com");
    updaterStatus = updaterEstablishConnection;
    userCheckRequest = userRequest;
    forceUpdate = force;
    networkManager.connectToHostEncrypted("raw.githubusercontent.com");
  }
  else if (VERSION_CHECK)
  {
    // We can check the Github API for the commit hash. After that we can say if there is a new version available on Github.
    updaterStatus = updaterChecking;
    userCheckRequest = userRequest;
    forceUpdate = force;
    networkManager.get(QNetworkRequest(QUrl("https://api.github.com/repos/IENT/YUView/commits")));
  }
  else
  {
    // We don't know the current version. We cannot check if there is a newer one.
    if (userRequest)
      QMessageBox::information(mainWidget, "Can not check for updates", "Unfortunately no version information has been compiled into this YUView version. Because of this we cannot check for updates.");
  }
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
  
  if (UPDATE_FEATURE_ENABLE && is_Q_OS_WIN)
  {
    if (updaterStatus == updaterEstablishConnection && !error)
    {
      // The secure connection was successfully established. Now request the update.txt file
      DEBUG_UPDATE("updateHandler::replyFinished request version info file from https://raw.githubusercontent.com/IENT/YUView/binariesAutoUpdate/win/" UPDATEFILEHANDLER_FILE_NAME);
      networkManager.get(QNetworkRequest(QUrl("https://raw.githubusercontent.com/IENT/YUView/binariesAutoUpdate/win/" UPDATEFILEHANDLER_FILE_NAME)));
      updaterStatus = updaterChecking;
      return;
    }
    else if (updaterStatus == updaterChecking && !error)
    {
      // We recieved the version info file. See what it contains.
      QByteArray updateFileInfo = reply->readAll();
      updateFileHandler remoteFile(updateFileInfo);

      // Next, also load the corresponding local file
      updateFileHandler localFile(updatePath + UPDATEFILEHANDLER_FILE_NAME);

      // Now compare the two so that we can download all files that require an update.
      // A file will be updated if:
      // - The version number of the remote and local file differ
      // - If the remote file does not exist locally
      downloadFiles = remoteFile.getFilesToUpdate(localFile);

      if (!downloadFiles.empty())
      {
        // There is a new YUView version available. Do we ask the user first or do we just install?
        QSettings settings;
        settings.beginGroup("updates");
        QString updateBehavior = settings.value("updateBehavior", "ask").toString();
        if (updateBehavior == "auto" || forceUpdate)
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
      }
      return;
    }
  }
  else if (VERSION_CHECK && !error && updaterStatus == updaterChecking)
  {
    // We can check the github master branch to see if there is a new version
    // However, we cannot automatically update
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

  // If the check worked and it was determined that there is a new version available, the function
  // should already have returned.
  if (userCheckRequest)
  {
    // Inform the user about the outcome of the check because he requested the check.
    if (error)
    {
      QMessageBox::critical(mainWidget, "Error checking for updates", "An error occured while checking for updates. Are you connected to the internet?\n");
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
  if (!UPDATE_FEATURE_ENABLE)
    return;

  assert(updaterStatus == updaterChecking);

  // The next step is to download the update files.
  updaterStatus = updaterDownloading;

  // Create a progress dialog.
  // downloadProgress is a weak pointer since the dialog's lifetime is managed by the mainWidget.
  assert(downloadProgress.isNull());
  assert(!mainWidget.isNull()); // dialog would leak otherwise
  downloadProgress = new QProgressDialog("Downloading YUView Update...", "Cancel", 0, 100, mainWidget);
  
  // Start downloading
  downloadNextFile();
}

void updateHandler::updateDownloadProgress(qint64 val, qint64 max)
{
  downloadProgress->setMaximum(max);
  downloadProgress->setValue(val);
}

void updateHandler::downloadNextFile()
{
  if (downloadFiles.isEmpty())
    return;

  currentDownloadFile = downloadFiles.takeFirst();
  // If the file includes a path, it could contain '\' characters.
  // Replace them by forward slashes
  for (int i = 0; i < currentDownloadFile.length(); i++)
  {
    if (currentDownloadFile[i] == '\\')
      currentDownloadFile[i] = '/';
  }

  DEBUG_UPDATE("updateHandler::downloadNextFile %s", currentDownloadFile);
  QString fullURL = "https://raw.githubusercontent.com/IENT/YUView/binariesAutoUpdate/" + currentDownloadFile;
  QNetworkReply *reply = networkManager.get(QNetworkRequest(QUrl(fullURL)));
  connect(reply, &QNetworkReply::downloadProgress, this, &updateHandler::updateDownloadProgress);
}

void updateHandler::downloadFinished(QNetworkReply *reply)
{
  if (!UPDATE_FEATURE_ENABLE)
    return;

  bool error = (reply->error() != QNetworkReply::NoError);
  bool downloadEncrypted = reply->attribute(QNetworkRequest::ConnectionEncryptedAttribute).toBool();
  DEBUG_UPDATE("updateHandler::downloadFinished %s %s %d", error ? "error" : "", downloadEncrypted ? "encrypted" : "not encrypted", reply->error());
  if (error)
  {
    QMessageBox::critical(mainWidget, "Error downloading update.", "An error occured while downloading YUView.");
    updaterStatus = updaterIdle;
    return;
  }
  else if (!downloadEncrypted)
  {
    QMessageBox::critical(mainWidget, "Error downloading update.", "YUView could not be downloaded using a secure connection. Aborting.");
    updaterStatus = updaterIdle;
    return;
  }
  else
  {
    // A file was downloaded successfully. Get the data.
    QByteArray data = reply->readAll();

    // Here, some check could go that checks the MD5 sum.
    // However, we got the file from a secure connection from our github server. So I don't know if 
    // this check would really add any more security.

    // Save the file locally
    QString fullPath = updatePath + currentDownloadFile;
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly))
      QMessageBox::critical(mainWidget, "Error installing update.", QString("Could not open the file %1 locally for writing.").arg(currentDownloadFile));
    else
    {
      file.write(data);
      file.close();

      if (downloadFiles.isEmpty())
      {
        // No more files to download. Update successfull.
        QMessageBox::information(mainWidget, "Download successfull.", "Download was successfull. We will now start the updater and restart YUView.");
        
        // Disconnect/delete the update progress dialog.
        delete downloadProgress;
        updaterStatus = updaterIdle;

        // TODO: Start the updater and quit YUView ...
      }
      else
      {
        // Start download of the next file.
        downloadNextFile();
      }
    }
  }
}

void updateHandler::forceUpdateElevated()
{
  if (UPDATE_FEATURE_ENABLE && is_Q_OS_WIN)
  {
    elevatedRights = true;
    startCheckForNewVersion(false, true);
  }
}

// ------------------ UpdateDialog -----------------

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

  connect(ui.cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  if (!UPDATE_FEATURE_ENABLE)
    // If the update feature is not available, we will grey this out.
    ui.updateSettingComboBox->setEnabled(false);
}

void UpdateDialog::on_updateButton_clicked()
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