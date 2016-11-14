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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAction>
#include <QKeyEvent>

#include "typedef.h"
#include "settingsDialog.h"
#include "playlistTreeWidget.h"
#include "playlistItem.h"
#include "videoCache.h"
#include "separateWindow.h"
#include "updateHandler.h"
#include "viewStateHandler.h"

#include "ui_mainwindow.h"

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
  
private:
  Ui::MainWindow ui;
  
public:
  void loadFiles(const QStringList &files) { ui.playlistTreeWidget->loadFiles( files ); }

  // Check for a new update (if we do this automatically)
  void autoUpdateCheck() { updater->startCheckForNewVersion(false); }
#if UPDATE_FEATURE_ENABLE && _WIN32
  // The application was restarted with elevated rights. Force an update now.
  void forceUpdateElevated() { updater->forceUpdateElevated(); }
#endif
  
public slots:
  void toggleFullscreen();
  void deleteItem();          //< Delete the selcted items
  void showAbout();
  void showSettingsWindow();
  void openProjectWebsite();
  void saveScreenshot();
  void showFileOpenDialog();
  void resetWindowLayout();

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
  bool handleKeyPress(QKeyEvent *event, bool keyFromSeparateView=true);

private:

  void createMenusAndActions();
  void updateRecentFileActions();
  
  // This window is shown for seperate windows mode. The main central splitViewWidget goes in here in this case.
  SeparateWindow separateViewWindow;

  // The video cache and the thread in which it is running
  videoCache *cache;

  // We save the recent file actions so that we can change them (their text) later
  QAction *recentFileActions[MAX_RECENT_FILES];
  
  // If the window is shown full screen, this saves if it was maximized before going to full screen
  bool showNormalMaximized;

  updateHandler *updater;

  viewStateHandler stateHandler;
};

#endif // MAINWINDOW_H
