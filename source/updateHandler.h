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

#ifndef UPDATEHANDLER_H
#define UPDATEHANDLER_H

#include <QNetworkAccessManager>
#include <QPointer>
#include "typedef.h"
#include "ui_updateDialog.h"

class QNetworkReply;
class QProgressDialog;

// Ask the user if he wants to update to the new version and how to handle updates in the future.
class UpdateDialog : public QDialog
{
  Q_OBJECT

public:
  explicit UpdateDialog(QWidget *parent = 0);

private slots:
  void on_updateButton_clicked();

private:
  Ui::UpdateDialog ui;
};

/* The update handler does what it's name suggestes. It handles updates for YUView.
 * Updates are enabled if UPDATE_FEATURE_ENABLE is set to 1. In order for automatic
 * updates to work, different compilations of YUView must not be mixed. Therefor, 
 * the UPDATE_FEATURE_ENABLE flag is set by out buildbot before compilation. The resulting
 * binary files are then put on github so that the updater can donwload them from there.
 * 
 * The first step is to establish a list of files that we need to download/update. For this,
 * we download the file 'versioninfo.txt' from github. We then compare that to the local 
 * 'versioninfo.txt' file to get a list of files that need to be downloaded.
 *
 * On windows, we need administrative rights to write to the 'Program Files' folder or even
 * to rename files. So first, we restart YUView with elevated rights and the command line
 * argument 'updateElevated'. This argument will let YUView know, to immediately perform the
 * update without asking the user again. 
 *
 * The update process itself then works like this: We remove the files that need updating
 * and download the new versions. If a file can not be removed, we rename it to "Something_old.ext".
 * If the _old file already exists, it is left from a previous update and we should be able
 * to delete it now. When everything is done, we restart YUView one final time to start the 
 * now updated version of YUView.
*/
class updateHandler : public QObject
{
  Q_OBJECT

public:
  // Construct a new update handler. The mainWindows pointer is used if a dialog is shown.
  updateHandler(QWidget *mainWindow, bool useAlternativeSources);

public slots:
  // Send the request to check for a new version of YUView
  void startCheckForNewVersion(bool userRequest=true, bool force=false);

  // The windows process should have elevated rights now and we can do the update
  void forceUpdateElevated();

private slots:
  void replyFinished(QNetworkReply *reply);
  void downloadFinished(QNetworkReply *reply);
  void updateDownloadProgress(int64_t val, int64_t max);
  void sslErrors(QNetworkReply * reply, const QList<QSslError> & errors);

private:
  void downloadAndInstallUpdate();
  void restartYUView(bool elevated);

  // Abort the update (reset updaterStatus to idle and show a QMessageBox::critical with the given message)
  void abortUpdate(QString errorMsg);

  QPointer<QWidget> mainWidget;
  QNetworkAccessManager networkManager;

  QPointer<QProgressDialog> downloadProgress; 

  enum updaterStatusEnum
  {
    updaterIdle,                // The updater is idle. We can start checking for an update.
    updaterEstablishConnection, // The updater is trying to establish a secure connection
    updaterChecking,            // The updater is currently checking for an update. Don't start another check.
    updaterDownloading          // The updater is currently donwloading/installing updates. Do not start another check for updates.
  };
  updaterStatusEnum updaterStatus { updaterIdle };

  bool userCheckRequest      { false }; //< The request has been issued by the user.
  bool elevatedRights        { false }; // On windows this can indicate if the process should have elevated rights
  bool forceUpdate           { false }; // If an update is availabe and this is set, we will just install the update no matter what
  bool useAlternativeSources { false }; // Use the alternative (test) source to get the update files

  // The list or remote files we are downloading. For each file, we keep the path and name and it's size in bytes.
  QList<QPair<QString, int>> downloadFiles;
  
  // Initiate the download of the next file.
  void downloadNextFile();
  // The full name (including subdirs) and size of the file being downloaded currently
  QPair<QString, int> currentDownloadFile;

  // When downloading files is started, these contains the size (in bytes) of all files to be downloaded and the
  // current amount of bytes that were already downloaded.
  int totalDownloadSize;
  int currentDownloadProgress;

  QString updatePath;
};

#endif // UPDATEHANDLER_H
