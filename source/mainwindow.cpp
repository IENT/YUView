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
#include "playlistitem.h"
#include "statslistmodel.h"
#include "plistparser.h"
#include "plistserializer.h"


#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
  QSettings settings;

  // set some defaults
  if (!settings.contains("Background/Color"))
    settings.setValue("Background/Color", QColor(128, 128, 128));
  if (!settings.contains("OverlayGrid/Color"))
    settings.setValue("OverlayGrid/Color", QColor(0, 0, 0));

  p_playlistWidget = NULL;
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
  //p_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  //connect(p_playlistWidget, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
  //connect(p_playlistWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(onItemDoubleClicked(QTreeWidgetItem*, int)));

  ui->displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);

  ui->playlistTreeWidget->setPropertiesStack( ui->propertiesStack );
  ui->playlistTreeWidget->setPropertiesDockWidget( ui->propertiesWidget );
  ui->playlistTreeWidget->setFileInfoGroupBox( ui->fileInfoGroupBox );

  if (!settings.value("SplitViewEnabled", true).toBool())
    on_SplitViewgroupBox_toggled(false);
  
  createMenusAndActions();

  StatsListModel *model = new StatsListModel(this);
  this->ui->statsListView->setModel(model);
  QObject::connect(model, SIGNAL(signalStatsTypesChanged()), this, SLOT(statsTypesChanged()));
  
  // load geometry and active dockable widgets from user preferences
  restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
  restoreState(settings.value("mainWindow/windowState").toByteArray());
  p_playlistWindow.restoreGeometry(settings.value("playlistWindow/geometry").toByteArray());
  p_playlistWindow.restoreState(settings.value("playlistWindow/windowState").toByteArray());
  p_inspectorWindow.restoreGeometry(settings.value("inspectorWindow/geometry").toByteArray());
  p_inspectorWindow.restoreState(settings.value("inspectorWindow/windowState").toByteArray());

  ui->opacityGroupBox->setEnabled(false);
  ui->opacitySlider->setEnabled(false);
  ui->gridCheckBox->setEnabled(false);
  QObject::connect(&p_settingswindow, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));

  // Update the selected item. Nothing is selected but the function will then set some default values.
  updateSelectedItems();
  // Call this once to init FrameCache and other settings
  updateSettings();
}

void MainWindow::createMenusAndActions()
{
  fileMenu = menuBar()->addMenu(tr("&File"));
  openYUVFileAction = fileMenu->addAction("&Open File...", this, SLOT(showFileOpenDialog()), Qt::CTRL + Qt::Key_O);
  addTextAction = fileMenu->addAction("&Add Text Frame", this, SLOT(addTextFrame()));
  addDifferenceAction = fileMenu->addAction("&Add Difference Sequence", this, SLOT(addDifferenceSequence()));
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
    savePlaylistAction = fileMenu->addAction("&Save Playlist...", this, SLOT(savePlaylistToFile()),Qt::CTRL + Qt::Key_S);
    fileMenu->addSeparator();
    saveScreenshotAction = fileMenu->addAction("&Save Screenshot...", this, SLOT(saveScreenshot()) );
    fileMenu->addSeparator();
    showSettingsAction = fileMenu->addAction("&Settings", &p_settingswindow, SLOT(show()) );

    viewMenu = menuBar()->addMenu(tr("&View"));
    /*zoomToStandardAction = viewMenu->addAction("Zoom to 1:1", ui->displaySplitView, SLOT(zoomToStandard()), Qt::CTRL + Qt::Key_0);
    zoomToFitAction = viewMenu->addAction("Zoom to Fit", ui->displaySplitView, SLOT(zoomToFit()), Qt::CTRL + Qt::Key_9);
    zoomInAction = viewMenu->addAction("Zoom in", ui->displaySplitView, SLOT(zoomIn()), Qt::CTRL + Qt::Key_Plus);
    zoomOutAction = viewMenu->addAction("Zoom out", ui->displaySplitView, SLOT(zoomOut()), Qt::CTRL + Qt::Key_Minus);*/
    viewMenu->addSeparator();
    togglePlaylistAction = viewMenu->addAction("Hide/Show P&laylist", ui->playlistDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_L);
    toggleStatisticsAction = viewMenu->addAction("Hide/Show &Statistics", ui->statsDockWidget->toggleViewAction(), SLOT(trigger()));
    viewMenu->addSeparator();
    toggleDisplayOptionsAction = viewMenu->addAction("Hide/Show &Display Options", ui->displayDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_D);
    togglePropertiesAction = viewMenu->addAction("Hide/Show &Properties", ui->propertiesWidget->toggleViewAction(), SLOT(trigger()));
    toggleFileInfoAction = viewMenu->addAction("Hide/Show &FileInfo", ui->fileInfoDockWidget->toggleViewAction(), SLOT(trigger()));
    viewMenu->addSeparator();
    toggleControlsAction = viewMenu->addAction("Hide/Show Playback &Controls", ui->playbackControllerDock->toggleViewAction(), SLOT(trigger()));
    viewMenu->addSeparator();
    toggleFullscreenAction = viewMenu->addAction("&Fullscreen Mode", this, SLOT(toggleFullscreen()), Qt::CTRL + Qt::Key_F);
    enableSingleWindowModeAction = viewMenu->addAction("&Single Window Mode", this, SLOT(enableSingleWindowMode()), Qt::CTRL + Qt::Key_1);
    enableSeparateWindowModeAction = viewMenu->addAction("&Separate Windows Mode", this, SLOT(enableSeparateWindowsMode()), Qt::CTRL + Qt::Key_2);

    playbackMenu = menuBar()->addMenu(tr("&Playback"));
    playPauseAction = playbackMenu->addAction("Play/Pause", this, SLOT(togglePlayback()), Qt::Key_Space);
    nextItemAction = playbackMenu->addAction("Next Playlist Item", this, SLOT(selectNextItem()), Qt::Key_Down);
    previousItemAction = playbackMenu->addAction("Previous Playlist Item", this, SLOT(selectPreviousItem()), Qt::Key_Up);
    nextFrameAction = playbackMenu->addAction("Next Frame", this, SLOT(nextFrame()), Qt::Key_Right);
    previousFrameAction = playbackMenu->addAction("Previous Frame", this, SLOT(previousFrame()), Qt::Key_Left);

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

    QString text = tr("&%1 %2").arg(fileIdx++).arg(strippedName(files[i]));
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
    else if (resBtn == QMessageBox::Close)
      event->accept();
    else
      savePlaylistToFile();
  }
  QSettings settings;
  settings.setValue("windowMode", p_windowMode);
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/windowState", saveState());
  settings.setValue("playlistWindow/geometry", p_playlistWindow.saveGeometry());
  settings.setValue("playlistWindow/windowState", p_playlistWindow.saveState());
  settings.setValue("inspectorWindow/geometry", p_inspectorWindow.saveGeometry());
  settings.setValue("inspectorWindow/windowState", p_inspectorWindow.saveState());
  QMainWindow::closeEvent(event);
}

void MainWindow::loadPlaylistFile(QString filePath)
{
  //// clear playlist first
  //p_playlistWidget->clear();

  //// parse plist structure of playlist file
  //QFile file(filePath);
  //QFileInfo fileInfo(file);

  //if (!file.open(QIODevice::ReadOnly))
  //  return;

  //QByteArray fileBytes = file.readAll();
  //QBuffer buffer(&fileBytes);
  //QVariantMap map = PListParser::parsePList(&buffer).toMap();

  //QVariantList itemList = map["Modules"].toList();
  //for (int i = 0; i < itemList.count(); i++)
  //{
  //  QVariantMap itemInfo = itemList[i].toMap();
  //  QVariantMap itemProps = itemInfo["Properties"].toMap();

  //  if (itemInfo["Class"].toString() == "TextFrameProvider")
  //  {
  //    float duration = itemProps["duration"].toFloat();
  //    QString fontName = itemProps["fontName"].toString();
  //    int fontSize = itemProps["fontSize"].toInt();
  //    QColor color = QColor(itemProps["fontColor"].toString());
  //    QFont font(fontName, fontSize);
  //    QString text = itemProps["text"].toString();

  //    // create text item and set properties
  //    PlaylistItem *newPlayListItemText = new PlaylistItem(PlaylistItem_Text, text, p_playlistWidget);
  //    QSharedPointer<TextObject> newText = newPlayListItemText->getTextObject();
  //    newText->setFont(font);
  //    newText->setDuration(duration);
  //    newText->setColor(color);
  //  }
  //  else if (itemInfo["Class"].toString() == "YUVFile")
  //  {
  //    QString fileURL = itemProps["URL"].toString();
  //    QString filePath = QUrl(fileURL).path();

  //    QString relativeURL = itemProps["rURL"].toString();
  //    QFileInfo checkAbsoluteFile(filePath);
  //    // check if file with absolute path exists, otherwise check relative path
  //    if (!checkAbsoluteFile.exists())
  //    {
  //      QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
  //      QFileInfo checkRelativeFile(combinePath);
  //      if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  //      {
  //        filePath = QDir::cleanPath(combinePath);
  //      }
  //      else
  //      {
  //        QMessageBox msgBox;
  //        msgBox.setTextFormat(Qt::RichText);
  //        msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
  //        msgBox.exec();
  //        break;
  //      }
  //    }

  //    int endFrame = itemProps["endFrame"].toInt();
  //    int frameOffset = itemProps["frameOffset"].toInt();
  //    int frameSampling = itemProps["frameSampling"].toInt();
  //    float frameRate = itemProps["framerate"].toFloat();
  //    YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)itemProps["pixelFormat"].toInt();
  //    int height = itemProps["height"].toInt();
  //    int width = itemProps["width"].toInt();


  //    // create video item and set properties
  //    PlaylistItem *newPlayListItemVideo = new PlaylistItem(PlaylistItem_Video, filePath, p_playlistWidget);
  //    QSharedPointer<FrameObject> newVideo = newPlayListItemVideo->getFrameObject();
  //    newVideo->setSize(width, height);
  //    newVideo->setSrcPixelFormat(pixelFormat);
  //    newVideo->setFrameRate(frameRate);
  //    newVideo->setSampling(frameSampling);
  //    newVideo->setStartFrame(frameOffset);
  //    newVideo->setEndFrame(endFrame);

  //    // load potentially associated statistics file
  //    if (itemProps.contains("statistics"))
  //    {
  //      QVariantMap itemInfoAssoc = itemProps["statistics"].toMap();
  //      QVariantMap itemPropsAssoc = itemInfoAssoc["Properties"].toMap();

  //      QString fileURL = itemProps["URL"].toString();
  //      QString filePath = QUrl(fileURL).path();

  //      QString relativeURL = itemProps["rURL"].toString();
  //      QFileInfo checkAbsoluteFile(filePath);
  //      // check if file with absolute path exists, otherwise check relative path
  //      if (!checkAbsoluteFile.exists())
  //      {
  //        QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
  //        QFileInfo checkRelativeFile(combinePath);
  //        if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  //        {
  //          filePath = QDir::cleanPath(combinePath);
  //        }
  //        else
  //        {
  //          QMessageBox msgBox;
  //          msgBox.setTextFormat(Qt::RichText);
  //          msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
  //          msgBox.exec();
  //          break;
  //        }
  //      }

  //      int endFrame = itemPropsAssoc["endFrame"].toInt();
  //      int frameOffset = itemPropsAssoc["frameOffset"].toInt();
  //      int frameSampling = itemPropsAssoc["frameSampling"].toInt();
  //      float frameRate = itemPropsAssoc["framerate"].toFloat();
  //      int height = itemPropsAssoc["height"].toInt();
  //      int width = itemPropsAssoc["width"].toInt();
  //      QVariantList activeStatsTypeList = itemPropsAssoc["typesChecked"].toList();

  //      // create associated statistics item and set properties
  //      PlaylistItem *newPlayListItemStat = new PlaylistItem(PlaylistItem_Statistics, filePath, newPlayListItemVideo);
  //      QSharedPointer<StatisticsObject> newStat = newPlayListItemStat->getStatisticsObject();
  //      newStat->setSize(width, height);
  //      newStat->setFrameRate(frameRate);
  //      newStat->setSampling(frameSampling);
  //      newStat->setStartFrame(frameOffset);
  //      newStat->setEndFrame(endFrame);

  //      // set active statistics
  //      StatisticsTypeList statsTypeList;

  //      for (int i = 0; i < activeStatsTypeList.count(); i++)
  //      {
  //        QVariantMap statsTypeParams = activeStatsTypeList[i].toMap();

  //        StatisticsType aType;
  //        aType.typeID = statsTypeParams["typeID"].toInt();
  //        aType.typeName = statsTypeParams["typeName"].toString();
  //        aType.render = true;
  //        aType.renderGrid = statsTypeParams["drawGrid"].toBool();
  //        aType.alphaFactor = statsTypeParams["alpha"].toInt();

  //        statsTypeList.append(aType);
  //      }

  //      if (statsTypeList.count() > 0)
  //        newStat->setStatisticsTypeList(statsTypeList);
  //    }
  //  }
  //  else if (itemInfo["Class"].toString() == "StatisticsFile")
  //  {
  //    QString fileURL = itemProps["URL"].toString();
  //    QString filePath = QUrl(fileURL).path();

  //    QString relativeURL = itemProps["rURL"].toString();
  //    QFileInfo checkAbsoluteFile(filePath);
  //    // check if file with absolute path exists, otherwise check relative path
  //    if (!checkAbsoluteFile.exists())
  //    {
  //      QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
  //      QFileInfo checkRelativeFile(combinePath);
  //      if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  //      {
  //        filePath = QDir::cleanPath(combinePath);
  //      }
  //      else
  //      {
  //        QMessageBox msgBox;
  //        msgBox.setTextFormat(Qt::RichText);
  //        msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
  //        msgBox.exec();
  //        break;
  //      }
  //    }

  //    int endFrame = itemProps["endFrame"].toInt();
  //    int frameOffset = itemProps["frameOffset"].toInt();
  //    int frameSampling = itemProps["frameSampling"].toInt();
  //    float frameRate = itemProps["framerate"].toFloat();
  //    int height = itemProps["height"].toInt();
  //    int width = itemProps["width"].toInt();
  //    QVariantList activeStatsTypeList = itemProps["typesChecked"].toList();

  //    // create statistics item and set properties
  //    PlaylistItem *newPlayListItemStat = new PlaylistItem(PlaylistItem_Statistics, filePath, p_playlistWidget);
  //    QSharedPointer<StatisticsObject> newStat = newPlayListItemStat->getStatisticsObject();
  //    newStat->setSize(width, height);
  //    newStat->setFrameRate(frameRate);
  //    newStat->setSampling(frameSampling);
  //    newStat->setStartFrame(frameOffset);
  //    newStat->setEndFrame(endFrame);

  //    // set active statistics
  //    StatisticsTypeList statsTypeList;

  //    for (int i = 0; i < activeStatsTypeList.count(); i++)
  //    {
  //      QVariantMap statsTypeParams = activeStatsTypeList[i].toMap();

  //      StatisticsType aType;
  //      aType.typeID = statsTypeParams["typeID"].toInt();
  //      aType.typeName = statsTypeParams["typeName"].toString();
  //      aType.render = true;
  //      aType.renderGrid = statsTypeParams["drawGrid"].toBool();
  //      aType.alphaFactor = statsTypeParams["alpha"].toInt();

  //      statsTypeList.append(aType);
  //    }

  //    if (statsTypeList.count() > 0)
  //      newStat->setStatisticsTypeList(statsTypeList);
  //  }
  //}

  //if (p_playlistWidget->topLevelItemCount() > 0)
  //{
  //  p_playlistWidget->setCurrentItem(p_playlistWidget->topLevelItem(0), 0, QItemSelectionModel::ClearAndSelect);
  //  // this is the first playlist that was loaded, therefore no saving needed
  //  p_playlistWidget->setIsSaved(true);
  //}
}

void MainWindow::savePlaylistToFile()
{
  //// ask user for file location
  //QSettings settings;

  //QString filename = QFileDialog::getSaveFileName(this, tr("Save Playlist"), settings.value("LastPlaylistPath").toString(), tr("Playlist Files (*.yuvplaylist)"));
  //if (filename.isEmpty())
  //  return;
  //if (!filename.endsWith(".yuvplaylist", Qt::CaseInsensitive))
  //  filename += ".yuvplaylist";

  //// remember this directory for next time
  //QDir dirName(filename.section('/', 0, -2));
  //settings.setValue("LastPlaylistPath", dirName.path());

  //QVariantList itemList;

  //for (int i = 0; i < p_playlistWidget->topLevelItemCount(); i++)
  //{
  //  PlaylistItem* anItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(i));

  //  QVariantMap itemInfo;
  //  QVariantMap itemProps;

  //  if (anItem->itemType() == PlaylistItem_Video)
  //  {
  //    QSharedPointer<FrameObject> vidItem = anItem->getFrameObject();

  //    itemInfo["Class"] = "YUVFile";

  //    QUrl fileURL(vidItem->path());
  //    fileURL.setScheme("file");
  //    QString relativePath = dirName.relativeFilePath(vidItem->path());
  //    itemProps["URL"] = fileURL.toString();
  //    itemProps["rURL"] = relativePath;
  //    itemProps["endFrame"] = vidItem->endFrame();
  //    itemProps["frameOffset"] = vidItem->startFrame();
  //    itemProps["frameSampling"] = vidItem->sampling();
  //    itemProps["framerate"] = vidItem->frameRate();
  //    itemProps["pixelFormat"] = vidItem->pixelFormat();
  //    itemProps["height"] = vidItem->height();
  //    itemProps["width"] = vidItem->width();

  //    // store potentially associated statistics file
  //    if (anItem->childCount() == 1)
  //    {
  //      QSharedPointer<StatisticsObject> statsItem = anItem->getStatisticsObject();

  //      Q_ASSERT(statsItem);

  //      QVariantMap itemInfoAssoc;
  //      itemInfoAssoc["Class"] = "AssociatedStatisticsFile";

  //      QVariantMap itemPropsAssoc;
  //      QUrl fileURL(statsItem->path());
  //      QString relativePath = dirName.relativeFilePath(statsItem->path());

  //      fileURL.setScheme("file");
  //      itemPropsAssoc["URL"] = fileURL.toString();
  //      itemProps["rURL"] = relativePath;
  //      itemPropsAssoc["endFrame"] = statsItem->endFrame();
  //      itemPropsAssoc["frameOffset"] = statsItem->startFrame();
  //      itemPropsAssoc["frameSampling"] = statsItem->sampling();
  //      itemPropsAssoc["framerate"] = statsItem->frameRate();
  //      itemPropsAssoc["height"] = statsItem->height();
  //      itemPropsAssoc["width"] = statsItem->width();

  //      // save active statistics types
  //      StatisticsTypeList statsTypeList = statsItem->getStatisticsTypeList();

  //      QVariantList activeStatsTypeList;
  //      Q_FOREACH(StatisticsType aType, statsTypeList)
  //      {
  //        if (aType.render)
  //        {
  //          QVariantMap statsTypeParams;

  //          statsTypeParams["typeID"] = aType.typeID;
  //          statsTypeParams["typeName"] = aType.typeName;
  //          statsTypeParams["drawGrid"] = aType.renderGrid;
  //          statsTypeParams["alpha"] = aType.alphaFactor;

  //          activeStatsTypeList.append(statsTypeParams);
  //        }
  //      }

  //      if (activeStatsTypeList.count() > 0)
  //        itemPropsAssoc["typesChecked"] = activeStatsTypeList;

  //      itemInfoAssoc["Properties"] = itemPropsAssoc;

  //      // link to video item
  //      itemProps["statistics"] = itemInfoAssoc;
  //    }
  //  }
  //  else if (anItem->itemType() == PlaylistItem_Text)
  //  {
  //    QSharedPointer<TextObject> textItem = anItem->getTextObject();

  //    itemInfo["Class"] = "TextFrameProvider";

  //    itemProps["duration"] = textItem->duration();
  //    itemProps["fontSize"] = textItem->font().pointSize();
  //    itemProps["fontName"] = textItem->font().family();
  //    itemProps["fontColor"] = textItem->color().name();
  //    itemProps["text"] = textItem->text();
  //  }
  //  else if (anItem->itemType() == PlaylistItem_Statistics)
  //  {
  //    QSharedPointer<StatisticsObject> statsItem = anItem->getStatisticsObject();

  //    itemInfo["Class"] = "StatisticsFile";

  //    QUrl fileURL(statsItem->path());
  //    QString relativePath = dirName.relativeFilePath(statsItem->path());

  //    fileURL.setScheme("file");
  //    itemProps["URL"] = fileURL.toString();
  //    itemProps["rURL"] = relativePath;
  //    itemProps["endFrame"] = statsItem->endFrame();
  //    itemProps["frameOffset"] = statsItem->startFrame();
  //    itemProps["frameSampling"] = statsItem->sampling();
  //    itemProps["framerate"] = statsItem->frameRate();
  //    itemProps["height"] = statsItem->height();
  //    itemProps["width"] = statsItem->width();

  //    // save active statistics types
  //    StatisticsTypeList statsTypeList = statsItem->getStatisticsTypeList();

  //    QVariantList activeStatsTypeList;
  //    Q_FOREACH(StatisticsType aType, statsTypeList)
  //    {
  //      if (aType.render)
  //      {
  //        QVariantMap statsTypeParams;

  //        statsTypeParams["typeID"] = aType.typeID;
  //        statsTypeParams["typeName"] = aType.typeName;
  //        statsTypeParams["drawGrid"] = aType.renderGrid;
  //        statsTypeParams["alpha"] = aType.alphaFactor;

  //        activeStatsTypeList.append(statsTypeParams);
  //      }
  //    }

  //    if (activeStatsTypeList.count() > 0)
  //      itemProps["typesChecked"] = activeStatsTypeList;
  //  }
  //  else
  //  {
  //    continue;
  //  }

  //  itemInfo["Properties"] = itemProps;

  //  itemList.append(itemInfo);
  //}

  //// generate plist from item list
  //QVariantMap plistMap;
  //plistMap["Modules"] = itemList;

  //QString plistFileContents = PListSerializer::toPList(plistMap);

  //QFile file(filename);
  //file.open(QIODevice::WriteOnly | QIODevice::Text);
  //QTextStream outStream(&file);
  //outStream << plistFileContents;
  //file.close();
  //p_playlistWidget->setIsSaved(true);
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

void MainWindow::addTextFrame()
{

  /*EditTextDialog newTextObjectDialog;
  int done = newTextObjectDialog.exec();
  if (done == QDialog::Accepted)
  {
    PlaylistItem* newPlayListItemText = new PlaylistItem(PlaylistItem_Text, newTextObjectDialog.getText(), p_playlistWidget);
    QSharedPointer<TextObject> newText = newPlayListItemText->getTextObject();
    newText->setFont(newTextObjectDialog.getFont());
    newText->setDuration(newTextObjectDialog.getDuration());
    newText->setColor(newTextObjectDialog.getColor());
    p_playlistWidget->setCurrentItem(newPlayListItemText, 0, QItemSelectionModel::ClearAndSelect);
    p_playlistWidget->setIsSaved(false);
  }*/
}

void MainWindow::addDifferenceSequence()
{
  /*PlaylistItem* newPlayListItemDiff = new PlaylistItem(PlaylistItem_Difference, "Difference", p_playlistWidget);

  QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();

  for (int i = 0; i < MIN(selectedItems.count(), 2); i++)
  {
    QTreeWidgetItem* item = selectedItems[i];

    int index = p_playlistWidget->indexOfTopLevelItem(item);
    if (index != INT_INVALID)
    {
      item = p_playlistWidget->takeTopLevelItem(index);
      newPlayListItemDiff->addChild(item);
      newPlayListItemDiff->setExpanded(true);

    }
  }

  p_playlistWidget->setCurrentItem(newPlayListItemDiff, 0, QItemSelectionModel::ClearAndSelect);
  p_playlistWidget->setIsSaved(false);
  updateSelectedItems();*/
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
  playlistItem* selectedItemSecondary = selectedSecondaryPlaylistItem();

  if (selectedItemPrimary == NULL)
  {
    // Nothing is selected
    setWindowTitle("YUView");

  //      ui->fileDockWidget->setEnabled(false);
  //      ui->displayDockWidget->setEnabled(true);
  //      ui->YUVMathdockWidget->setEnabled(false);
  //      ui->statsDockWidget->setEnabled(false);
  //      ui->displaySplitView->setActiveDisplayObjects(QSharedPointer<DisplayObject>(), QSharedPointer<DisplayObject>());
  //      //ui->displaySplitView->setActiveStatisticsObjects(QSharedPointer<StatisticsObject>(), QSharedPointer<StatisticsObject>());

  //  // update model
  //  dynamic_cast<StatsListModel*>(ui->statsListView->model())->setStatisticsTypeList(StatisticsTypeList());

  //  setCurrentFrame(0);
  //  setControlsEnabled(false);

    return;
  }

  // Update the objects signal that something changed in the background
  //if (selectedItemPrimary->displayObject() != previouslySelectedDisplayObject) 
  //{
  //  // New item was selected
  //  if (previouslySelectedDisplayObject != NULL) 
  //    // Disconnect old playlist Item
  //    QObject::disconnect(previouslySelectedDisplayObject.data(), SIGNAL(signal_objectInformationChanged()), NULL, NULL);
  //  // Update last object
  //  previouslySelectedDisplayObject = selectedItemPrimary->displayObject();
  //  // Connect new object
  //  QObject::connect(previouslySelectedDisplayObject.data(), SIGNAL(signal_objectInformationChanged()), this, SLOT(currentSelectionInformationChanged()));
  //}

  // update window caption
  QString newCaption = "YUView - " + selectedItemPrimary->text(0);
  setWindowTitle(newCaption);
  
  //QSharedPointer<StatisticsObject> statsObject;    // used for model as source

  //// if the newly selected primary (!) item is of type statistics, use it as source for types
  //if (selectedItemPrimary && selectedItemPvnrimary->itemType() == PlaylistItem_Statistics)
  //{
  //  statsObject = selectedItemPrimary->getStatisticsObject();
  //  Q_ASSERT(!statsObject.isNull());
  //}
  //else if (selectedItemSecondary && selectedItemSecondary->itemType() == PlaylistItem_Statistics)
  //{
  //  statsObject = selectedItemSecondary->getStatisticsObject();
  //  Q_ASSERT(!statsObject.isNull());
  //}

  //// if selected item is of type 'diff', update child items
  //if (selectedItemPrimary && selectedItemPrimary->itemType() == PlaylistItem_Difference && selectedItemPrimary->childCount() == 2)
  //{
  //  QSharedPointer<DifferenceObject> diffObject = selectedItemPrimary->getDifferenceObject();
  //  PlaylistItem* firstChild = dynamic_cast<PlaylistItem*>(selectedItemPrimary->child(0));
  //  PlaylistItem* secondChild = dynamic_cast<PlaylistItem*>(selectedItemPrimary->child(1));
  //  QSharedPointer<FrameObject> firstVidObject = firstChild->getFrameObject();
  //  QSharedPointer<FrameObject> secondVidObject = secondChild->getFrameObject();

  //  //TO-Do: Implement this as a different function
  //  if (firstVidObject && secondVidObject)
  //    diffObject->setFrameObjects(firstVidObject, secondVidObject);
  //  bool isChecked = ui->markDifferenceCheckBox->isChecked();
  //  QSettings settings;
  //  QColor color = settings.value("Difference/Color").value<QColor>();
  //  bool diff = diffObject->markDifferences(isChecked, color);
  //  if (isChecked)
  //  {
  //    if (diff)
  //    {
  //      ui->differenceLabel->setVisible(true);
  //      ui->differenceLabel->setText("There are differences in the pixels");
  //    }
  //    else
  //    {
  //      ui->differenceLabel->setVisible(true);
  //      ui->differenceLabel->setText("There is no difference");
  //    }
  //  }
  //  else
  //    ui->differenceLabel->setVisible(false);
  //}

  //// check for associated statistics
  //QSharedPointer<StatisticsObject> statsItemPrimary;
  //QSharedPointer<StatisticsObject> statsItemSecondary;
  //if (selectedItemPrimary && selectedItemPrimary->itemType() == PlaylistItem_Video && selectedItemPrimary->childCount() > 0)
  //{
  //  PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedItemPrimary->child(0));
  //  statsItemPrimary = childItem->getStatisticsObject();

  //  Q_ASSERT(statsItemPrimary != NULL);

  //  if (statsObject == NULL)
  //    statsObject = statsItemPrimary;
  //}
  //if (selectedItemSecondary && selectedItemSecondary->itemType() == PlaylistItem_Video && selectedItemSecondary->childCount() > 0)
  //{
  //  PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedItemSecondary->child(0));
  //  statsItemSecondary = childItem->getStatisticsObject();

  //  Q_ASSERT(statsItemSecondary != NULL);

  //  if (statsObject == NULL)
  //    statsObject = statsItemSecondary;
  //}

  //  // update statistics mode, if statistics is selected or associated with a selected item
  //  if (statsObject != NULL)
  //      dynamic_cast<StatsListModel*>(ui->statsListView->model())->setStatisticsTypeList(statsObject->getStatisticsTypeList());

  //// update display widget
  ////ui->displaySplitView->setActiveStatisticsObjects(statsItemPrimary, statsItemSecondary);

  //if (selectedItemPrimary == NULL || selectedItemPrimary->displayObject() == NULL)
  //  return;

  //// tell our display widget about new objects
  //ui->displaySplitView->setActiveDisplayObjects(
  //  selectedItemPrimary ? selectedItemPrimary->displayObject() : QSharedPointer<DisplayObject>(),
  //  selectedItemSecondary ? selectedItemSecondary->displayObject() : QSharedPointer<DisplayObject>());

  //// update playback controls
  //setControlsEnabled(true);
  //ui->fileDockWidget->setEnabled(selectedItemPrimary->itemType() != PlaylistItem_Text);
  //ui->displayDockWidget->setEnabled(true);
  //ui->YUVMathdockWidget->setEnabled(selectedItemPrimary->itemType() == PlaylistItem_Video || selectedItemPrimary->itemType() == PlaylistItem_Difference);
  //ui->statsDockWidget->setEnabled(statsObject != NULL);

  //// update displayed information
  //updateSelectionMetaInfo();

  //// update playback widgets
  //refreshPlaybackWidgets();
}

void MainWindow::onItemDoubleClicked(QTreeWidgetItem* item, int)
{
  //PlaylistItem* listItem = dynamic_cast<PlaylistItem*>(item);

  //if (listItem->itemType() == PlaylistItem_Statistics)
  //{
  //  // TODO: Double Click Behavior for Stats
  //}
  //if (listItem->itemType() == PlaylistItem_Video)
  //{
  //  // TODO: Double Click Behavior for Video
  //}
  //if (listItem->itemType() == PlaylistItem_Text)
  //{
  //  editTextFrame();
  //}
  //if (listItem->itemType() == PlaylistItem_Difference)
  //{
  //  // TODO: Double Click Behavior for difference
  //}
}

void MainWindow::editTextFrame()
{
  /*PlaylistItem* current = dynamic_cast<PlaylistItem*>(p_playlistWidget->currentItem());
  if (current->itemType() != PlaylistItem_Text)
    return;

  QSharedPointer<TextObject> text = current->getTextObject();

  EditTextDialog newTextObjectDialog;

  newTextObjectDialog.loadItemStettings(text);
  int done = newTextObjectDialog.exec();
  if (done == QDialog::Accepted)
  {
    text->setText(newTextObjectDialog.getText());
    text->setFont(newTextObjectDialog.getFont());
    text->setDuration(newTextObjectDialog.getDuration());
    text->setColor(newTextObjectDialog.getColor());
    current->setText(0, newTextObjectDialog.getText().replace("\n", " "));
    p_playlistWidget->setIsSaved(false);
    updateSelectedItems();
  }*/
}

/** Called when the user selects a new statistic
  * statsListView->clicked() is connected to this slot
  */
void MainWindow::setSelectedStats() 
{
  //deactivate all GUI elements
  ui->opacityGroupBox->setEnabled(false);
  ui->opacitySlider->setEnabled(false);
  ui->gridCheckBox->setEnabled(false);

  QModelIndexList list = ui->statsListView->selectionModel()->selectedIndexes();
  if (list.size() < 1)
  {
    statsTypesChanged();
    return;
  }

  // update GUI
  ui->opacitySlider->setValue(dynamic_cast<StatsListModel*>(ui->statsListView->model())->data(list.at(0), Qt::UserRole + 1).toInt());
  ui->gridCheckBox->setChecked(dynamic_cast<StatsListModel*>(ui->statsListView->model())->data(list.at(0), Qt::UserRole + 2).toBool());

  ui->opacitySlider->setEnabled(true);
  ui->gridCheckBox->setEnabled(true);
  ui->opacityGroupBox->setEnabled(true);

  statsTypesChanged();
}

/* Update the selected statitics item's opacity.
 * The signal opacitySlider->valueChanged is connected to this slot
 */
void MainWindow::updateStatsOpacity(int val) 
{
  QModelIndexList list = ui->statsListView->selectionModel()->selectedIndexes();
  if (list.size() < 1)
    return;
  dynamic_cast<StatsListModel*>(ui->statsListView->model())->setData(list.at(0), val, Qt::UserRole + 1);
}

/* Update the selected statistics item's option to draw a grid.
 * The signal gridCheckbox->toggle is connected to this slot.
 */
void MainWindow::updateStatsGrid(bool val)
{
  QModelIndexList list = ui->statsListView->selectionModel()->selectedIndexes();
  if (list.size() < 1)
    return;
  dynamic_cast<StatsListModel*>(ui->statsListView->model())->setData(list.at(0), val, Qt::UserRole + 2);
}

/* Set the current frame to show.
 * The signal frameSLider->valueChanged is connected to this slot.
 * The signal frameCounterSpinBox->valueChanged is connected to this slot.
 */
void MainWindow::setCurrentFrame(int frame, bool bForceRefresh)
{
  ////qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::setCurrentFrame()";

  //if (selectedPrimaryPlaylistItem() == NULL || selectedPrimaryPlaylistItem()->displayObject() == NULL)
  //{
  //  p_currentFrame = 0;
  //  ui->displaySplitView->clear();
  //  return;
  //}

  //if (frame != p_currentFrame || bForceRefresh)
  //{
  //  // get real frame index
  //  int objEndFrame = selectedPrimaryPlaylistItem()->displayObject()->endFrame();
  //  if (frame < selectedPrimaryPlaylistItem()->displayObject()->startFrame())
  //    frame = selectedPrimaryPlaylistItem()->displayObject()->startFrame();
  //  else if (objEndFrame >= 0 && frame > objEndFrame)
  //    // Don't change the frame.
  //    return;	// TODO: Shouldn't this set it to the last frame?

  //  p_currentFrame = frame;

  //  // update frame index in GUI without toggeling more signals
  //  updateFrameControls();

  //  // draw new frame
  //  ui->displaySplitView->drawFrame(p_currentFrame);
  //}
}

/* The information of the currently selected item changed in the background.
 * We need to update the metadata of the selected item.
*/
void MainWindow::currentSelectionInformationChanged()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::currentSelectionInformationChanged()";
  
  // Refresh the playback widget
  refreshPlaybackWidgets();
}

void MainWindow::refreshPlaybackWidgets()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::refreshPlaybackWidgets()";

  //// don't do anything if not yet initialized
  //if (selectedPrimaryPlaylistItem() == NULL)
  //    return;

  //// update information about newly selected video

  //// update our timer
  //int newInterval = 1000.0 / selectedPrimaryPlaylistItem()->frameRate();
  //if (p_timerRunning && newInterval != p_timerInterval) 
  //{
  //  // The timer interval changed while the timer is running. Update it.
  //  killTimer(p_timerId);
  //  p_timerInterval = newInterval;
  //  p_timerId = startTimer(p_timerInterval, Qt::PreciseTimer);
  //}

  //int minFrameIdx, maxFrameIdx;
  //selectedPrimaryPlaylistItem()->displayObject()->frameIndexLimits(minFrameIdx, maxFrameIdx);
  //
  //ui->frameSlider->setMinimum( minFrameIdx );
  //ui->frameSlider->setMaximum( maxFrameIdx );

  //int objEndFrame = selectedPrimaryPlaylistItem()->displayObject()->endFrame();
  //if (ui->endSpinBox->value() != objEndFrame)
  //  ui->endSpinBox->setValue(objEndFrame);

  //int modifiedFrame = p_currentFrame;

  //if (p_currentFrame < minFrameIdx)
  //  modifiedFrame = minFrameIdx;
  //else if (p_currentFrame > maxFrameIdx)
  //  modifiedFrame = maxFrameIdx;

  //// make sure that changed info is resembled in display frame - might be due to changes to playback range
  //setCurrentFrame(modifiedFrame, true);
}

/* Toggle play/pause
 * The signal playButton->clicked is connected to this slot.
 */
void MainWindow::togglePlayback()
{
  /*if (p_timerRunning)
    pause();
  else 
  {
    if (selectedPrimaryPlaylistItem()) 
    {
      int minIdx, maxIdx;
      selectedPrimaryPlaylistItem()->displayObject()->frameIndexLimits(minIdx, maxIdx);
      if (p_currentFrame >= maxIdx && p_repeatMode == RepeatModeOff)
        setCurrentFrame(minIdx);
      play();
    }
  }*/
}

void MainWindow::deleteItem()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::deleteItem()";

  // stop playback first
  ui->playbackController->stop();

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

void MainWindow::selectNextItem()
{
  QTreeWidgetItem* selectedItem = selectedPrimaryPlaylistItem();
  if (selectedItem != NULL && p_playlistWidget->itemBelow(selectedItem) != NULL)
    p_playlistWidget->setCurrentItem(p_playlistWidget->itemBelow(selectedItem));
}

void MainWindow::selectPreviousItem()
{
  QTreeWidgetItem* selectedItem = selectedPrimaryPlaylistItem();
  if (selectedItem != NULL && p_playlistWidget->itemAbove(selectedItem) != NULL)
    p_playlistWidget->setCurrentItem(p_playlistWidget->itemAbove(selectedItem));
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
  // more keyboard shortcuts can be implemented here...
  switch (event->key())
  {
    case Qt::Key_Escape:
    {
      if (isFullScreen())
        toggleFullscreen();
      break;
    }
    case Qt::Key_F:
    {
      if (event->modifiers() == Qt::ControlModifier)
        toggleFullscreen();
      break;
    }
    case Qt::Key_1:
    {
      if (event->modifiers() == Qt::ControlModifier)
        enableSingleWindowMode();
      break;
    }
    case Qt::Key_2:
    {
      if (event->modifiers() == Qt::ControlModifier)
        enableSeparateWindowsMode();
      break;
    }
    case Qt::Key_Space:
    {
      togglePlayback();
      break;
    }
    case Qt::Key_Left:
    {
      previousFrame();
      break;
    }
    case Qt::Key_Right:
    {
      nextFrame();
      break;
    }
    case Qt::Key_Up:
    {
      selectPreviousItem();
      break;
    }
    case Qt::Key_Down:
    {
      selectNextItem();
      break;
    }
    /*case Qt::Key_Plus:
    {
      ui->displaySplitView->zoomIn();
      break;
    }
    case Qt::Key_Minus:
    {
      ui->displaySplitView->zoomOut();
      break;
    }
    case Qt::Key_0:
    {
      ui->displaySplitView->zoomToStandard();
      break;
    }
    case Qt::Key_9:
    {
      ui->displaySplitView->zoomToFit();
      break;
    }*/
  }
}

void MainWindow::toggleFullscreen()
{
  QSettings settings;

  if (isFullScreen())
  {
    // show panels
    ui->playlistDockWidget->show();
    ui->statsDockWidget->show();
    ui->displayDockWidget->show();
    ui->playbackController->show();
    ui->fileInfoDockWidget->show();

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
      ui->fileInfoDockWidget->hide();
      ui->playlistDockWidget->hide();
      ui->statsDockWidget->hide();
      ui->displayDockWidget->hide();
    }
#ifndef QT_OS_MAC
    // hide menu
    ui->menuBar->hide();
#endif
    // always hide playback controls in full screen mode
    ui->playbackController->hide();

    ui->displaySplitView->showFullScreen();

    showFullScreen();
  }
  ui->displaySplitView->resetViews();
}

/* The playback timer has fired. Update the frame counter.
 * Also update the frame rate label every second.
 */
void MainWindow::timerEvent(QTimerEvent * event)
{
  //if (event->timerId() != p_timerId)
  //  return;
  ////qDebug() << "Other timer event!!";

  //if (!isPlaylistItemSelected())
  //  return stop();

  //// if we reached the end of a sequence, react...
  //int nextFrame = p_currentFrame + selectedPrimaryPlaylistItem()->displayObject()->sampling();
  //int minIdx, maxIdx;
  //selectedPrimaryPlaylistItem()->displayObject()->frameIndexLimits(minIdx, maxIdx);
  //if (nextFrame > maxIdx)
  //{
  //  switch (p_repeatMode)
  //  {
  //    case RepeatModeOff:
  //      pause();
  //      if (p_ClearFrame)
  //        ui->displaySplitView->drawFrame(INT_INVALID);
  //      break;
  //    case RepeatModeOne:
  //      setCurrentFrame(minIdx);
  //      break;
  //    case RepeatModeAll:
  //      // get next item in list
  //      QModelIndex curIdx = p_playlistWidget->indexForItem(selectedPrimaryPlaylistItem());
  //      int rowIdx = curIdx.row();
  //      PlaylistItem* nextItem = NULL;
  //      if (rowIdx == p_playlistWidget->topLevelItemCount() - 1)
  //        nextItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(0));
  //      else
  //        nextItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(rowIdx + 1));

  //      p_playlistWidget->setCurrentItem(nextItem, 0, QItemSelectionModel::ClearAndSelect);
  //      setCurrentFrame(nextItem->displayObject()->startFrame());
  //  }
  //}
  //else
  //{
  //  // update current frame
  //  setCurrentFrame(nextFrame);
  //}

  //// FPS counter. Every 50th call of this function update the FPS counter.
  //p_timerFPSCounter++;
  //if (p_timerFPSCounter > 50) 
  //{
  //  QTime newFrameTime = QTime::currentTime();
  //  float msecsSinceLastUpdate = (float)p_timerLastFPSTime.msecsTo(newFrameTime);

  //  int framesPerSec = (int)(50 / (msecsSinceLastUpdate / 1000.0));
  //  if (framesPerSec > 0)
  //    ui->frameRateLabel->setText(QString().setNum(framesPerSec));

  //  p_timerLastFPSTime = QTime::currentTime();
  //  p_timerFPSCounter = 0;
  //}
}

/////////////////////////////////////////////////////////////////////////////////

void MainWindow::convertFrameSizeComboBoxIndexToSize(int *width, int*height)
{
  //int index = ui->framesizeComboBox->currentIndex();
  //
  //if (index <= 0 || (index-1) >= presetFrameSizesList().size()) {
  //  // "Custom Size" or non valid index
  //  *width = -1;
  //  *height = -1;
  //  return;
  //}

  //// Convert the index to width/height using the presetFrameSizesList
  //frameSizePreset p = presetFrameSizesList().at(index - 1);
  //*width = p.second.width();
  //*height = p.second.height();
}

void MainWindow::statsTypesChanged()
{
  ////qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::statsTypesChanged()";

  //// update all displayed statistics of primary item
  //bool bUpdateNeeded = false;
  //if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == PlaylistItem_Statistics)
  //{
  //  QSharedPointer<StatisticsObject> statsItem = selectedPrimaryPlaylistItem()->getStatisticsObject();
  //  Q_ASSERT(statsItem);

  //  // update list of activated types
  //  bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
  //}
  //else if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == PlaylistItem_Video && selectedPrimaryPlaylistItem()->childCount() > 0)
  //{
  //  PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedPrimaryPlaylistItem()->child(0));
  //  Q_ASSERT(childItem->itemType() == PlaylistItem_Statistics);

  //  // update list of activated types
  //  QSharedPointer<StatisticsObject> statsItem = childItem->getStatisticsObject();
  //  bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
  //}

  //// update all displayed statistics of secondary item
  //if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == PlaylistItem_Statistics)
  //{
  //  QSharedPointer<StatisticsObject> statsItem = selectedSecondaryPlaylistItem()->getStatisticsObject();
  //  Q_ASSERT(statsItem);

  //  // update list of activated types
  //  bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
  //}
  //else if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == PlaylistItem_Video && selectedSecondaryPlaylistItem()->childCount() > 0)
  //{
  //  PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedSecondaryPlaylistItem()->child(0));
  //  Q_ASSERT(childItem->itemType() == PlaylistItem_Statistics);

  //  // update list of activated types
  //  QSharedPointer<StatisticsObject> statsItem = childItem->getStatisticsObject();
  //  bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
  //}

  //if (bUpdateNeeded) 
  //{
  //  // refresh display widget
  //  ui->displaySplitView->drawFrame(p_currentFrame);
  //}
}

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

QString MainWindow::strippedName(const QString &fullFileName)
{
  return QFileInfo(fullFileName).fileName();
}

void MainWindow::on_viewComboBox_currentIndexChanged(int index)
{
  switch (index)
  {
    case 0: // SIDE_BY_SIDE
      ui->displaySplitView->setViewMode(SIDE_BY_SIDE);
      break;
    case 1: // COMPARISON
      ui->displaySplitView->setViewMode(COMPARISON);
      break;
  }
}

void MainWindow::on_zoomBoxCheckBox_toggled(bool checked)
{
  //ui->displaySplitView->setZoomBoxEnabled(checked);
}

void MainWindow::on_SplitViewgroupBox_toggled(bool checkState)
{
  ui->displaySplitView->setSplitEnabled(checkState);
  ui->SplitViewgroupBox->setChecked(checkState);
  ui->displaySplitView->setViewMode(SIDE_BY_SIDE);
  ui->viewComboBox->setCurrentIndex(0);
  QSettings settings;
  settings.setValue("SplitViewEnabled", checkState);
  ui->displaySplitView->resetViews();
}

void MainWindow::enableSeparateWindowsMode()
{
  // if we are in fullscreen, get back to windowed mode
  if (isFullScreen())
    this->toggleFullscreen();

  // show inspector window with default dockables
  p_inspectorWindow.hide();
  ui->fileInfoDockWidget->show();
  p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->fileInfoDockWidget);
  ui->displayDockWidget->show();
  p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->displayDockWidget);
  p_inspectorWindow.show();

  // show playlist with default dockables
  p_playlistWindow.hide();
  ui->playlistDockWidget->show();
  p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->playlistDockWidget);
  ui->statsDockWidget->show();
  p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->statsDockWidget);
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
  ui->fileInfoDockWidget->show();
  this->addDockWidget(Qt::RightDockWidgetArea, ui->fileInfoDockWidget);
  ui->displayDockWidget->show();
  this->addDockWidget(Qt::RightDockWidgetArea, ui->displayDockWidget);
  
  // hide playlist window and move dockables to main window
  p_playlistWindow.hide();
  ui->playlistDockWidget->show();
  this->addDockWidget(Qt::LeftDockWidgetArea, ui->playlistDockWidget);
  ui->statsDockWidget->show();
  this->addDockWidget(Qt::LeftDockWidgetArea, ui->statsDockWidget);
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