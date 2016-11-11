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

#ifndef UPDATEHANDLER_H
#define UPDATEHANDLER_H

#include <QDialog>
#include <QProgressDialog>
#include <QWidget>
#include <QNetworkAccessManager>
#include "ui_updateDialog.h"

#include "typedef.h"

class updateHandler : public QObject
{
  Q_OBJECT

public:
  // Construct a new update handler. The mainWindows pointer is used if a dialog is shown.
  updateHandler(QWidget *mainWindow);

public slots:
  // Send the request to check for a new version of YUView
  void startCheckForNewVersion(bool userRequest=true, bool forceUpdate=false);

#if UPDATE_FEATURE_ENABLE && _WIN32
  // The windows process should have elevated rights now and we can do the update
  void forceUpdateElevated() { elevatedRights = true; startCheckForNewVersion(false, true); }
#endif

private slots:
  void replyFinished(QNetworkReply *reply);
  void downloadFinished(QNetworkReply *reply);
  void updateDownloadProgress(qint64 val, qint64 max);

private:
  void downloadAndInstallUpdate();

  bool userCheckRequest;  //< The request has been issued by the user.

  QWidget *mainWidget;
  QNetworkAccessManager networkManager;

  QProgressDialog *downloadProgress;

  enum updaterStatusEnum
  {
    updaterIdle,           // The updater is idle. We can start checking for an update.
    updaterChecking,       // The updater is currently checking for an update. Don't start another check.
    updaterCheckingForce,  // The updater is currently checking for an update. If there is an update it will be installed. Don't start another check.
    updaterDownloading     // The updater is currently donwloading/installing updates. Do not start another check for updates.
  };
  updaterStatusEnum updaterStatus;

  bool elevatedRights;     // On windows this can indicate if the process should have elevated rights
};

/// Ask the user if he wants to update to the new version and how to handle updates in the future.
class UpdateDialog : public QDialog
{
  Q_OBJECT

public:
  explicit UpdateDialog(QWidget *parent = 0);

private slots:
  void onButtonUpdateClicked();
  void onButtonCancelClicked();

private:
  Ui::UpdateDialog *ui;
};

#endif // UPDATEHANDLER_H
