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

#include "typedef.h"

updateHandler::updateHandler(QWidget *mainWindow)
{
  mainWidget = mainWindow;
  checkRunning = false;

  connect(&networkManager, SIGNAL(finished(QNetworkReply*)),this, SLOT(replyFinished(QNetworkReply*)));

  // Let's check for updates if checking is enabled
  QSettings settings;
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  settings.endGroup();
  if (checkForUpdates)
    startCheckForNewVersion(false);
}

void updateHandler::replyFinished(QNetworkReply *reply)
{
  bool error = (reply->error() == QNetworkReply::NoError);

#if UPDATE_FEATURE_ENABLE && _WIN32
  // We requested the update.txt file. See what is contains.
  if (error)
  {
    QString strReply = (QString)reply->readAll();
  }
#else
#if VERSION_CHECK
  // We can check the github master branch to see if there is a new version
  // However, we cannot automatically update
  if (error)
  {
    QString strReply = (QString)reply->readAll();
    //parse json
    QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());
    QJsonArray jsonArray = jsonResponse.array();
    QJsonObject jsonObject = jsonArray[0].toObject();
    QString currentHash = jsonObject["sha"].toString();
    QString buildHash = QString::fromUtf8(YUVIEW_HASH);
    if (QString::compare(currentHash, buildHash))
    {
      QMessageBox msgBox;
      msgBox.setTextFormat(Qt::RichText);
      msgBox.setInformativeText("Unfortunately your version of YUView does not support automatic updating. If you compiled YUView yourself, use GIT to pull the changes and rebuild YUView. Precompiled versions of YUView are also available on Github in the releases section: <a href='https://github.com/IENT/YUView/releases'>https://github.com/IENT/YUView/releases</a>");
      msgBox.setText("A newer YUView version than the one you are currently using is available on Github.");
      msgBox.exec();

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
  checkRunning = false;
}

// Start the asynchronous checking for an update.
void updateHandler::startCheckForNewVersion(bool userRequest)
{
  if (checkRunning)
    // Check is already running
    return;

  checkRunning = true;
  userCheckRequest = userRequest;

#if UPDATE_FEATURE_ENABLE && _WIN32
  // We are on windows and the update feature is available.
  // Check the IENT websize if there is a new version of the YUView executable available.
  networkManager.get(QNetworkRequest(QUrl("http://www.ient.rwth-aachen.de/~blaeser/YUView/updateWindws/version.txt")));
#else
#if VERSION_CHECK
  networkManager.get(QNetworkRequest(QUrl("https://api.github.com/repos/IENT/YUView/commits")));  
#else
  // We cannot check for a new version
  checkRunning = false;
#endif
#endif
}

void updateHandler::downloadInstallUpdate()
{
#if UPDATE_FEATURE_ENABLE

#endif
}

UpdateDialog::UpdateDialog(QWidget *parent) : 
  QDialog(parent),
  ui(new Ui::UpdateDialog)
{
  // Load the update settings from the QSettings
  QSettings settings;
  settings.beginGroup("updates");
  bool checkForUpdates = settings.value("checkForUpdates", true).toBool();
  QString updateBehavior = settings.value("updateBehavior", "ask").toString();
  settings.endGroup();

  ui->checkUpdatesGroupBox->setChecked(checkForUpdates);
  if (updateBehavior == "ask")
    ui->updateSettingComboBox->setCurrentIndex(1);
  else if (updateBehavior == "auto")
    ui->updateSettingComboBox->setCurrentIndex(0);

#if !UPDATE_FEATURE_ENABLE
  // If the update feature is not available, we will grey this out.
  ui->updateSettingComboBox->setEnabled(false);
#endif
}

void UpdateDialog::onButtonUpdateClicked()
{
  // The user wants to download/install the update.
  
  // First save the settings
  QSettings settings;
  settings.beginGroup("updates");
  settings.setValue("checkForUpdates", ui->checkUpdatesGroupBox->isChecked());
  QString updateBehavior = "ask";
  if (ui->updateSettingComboBox->currentIndex() == 0)
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


