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
#include <QThread>
#include <QStringList>
#include <QInputDialog>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QTextEdit>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCache>
#include <QMetaType>
#include "playlistItem.h"
#include "playlistItemYUVFile.h"

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
  p_inspectorWindow.setWindowTitle("Inspector");
  p_playlistWindow.setWindowTitle("Playlist");
  p_playlistWindow.setGeometry(0, 0, 300, 600);


  // load window mode from preferences
  p_windowMode = (WindowMode)settings.value("windowMode").toInt();
  switch (p_windowMode)
  {
    case WindowModeSingle:
      enableSingleWindowMode();
      break;
    case WindowModeSeparate:
         enableSeparateWindowsMode();
        break;
    }

  p_playlistWidget = ui->playlistTreeWidget;

  // Setup the display controls of the splitViewWidget and add them to the displayDockWidget.
  ui->displaySplitView->setuptControls( ui->displayDockWidget );

  // Connect the playlistWidget signals to some slots
  connect(p_playlistWidget, SIGNAL(selectionChanged(playlistItem*, playlistItem*)), ui->fileInfoWidget, SLOT(currentSelectedItemsChanged(playlistItem*, playlistItem*)));
  connect(p_playlistWidget, SIGNAL(selectedItemChanged(bool)), ui->fileInfoWidget, SLOT(updateFileInfo(bool)));
  connect(p_playlistWidget, SIGNAL(selectionChanged(playlistItem*, playlistItem*)), ui->playbackController, SLOT(currentSelectedItemsChanged(playlistItem*, playlistItem*)));
  connect(p_playlistWidget, SIGNAL(selectionChanged(playlistItem*, playlistItem*)), ui->propertiesWidget, SLOT(currentSelectedItemsChanged(playlistItem*, playlistItem*)));
  connect(p_playlistWidget, SIGNAL(selectedItemChanged(bool)), ui->playbackController, SLOT(selectionPropertiesChanged(bool)));
  connect(p_playlistWidget, SIGNAL(itemAboutToBeDeleted(playlistItem*)), ui->propertiesWidget, SLOT(itemAboutToBeDeleted(playlistItem*)));
  connect(p_playlistWidget, SIGNAL(openFileDialog()), this, SLOT(showFileOpenDialog()));
  
  ui->displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);

  createMenusAndActions();

  ui->playbackController->setSplitView( ui->displaySplitView );
  ui->displaySplitView->setPlaybackController( ui->playbackController );
  ui->displaySplitView->setPlaylistTreeWidget( p_playlistWidget );

  // load geometry and active dockable widgets from user preferences
  //restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
  restoreState(settings.value("mainWindow/windowState").toByteArray());
  p_playlistWindow.restoreGeometry(settings.value("playlistWindow/geometry").toByteArray());
  p_playlistWindow.restoreState(settings.value("playlistWindow/windowState").toByteArray());
  p_inspectorWindow.restoreGeometry(settings.value("inspectorWindow/geometry").toByteArray());
  p_inspectorWindow.restoreState(settings.value("inspectorWindow/windowState").toByteArray());

  QObject::connect(&p_settingswindow, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));

  // Update the selected item. Nothing is selected but the function will then set some default values.
  updateSelectedItems();
  // Call this once to init FrameCache and other settings
  updateSettings();

  // Create the videoCache object, move it to it's thread and start it.
  cache = new videoCache(p_playlistWidget, ui->playbackController);
}

void MainWindow::createMenusAndActions()
{
  fileMenu = menuBar()->addMenu(tr("&File"));
  openYUVFileAction = fileMenu->addAction("&Open File...", this, SLOT(showFileOpenDialog()), Qt::CTRL + Qt::Key_O);
  addTextAction = fileMenu->addAction("&Add Text Frame", ui->playlistTreeWidget, SLOT(addTextItem()));
  addDifferenceAction = fileMenu->addAction("&Add Difference Sequence", ui->playlistTreeWidget, SLOT(addDifferenceItem()));
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
    enableSingleWindowModeAction = viewMenu->addAction("&Single Window Mode", this, SLOT(enableSingleWindowMode()), Qt::CTRL + Qt::Key_1);
    enableSeparateWindowModeAction = viewMenu->addAction("&Separate Windows Mode", this, SLOT(enableSeparateWindowsMode()), Qt::CTRL + Qt::Key_2);

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
  p_playlistWindow.close();
  p_inspectorWindow.close();

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
  settings.setValue("windowMode", p_windowMode);
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());
  settings.setValue("playlistWindow/geometry", p_playlistWindow.saveGeometry());
  settings.setValue("playlistWindow/windowState", p_playlistWindow.saveState());
  settings.setValue("inspectorWindow/geometry", p_inspectorWindow.saveGeometry());
  settings.setValue("inspectorWindow/windowState", p_inspectorWindow.saveState());

  // Delete all items in the playlist. This will also kill all eventual runnig background processes.
  p_playlistWidget->deleteAllPlaylistItems();

  event->accept();
  //QMainWindow::closeEvent(event);
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

/** Update (activate/deactivate) the grid (Draw Grid).
  * The signal regularGridCheckBox->clicked is connected to this slot.
  */
void MainWindow::updateGrid()
{
  /*bool enableGrid = ui->regularGridCheckBox->checkState() == Qt::Checked;
  QSettings settings;
  QColor color = settings.value("OverlayGrid/Color").value<QColor>();
  ui->displaySplitView->setRegularGridParameters(enableGrid, ui->gridSizeBox->value(), color);*/
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
  qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<< event;

  // more keyboard shortcuts can be implemented here...
  switch (event->key())
  {
    case Qt::Key_Escape:
    {
      if (isFullScreen())
        toggleFullscreen();
      return;
    }
    case Qt::Key_F:
    {
      if (event->modifiers() == Qt::ControlModifier)
        toggleFullscreen();
      return;
    }
    case Qt::Key_1:
    {
      if (event->modifiers() == Qt::ControlModifier)
        enableSingleWindowMode();
      return;
    }
    case Qt::Key_2:
    {
      if (event->modifiers() == Qt::ControlModifier)
        enableSeparateWindowsMode();
      return;
    }
    case Qt::Key_Space:
    {
      ui->playbackController->on_playPauseButton_clicked();
      return;
    }
    case Qt::Key_Right:
    {
      ui->playbackController->nextFrame();
      return;
    }
    case Qt::Key_Left:
    {
      ui->playbackController->previousFrame();
      return;
    }
    case Qt::Key_0:
    {
      if (event->modifiers() == Qt::ControlModifier)
      {
        ui->displaySplitView->resetViews();
        return;
      }
    }
    case Qt::Key_9:
    {
      if (event->modifiers() == Qt::ControlModifier)
      {
        ui->displaySplitView->zoomToFit();
        return;
      }
    }
    case Qt::Key_Plus:
    {
      if (event->modifiers() == Qt::ControlModifier)
      {
        ui->displaySplitView->zoomIn();
        return;
      }
    }
    case Qt::Key_Minus:
    {
      if (event->modifiers() == Qt::ControlModifier)
      {
        ui->displaySplitView->zoomOut();
        return;
      }
    }
    case Qt::Key_Down:
    {
      ui->playlistTreeWidget->selectNextItem();
      return;
    }
    case Qt::Key_Up:
    {
      ui->playlistTreeWidget->selectPreviousItem();
      return;
    }
  }

  QWidget::keyPressEvent(event);
}

void MainWindow::toggleFullscreen()
{
  QSettings settings;

  if (isFullScreen())
  {
    // show panels
    ui->propertiesDock->show();
    ui->playlistDockWidget->show();
    ui->displayDockWidget->show();
    ui->playbackControllerDock->show();
    ui->fileInfoDock->show();

#ifndef QT_OS_MAC
    // show menu
    ui->menuBar->show();
#endif
    ui->displaySplitView->showNormal();
    showNormal();
    restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    restoreState(settings.value("mainWindow/windowState").toByteArray());
  }
  else
  {
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/windowState", saveState());

    if (p_windowMode == WindowModeSingle)
    {
      // hide panels
      ui->propertiesDock->hide();
      ui->fileInfoDock->hide();
      ui->playlistDockWidget->hide();
      ui->displayDockWidget->hide();
    }
#ifndef QT_OS_MAC
    // hide menu
    ui->menuBar->hide();
#endif
    // always hide playback controls in full screen mode
    ui->playbackControllerDock->hide();

    ui->displaySplitView->showFullScreen();

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

  /*QSettings settings;

  QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), settings.value("LastScreenshotPath").toString(), tr("PNG Files (*.png);"));

  ui->displaySplitView->captureScreenshot().save(filename);

  filename = filename.section('/', 0, -2);
  settings.setValue("LastScreenshotPath", filename);*/
}

void MainWindow::updateSettings()
{
  //FrameObject::frameCache.setMaxCost(p_settingswindow.getCacheSizeInMB());

  updateGrid();

  p_ClearFrame = p_settingswindow.getClearFrameState();
  updateSelectedItems();
  ui->displaySplitView->updateSettings();
}

void MainWindow::enableSeparateWindowsMode()
{
  // if we are in fullscreen, get back to windowed mode
  if (isFullScreen())
    this->toggleFullscreen();

  // show inspector window with default dockables
  p_inspectorWindow.hide();
  ui->fileInfoDock->show();
  p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->fileInfoDock);
  ui->displayDockWidget->show();
  p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->displayDockWidget);
  p_inspectorWindow.show();

  // show playlist with default dockables
  p_playlistWindow.hide();
  ui->playlistDockWidget->show();
  p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->playlistDockWidget);
  ui->playbackControllerDock->show();
  p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->playbackControllerDock);
  p_playlistWindow.show();
  ui->playlistTreeWidget->setFocus();
  p_windowMode = WindowModeSeparate;
}

// if we are in fullscreen, get back to windowed mode
void MainWindow::enableSingleWindowMode()
{
  // if we are in fullscreen, get back to windowed mode
  if (isFullScreen())
    this->toggleFullscreen();

  // hide inspector window and move dockables to main window
  p_inspectorWindow.hide();
  ui->fileInfoDock->show();
  this->addDockWidget(Qt::RightDockWidgetArea, ui->fileInfoDock);
  ui->displayDockWidget->show();
  this->addDockWidget(Qt::RightDockWidgetArea, ui->displayDockWidget);

  // hide playlist window and move dockables to main window
  p_playlistWindow.hide();
  ui->playlistDockWidget->show();
  this->addDockWidget(Qt::LeftDockWidgetArea, ui->playlistDockWidget);
  ui->playbackControllerDock->show();
  this->addDockWidget(Qt::BottomDockWidgetArea, ui->playbackControllerDock);
  activateWindow();

  p_windowMode = WindowModeSingle;
}

/* Show the file open dialog and open the selected files
 */
void MainWindow::showFileOpenDialog()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::openFile()";

  // load last used directory from QPreferences
  QSettings settings;
  QStringList filter;
  filter << "All Supported Files (*.yuv *.yuvplaylist *.csv *.hevc)" << "Video Files (*.yuv)" << "Playlist Files (*.yuvplaylist)" << "Statistics Files (*.csv)" << "HEVC File (*.hevc)";

  QFileDialog openDialog(this);
  openDialog.setDirectory(settings.value("lastFilePath").toString());
  openDialog.setFileMode(QFileDialog::ExistingFiles);
  openDialog.setNameFilters(filter);

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
