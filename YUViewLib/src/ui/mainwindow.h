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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDesktopServices>
#include <QMainWindow>
#include <QSettings>

#include "ui/separateWindow.h"
#include "handler/updateHandler.h"
#include "video/videoCache.h"

#include "ui_mainwindow.h"

class QAction;
class playlistItem;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(bool useAlternativeSources, QWidget *parent = 0);

  void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
  
private:
  Ui::MainWindow ui;
  
public:
  // Check for a new update (if we do this automatically)
  void autoUpdateCheck() { updater->startCheckForNewVersion(false, false); }
  // The application was restarted with elevated rights. Force an update now.
  // This is a NO-OP on platforms other than windows.
  void forceUpdateElevated() { updater->forceUpdateElevated(); }

  // Get the main window from the QApplication
  static QWidget *getMainWindow();
  
public slots:
  void loadFiles(const QStringList &files);

  void toggleFullscreen();
  void deleteSelectedItems();
  void showAbout() { showAboutHelp(true); }
  void showHelp() { showAboutHelp(false); }
  void showSettingsWindow();
  void saveScreenshot();
  void showFileOpenDialog();
  void resetWindowLayout();
  void closeAndClearSettings();

  // A new item was selected. Update the window title.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2);

protected:

  virtual bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
  virtual void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

  // Check if one of the loaded files has changed (if activated in the settings)
  virtual void focusInEvent(QFocusEvent *event) Q_DECL_OVERRIDE;

private slots:
  
  void openRecentFile();

  // Slot: Handle the key press event.
  bool handleKeyPressFromSeparateView(QKeyEvent *event) { return handleKeyPress(event); }
  bool handleKeyPress(QKeyEvent *event, bool keyFromSeparateView=true);

  // Some slots for the actions.
  void openProjectWebsite()  { QDesktopServices::openUrl(QUrl("https://github.com/IENT/YUView")); }
  void openLibde265Website() { QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/libde265/releases")); }
  void openHMWebsite()       { QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/libHM/releases")); }
  void openVTMWebsize()      { QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/VTM/releases")); }
  void openDav1dWebsite()    { QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/dav1d/releases")); }
  void checkForNewVersion()  { updater->startCheckForNewVersion(); }
  void performanceTest();

private:

  void createMenusAndActions();
  void updateRecentFileActions();

  void showAboutHelp(bool about);
  void updateSettings();

  QPointer<QAction> recentFileActions[MAX_RECENT_FILES];
  QScopedPointer<videoCache> cache;
  bool saveWindowsStateOnExit;
  QScopedPointer<updateHandler> updater;
  viewStateHandler stateHandler;
  SeparateWindow separateViewWindow;
  bool showNormalMaximized; // When going to full screen: Was this windows maximized?  
  bool panelsVisible[5] {false};  // Which panels are visible when going to full-screen mode?
};

#endif // MAINWINDOW_H
