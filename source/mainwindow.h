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
#include <QListWidget>
#include <QList>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QString>
#include <QMessageBox>
#include <QSettings>
#include <QTreeWidget>
#include <QMouseEvent>
#include <QTreeWidgetItem>
#include <QDesktopWidget>
#include <QKeyEvent>

#include "settingswindow.h"
#include "playlistTreeWidget.h"
#include "playlistItem.h"
#include "videoCache.h"
#include "separateWindow.h"
#include "ui_mainwindow.h"

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  bool eventFilter(QObject * target, QEvent * event);
  void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
  
private:
  PlaylistTreeWidget *p_playlistWidget;
  Ui::MainWindow *ui;

  QMessageBox *p_msg;
  bool p_ClearFrame;

  QMenu* fileMenu;
  QMenu* viewMenu;
  QMenu* playbackMenu;
  QMenu* helpMenu;

public:
  void loadFiles(QStringList files) { p_playlistWidget->loadFiles( files ); }
  void loadPlaylistFile(QString filePath);

  bool isPlaylistItemSelected() { return selectedPrimaryPlaylistItem() != NULL; }

public slots:
  
  //! Toggle fullscreen playback
  void toggleFullscreen();

  //! Deletes a group from playlist
  void deleteItem();
    
  void updateSelectedItems();
      
  void showAbout();

  void openProjectWebsite();

  void saveScreenshot();

  void updateSettings();
  
  void handleKeyPress(QKeyEvent* key);

  void checkNewVersion();

  // Show the open file dialog
  void showFileOpenDialog();

  void resetWindowLayout();

private slots:
  //! Timeout function for playback timer
  //void newFrameTimeout();

  void openRecentFile();
  
protected:

  virtual void keyPressEvent( QKeyEvent * event );

private:

  /// Return the primary and secondary playlist item that is currently selected
  playlistItem* selectedPrimaryPlaylistItem();
  playlistItem* selectedSecondaryPlaylistItem();

  /// Get the width/height for the current frameSize selection (in frameSizeComboBox)
  void convertFrameSizeComboBoxIndexToSize(int *width, int*height);
  
  SettingsWindow p_settingswindow;

  void createMenusAndActions();
  void updateRecentFileActions();

  // This window is shown for seperate windows mode. The main central splitViewWidget goes in here in this case.
  SeparateWindow separateViewWindow;

  // The video cache and the thread in which it is running
  videoCache *cache;

  QAction* openYUVFileAction;
  QAction* savePlaylistAction;
  QAction* addTextAction;
  QAction* addDifferenceAction;
  QAction* addOverlayAction;
  QAction* saveScreenshotAction;
  QAction* showSettingsAction;
  QAction* deleteItemAction;

  QAction* zoomToStandardAction;
  QAction* zoomToFitAction;
  QAction* zoomInAction;
  QAction* zoomOutAction;

  QAction* togglePlaylistAction;
  QAction* toggleStatisticsAction;
  QAction* toggleDisplayOptionsAction;
  QAction* toggleFileInfoAction;
  QAction* togglePropertiesAction;
  QAction* toggleControlsAction;
  QAction* toggleFullscreenAction;
  QAction* enableSingleWindowModeAction;
  QAction* enableSeparateWindowModeAction;

  QAction* playPauseAction;
  QAction* nextItemAction;
  QAction* previousItemAction;
  QAction* nextFrameAction;
  QAction* previousFrameAction;

  QAction *aboutAction;
  QAction *bugReportAction;
  QAction *featureRequestAction;
  QAction *checkNewVersionAction;
  QAction *resetWindowLayoutAction;

  QAction *recentFileActs[MAX_RECENT_FILES];

  QString strippedName(const QString &fullFileName);

  // If the window is shown full screen, this saves if it was maximized before going to full screen
  bool showNormalMaximized;
};

#endif // MAINWINDOW_H
