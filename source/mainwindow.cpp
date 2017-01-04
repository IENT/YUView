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

#include "mainwindow.h"

#include <QByteArray>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QTextBrowser>
#include "playlistItems.h"
#include "settingsDialog.h"
#include "signalsSlots.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  QSettings settings;
  qRegisterMetaType<indexRange>("indexRange");

  // set some defaults
  if (!settings.contains("Background/Color"))
    settings.setValue("Background/Color", QColor(128, 128, 128));
  if (!settings.contains("OverlayGrid/Color"))
    settings.setValue("OverlayGrid/Color", QColor(0, 0, 0));

  ui.setupUi(this);

  // Create the update handler
  updater.reset(new updateHandler(this));

  setFocusPolicy(Qt::StrongFocus);

  statusBar()->hide();

  // Initialize the separate window
  separateViewWindow.setWindowTitle("Separate View");
  separateViewWindow.setGeometry(0, 0, 300, 600);

  // Setup the display controls of the splitViewWidget and add them to the displayDockWidget.
  ui.displaySplitView->setupControls(ui.displayDockWidget);
  connect(ui.displaySplitView, &splitViewWidget::signalToggleFullScreen, this, &MainWindow::toggleFullscreen);

  // Setup primary/separate splitView
  ui.displaySplitView->setSeparateWidget(&separateViewWindow.splitView);
  separateViewWindow.splitView.setPrimaryWidget( ui.displaySplitView );
  connect(ui.displaySplitView, &splitViewWidget::signalShowSeparateWindow, &separateViewWindow, &QWidget::setVisible);

  // Connect the playlistWidget signals to some slots
  auto const fileInfoAdapter = [this]{
    auto items = ui.playlistTreeWidget->getSelectedItems();
    ui.fileInfoWidget->setInfo(items[0] ? items[0]->getInfo() : infoData(),
                               items[1] ? items[1]->getInfo() : infoData());
  };
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectionRangeChanged, ui.fileInfoWidget, fileInfoAdapter);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectedItemChanged, ui.fileInfoWidget, fileInfoAdapter);
  connect(ui.fileInfoWidget, &FileInfoWidget::infoButtonClicked, [this](int infoIndex, int row){
    auto items = ui.playlistTreeWidget->getSelectedItems();
    if (items[infoIndex]) items[infoIndex]->infoListButtonPressed(row);
  });
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectionRangeChanged, ui.playbackController, &PlaybackController::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectionRangeChanged, ui.propertiesWidget, &PropertiesWidget::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectionRangeChanged, this, &MainWindow::currentSelectedItemsChanged);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectedItemChanged, ui.playbackController, &PlaybackController::selectionPropertiesChanged);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::itemAboutToBeDeleted, ui.propertiesWidget, &PropertiesWidget::itemAboutToBeDeleted);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::openFileDialog, this, &MainWindow::showFileOpenDialog);
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::selectedItemDoubleBufferLoad, ui.playbackController, &PlaybackController::currentSelectedItemsDoubleBufferLoad);

  ui.displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);

  createMenusAndActions();

  // Create the videoCache object
  cache.reset(new videoCache(ui.playlistTreeWidget, ui.playbackController, this));

  ui.playbackController->setSplitViews(ui.displaySplitView, &separateViewWindow.splitView);
  ui.playbackController->setPlaylist(ui.playlistTreeWidget);
  ui.displaySplitView->setPlaybackController(ui.playbackController);
  ui.displaySplitView->setPlaylistTreeWidget(ui.playlistTreeWidget);
  ui.displaySplitView->setVideoCache(cache.data());
  separateViewWindow.splitView.setPlaybackController(ui.playbackController);
  separateViewWindow.splitView.setPlaylistTreeWidget(ui.playlistTreeWidget);

  // load geometry and active dockable widgets from user preferences
  restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
  restoreState(settings.value("mainWindow/windowState").toByteArray());
  separateViewWindow.restoreGeometry(settings.value("separateViewWindow/geometry").toByteArray());
  separateViewWindow.restoreState(settings.value("separateViewWindow/windowState").toByteArray());
  
  connect(ui.openButton, &QPushButton::clicked, this, &MainWindow::showFileOpenDialog);

  // Connect signals from the separate window
  connect(&separateViewWindow, &SeparateWindow::signalSingleWindowMode, ui.displaySplitView, &splitViewWidget::toggleSeparateViewHideShow);
  connect(&separateViewWindow, &SeparateWindow::unhandledKeyPress, this, &MainWindow::handleKeyPressFromSeparateView);

  // Setup the video cache status widget
  ui.videoCacheStatus->setPlaylist(ui.playlistTreeWidget);
  ui.videoCacheStatus->setCache(cache.data());
  connect(ui.playlistTreeWidget, &PlaylistTreeWidget::bufferStatusUpdate, ui.videoCacheStatus, QWidget_update); // FIXME TODO the widget should know when to update

  // Set the controls in the state handler. This way, the state handler can save/load the current state of the view.
  stateHandler.setConctrols(ui.playbackController, ui.playlistTreeWidget, ui.displaySplitView, &separateViewWindow.splitView);
  // Give the playlist a pointer to the state handler so it can save the states ti playlist
  ui.playlistTreeWidget->setViewStateHandler(&stateHandler);
}

void MainWindow::createMenusAndActions()
{
  // File menu
  QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction("&Open File...", this, &MainWindow::showFileOpenDialog, Qt::CTRL + Qt::Key_O);
  QMenu *recentFileMenu = fileMenu->addMenu("Recent Files");
  for (int i = 0; i < MAX_RECENT_FILES; i++)
  {
    recentFileActions[i] = new QAction(this);
    connect(recentFileActions[i], &QAction::triggered, this, &MainWindow::openRecentFile);
    recentFileMenu->addAction(recentFileActions[i]);
  }
  fileMenu->addSeparator();
  fileMenu->addAction("&Add Text Frame", ui.playlistTreeWidget, &PlaylistTreeWidget::addTextItem);
  fileMenu->addAction("&Add Difference Sequence", ui.playlistTreeWidget, &PlaylistTreeWidget::addDifferenceItem);
  fileMenu->addAction("&Add Overlay", ui.playlistTreeWidget, &PlaylistTreeWidget::addOverlayItem);
  fileMenu->addSeparator();
  fileMenu->addAction("&Delete Item", this, &MainWindow::deleteItem, Qt::Key_Delete);
  fileMenu->addSeparator();
  fileMenu->addAction("&Save Playlist...", ui.playlistTreeWidget, &PlaylistTreeWidget::savePlaylistToFile, Qt::CTRL + Qt::Key_S);
  fileMenu->addSeparator();
  fileMenu->addAction("&Save Screenshot...", this, &MainWindow::saveScreenshot);
  fileMenu->addSeparator();
  fileMenu->addAction("&Settings...", this, &MainWindow::showSettingsWindow);
  fileMenu->addSeparator();
  fileMenu->addAction("Exit", this, &MainWindow::close);

  // View menu
  QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
  QMenu *saveStateMenu = viewMenu->addMenu("Save View State");
  QMenu *loadStateMenu = viewMenu->addMenu("Restore View State");
  for (int i = 0; i < 8; i++)
  {
    saveStateMenu->addAction(QString("Slot %1").arg(i+1), this, [=]{ stateHandler.saveViewState(i, false); }, Qt::CTRL + Qt::Key_1 + i);
    loadStateMenu->addAction(QString("Slot %1").arg(i+1), this, [=]{ stateHandler.loadViewState(i, false); }, Qt::Key_1 + i);
  }
  viewMenu->addSeparator();
  viewMenu->addAction("Zoom to 1:1", ui.displaySplitView, &splitViewWidget::resetViews, Qt::CTRL + Qt::Key_0);
  viewMenu->addAction("Zoom to Fit", ui.displaySplitView, &splitViewWidget::zoomToFit, Qt::CTRL + Qt::Key_9);
  viewMenu->addAction("Zoom in", this, [=]{ ui.displaySplitView->zoomIn(); }, Qt::CTRL + Qt::Key_Plus);
  viewMenu->addAction("Zoom out", this, [=]{ ui.displaySplitView->zoomOut(); }, Qt::CTRL + Qt::Key_Minus);
  viewMenu->addSeparator();
  viewMenu->addAction("Hide/Show P&laylist", ui.playlistDockWidget->toggleViewAction(), &QAction::trigger, Qt::CTRL + Qt::Key_L);
  viewMenu->addAction("Hide/Show &Display Options", ui.displayDockWidget->toggleViewAction(), &QAction::trigger, Qt::CTRL + Qt::Key_D);
  viewMenu->addAction("Hide/Show &Properties", ui.propertiesDock->toggleViewAction(), &QAction::trigger, Qt::CTRL + Qt::Key_P);
  viewMenu->addAction("Hide/Show &Info", ui.fileInfoDock->toggleViewAction(), &QAction::trigger, Qt::CTRL + Qt::Key_I);
  viewMenu->addSeparator();
  viewMenu->addAction("Hide/Show Playback &Controls", ui.playbackControllerDock->toggleViewAction(), &QAction::trigger);
  viewMenu->addSeparator();
  viewMenu->addAction("&Fullscreen Mode", this, &MainWindow::toggleFullscreen, Qt::CTRL + Qt::Key_F);
  viewMenu->addAction("&Single/Separate Window Mode", ui.displaySplitView, &splitViewWidget::toggleSeparateViewHideShow, Qt::CTRL + Qt::Key_W);

  // The playback menu
  QMenu *playbackMenu = menuBar()->addMenu(tr("&Playback"));
  playbackMenu->addAction("Play/Pause", ui.playbackController, &PlaybackController::on_playPauseButton_clicked, Qt::Key_Space);
  playbackMenu->addAction("Next Playlist Item", this, [=]{ ui.playlistTreeWidget->selectNextItem(); }, Qt::Key_Down);
  playbackMenu->addAction("Previous Playlist Item", ui.playlistTreeWidget, &PlaylistTreeWidget::selectPreviousItem, Qt::Key_Up);
  playbackMenu->addAction("Next Frame", ui.playbackController, &PlaybackController::nextFrame, Qt::Key_Right);
  playbackMenu->addAction("Previous Frame", ui.playbackController, &PlaybackController::previousFrame, Qt::Key_Left);

  // The Help menu
  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction("About YUView", this, &MainWindow::showAbout);
  helpMenu->addAction("Help", this, &MainWindow::showHelp);
  helpMenu->addAction("Open Project Website...", this, [=]{ QDesktopServices::openUrl(QUrl("https://github.com/IENT/YUView")); });
  helpMenu->addAction("Check for new version", updater.data(), [=]{ updater->startCheckForNewVersion(); });
  helpMenu->addAction("Reset Window Layout", this, &MainWindow::resetWindowLayout);

  updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
  QSettings settings;
  QStringList files = settings.value("recentFileList").toStringList();

  int numRecentFiles = qMin(files.size(), MAX_RECENT_FILES);

  int fileIdx = 0;
  for (int i = 0; i < numRecentFiles; ++i)
  {
    if (!(QFile(files[i]).exists()))
      continue;

    //QString text = tr("&%1 %2").arg(fileIdx++).arg(QFileInfo(files[i]).fileName());
    QString text = tr("&%1 %2").arg(fileIdx+1).arg(files[i]);
    recentFileActions[fileIdx]->setText(text);
    recentFileActions[fileIdx]->setData(files[i]);
    recentFileActions[fileIdx]->setVisible(true);

    fileIdx++;
  }
  for (int j = fileIdx; j < MAX_RECENT_FILES; ++j)
    recentFileActions[j]->setVisible(false);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (!ui.playlistTreeWidget->getIsSaved())
  {
    QMessageBox::StandardButton resBtn = QMessageBox::question(this, "Quit YUView",
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

  QSettings settings;
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());
  settings.setValue("separateViewWindow/geometry", separateViewWindow.saveGeometry());
  settings.setValue("separateViewWindow/windowState", separateViewWindow.saveState());

  // Delete all items in the playlist. This will also kill all eventual running background processes.
  ui.playlistTreeWidget->deleteAllPlaylistItems();

  event->accept();

  if (!separateViewWindow.isHidden())
    separateViewWindow.close();
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction*>(sender());
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
void MainWindow::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::currentSelectedItemsChanged()";
  Q_UNUSED(item2);

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

void MainWindow::deleteItem()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::deleteItem()";

  // stop playback first
  ui.playbackController->pausePlayback();

  ui.playlistTreeWidget->deleteSelectedPlaylistItems();
}

bool MainWindow::handleKeyPress(QKeyEvent *event, bool keyFromSeparateView)
{
  int key = event->key();
  bool controlOnly = (event->modifiers() == Qt::ControlModifier);

  if (key == Qt::Key_Escape)
  {
    if (isFullScreen())
    {
      toggleFullscreen();
      return true;
    }
  }
  else if (key == Qt::Key_F && controlOnly)
  {
    toggleFullscreen();
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
  if (watched == qApp && event->type() == QEvent::FileOpen) {
    QStringList fileList(static_cast<QFileOpenEvent *>(event)->file());
    loadFiles(fileList);
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<< event;

  if (!handleKeyPress(event, false))
    QWidget::keyPressEvent(event);
}

void MainWindow::focusInEvent(QFocusEvent *event)
{
  Q_UNUSED(event);

  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    ui.playlistTreeWidget->checkAndUpdateItems();
}

void MainWindow::toggleFullscreen()
{
  QSettings settings;

  // Single window mode. Hide/show all panels and set/restore the main window to/from fullscreen.

  if (isFullScreen())
  {
    // We are in full screen mode. Toggle back to windowed mode.

    // Show all dock panels
    ui.propertiesDock->show();
    ui.playlistDockWidget->show();
    ui.displayDockWidget->show();
    ui.playbackControllerDock->show();
    ui.fileInfoDock->show();

    // show the menu bar
    if (!is_Q_OS_MAC)
      ui.menuBar->show();

    // Show the window normal or maximized (depending on how it was shown before)
    if (showNormalMaximized)
      showMaximized();
    else
      showNormal();
  }
  else
  {
    // Toggle to full screen mode

    // Hide panels
    ui.propertiesDock->hide();
    ui.playlistDockWidget->hide();
    ui.displayDockWidget->hide();
    ui.playbackControllerDock->hide();
    ui.fileInfoDock->hide();

    // hide menu bar
    if (!is_Q_OS_MAC)
      ui.menuBar->hide();

    // Save if the window is currently maximized
    showNormalMaximized = isMaximized();

    showFullScreen();
  }

  ui.displaySplitView->resetViews();
}

void MainWindow::showAboutHelp(bool showAbout)
{
  // Try to open the about.html file from the resource
  QFile file(showAbout ? ":/about.html" : ":/help.html");
  if (!file.open(QIODevice::ReadOnly))
  {
    QMessageBox::critical(this, "Error opening about or help", "The html file from the resource could not be loaded.");
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
  //about->setWindowFlags(Qt::Window | Qt::MSWindowsFixedSizeDialogHint | Qt::FramelessWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
  about->setWindowFlags(Qt::Dialog);
  about->setReadOnly(true);
  about->setOpenExternalLinks(true);
  about->setHtml(htmlString);
  about->setMinimumHeight(800);
  about->setMinimumWidth(showAbout ? 900 : 1200);  // Width is fixed. Is this OK for high DPI?
  about->setMaximumWidth(showAbout ? 900 : 1200);
  about->setWindowModality(Qt::WindowModal);
  about->show();
}

void MainWindow::showSettingsWindow()
{
  SettingsDialog dialog;
  int result = dialog.exec();

  if (result == QDialog::Accepted)
  {
    // Load the new settings
    ui.displaySplitView->updateSettings();
    separateViewWindow.splitView.updateSettings();
    ui.playlistTreeWidget->updateSettings();
    cache->updateSettings();
  }
}

void MainWindow::saveScreenshot() 
{
  // Ask the use if he wants to save the current view as it is or the complete frame of the item.
  QMessageBox msgBox;
  msgBox.setWindowTitle("Select screenshot mode");
  msgBox.setText("<b>Current View: </b>Save a screenshot of the central view as you can see it right now.<br><b>Item Frame: </b>Save the entire current frame of the selected item in it's original resolution.");
  msgBox.addButton(tr("Current View"), QMessageBox::AcceptRole);
  QPushButton *itemFrame   = msgBox.addButton(tr("Item Frame"), QMessageBox::AcceptRole);
  QPushButton *abortButton = msgBox.addButton(QMessageBox::Abort);
  msgBox.exec();

  bool fullItem = (msgBox.clickedButton() == itemFrame);
  if (msgBox.clickedButton() == abortButton) 
    // The use pressed cancel
    return;

  // Get the filename for the screenshot
  QSettings settings;
  QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), settings.value("LastScreenshotPath").toString(), tr("PNG Files (*.png);"));

  if (!filename.isEmpty())
  {
    ui.displaySplitView->getScreenshot(fullItem).save(filename);
    
    // Save the path to the file so that we can open the next "save screenshot" file dialog in the same directory.
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
    filePath = filePath.section('/', 0, -2);
    settings.setValue("lastFilePath", filePath);
  }

  ui.playlistTreeWidget->loadFiles(fileNames);

  updateRecentFileActions();
}

/// End Full screen. Goto one window mode and reset all the geometry and state settings that were saved.
void MainWindow::resetWindowLayout()
{
  // This is the code to obtain the raw value that is used below for restoring the state
  /*QByteArray windowState = saveState();
  QString test = windowState.toHex();
  qDebug() << test;*/

  QSettings settings;

  // Quit full screen if anything is in full screen
  ui.displaySplitView->showNormal();
  separateViewWindow.showNormal();
  showNormal();

  // Get the split view widget back to the main window and hide the separate window
  setCentralWidget(ui.displaySplitView);

  // Reset the separate window and save the state
  separateViewWindow.hide();
  separateViewWindow.setGeometry(0, 0, 500, 300);
  separateViewWindow.move(0,0);
  settings.setValue("separateViewWindow/geometry", separateViewWindow.saveGeometry());
  settings.setValue("separateViewWindow/windowState", separateViewWindow.saveState());

  // Dock all dock panels
  ui.playlistDockWidget->setFloating(false);
  ui.propertiesDock->setFloating(false);
  ui.displayDockWidget->setFloating(false);
  ui.playbackControllerDock->setFloating(false);
  ui.fileInfoDock->setFloating(false);
  
  // show the menu bar
  if (!is_Q_OS_MAC)
    ui.menuBar->show();

  // Reset main window state (the size and position of the dock widgets). The code obtain this raw value is above.
  QByteArray mainWindowState = QByteArray::fromHex("000000ff00000000fd0000000300000000000000d1000002c8fc0200000002fb000000240070006c00610079006c0069007300740044006f0063006b0057006900640067006500740100000015000002c8000000c000fffffffb0000001e007300740061007400730044006f0063006b005700690064006700650074010000038d000000de000000000000000000000001000000b9000002c8fc0200000008fb0000001c00660069006c00650044006f0063006b0057006900640067006500740100000015000001910000000000000000fb00000022005900550056004d0061007400680064006f0063006b00570069006400670065007401000001aa0000018f0000000000000000fb0000002000700072006f00700065007200740069006500730057006900640067006500740100000015000001af0000000000000000fb000000100069006e0066006f0044006f0063006b0100000188000000810000000000000000fb0000002400660069006c00650049006e0066006f0044006f0063006b0057006900640067006500740100000177000000a20000000000000000fb0000001c00700072006f00700065007200740069006500730044006f0063006b0100000015000001a90000001600fffffffb0000001800660069006c00650049006e0066006f0044006f0063006b01000001c2000000830000002800fffffffb000000220064006900730070006c006100790044006f0063006b0057006900640067006500740100000249000000940000008e0007ffff000000030000049100000026fc0100000002fb000000240063006f006e00740072006f006c00730044006f0063006b0057006900640067006500740100000000000007800000000000000000fb0000002c0070006c00610079006200610063006b0043006f006e00740072006f006c006c006500720044006f0063006b010000000000000491000000fd0007ffff000002ff000002c800000004000000040000000800000008fc00000000");
  restoreState(mainWindowState);

  // Set the size/position of the main window
  setGeometry(0, 0, 1100, 750);
  move(0,0);  // move the top left with window frame

  // Save main window state (including the layout of the dock widgets)
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());

  // Reset the split view
  ui.displaySplitView->resetViews();
}
