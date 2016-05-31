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
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <math.h>
#include <QStringList>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QXmlStreamReader>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImageReader>
#include "playlistItems.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
  QSettings settings;
  qRegisterMetaType<indexRange>("indexRange");

  // set some defaults
  if (!settings.contains("Background/Color"))
    settings.setValue("Background/Color", QColor(128, 128, 128));
  if (!settings.contains("OverlayGrid/Color"))
    settings.setValue("OverlayGrid/Color", QColor(0, 0, 0));

  ui->setupUi(this);

  setFocusPolicy(Qt::StrongFocus);

  statusBar()->hide();

  // Initialize the seperate window
  separateViewWindow.setWindowTitle("Seperate View");
  separateViewWindow.setGeometry(0, 0, 300, 600);

  p_playlistWidget = ui->playlistTreeWidget;

  // Setup the display controls of the splitViewWidget and add them to the displayDockWidget.
  ui->displaySplitView->setupControls( ui->displayDockWidget );
  connect(ui->displaySplitView, SIGNAL(signalToggleFullScreen()), this, SLOT(toggleFullscreen()));

  // Setup primary/separate splitView
  ui->displaySplitView->setSeparateWidget( separateViewWindow.splitView );
  separateViewWindow.splitView->setPrimaryWidget( ui->displaySplitView );
  connect(ui->displaySplitView, SIGNAL(signalShowSeparateWindow(bool)), &separateViewWindow, SLOT(setVisible(bool)));

  // Connect the playlistWidget signals to some slots
  connect(p_playlistWidget, SIGNAL(selectionChanged(playlistItem*, playlistItem*, bool)), ui->fileInfoWidget, SLOT(currentSelectedItemsChanged(playlistItem*, playlistItem*)));
  connect(p_playlistWidget, SIGNAL(selectedItemChanged(bool)), ui->fileInfoWidget, SLOT(updateFileInfo(bool)));
  connect(p_playlistWidget, SIGNAL(selectionChanged(playlistItem*, playlistItem*, bool)), ui->playbackController, SLOT(currentSelectedItemsChanged(playlistItem*, playlistItem*, bool)));
  connect(p_playlistWidget, SIGNAL(selectionChanged(playlistItem*, playlistItem*, bool)), ui->propertiesWidget, SLOT(currentSelectedItemsChanged(playlistItem*, playlistItem*)));
  connect(p_playlistWidget, SIGNAL(selectedItemChanged(bool)), ui->playbackController, SLOT(selectionPropertiesChanged(bool)));
  connect(p_playlistWidget, SIGNAL(itemAboutToBeDeleted(playlistItem*)), ui->propertiesWidget, SLOT(itemAboutToBeDeleted(playlistItem*)));
  connect(p_playlistWidget, SIGNAL(openFileDialog()), this, SLOT(showFileOpenDialog()));
      
  ui->displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);

  createMenusAndActions();

  ui->playbackController->setSplitViews( ui->displaySplitView, separateViewWindow.splitView );
  ui->playbackController->setPlaylist( ui->playlistTreeWidget );
  ui->displaySplitView->setPlaybackController( ui->playbackController );
  ui->displaySplitView->setPlaylistTreeWidget( p_playlistWidget );
  separateViewWindow.splitView->setPlaybackController( ui->playbackController );
  separateViewWindow.splitView->setPlaylistTreeWidget( p_playlistWidget );

  // load geometry and active dockable widgets from user preferences
  restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
  restoreState(settings.value("mainWindow/windowState").toByteArray());
  separateViewWindow.restoreGeometry(settings.value("separateViewWindow/geometry").toByteArray());
  separateViewWindow.restoreState(settings.value("separateViewWindow/windowState").toByteArray());
  
  connect(&p_settingswindow, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
  connect(ui->openButton, SIGNAL(clicked()), this, SLOT(showFileOpenDialog()));

  // Connect signals from the separate window
  connect(&separateViewWindow, SIGNAL(signalSingleWindowMode()), ui->displaySplitView, SLOT(separateViewHide()));
  connect(&separateViewWindow, SIGNAL(signalNextFrame()), ui->playbackController, SLOT(nextFrame()));
  connect(&separateViewWindow, SIGNAL(signalPreviousFrame()), ui->playbackController, SLOT(previousFrame()));
  connect(&separateViewWindow, SIGNAL(signalPlayPauseToggle()), ui->playbackController, SLOT(on_playPauseButton_clicked()));
  connect(&separateViewWindow, SIGNAL(signalNextItem()), ui->playlistTreeWidget, SLOT(selectNextItem()));
  connect(&separateViewWindow, SIGNAL(signalPreviousItem()), ui->playlistTreeWidget, SLOT(selectPreviousItem()));
  
  // Update the selected item. Nothing is selected but the function will then set some default values.
  updateSelectedItems();
  // Call this once to init FrameCache and other settings
  updateSettings();

  // Create the videoCache object
  cache = new videoCache(p_playlistWidget, ui->playbackController);
}

void MainWindow::createMenusAndActions()
{
  fileMenu = menuBar()->addMenu(tr("&File"));
  openYUVFileAction = fileMenu->addAction("&Open File...", this, SLOT(showFileOpenDialog()), Qt::CTRL + Qt::Key_O);
  addTextAction = fileMenu->addAction("&Add Text Frame", ui->playlistTreeWidget, SLOT(addTextItem()));
  addDifferenceAction = fileMenu->addAction("&Add Difference Sequence", ui->playlistTreeWidget, SLOT(addDifferenceItem()));
  addOverlayAction = fileMenu->addAction("&Add Overlay", ui->playlistTreeWidget, SLOT(addOverlayItem()));
  fileMenu->addSeparator();
  for (int i = 0; i < MAX_RECENT_FILES; ++i)
  {
    recentFileActs[i] = new QAction(this);
    recentFileActs[i]->setVisible(false);
    connect(recentFileActs[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
    fileMenu->addAction(recentFileActs[i]);
  }
  fileMenu->addSeparator();
  deleteItemAction = fileMenu->addAction("&Delete Item", this, SLOT(deleteItem()), Qt::Key_Delete);
  p_playlistWidget->addAction(deleteItemAction);
#if defined (Q_OS_MACX)
  deleteItemAction->setShortcut(QKeySequence(Qt::Key_Backspace));
#endif
  fileMenu->addSeparator();
  savePlaylistAction = fileMenu->addAction("&Save Playlist...", p_playlistWidget, SLOT(savePlaylistToFile()),Qt::CTRL + Qt::Key_S);
  fileMenu->addSeparator();
  saveScreenshotAction = fileMenu->addAction("&Save Screenshot...", this, SLOT(saveScreenshot()) );
  fileMenu->addSeparator();
  showSettingsAction = fileMenu->addAction("&Settings", &p_settingswindow, SLOT(show()) );

  viewMenu = menuBar()->addMenu(tr("&View"));
  zoomToStandardAction = viewMenu->addAction("Zoom to 1:1", ui->displaySplitView, SLOT(resetViews()), Qt::CTRL + Qt::Key_0);
  zoomToFitAction = viewMenu->addAction("Zoom to Fit", ui->displaySplitView, SLOT(zoomToFit()), Qt::CTRL + Qt::Key_9);
  zoomInAction = viewMenu->addAction("Zoom in", ui->displaySplitView, SLOT(zoomIn()), Qt::CTRL + Qt::Key_Plus);
  zoomOutAction = viewMenu->addAction("Zoom out", ui->displaySplitView, SLOT(zoomOut()), Qt::CTRL + Qt::Key_Minus);
  viewMenu->addSeparator();
  togglePlaylistAction = viewMenu->addAction("Hide/Show P&laylist", ui->playlistDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_L);
  toggleDisplayOptionsAction = viewMenu->addAction("Hide/Show &Display Options", ui->displayDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_D);
  togglePropertiesAction = viewMenu->addAction("Hide/Show &Properties", ui->propertiesDock->toggleViewAction(), SLOT(trigger()));
  toggleFileInfoAction = viewMenu->addAction("Hide/Show &FileInfo", ui->fileInfoDock->toggleViewAction(), SLOT(trigger()));
  viewMenu->addSeparator();
  toggleControlsAction = viewMenu->addAction("Hide/Show Playback &Controls", ui->playbackControllerDock->toggleViewAction(), SLOT(trigger()));
  viewMenu->addSeparator();
  toggleFullscreenAction = viewMenu->addAction("&Fullscreen Mode", this, SLOT(toggleFullscreen()), Qt::CTRL + Qt::Key_F);
  enableSingleWindowModeAction = viewMenu->addAction("&Single Window Mode", ui->displaySplitView, SLOT(separateViewHide()), Qt::CTRL + Qt::Key_1);
  enableSeparateWindowModeAction = viewMenu->addAction("&Separate Windows Mode", ui->displaySplitView, SLOT(separateViewShow()), Qt::CTRL + Qt::Key_2);

  playbackMenu = menuBar()->addMenu(tr("&Playback"));
  playPauseAction = playbackMenu->addAction("Play/Pause", ui->playbackController, SLOT(on_playPauseButton_clicked()), Qt::Key_Space);
  nextItemAction = playbackMenu->addAction("Next Playlist Item", ui->playlistTreeWidget, SLOT(selectNextItem()), Qt::Key_Down);
  previousItemAction = playbackMenu->addAction("Previous Playlist Item", ui->playlistTreeWidget, SLOT(selectPreviousItem()), Qt::Key_Up);
  nextFrameAction = playbackMenu->addAction("Next Frame", ui->playbackController, SLOT(nextFrame()), Qt::Key_Right);
  previousFrameAction = playbackMenu->addAction("Previous Frame", ui->playbackController, SLOT(previousFrame()), Qt::Key_Left);

  helpMenu = menuBar()->addMenu(tr("&Help"));
  aboutAction = helpMenu->addAction("About YUView", this, SLOT(showAbout()));
  bugReportAction = helpMenu->addAction("Open Project Website...", this, SLOT(openProjectWebsite()));
  checkNewVersionAction = helpMenu->addAction("Check for new version",this,SLOT(checkNewVersion()));
  resetWindowLayoutAction = helpMenu->addAction("Reset Window Layout", this, SLOT(resetWindowLayout()));

  updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
  QSettings settings;
  QStringList files = settings.value("recentFileList").toStringList();

  int numRecentFiles = qMin(files.size(), MAX_RECENT_FILES);

  int fileIdx = 1;
  for (int i = 0; i < numRecentFiles; ++i)
  {
    if (!(QFile(files[i]).exists()))
      continue;

    QString text = tr("&%1 %2").arg(fileIdx++).arg(QFileInfo(files[i]).fileName());
    recentFileActs[i]->setText(text);
    recentFileActs[i]->setData(files[i]);
    recentFileActs[i]->setVisible(true);
  }
  for (int j = numRecentFiles; j < MAX_RECENT_FILES; ++j)
    recentFileActs[j]->setVisible(false);
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (!p_playlistWidget->getIsSaved())
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
      p_playlistWidget->savePlaylistToFile();
  }

  QSettings settings;
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());
  settings.setValue("separateViewWindow/geometry", separateViewWindow.saveGeometry());
  settings.setValue("separateViewWindow/windowState", separateViewWindow.saveState());
  
  // Delete all items in the playlist. This will also kill all eventual runnig background processes.
  p_playlistWidget->deleteAllPlaylistItems();

  event->accept();

  separateViewWindow.close();
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action)
  {
    QStringList fileList = QStringList(action->data().toString());
    p_playlistWidget->loadFiles(fileList);
  }
}

playlistItem* MainWindow::selectedPrimaryPlaylistItem()
{
  if (p_playlistWidget == NULL)
    return NULL;

  QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();
  playlistItem* selectedItemPrimary = NULL;

  if (selectedItems.count() >= 1)
  {
    bool found_parent = false;
    bool allowAssociatedItem = selectedItems.count() == 1;  // if only one item is selected, it can also be a child (associated)

    foreach(QTreeWidgetItem* anItem, selectedItems)
    {
      // we search an item that does not have a parent
      if (allowAssociatedItem || !dynamic_cast<playlistItem*>(anItem->parent()))
      {
        found_parent = true;
        selectedItemPrimary = dynamic_cast<playlistItem*>(anItem);
        break;
      }
    }
    if (!found_parent)
    {
      selectedItemPrimary = dynamic_cast<playlistItem*>(selectedItems.first());
    }
  }
  return selectedItemPrimary;
}

playlistItem* MainWindow::selectedSecondaryPlaylistItem()
{
  if (p_playlistWidget == NULL)
    return NULL;

  QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();
  playlistItem* selectedItemSecondary = NULL;

  if (selectedItems.count() >= 2)
  {
    playlistItem* selectedItemPrimary = selectedPrimaryPlaylistItem();

    foreach(QTreeWidgetItem* anItem, selectedItems)
    {
      // we search an item that does not have a parent and that is not the primary item
      //playlistItem* aPlaylistParentItem = dynamic_cast<playlistItem*>(anItem->parent());
      //if(!aPlaylistParentItem && anItem != selectedItemPrimary)
      if (anItem != selectedItemPrimary)
      {
        selectedItemSecondary = dynamic_cast<playlistItem*>(anItem);
        break;
      }
    }
  }

  return selectedItemSecondary;
}

/** A new item has been selected. Update all the controls (some might be enabled/disabled for this
  * type of object and the values probably changed).
  * The signal playlistTreeWidget->itemSelectionChanged is connected to this slot.
  */
void MainWindow::updateSelectedItems()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::updateSelectedItems()";

  // Get the selected item(s)
  playlistItem* selectedItemPrimary = selectedPrimaryPlaylistItem();
  //playlistItem* selectedItemSecondary = selectedSecondaryPlaylistItem();

  if (selectedItemPrimary == NULL)
  {
    // Nothing is selected
    setWindowTitle("YUView");
  }
  else
  {
    // update window caption
    QString newCaption = "YUView - " + selectedItemPrimary->text(0);
    setWindowTitle(newCaption);
  }
}

void MainWindow::deleteItem()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::deleteItem()";

  // stop playback first
  ui->playbackController->pausePlayback();

  p_playlistWidget->deleteSelectedPlaylistItems();
}

// for debug only
bool MainWindow::eventFilter(QObject *target, QEvent *event)
{
  if (event->type() == QEvent::KeyPress)
  {
    //QKeyEvent* keyEvent = (QKeyEvent*)event;
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<<keyEvent<<"Object: "<<target;
  }
  return QWidget::eventFilter(target, event);
}

void MainWindow::handleKeyPress(QKeyEvent *key)
{
  keyPressEvent(key);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<< event;

  // more keyboard shortcuts can be implemented here...
  int key = event->key();
  bool control = (event->modifiers() == Qt::ControlModifier);

  if (key == Qt::Key_Escape)
  {
    if (isFullScreen())
      toggleFullscreen();
  }
  else if (key == Qt::Key_F && control)
    toggleFullscreen();
  else if (key == Qt::Key_1 && control)
    ui->displaySplitView->separateViewHide();
  else if (key == Qt::Key_2 && control)
    ui->displaySplitView->separateViewShow();
  else if (key == Qt::Key_Space)
    ui->playbackController->on_playPauseButton_clicked();
  else if (key == Qt::Key_Right)
    ui->playbackController->nextFrame();
  else if (key == Qt::Key_Left)
    ui->playbackController->previousFrame();
  else if (key == Qt::Key_0 && control)
    ui->displaySplitView->resetViews();
  else if (key == Qt::Key_9 && control)
    ui->displaySplitView->zoomToFit();
  else if (key == Qt::Key_Plus && control)
    ui->displaySplitView->zoomIn();
  else if (key == Qt::Key_BracketRight && control)
    // This seems to be a bug in the Qt localization routine. On the german keyboard layout this key is returned
    // if Ctrl + is pressed. 
    ui->displaySplitView->zoomIn();
  else if (key == Qt::Key_Minus && control)
    ui->displaySplitView->zoomOut();
  else if (key == Qt::Key_Down)
    ui->playlistTreeWidget->selectNextItem();
  else if (key == Qt::Key_Up)
    ui->playlistTreeWidget->selectPreviousItem();
  else
    QWidget::keyPressEvent(event);
}

void MainWindow::toggleFullscreen()
{
  QSettings settings;

  // Single window mode. Hide/show all panels and set/restore the main window to/from fullscreen.

  if (isFullScreen())
  {
    // We are in full screen mode. Toggle back to windowed mode.

    // Show all dock panels
    ui->propertiesDock->show();
    ui->playlistDockWidget->show();
    ui->displayDockWidget->show();
    ui->playbackControllerDock->show();
    ui->fileInfoDock->show();

#ifndef QT_OS_MAC
    // show the menu bar
    ui->menuBar->show();
#endif

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
    ui->propertiesDock->hide();
    ui->playlistDockWidget->hide();
    ui->displayDockWidget->hide();
    ui->playbackControllerDock->hide();
    ui->fileInfoDock->hide();
#ifndef QT_OS_MAC
    // hide menu bar
    ui->menuBar->hide();
#endif
    
    // Save if the window is currently maximized
    showNormalMaximized = isMaximized();

    showFullScreen();
  }

  ui->displaySplitView->resetViews();
}

/////////////////////////////////////////////////////////////////////////////////

void MainWindow::checkNewVersion()
{
#if VERSION_CHECK
  QEventLoop eventLoop;
  QNetworkAccessManager networkManager;
  QObject::connect(&networkManager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
  QUrl url(QString("https://api.github.com/repos/IENT/YUView/commits"));
  QNetworkRequest request;
  request.setUrl(url);
  QNetworkReply* currentReply = networkManager.get(request);
  eventLoop.exec();

  if (currentReply->error() == QNetworkReply::NoError)
  {
    QString strReply = (QString)currentReply->readAll();
    //parse json
    QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());
    QJsonArray jsonArray = jsonResponse.array();
    QJsonObject jsonObject = jsonArray[0].toObject();
    QString currentHash = jsonObject["sha"].toString();
    QString buildHash = QString::fromUtf8(YUVIEW_HASH);
    QString buildVersion = QString::fromUtf8(YUVIEW_VERSION);
    if (QString::compare(currentHash, buildHash))
    {
      QMessageBox msgBox;
      msgBox.setTextFormat(Qt::RichText);
      msgBox.setInformativeText("<a href='https://github.com/IENT/YUView/releases'>https://github.com/IENT/YUView/releases</a>");
      msgBox.setText("You need to update to the newest version.<br>Your Version: " + buildVersion);
      msgBox.exec();
    }
    else
    {
      QMessageBox msgBox;
      msgBox.setText("Already up to date.");
      msgBox.exec();
    }

  }
  else
  {
    //failure
    QMessageBox msgBox;
    msgBox.setText("Connection error. Are you connected?");
    msgBox.exec();
  }
  delete currentReply;
#else
  QMessageBox msgBox;
  msgBox.setText("Version Checking is not included in your Build.");
  msgBox.exec();
#endif
}

void MainWindow::showAbout()
{
  QTextBrowser *about = new QTextBrowser(this);
  Qt::WindowFlags flags = 0;

  flags = Qt::Window;
  flags |= Qt::MSWindowsFixedSizeDialogHint;
  //flags |= Qt::X11BypassWindowManagerHint;
  flags |= Qt::FramelessWindowHint;
  flags |= Qt::WindowTitleHint;
  flags |= Qt::WindowCloseButtonHint;
  //flags |= Qt::WindowStaysOnTopHint;
  flags |= Qt::CustomizeWindowHint;

  about->setWindowFlags(flags);
  about->setReadOnly(true);
  about->setOpenExternalLinks(true);

  QFile file(":/about.html");
  if (!file.open(QIODevice::ReadOnly))
  {
    //some error report
  }

  QByteArray total;
  QByteArray line;
  while (!file.atEnd()) {
    line = file.read(1024);
    total.append(line);
  }
  file.close();

  QString htmlString = QString(total);
  htmlString.replace("##VERSION##", QApplication::applicationVersion());

  about->setHtml(htmlString);
  about->setFixedSize(QSize(900, 800));

  about->show();
}

void MainWindow::openProjectWebsite()
{
  QDesktopServices::openUrl(QUrl("https://github.com/IENT/YUView"));
}

void MainWindow::saveScreenshot() {

  QSettings settings;

  QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), settings.value("LastScreenshotPath").toString(), tr("PNG Files (*.png);"));

  ui->displaySplitView->getScreenshot().save(filename);

  filename = filename.section('/', 0, -2);
  settings.setValue("LastScreenshotPath", filename);
}

void MainWindow::updateSettings()
{
  p_ClearFrame = p_settingswindow.getClearFrameState();
  updateSelectedItems();
  ui->displaySplitView->updateSettings();
}

/* Show the file open dialog and open the selected files
 */
void MainWindow::showFileOpenDialog()
{
  // load last used directory from QPreferences
  QSettings settings;

  // Get all supported extensions/filters
  QStringList filters;
  playlistItems::getSupportedFormatsFilters(filters);
  
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

  p_playlistWidget->loadFiles(fileNames);

  updateRecentFileActions();

  //if (selectedPrimaryPlaylistItem())
  //{
  //  // Commented: This does not work for multiple screens. In this case QDesktopWidget().availableGeometry()
  //  // returns the size of the primary screen and not the size of the screen the application is on.
  //  // Also when in full screen this should not be done.
  //  // In general resizing the application without user interaction seems like a bug.

  //  //// resize player window to fit video size
  //  //QRect screenRect = QDesktopWidget().availableGeometry();
  //  //unsigned int newWidth = MIN( MAX( selectedPrimaryPlaylistItem()->displayObject()->width()+680, width() ), screenRect.width() );
  //  //unsigned int newHeight = MIN( MAX( selectedPrimaryPlaylistItem()->displayObject()->height()+140, height() ), screenRect.height() );
  //  //resize( newWidth, newHeight );
  //}
}

/// End Full screen. Goto one window mode and reset all the geometrie and state settings that were saved.
void MainWindow::resetWindowLayout()
{
  // This is the code to obtain the raw value that is used below for restoring the state
  /*QByteArray windowState = saveState();
  QString test = windowState.toHex();
  qDebug() << test;*/

  QSettings settings;

  // Quit full screen if anything is in full screen
  ui->displaySplitView->showNormal();
  separateViewWindow.showNormal();
  showNormal();

  // Get the split view widget back to the main window and hide the seperate window
  setCentralWidget(ui->displaySplitView);

  // Reset the seperate window and save the state
  separateViewWindow.hide();
  separateViewWindow.setGeometry(0, 0, 500, 300);
  separateViewWindow.move(0,0);
  settings.setValue("separateViewWindow/geometry", separateViewWindow.saveGeometry());
  settings.setValue("separateViewWindow/windowState", separateViewWindow.saveState());

  // Dock all dock panels
  ui->playlistDockWidget->setFloating(false);
  ui->propertiesDock->setFloating(false);
  ui->displayDockWidget->setFloating(false);
  ui->playbackControllerDock->setFloating(false);
  ui->fileInfoDock->setFloating(false);
  
#ifndef QT_OS_MAC
  // show the menu bar
  ui->menuBar->show();
#endif

  // Reset main window state (the size and position of the dock widgets)
  QByteArray mainWindowState = QByteArray::fromHex("000000ff00000000fd0000000300000000000000d1000002c8fc0200000002fb000000240070006c00610079006c0069007300740044006f0063006b0057006900640067006500740100000015000002c8000000c000fffffffb0000001e007300740061007400730044006f0063006b005700690064006700650074010000038d000000de000000000000000000000001000000b9000002c8fc0200000008fb0000001c00660069006c00650044006f0063006b0057006900640067006500740100000015000001910000000000000000fb00000022005900550056004d0061007400680064006f0063006b00570069006400670065007401000001aa0000018f0000000000000000fb0000002000700072006f00700065007200740069006500730057006900640067006500740100000015000001af0000000000000000fb000000100069006e0066006f0044006f0063006b0100000188000000810000000000000000fb0000002400660069006c00650049006e0066006f0044006f0063006b0057006900640067006500740100000177000000a20000000000000000fb0000001c00700072006f00700065007200740069006500730044006f0063006b0100000015000001a90000001600fffffffb0000001800660069006c00650049006e0066006f0044006f0063006b01000001c2000000830000002800fffffffb000000220064006900730070006c006100790044006f0063006b0057006900640067006500740100000249000000940000008e0007ffff000000030000049100000026fc0100000002fb000000240063006f006e00740072006f006c00730044006f0063006b0057006900640067006500740100000000000007800000000000000000fb0000002c0070006c00610079006200610063006b0043006f006e00740072006f006c006c006500720044006f0063006b010000000000000491000000fd0007ffff000002ff000002c800000004000000040000000800000008fc00000000");
  restoreState(mainWindowState);

  // Set the size/position of the main window
  setGeometry(0, 0, 1100, 750);
  move(0,0);  // move the top left with window frame
    
  // Save main window state (including the layout of the dock widgets)
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());

  // Reset the split view
  ui->displaySplitView->resetViews();
}