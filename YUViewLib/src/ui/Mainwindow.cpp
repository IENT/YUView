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

#include "Mainwindow.h"

#include <QByteArray>
#include <QFileDialog>
#include <QImageWriter>
#include <QMessageBox>
#include <QShortcut>
#include <QStringList>
#include <QTextBrowser>
#include <QTextStream>

#include <common/Functions.h>
#include <common/FunctionsGui.h>
#include <playlistitem/playlistItems.h>
#include <ui/Mainwindow_performanceTestDialog.h>
#include <ui/SettingsDialog.h>
#include <ui/widgets/PlaylistTreeWidget.h>

MainWindow::MainWindow(bool useAlternativeSources, QWidget *parent) : QMainWindow(parent)
{
  Q_INIT_RESOURCE(images);
  Q_INIT_RESOURCE(docs);

  SettingsDialog::initializeDefaults();

  QSettings settings;
  qRegisterMetaType<indexRange>("indexRange");

  ui.setupUi(this);

  // Create the update handler
  updater.reset(new updateHandler(this, useAlternativeSources));

  setFocusPolicy(Qt::StrongFocus);

  statusBar()->hide();

  saveWindowsStateOnExit = true;
  for (int i = 0; i < 5; i++)
    panelsVisible[i] = false;

  // Initialize the separate window
  separateViewWindow.setWindowTitle("Separate View");
  separateViewWindow.setGeometry(0, 0, 300, 600);

  connect(ui.displaySplitView,
          &splitViewWidget::signalToggleFullScreen,
          this,
          &MainWindow::toggleFullscreen);

  // Setup primary/separate splitView
  ui.displaySplitView->addSlaveView(&separateViewWindow.splitView);
  connect(ui.displaySplitView,
          &splitViewWidget::signalShowSeparateWindow,
          &separateViewWindow,
          &QWidget::setVisible);

  // Connect the playlistWidget signals to some slots
  auto const fileInfoAdapter = [this] {
    auto items = ui.playlistTreeWidget->getSelectedItems();
    ui.fileInfoWidget->setInfo(items[0] ? items[0]->getInfo() : InfoData(),
                               items[1] ? items[1]->getInfo() : InfoData());
  };
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectionRangeChanged,
          ui.fileInfoWidget,
          fileInfoAdapter);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectedItemChanged,
          ui.fileInfoWidget,
          fileInfoAdapter);
  connect(ui.fileInfoWidget, &FileInfoWidget::infoButtonClicked, [this](int infoIndex, int row) {
    auto items = ui.playlistTreeWidget->getSelectedItems();
    if (items[infoIndex])
      items[infoIndex]->infoListButtonPressed(row);
  });
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectionRangeChanged,
          ui.playbackController,
          &PlaybackController::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectionRangeChanged,
          ui.propertiesWidget,
          &PropertiesWidget::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectionRangeChanged,
          ui.displaySplitView,
          &splitViewWidget::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectionRangeChanged,
          ui.bitstreamAnalysis,
          &BitstreamAnalysisWidget::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectionRangeChanged,
          this,
          &MainWindow::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectedItemChanged,
          ui.playbackController,
          &PlaybackController::selectionPropertiesChanged);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::itemAboutToBeDeleted,
          ui.propertiesWidget,
          &PropertiesWidget::itemAboutToBeDeleted);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::openFileDialog,
          this,
          &MainWindow::showFileOpenDialog);
  connect(ui.playlistTreeWidget,
          &PlaylistTreeWidget::selectedItemDoubleBufferLoad,
          ui.playbackController,
          &PlaybackController::currentSelectedItemsDoubleBufferLoad);

  ui.displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);

  // Create the VideoCache object
  cache.reset(new video::VideoCache(
      ui.playlistTreeWidget, ui.playbackController, ui.displaySplitView, this));
  connect(cache.data(),
          &video::VideoCache::updateCacheStatus,
          ui.cachingInfoWidget,
          &VideoCacheInfoWidget::onUpdateCacheStatus);

  createMenusAndActions();

  ui.playbackController->setSplitViews(ui.displaySplitView, &separateViewWindow.splitView);
  ui.playbackController->setPlaylist(ui.playlistTreeWidget);
  ui.displaySplitView->setPlaybackController(ui.playbackController);
  ui.displaySplitView->setPlaylistTreeWidget(ui.playlistTreeWidget);
  ui.displaySplitView->setVideoCache(this->cache.data());
  ui.cachingInfoWidget->setPlaylistAndCache(ui.playlistTreeWidget, this->cache.data());
  separateViewWindow.splitView.setPlaybackController(ui.playbackController);
  separateViewWindow.splitView.setPlaylistTreeWidget(ui.playlistTreeWidget);

  if (!settings.contains("mainWindow/geometry"))
    // There is no previously saved window layout. This is possibly the first time YUView is
    // started. Reset the window layout
    resetWindowLayout();
  else
  {
    // load geometry and active dockable widgets from user preferences
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/windowState").toByteArray());
    separateViewWindow.restoreGeometry(settings.value("separateViewWindow/geometry").toByteArray());
    separateViewWindow.restoreState(settings.value("separateViewWindow/windowState").toByteArray());
  }

  connect(ui.openButton, &QPushButton::clicked, this, &MainWindow::showFileOpenDialog);

  // Connect signals from the separate window
  connect(&separateViewWindow,
          &SeparateWindow::signalSingleWindowMode,
          ui.displaySplitView,
          &splitViewWidget::triggerActionSeparateView);
  connect(&separateViewWindow,
          &SeparateWindow::unhandledKeyPress,
          this,
          &MainWindow::handleKeyPressFromSeparateView);

  // Set the controls in the state handler. This way, the state handler can save/load the current
  // state of the view.
  stateHandler.setConctrols(ui.playbackController,
                            ui.playlistTreeWidget,
                            ui.displaySplitView,
                            &separateViewWindow.splitView);
  // Give the playlist a pointer to the state handler so it can save the states ti playlist
  ui.playlistTreeWidget->setViewStateHandler(&stateHandler);

  if (ui.playlistTreeWidget->isAutosaveAvailable())
  {
    QMessageBox::StandardButton resBtn =
        QMessageBox::question(this,
                              "Restore Playlist",
                              tr("It looks like YUView crashed the last time you used it. We are "
                                 "sorry about that. However, we have an autosave of the playlist "
                                 "you were working with. Do you want to restore this playlist?\n"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (resBtn == QMessageBox::Yes)
      ui.playlistTreeWidget->loadAutosavedPlaylist();
    else
      ui.playlistTreeWidget->dropAutosavedPlaylist();
  }
  // Start the timer now (and not in the constructor of rht playlistTreeWidget) so that the autosave
  // is not accidetly overwritten.
  ui.playlistTreeWidget->startAutosaveTimer();

  updateSettings();
}

QWidget *MainWindow::getMainWindow()
{
  QWidgetList l = QApplication::topLevelWidgets();
  for (QWidget *w : l)
  {
    MainWindow *mw = dynamic_cast<MainWindow *>(w);
    if (mw)
      return mw;
  }
  return nullptr;
}

void MainWindow::loadFiles(const QStringList &files)
{
  ui.playlistTreeWidget->loadFiles(files);
}

void MainWindow::createMenusAndActions()
{
  auto addActionToMenu = [this](QMenu *            menu,
                                QString            name,
                                auto               reciever,
                                auto               functionPointer,
                                const QKeySequence shortcut = {}) {
    auto action = new QAction(name, menu);
    action->setShortcut(shortcut);
    QObject::connect(action, &QAction::triggered, reciever, functionPointer);
    menu->addAction(action);
  };

  auto fileMenu = menuBar()->addMenu(tr("&File"));
  addActionToMenu(
      fileMenu, "&Open File...", this, &MainWindow::showFileOpenDialog, Qt::CTRL | Qt::Key_O);

  auto recentFileMenu = fileMenu->addMenu("Recent Files");
  for (int i = 0; i < MAX_RECENT_FILES; i++)
  {
    this->recentFileActions[i] = new QAction(this);
    connect(
        this->recentFileActions[i].data(), &QAction::triggered, this, &MainWindow::openRecentFile);
    recentFileMenu->addAction(this->recentFileActions[i]);
  }

  fileMenu->addSeparator();
  addActionToMenu(
      fileMenu, "&Add Text Frame", ui.playlistTreeWidget, &PlaylistTreeWidget::addTextItem);
  addActionToMenu(fileMenu,
                  "&Add Difference Sequence",
                  ui.playlistTreeWidget,
                  &PlaylistTreeWidget::addDifferenceItem);
  addActionToMenu(
      fileMenu, "&Add Overlay", ui.playlistTreeWidget, &PlaylistTreeWidget::addOverlayItem);
  fileMenu->addSeparator();
  addActionToMenu(fileMenu, "&Delete Item", this, &MainWindow::deleteSelectedItems, Qt::Key_Delete);
  fileMenu->addSeparator();
  addActionToMenu(fileMenu,
                  "&Save Playlist...",
                  ui.playlistTreeWidget,
                  &PlaylistTreeWidget::savePlaylistToFile,
                  Qt::CTRL | Qt::Key_S);
  fileMenu->addSeparator();
  addActionToMenu(fileMenu, "&Save Screenshot...", this, &MainWindow::saveScreenshot);
  fileMenu->addSeparator();
  addActionToMenu(fileMenu, "&Settings...", this, &MainWindow::showSettingsWindow);
  fileMenu->addSeparator();
  addActionToMenu(fileMenu, "Exit", this, &MainWindow::close);

  // On Mac, the key to delete an item is backspace. We will add this for all platforms
  auto backSpaceDelete = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
  QObject::connect(backSpaceDelete, &QShortcut::activated, this, &MainWindow::deleteSelectedItems);

  auto viewMenu = menuBar()->addMenu(tr("&View"));
  // Sub menu save/load state
  auto saveStateMenu = viewMenu->addMenu("Save View State");
  addActionToMenu(saveStateMenu,
                  "Slot 1",
                  &stateHandler,
                  &ViewStateHandler::saveViewState1,
                  Qt::CTRL | Qt::Key_1);
  addActionToMenu(saveStateMenu,
                  "Slot 2",
                  &stateHandler,
                  &ViewStateHandler::saveViewState2,
                  Qt::CTRL | Qt::Key_2);
  addActionToMenu(saveStateMenu,
                  "Slot 3",
                  &stateHandler,
                  &ViewStateHandler::saveViewState3,
                  Qt::CTRL | Qt::Key_3);
  addActionToMenu(saveStateMenu,
                  "Slot 4",
                  &stateHandler,
                  &ViewStateHandler::saveViewState4,
                  Qt::CTRL | Qt::Key_4);
  addActionToMenu(saveStateMenu,
                  "Slot 5",
                  &stateHandler,
                  &ViewStateHandler::saveViewState5,
                  Qt::CTRL | Qt::Key_5);
  addActionToMenu(saveStateMenu,
                  "Slot 6",
                  &stateHandler,
                  &ViewStateHandler::saveViewState6,
                  Qt::CTRL | Qt::Key_6);
  addActionToMenu(saveStateMenu,
                  "Slot 7",
                  &stateHandler,
                  &ViewStateHandler::saveViewState7,
                  Qt::CTRL | Qt::Key_7);
  addActionToMenu(saveStateMenu,
                  "Slot 8",
                  &stateHandler,
                  &ViewStateHandler::saveViewState8,
                  Qt::CTRL | Qt::Key_8);

  auto loadStateMenu = viewMenu->addMenu("Restore View State");
  addActionToMenu(
      loadStateMenu, "Slot 1", &stateHandler, &ViewStateHandler::loadViewState1, Qt::Key_1);
  addActionToMenu(
      loadStateMenu, "Slot 2", &stateHandler, &ViewStateHandler::loadViewState2, Qt::Key_2);
  addActionToMenu(
      loadStateMenu, "Slot 3", &stateHandler, &ViewStateHandler::loadViewState3, Qt::Key_3);
  addActionToMenu(
      loadStateMenu, "Slot 4", &stateHandler, &ViewStateHandler::loadViewState4, Qt::Key_4);
  addActionToMenu(
      loadStateMenu, "Slot 5", &stateHandler, &ViewStateHandler::loadViewState5, Qt::Key_5);
  addActionToMenu(
      loadStateMenu, "Slot 6", &stateHandler, &ViewStateHandler::loadViewState6, Qt::Key_6);
  addActionToMenu(
      loadStateMenu, "Slot 7", &stateHandler, &ViewStateHandler::loadViewState7, Qt::Key_7);
  addActionToMenu(
      loadStateMenu, "Slot 8", &stateHandler, &ViewStateHandler::loadViewState8, Qt::Key_8);
  viewMenu->addSeparator();

  auto dockPanelsMenu = viewMenu->addMenu("Dock Panels");
  auto addDockViewAction =
      [dockPanelsMenu](QDockWidget *dockWidget, QString text, const QKeySequence &shortcut = {}) {
        auto action = dockWidget->toggleViewAction();
        action->setText(text);
        action->setShortcut(shortcut);
        dockPanelsMenu->addAction(action);
      };
  addDockViewAction(ui.playlistDockWidget, "Show P&laylist", Qt::CTRL | Qt::Key_L);
  addDockViewAction(ui.propertiesDock, "Show &Properties", Qt::CTRL | Qt::Key_P);
  addDockViewAction(ui.fileInfoDock, "Show &Info", Qt::CTRL | Qt::Key_I);
  addDockViewAction(ui.cachingInfoDock, "Show Caching Info");
  viewMenu->addSeparator();
  addDockViewAction(ui.playbackControllerDock, "Show Playback &Controls", Qt::CTRL | Qt::Key_D);

  auto splitViewMenu = viewMenu->addMenu("Split View");
  ui.displaySplitView->addMenuActions(splitViewMenu);

  auto zoomMenu = viewMenu->addMenu("Zoom");
  addActionToMenu(
      zoomMenu, "Zoom to 1:1", this, &MainWindow::onMenuResetView, Qt::CTRL | Qt::Key_0);
  addActionToMenu(
      zoomMenu, "Zoom to Fit", this, &MainWindow::onMenuZoomToFit, Qt::CTRL | Qt::Key_9);
  addActionToMenu(zoomMenu, "Zoom in", this, &MainWindow::onMenuZoomIn, Qt::CTRL | Qt::Key_Plus);
  addActionToMenu(zoomMenu, "Zoom out", this, &MainWindow::onMenuZoomOut, Qt::CTRL | Qt::Key_Minus);
  zoomMenu->addSeparator();
  addActionToMenu(zoomMenu, "Zoom to 50%", this, &MainWindow::onMenuZoomTo50);
  addActionToMenu(zoomMenu, "Zoom to 100%", this, &MainWindow::onMenuZoomTo100);
  addActionToMenu(zoomMenu, "Zoom to 200%", this, &MainWindow::onMenuZoomTo200);
  addActionToMenu(zoomMenu, "Zoom to ...", this, &MainWindow::onMenuZoomToCustom);

  auto playbackMenu = menuBar()->addMenu(tr("&Playback"));
  addActionToMenu(playbackMenu,
                  "Play/Pause",
                  ui.playbackController,
                  &PlaybackController::on_playPauseButton_clicked,
                  Qt::Key_Space);
  addActionToMenu(playbackMenu,
                  "Next Playlist Item",
                  ui.playlistTreeWidget,
                  &PlaylistTreeWidget::onSelectNextItem,
                  Qt::Key_Down);
  addActionToMenu(playbackMenu,
                  "Previous Playlist Item",
                  ui.playlistTreeWidget,
                  &PlaylistTreeWidget::selectPreviousItem,
                  Qt::Key_Up);
  addActionToMenu(playbackMenu,
                  "Next Frame",
                  ui.playbackController,
                  &PlaybackController::nextFrame,
                  Qt::Key_Right);
  addActionToMenu(playbackMenu,
                  "Previous Frame",
                  ui.playbackController,
                  &PlaybackController::previousFrame,
                  Qt::Key_Left);

  auto addLambdaActionToMenu = [this](QMenu *menu, QString name, auto lambda) {
    auto action = new QAction(name, menu);
    QObject::connect(action, &QAction::triggered, lambda);
    menu->addAction(action);
  };

  auto helpMenu = menuBar()->addMenu(tr("&Help"));
  addActionToMenu(helpMenu, "About YUView", this, &MainWindow::showAbout);
  addActionToMenu(helpMenu, "Help", this, &MainWindow::showHelp);
  helpMenu->addSeparator();
  addLambdaActionToMenu(helpMenu, "Open Project Website...", []() {
    QDesktopServices::openUrl(QUrl("https://github.com/IENT/YUView"));
  });
  addLambdaActionToMenu(
      helpMenu, "Check for new version", [this]() { this->updater->startCheckForNewVersion(); });

  auto downloadsMenu = helpMenu->addMenu("Downloads");
  addLambdaActionToMenu(downloadsMenu, "libde265 HEVC decoder", []() {
    QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/libde265/releases"));
  });
  addLambdaActionToMenu(downloadsMenu, "HM reference HEVC decoder", []() {
    QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/libHM/releases"));
  });
  addLambdaActionToMenu(downloadsMenu, "VTM VVC decoder", []() {
    QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/VTM/releases"));
  });
  addLambdaActionToMenu(downloadsMenu, "dav1d AV1 decoder", []() {
    QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/dav1d/releases"));
  });
  addLambdaActionToMenu(downloadsMenu, "FFmpeg libraries", []() {
    QDesktopServices::openUrl(QUrl("https://ffmpeg.org/"));
  });
  addLambdaActionToMenu(downloadsMenu, "VVDec libraries", []() {
    QDesktopServices::openUrl(QUrl("https://github.com/ChristianFeldmann/vvdec/releases"));
  });
  helpMenu->addSeparator();
  addLambdaActionToMenu(downloadsMenu, "Performance Tests", [this]() { this->performanceTest(); });
  addActionToMenu(helpMenu, "Reset Window Layout", this, &MainWindow::resetWindowLayout);
  addActionToMenu(helpMenu, "Clear Settings", this, &MainWindow::closeAndClearSettings);

  this->updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
  QSettings settings;
  auto      files = settings.value("recentFileList").toStringList();

  int  fileIdx = 0;
  auto it      = files.begin();
  while (it != files.end())
  {
    if ((QFile(*it).exists()))
    {
      QString text = tr("&%1 %2").arg(fileIdx + 1).arg(*it);
      recentFileActions[fileIdx]->setText(text);
      recentFileActions[fileIdx]->setData(*it);
      recentFileActions[fileIdx]->setVisible(true);
      fileIdx++;
      it++;
    }
    else
      it = files.erase(it);
  }
  for (int j = fileIdx; j < MAX_RECENT_FILES; ++j)
    recentFileActions[j]->setVisible(false);

  settings.setValue("recentFileList", files);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Stop playback
  ui.playbackController->pausePlayback();

  QSettings settings;
  if (!ui.playlistTreeWidget->getIsSaved() && settings.value("AskToSaveOnExit", true).toBool())
  {
    QMessageBox::StandardButton resBtn =
        QMessageBox::question(this,
                              "Quit YUView",
                              tr("You have not saved the current playlist, are you sure?\n"),
                              QMessageBox::Cancel | QMessageBox::Close | QMessageBox::Save,
                              QMessageBox::Close);
    if (resBtn == QMessageBox::Cancel)
    {
      event->ignore();
      return;
    }
    else if (resBtn == QMessageBox::Save)
      ui.playlistTreeWidget->savePlaylistToFile();
  }

  if (saveWindowsStateOnExit)
  {
    // Do not save the window state if we just cleared the settings
    QSettings settings;
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/windowState", saveState());
    settings.setValue("separateViewWindow/geometry", separateViewWindow.saveGeometry());
    settings.setValue("separateViewWindow/windowState", separateViewWindow.saveState());
  }

  // Delete all items in the playlist. This will also kill all eventual running background
  // processes.
  ui.playlistTreeWidget->deletePlaylistItems(true);

  event->accept();

  if (!separateViewWindow.isHidden())
    separateViewWindow.close();
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action)
  {
    QStringList fileList = QStringList(action->data().toString());
    ui.playlistTreeWidget->loadFiles(fileList);
  }
}

/** A new item has been selected. Update all the controls (some might be enabled/disabled for this
 * type of object and the values probably changed).
 * The signal playlistTreeWidget->selectionRangeChanged is connected to this slot.
 */
void MainWindow::currentSelectedItemsChanged(playlistItem *item1, playlistItem *)
{
  // qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") <<
  // "MainWindow::currentSelectedItemsChanged()";
  if (item1 == nullptr)
  {
    // Nothing is selected
    setWindowTitle("YUView");
  }
  else
  {
    // update window caption
    QString newCaption = "YUView - " + item1->text(0);
    setWindowTitle(newCaption);
  }
}

void MainWindow::deleteSelectedItems()
{
  // qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") <<
  // "MainWindow::deleteSelectedItems()";

  // stop playback first
  ui.playbackController->pausePlayback();

  ui.playlistTreeWidget->deletePlaylistItems(false);
}

bool MainWindow::handleKeyPress(QKeyEvent *event, bool keyFromSeparateView)
{
  int  key         = event->key();
  bool controlOnly = (event->modifiers() == Qt::ControlModifier);

  if (key == Qt::Key_Escape)
  {
    if (isFullScreen())
    {

      ui.displaySplitView->toggleFullScreenAction();
      return true;
    }
  }
  else if (key == Qt::Key_F && controlOnly)
  {
    ui.displaySplitView->toggleFullScreenAction();
    return true;
  }
  else if (key == Qt::Key_Space)
  {
    ui.playbackController->on_playPauseButton_clicked();
    return true;
  }
  else if (key == Qt::Key_Right)
  {
    ui.playbackController->nextFrame();
    return true;
  }
  else if (key == Qt::Key_Left)
  {
    ui.playbackController->previousFrame();
    return true;
  }
  else if (key == Qt::Key_Down)
  {
    ui.playlistTreeWidget->selectNextItem();
    return true;
  }
  else if (key == Qt::Key_Up)
  {
    ui.playlistTreeWidget->selectPreviousItem();
    return true;
  }
  else if (stateHandler.handleKeyPress(event, keyFromSeparateView))
  {
    return true;
  }
  else if (!keyFromSeparateView)
  {
    // See if the split view widget handles this key press. If not, return false.
    return ui.displaySplitView->handleKeyPress(event);
  }

  return false;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  // On MAC, this works to open a file with an existing application
  if (watched == qApp && event->type() == QEvent::FileOpen)
  {
    QStringList fileList(static_cast<QFileOpenEvent *>(event)->file());
    loadFiles(fileList);
  }

  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
  // qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<< event;

  if (!handleKeyPress(event, false))
    QWidget::keyPressEvent(event);
}

void MainWindow::focusInEvent(QFocusEvent *)
{
  QSettings settings;
  if (settings.value("WatchFiles", true).toBool())
    ui.playlistTreeWidget->checkAndUpdateItems();
}

MoveAndZoomableView *MainWindow::getCurrentActiveView()
{
  if (this->ui.tabWidget->currentIndex() == 0)
    return this->ui.displaySplitView;
  if (this->ui.tabWidget->currentIndex() == 1)
    return this->ui.bitstreamAnalysis->getCurrentActiveView();
  return {};
}

void MainWindow::toggleFullscreen()
{
  // Single window mode. Hide/show all panels and set/restore the main window to/from fullscreen.

  if (isFullScreen())
  {
    // We are in full screen mode. Toggle back to windowed mode.

    // Show all dock panels which were previously visible.
    if (panelsVisible[0])
      ui.propertiesDock->show();
    if (panelsVisible[1])
      ui.playlistDockWidget->show();
    if (panelsVisible[2])
      ui.playbackControllerDock->show();
    if (panelsVisible[3])
      ui.fileInfoDock->show();
    if (panelsVisible[4])
      ui.cachingInfoDock->show();

    if (!is_Q_OS_MAC)
      ui.menuBar->show();
    ui.tabWidget->tabBar()->show();

    // Show the window normal or maximized (depending on how it was shown before)
    if (showNormalMaximized)
      showMaximized();
    else
      showNormal();
  }
  else
  {
    // Toggle to full screen mode. Save which panels are currently visible. This
    // is restored when returning from full screen.
    panelsVisible[0] = ui.propertiesDock->isVisible();
    panelsVisible[1] = ui.playlistDockWidget->isVisible();
    panelsVisible[2] = ui.playbackControllerDock->isVisible();
    panelsVisible[3] = ui.fileInfoDock->isVisible();
    panelsVisible[4] = ui.cachingInfoDock->isVisible();

    // Hide panels
    ui.propertiesDock->hide();
    ui.playlistDockWidget->hide();
    QSettings settings;
    if (!settings.value("ShowPlaybackControlFullScreen", false).toBool())
      ui.playbackControllerDock->hide();
    ui.fileInfoDock->hide();
    ui.cachingInfoDock->hide();

    if (!is_Q_OS_MAC)
      ui.menuBar->hide();
    ui.tabWidget->tabBar()->hide();

    // Save if the window is currently maximized
    showNormalMaximized = isMaximized();

    showFullScreen();
  }
}

void MainWindow::showAboutHelp(bool showAbout)
{
  // Try to open the about.html file from the resource
  QFile file(showAbout ? ":/about.html" : ":/help.html");
  if (!file.open(QIODevice::ReadOnly))
  {
    QMessageBox::critical(this,
                          "Error opening about or help",
                          "The html file from the resource could not be loaded.");
    return;
  }

  // Read the content of the .html file into the byte array
  QByteArray total;
  QByteArray line;
  while (!file.atEnd())
  {
    line = file.read(1024);
    total.append(line);
  }
  file.close();

  // Replace the ##VERSION## keyword with the actual version
  QString htmlString = QString(total);
  if (showAbout)
    htmlString.replace("##VERSION##", QApplication::applicationVersion());

  // Create a QTextBrowser, set the text and the properties and show it
  QTextBrowser *about = new QTextBrowser(this);
  // about->setWindowFlags(Qt::Window | Qt::MSWindowsFixedSizeDialogHint | Qt::FramelessWindowHint |
  // Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
  about->setWindowFlags(Qt::Dialog);
  about->setReadOnly(true);
  about->setOpenExternalLinks(true);
  about->setHtml(htmlString);
  about->setMinimumHeight(800);
  about->setMinimumWidth(showAbout ? 900 : 1200); // Width is fixed. Is this OK for high DPI?
  about->setMaximumWidth(showAbout ? 900 : 1200);
  about->setWindowModality(Qt::ApplicationModal);
  about->show();
}

void MainWindow::showSettingsWindow()
{
  SettingsDialog dialog;
  int            result = dialog.exec();

  if (result == QDialog::Accepted)
    // Load the new settings
    updateSettings();
}

void MainWindow::updateSettings()
{
  // Set the right theme
  QSettings settings;
  QString   themeName = settings.value("Theme", "Default").toString();
  QString   themeFile = functions::getThemeFileName(themeName);

  QString styleSheet;
  if (!themeFile.isEmpty())
  {
    // Get the qss text of the theme
    QFile f(themeFile);
    if (f.exists())
    {
      f.open(QFile::ReadOnly | QFile::Text);
      QTextStream ts(&f);
      styleSheet = ts.readAll();

      // Now replace the placeholder color values with the real values
      QStringList colors = functions::getThemeColors(themeName);
      if (colors.count() == 4)
      {
        styleSheet.replace("#backgroundColor", colors[0]);
        styleSheet.replace("#activeColor", colors[1]);
        styleSheet.replace("#inactiveColor", colors[2]);
        styleSheet.replace("#highlightColor", colors[3]);
      }
      else
        styleSheet.clear();
    }
  }
  // Set the style sheet. If the string is empty, the default will be used/set.
  qApp->setStyleSheet(styleSheet);

  ui.displaySplitView->updateSettings();
  separateViewWindow.splitView.updateSettings();
  ui.playlistTreeWidget->updateSettings();
  ui.bitstreamAnalysis->updateSettings();
  this->cache->updateSettings();
  ui.playbackController->updateSettings();
}

void MainWindow::saveScreenshot()
{
  // Ask the use if he wants to save the current view as it is or the complete frame of the item.
  QMessageBox msgBox;
  msgBox.setWindowTitle("Select screenshot mode");
  msgBox.setText("<b>Current View: </b>Save a screenshot of the central view as you can see it "
                 "right now.<br><b>Item Frame: </b>Save the entire current frame of the selected "
                 "item in it's original resolution.");
  msgBox.addButton(tr("Current View"), QMessageBox::AcceptRole);
  QPushButton *itemFrame   = msgBox.addButton(tr("Item Frame"), QMessageBox::AcceptRole);
  QPushButton *abortButton = msgBox.addButton(QMessageBox::Abort);
  msgBox.exec();

  bool fullItem = (msgBox.clickedButton() == itemFrame);
  if (msgBox.clickedButton() == abortButton)
    // The use pressed cancel
    return;

  // What image formats are supported?
  QString     allFormats;
  QStringList fileExtensions;
  QStringList fileFilterStrings;
  for (QByteArray f : QImageWriter::supportedImageFormats())
  {
    QString filterString = QString("%1 (*.%1)").arg(QString(f));
    fileExtensions.append(QString(f));
    fileFilterStrings.append(filterString);

    allFormats += filterString + ";;";
  }

  // Get the filename for the screenshot
  QSettings settings;
  QString   selectedFilter = "png (*.png)";
  QString   filename       = QFileDialog::getSaveFileName(this,
                                                  tr("Save Screenshot"),
                                                  settings.value("LastScreenshotPath").toString(),
                                                  allFormats,
                                                  &selectedFilter);

  // Is a file extension set?
  QString setSuffix = QFileInfo(filename).suffix();
  if (!fileExtensions.contains(setSuffix))
  {
    // The file extension is not set or not used.
    // Add the file extensinos of the selected filter.
    int idx = fileFilterStrings.indexOf(selectedFilter);
    filename += "." + fileExtensions[idx];
  }

  if (!filename.isEmpty())
  {
    ui.displaySplitView->getScreenshot(fullItem).save(filename);

    // Save the path to the file so that we can open the next "save screenshot" file dialog in the
    // same directory.
    filename = filename.section('/', 0, -2);
    settings.setValue("LastScreenshotPath", filename);
  }
}

/* Show the file open dialog and open the selected files
 */
void MainWindow::showFileOpenDialog()
{
  // load last used directory from QPreferences
  QSettings settings;

  // Get all supported extensions/filters
  QStringList filters = playlistItems::getSupportedFormatsFilters();

  QFileDialog openDialog(this);
  openDialog.setDirectory(settings.value("lastFilePath").toString());
  openDialog.setFileMode(QFileDialog::ExistingFiles);
  openDialog.setNameFilters(filters);

  QStringList fileNames;
  if (openDialog.exec())
    fileNames = openDialog.selectedFiles();

  if (fileNames.count() > 0)
  {
    // save last used directory with QPreferences
    QString filePath = fileNames.at(0);
    filePath         = filePath.section('/', 0, -2);
    settings.setValue("lastFilePath", filePath);
  }

  ui.playlistTreeWidget->loadFiles(fileNames);

  updateRecentFileActions();
}

/// End Full screen. Goto one window mode and reset all the geometry and state settings that were
/// saved.
void MainWindow::resetWindowLayout()
{
  // This is the code to obtain the raw value that is used below for restoring the state
  /*QByteArray windowState = saveState();
  QString test = windowState.toHex();
  qDebug() << test;*/
  /*QByteArray windowsGeometry = separateViewWindow.saveState();
  QString test = windowsGeometry.toHex();
  qDebug() << test;*/

  QSettings settings;

  // Quit full screen if anything is in full screen
  ui.displaySplitView->showNormal();
  separateViewWindow.showNormal();
  showNormal();

  // Reset the separate window and save the state
  separateViewWindow.hide();
  separateViewWindow.setGeometry(0, 0, 500, 300);
  separateViewWindow.move(0, 0);
  settings.setValue("separateViewWindow/geometry", separateViewWindow.saveGeometry());
  settings.setValue("separateViewWindow/windowState", separateViewWindow.saveState());

  // Dock all dock panels
  ui.playlistDockWidget->setFloating(false);
  ui.propertiesDock->setFloating(false);
  ui.playbackControllerDock->setFloating(false);
  ui.fileInfoDock->setFloating(false);
  ui.cachingInfoDock->setFloating(false);

  // show the menu bar
  if (!is_Q_OS_MAC)
    ui.menuBar->show();

  // Reset main window state (the size and position of the dock widgets). The code to obtain this
  // raw value is above.
  QByteArray mainWindowState = QByteArray::fromHex(
      "000000ff00000000fd00000003000000000000011600000348fc0200000003fb000000240070006c00610079006c"
      "0069007300740044006f0063006b005700690064006700650074010000001500000212000000c000fffffffb0000"
      "001800660069006c00650049006e0066006f0044006f0063006b010000022b000000840000005b00fffffffb0000"
      "002000630061006300680069006e0067004400650062007500670044006f0063006b01000002b3000000aa000000"
      "aa00ffffff00000001000000b900000348fc0200000002fb0000001c00700072006f007000650072007400690065"
      "00730044006f0063006b0100000015000002670000002d00fffffffb000000220064006900730070006c00610079"
      "0044006f0063006b0057006900640067006500740100000280000000dd000000dd0007ffff000000030000048f00"
      "000032fc0100000001fb0000002c0070006c00610079006200610063006b0043006f006e00740072006f006c006c"
      "006500720044006f0063006b01000000000000048f000001460007ffff000002b800000348000000040000000400"
      "00000800000008fc00000000");
  restoreState(mainWindowState);

  // Set the size/position of the main window
  setGeometry(0, 0, 1100, 750);
  move(0, 0); // move the top left with window frame

  // Save main window state (including the layout of the dock widgets)
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());

  // Reset the split view
  ui.displaySplitView->resetView(false);
}

void MainWindow::closeAndClearSettings()
{
  QMessageBox::StandardButton resBtn = QMessageBox::question(
      this, "Clear Settings", "Do you want to quit YUView and clear all settings?");
  if (resBtn == QMessageBox::No)
    return;

  QSettings settings;
  settings.clear();

  saveWindowsStateOnExit = false;
  close();
}

void MainWindow::performanceTest()
{
  performanceTestDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted)
  {
    if (dialog.getSelectedTestIndex() == 0)
      this->cache->testConversionSpeed();
    else if (dialog.getSelectedTestIndex() == 1)
      ui.displaySplitView->testDrawingSpeed();
    else if (dialog.getSelectedTestIndex() == 2)
    {
      QString info;
      info.append(QString("YUVIEW_VERSION %1\n").arg(YUVIEW_VERSION));
      info.append(QString("YUVIEW_HASH %1\n").arg(YUVIEW_HASH));
      info.append(QString("VERSION_CHECK %1\n").arg(VERSION_CHECK));
      info.append(QString("UPDATE_FEATURE_ENABLE %1\n").arg(UPDATE_FEATURE_ENABLE));
      info.append(QString("pixmapImageFormat %1\n")
                      .arg(functionsGui::pixelFormatToString(functionsGui::pixmapImageFormat())));
      info.append(QString("getOptimalThreadCount %1\n").arg(functions::getOptimalThreadCount()));
      info.append(QString("systemMemorySizeInMB %1\n").arg(functions::systemMemorySizeInMB()));

      QMessageBox::information(this, "Internal Info", info);
    }
  }
}
