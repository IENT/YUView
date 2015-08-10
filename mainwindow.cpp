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
#include "displaywidget.h"
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
#include "playlistitemvid.h"
#include "playlistitemstats.h"
#include "playlistitemtext.h"
#include "playlistitemdifference.h"
#include "statslistmodel.h"
#include "displaysplitwidget.h"
#include "plistparser.h"
#include "plistserializer.h"

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    QSettings settings;

    // set some defaults
    if(!settings.contains("Background/Color"))
        settings.setValue("Background/Color", QColor(128,128,128));
    if(!settings.contains("OverlayGrid/Color"))
        settings.setValue("OverlayGrid/Color", QColor(0,0,0));

    p_playlistWidget = NULL;
    ui->setupUi(this);

    setFocusPolicy(Qt::StrongFocus);

    statusBar()->hide();
    p_inspectorWindow.setWindowTitle("Inspector");
    p_playlistWindow.setWindowTitle("Playlist");
    p_playlistWindow.setGeometry(0,0,300,600);


    // load window mode from preferences
    p_windowMode = (WindowMode)settings.value("windowMode").toInt();
    switch (p_windowMode) {
    case WindowModeSingle:
         enableSingleWindowMode();
        break;
    case WindowModeSeparate:
         enableSeparateWindowsMode();
        break;
    }

    p_playlistWidget = ui->playlistTreeWidget;
    //connect(p_playlistWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(treeItemDoubleClicked(QTreeWidgetItem*, int)));
    p_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(p_playlistWidget, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
    connect(p_playlistWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onItemDoubleClicked(QTreeWidgetItem*,int)));
    p_playTimer = new QTimer(this);
    QObject::connect(p_playTimer, SIGNAL(timeout()), this, SLOT(frameTimerEvent()));
    p_playTimer->setSingleShot(false);
#if QT_VERSION > 0x050000
    p_playTimer->setTimerType(Qt::PreciseTimer);
#endif

    ui->displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);
    p_heartbeatTimer = new QTimer(this);
    QObject::connect(p_heartbeatTimer, SIGNAL(timeout()), this, SLOT(heartbeatTimerEvent()));
    p_heartbeatTimer->setSingleShot(false);
    p_heartbeatTimer->start(1000);

    p_currentFrame = 0;

    p_playIcon = QIcon(":images/img_play.png");
    p_pauseIcon = QIcon(":images/img_pause.png");
    p_repeatOffIcon = QIcon(":images/img_repeat.png");
    p_repeatAllIcon = QIcon(":images/img_repeat_on.png");
    p_repeatOneIcon = QIcon(":images/img_repeat_one.png");

    setRepeatMode((RepeatMode)settings.value("RepeatMode", RepeatModeOff).toUInt());   // load parameter from user preferences
    if (!settings.value("SplitViewEnabled",true).toBool())
        on_SplitViewgroupBox_toggled(false);
    // populate combo box for pixel formats
    ui->pixelFormatComboBox->clear();
    for (unsigned int i=0; i<YUVFile::pixelFormatList().size(); i++)
    {
        YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)i;
        if( pixelFormat != YUVC_UnknownPixelFormat && YUVFile::pixelFormatList().count(pixelFormat) )
        {
            ui->pixelFormatComboBox->addItem(YUVFile::pixelFormatList().at(pixelFormat).name());
        }
    }

    createMenusAndActions();

    StatsListModel *model= new StatsListModel(this);
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
    ui->DifferencegroupBox->setEnabled(false);
    ui->DifferencegroupBox->setHidden(true);
    ui->differenceLabel->setVisible(false);
    QObject::connect(&p_settingswindow, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
    QObject::connect(p_playlistWidget,SIGNAL(playListKey(QKeyEvent*)),this,SLOT(handleKeyPress(QKeyEvent*)));

    updateSettings();

    updateSelectedItems();
}

void MainWindow::createMenusAndActions()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    openYUVFileAction = fileMenu->addAction("&Open File...", this, SLOT(openFile()),Qt::CTRL + Qt::Key_O);
    addTextAction = fileMenu->addAction("&Add Text Frame",this,SLOT(addTextFrame()));
    addDifferenceAction = fileMenu->addAction("&Add Difference Sequence",this,SLOT(addDifferenceSequence()));
    fileMenu->addSeparator();
    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActs[i] = new QAction(this);
        recentFileActs[i]->setVisible(false);
        connect(recentFileActs[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
        fileMenu->addAction(recentFileActs[i]);
    }
    fileMenu->addSeparator();
    deleteItemAction = fileMenu->addAction("&Delete Item",this,SLOT(deleteItem()), Qt::Key_Delete);
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
    zoomToStandardAction = viewMenu->addAction("Zoom to 1:1", ui->displaySplitView, SLOT(zoomToStandard()), Qt::CTRL + Qt::Key_0);
    zoomToFitAction = viewMenu->addAction("Zoom to Fit", ui->displaySplitView, SLOT(zoomToFit()), Qt::CTRL + Qt::Key_9);
    zoomInAction = viewMenu->addAction("Zoom in", ui->displaySplitView, SLOT(zoomIn()), Qt::CTRL + Qt::Key_Plus);
    zoomOutAction = viewMenu->addAction("Zoom out", ui->displaySplitView, SLOT(zoomOut()), Qt::CTRL + Qt::Key_Minus);
    viewMenu->addSeparator();
    togglePlaylistAction = viewMenu->addAction("Hide/Show P&laylist", ui->playlistDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_L);
    toggleStatisticsAction = viewMenu->addAction("Hide/Show &Statistics", ui->statsDockWidget->toggleViewAction(), SLOT(trigger()));
    viewMenu->addSeparator();
    toggleFileOptionsAction = viewMenu->addAction("Hide/Show F&ile Options", ui->fileDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_I);
    toggleDisplayOptionsActions = viewMenu->addAction("Hide/Show &Display Options", ui->displayDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_D);
    toggleYUVMathActions = viewMenu->addAction("Hide/Show &YUV Options", ui->YUVMathdockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_Y);
    viewMenu->addSeparator();
    toggleControlsAction = viewMenu->addAction("Hide/Show Playback &Controls", ui->controlsDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_P);
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

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    int fileIdx = 1;
    for (int i = 0; i < numRecentFiles; ++i)
    {
        if( !(QFile(files[i]).exists()) )
            continue;

        QString text = tr("&%1 %2").arg(fileIdx++).arg(strippedName(files[i]));
        recentFileActs[i]->setText(text);
        recentFileActs[i]->setData(files[i]);
        recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
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
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, "Quit YUView",
                                                                       tr("You have not saved the current playlist, are you sure?\n"),
                                                                       QMessageBox::Cancel | QMessageBox::Close | QMessageBox::Save,
                                                                       QMessageBox::Close);
           if (resBtn == QMessageBox::Cancel)
           {
               event->ignore();
               return;
           }
           else if (resBtn==QMessageBox::Close) {
               event->accept();
           }
           else {
               savePlaylistToFile();
           }

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
    // clear playlist first
    p_playlistWidget->clear();

    // parse plist structure of playlist file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray fileBytes = file.readAll();
    QBuffer buffer(&fileBytes);
    QVariantMap map = PListParser::parsePList(&buffer).toMap();

    QVariantList itemList = map["Modules"].toList();
    for(int i=0; i<itemList.count(); i++)
    {
        QVariantMap itemInfo = itemList[i].toMap();
        QVariantMap itemProps = itemInfo["Properties"].toMap();

        if(itemInfo["Class"].toString() == "TextFrameProvider")
        {
            float duration = itemProps["duration"].toFloat();
            QString fontName = itemProps["fontName"].toString();
            int fontSize = itemProps["fontSize"].toInt();
            QColor color = QColor(itemProps["fontColor"].toString());
            QFont font(fontName,fontSize);
            QString text = itemProps["text"].toString();

            // create text item and set properties
            PlaylistItemText* newPlayListItemText = new PlaylistItemText(text, p_playlistWidget);
            newPlayListItemText->displayObject()->setFont(font);
            newPlayListItemText->displayObject()->setDuration(duration);
            newPlayListItemText->displayObject()->setColor(color);
        }
        else if(itemInfo["Class"].toString() == "YUVFile")
        {
            QString fileURL = itemProps["URL"].toString();
            int endFrame = itemProps["endFrame"].toInt();
            int frameOffset = itemProps["frameOffset"].toInt();
            int frameSampling = itemProps["frameSampling"].toInt();
            float frameRate = itemProps["framerate"].toFloat();
            YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)itemProps["pixelFormat"].toInt();
            int height = itemProps["height"].toInt();
            int width = itemProps["width"].toInt();

            QString filePath = QUrl(fileURL).path();

            // create video item and set properties
            PlaylistItemVid *newListItemVid = new PlaylistItemVid(filePath, p_playlistWidget);
            newListItemVid->displayObject()->setWidth(width);
            newListItemVid->displayObject()->setHeight(height);
            newListItemVid->displayObject()->setSrcPixelFormat(pixelFormat);
            newListItemVid->displayObject()->setFrameRate(frameRate);
            newListItemVid->displayObject()->setSampling(frameSampling);
            newListItemVid->displayObject()->setStartFrame(frameOffset);
            newListItemVid->displayObject()->setEndFrame(endFrame);

            // load potentially associated statistics file
            if( itemProps.contains("statistics") )
            {
                QVariantMap itemInfoAssoc = itemProps["statistics"].toMap();
                QVariantMap itemPropsAssoc = itemInfoAssoc["Properties"].toMap();

                QString fileURL = itemPropsAssoc["URL"].toString();
                int endFrame = itemPropsAssoc["endFrame"].toInt();
                int frameOffset = itemPropsAssoc["frameOffset"].toInt();
                int frameSampling = itemPropsAssoc["frameSampling"].toInt();
                float frameRate = itemPropsAssoc["framerate"].toFloat();
                int height = itemPropsAssoc["height"].toInt();
                int width = itemPropsAssoc["width"].toInt();
                QVariantList activeStatsTypeList = itemPropsAssoc["typesChecked"].toList();

                QString filePath = QUrl(fileURL).path();

                // create associated statistics item and set properties
                PlaylistItemStats *newListItemStats = new PlaylistItemStats(filePath, newListItemVid);
                newListItemStats->displayObject()->setWidth(width);
                newListItemStats->displayObject()->setHeight(height);
                newListItemStats->displayObject()->setFrameRate(frameRate);
                newListItemStats->displayObject()->setSampling(frameSampling);
                newListItemStats->displayObject()->setStartFrame(frameOffset);
                newListItemStats->displayObject()->setEndFrame(endFrame);

                // set active statistics
                StatisticsTypeList statsTypeList;

                for(int i=0; i<activeStatsTypeList.count(); i++)
                {
                    QVariantMap statsTypeParams = activeStatsTypeList[i].toMap();

                    StatisticsType aType;
                    aType.typeID = statsTypeParams["typeID"].toInt();
                    aType.typeName = statsTypeParams["typeName"].toString();
                    aType.render = true;
                    aType.renderGrid = statsTypeParams["drawGrid"].toBool();
                    aType.alphaFactor = statsTypeParams["alpha"].toInt();

                    statsTypeList.append(aType);
                }

                if(statsTypeList.count() > 0)
                    newListItemStats->displayObject()->setStatisticsTypeList(statsTypeList);
            }
        }
        else if(itemInfo["Class"].toString() == "StatisticsFile")
        {
            QString fileURL = itemProps["URL"].toString();
            int endFrame = itemProps["endFrame"].toInt();
            int frameOffset = itemProps["frameOffset"].toInt();
            int frameSampling = itemProps["frameSampling"].toInt();
            float frameRate = itemProps["framerate"].toFloat();
            int height = itemProps["height"].toInt();
            int width = itemProps["width"].toInt();
            QVariantList activeStatsTypeList = itemProps["typesChecked"].toList();

            QString filePath = QUrl(fileURL).path();

            // create statistics item and set properties
            PlaylistItemStats *newListItemStats = new PlaylistItemStats(filePath, p_playlistWidget);
            newListItemStats->displayObject()->setWidth(width);
            newListItemStats->displayObject()->setHeight(height);
            newListItemStats->displayObject()->setFrameRate(frameRate);
            newListItemStats->displayObject()->setSampling(frameSampling);
            newListItemStats->displayObject()->setStartFrame(frameOffset);
            newListItemStats->displayObject()->setEndFrame(endFrame);

            // set active statistics
            StatisticsTypeList statsTypeList;

            for(int i=0; i<activeStatsTypeList.count(); i++)
            {
                QVariantMap statsTypeParams = activeStatsTypeList[i].toMap();

                StatisticsType aType;
                aType.typeID = statsTypeParams["typeID"].toInt();
                aType.typeName = statsTypeParams["typeName"].toString();
                aType.render = true;
                aType.renderGrid = statsTypeParams["drawGrid"].toBool();
                aType.alphaFactor = statsTypeParams["alpha"].toInt();

                statsTypeList.append(aType);
            }

            if(statsTypeList.count() > 0)
                newListItemStats->displayObject()->setStatisticsTypeList(statsTypeList);
        }
    }

    if( p_playlistWidget->topLevelItemCount() > 0 )
    {
        p_playlistWidget->setCurrentItem(p_playlistWidget->topLevelItem(0), 0, QItemSelectionModel::ClearAndSelect);
        // this is the first playlist that was loaded, therefore no saving needed
        p_playlistWidget->setIsSaved(true);
    }
}

void MainWindow::savePlaylistToFile()
{
    // ask user for file location
    QSettings settings;

    QString filename = QFileDialog::getSaveFileName(this, tr("Save Playlist"), settings.value("LastPlaylistPath").toString(), tr("Playlist Files (*.yuvplaylist)"));
    if(filename.isEmpty())
        return;
    if (!filename.endsWith(".yuvplaylist",Qt::CaseInsensitive))
            filename+=".yuvplaylist";

    // remember this directory for next time
    QString dirName = filename.section('/',0,-2);
    settings.setValue("LastPlaylistPath",dirName);

    QVariantList itemList;

    for(int i=0; i<p_playlistWidget->topLevelItemCount();i++)
    {
        PlaylistItem* anItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(i));

        QVariantMap itemInfo;
        QVariantMap itemProps;

        if( anItem->itemType() == VideoItemType )
        {
            PlaylistItemVid* vidItem = dynamic_cast<PlaylistItemVid*>(anItem);

            itemInfo["Class"] = "YUVFile";

            QUrl fileURL(vidItem->displayObject()->path());
            fileURL.setScheme("file");
            itemProps["URL"] = fileURL.toString();
            itemProps["endFrame"] = vidItem->displayObject()->endFrame();
            itemProps["frameOffset"] = vidItem->displayObject()->startFrame();
            itemProps["frameSampling"] = vidItem->displayObject()->sampling();
            itemProps["framerate"] = vidItem->displayObject()->frameRate();
            itemProps["pixelFormat"] = vidItem->displayObject()->pixelFormat();
            itemProps["height"] = vidItem->displayObject()->height();
            itemProps["width"] = vidItem->displayObject()->width();

            // store potentially associated statistics file
            if( vidItem->childCount() == 1 )
            {
                PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(vidItem->child(0));
                Q_ASSERT(statsItem);

                QVariantMap itemInfoAssoc;
                itemInfoAssoc["Class"] = "AssociatedStatisticsFile";

                QVariantMap itemPropsAssoc;
                QUrl fileURL(statsItem->displayObject()->path());
                fileURL.setScheme("file");
                itemPropsAssoc["URL"] = fileURL.toString();
                itemPropsAssoc["endFrame"] = statsItem->displayObject()->endFrame();
                itemPropsAssoc["frameOffset"] = statsItem->displayObject()->startFrame();
                itemPropsAssoc["frameSampling"] = statsItem->displayObject()->sampling();
                itemPropsAssoc["framerate"] = statsItem->displayObject()->frameRate();
                itemPropsAssoc["height"] = statsItem->displayObject()->height();
                itemPropsAssoc["width"] = statsItem->displayObject()->width();

                // save active statistics types
                StatisticsTypeList statsTypeList = statsItem->displayObject()->getStatisticsTypeList();

                QVariantList activeStatsTypeList;
                Q_FOREACH(StatisticsType aType, statsTypeList)
                {
                    if( aType.render )
                    {
                        QVariantMap statsTypeParams;

                        statsTypeParams["typeID"] = aType.typeID;
                        statsTypeParams["typeName"] = aType.typeName;
                        statsTypeParams["drawGrid"] = aType.renderGrid;
                        statsTypeParams["alpha"] = aType.alphaFactor;

                        activeStatsTypeList.append( statsTypeParams );
                    }
                }

                if( activeStatsTypeList.count() > 0 )
                    itemPropsAssoc["typesChecked"] = activeStatsTypeList;

                itemInfoAssoc["Properties"] = itemPropsAssoc;

                // link to video item
                itemProps["statistics"] = itemInfoAssoc;
            }
        }
        else if( anItem->itemType() == TextItemType )
        {
            PlaylistItemText* textItem = dynamic_cast<PlaylistItemText*>(anItem);

            itemInfo["Class"] = "TextFrameProvider";

            itemProps["duration"] = textItem->displayObject()->duration();
            itemProps["fontSize"] = textItem->displayObject()->font().pointSize();
            itemProps["fontName"] = textItem->displayObject()->font().family();
            itemProps["fontColor"]= textItem->displayObject()->color().name();
            itemProps["text"] = textItem->displayObject()->text();
        }
        else if( anItem->itemType() == StatisticsItemType )
        {
            PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(anItem);

            itemInfo["Class"] = "StatisticsFile";

            QUrl fileURL(statsItem->displayObject()->path());
            fileURL.setScheme("file");
            itemProps["URL"] = fileURL.toString();
            itemProps["endFrame"] = statsItem->displayObject()->endFrame();
            itemProps["frameOffset"] = statsItem->displayObject()->startFrame();
            itemProps["frameSampling"] = statsItem->displayObject()->sampling();
            itemProps["framerate"] = statsItem->displayObject()->frameRate();
            itemProps["height"] = statsItem->displayObject()->height();
            itemProps["width"] = statsItem->displayObject()->width();

            // save active statistics types
            StatisticsTypeList statsTypeList = statsItem->displayObject()->getStatisticsTypeList();

            QVariantList activeStatsTypeList;
            Q_FOREACH(StatisticsType aType, statsTypeList)
            {
                if( aType.render )
                {
                    QVariantMap statsTypeParams;

                    statsTypeParams["typeID"] = aType.typeID;
                    statsTypeParams["typeName"] = aType.typeName;
                    statsTypeParams["drawGrid"] = aType.renderGrid;
                    statsTypeParams["alpha"] = aType.alphaFactor;

                    activeStatsTypeList.append( statsTypeParams );
                }
            }

            if( activeStatsTypeList.count() > 0 )
                itemProps["typesChecked"] = activeStatsTypeList;
        }
        else
        {
            continue;
        }

        itemInfo["Properties"] = itemProps;

        itemList.append(itemInfo);
    }

    // generate plist from item list
    QVariantMap plistMap;
    plistMap["Modules"] = itemList;

    QString plistFileContents = PListSerializer::toPList(plistMap);

    QFile file( filename );
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream outStream(&file);
    outStream << plistFileContents;
    file.close();
    p_playlistWidget->setIsSaved(true);
}

void MainWindow::loadFiles(QStringList files)
{
    QStringList filter;

    // this might be used to associate a statistics item with a video item
    PlaylistItem* lastAddedItem = NULL;

    QStringList::Iterator it = files.begin();
    while(it != files.end())
    {
        QString fileName = *it;

        if( !(QFile(fileName).exists()) )
        {
            ++it;
            continue;
        }

        QFileInfo fi(fileName);

        if (fi.isDir())
        {
            QDir dir = QDir(*it);
            filter.clear();
            filter << "*.yuv";
            QStringList dirFiles = dir.entryList(filter);

            QStringList::const_iterator dirIt = dirFiles.begin();

            QStringList filePathList;
            while(dirIt != dirFiles.end())
            {
                filePathList.append( (*it) + "/" + (*dirIt) );

                // next file
                ++dirIt;
            }
        }
        else
        {
            QString ext = fi.suffix();
            ext = ext.toLower();
            // we have loaded a file, assume we have to save it later
            p_playlistWidget->setIsSaved(false);

            if( ext == "yuv" )
            {
                PlaylistItemVid *newListItemVid = new PlaylistItemVid(fileName, p_playlistWidget);
                lastAddedItem = newListItemVid;

                // save as recent
                QSettings settings;
                QStringList files = settings.value("recentFileList").toStringList();
                files.removeAll(fileName);
                files.prepend(fileName);
                while (files.size() > MaxRecentFiles)
                    files.removeLast();

                settings.setValue("recentFileList", files);
                updateRecentFileActions();
            }
            else if( ext == "csv" )
            {
                PlaylistItemStats *newListItemStats = new PlaylistItemStats(fileName, p_playlistWidget);
                lastAddedItem = newListItemStats;

                // save as recent
                QSettings settings;
                QStringList files = settings.value("recentFileList").toStringList();
                files.removeAll(fileName);
                files.prepend(fileName);
                while (files.size() > MaxRecentFiles)
                    files.removeLast();

                settings.setValue("recentFileList", files);
                updateRecentFileActions();
            }
            else if( ext == "yuvplaylist" )
            {
                // we found a playlist: cancel here and load playlist as a whole
                loadPlaylistFile(fileName);

                // save as recent
                QSettings settings;
                QStringList files = settings.value("recentFileList").toStringList();
                files.removeAll(fileName);
                files.prepend(fileName);
                while (files.size() > MaxRecentFiles)
                    files.removeLast();

                settings.setValue("recentFileList", files);
                updateRecentFileActions();
                return;
            }
        }

        ++it;
    }

    // select last added item
    p_playlistWidget->setCurrentItem(lastAddedItem, 0, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::openFile()
{
    // load last used directory from QPreferences
    QSettings settings;
    QStringList filter;
    filter << "All Supported Files (*.yuv *.yuvplaylist *.csv)" << "Video Files (*.yuv)" << "Playlist Files (*.yuvplaylist)" << "Statistics Files (*.csv)";

    QFileDialog openDialog(this);
    openDialog.setDirectory(settings.value("lastFilePath").toString());
    openDialog.setFileMode(QFileDialog::ExistingFiles);
    openDialog.setNameFilters(filter);

    QStringList fileNames;
    if (openDialog.exec())
        fileNames = openDialog.selectedFiles();

    if(fileNames.count() > 0)
    {
        // save last used directory with QPreferences
        QString filePath = fileNames.at(0);
        filePath = filePath.section('/',0,-2);
        settings.setValue("lastFilePath",filePath);
    }

    loadFiles(fileNames);

    if(selectedPrimaryPlaylistItem())
    {
        updateFrameSizeComboBoxSelection();
        updatePixelFormatComboBoxSelection(selectedPrimaryPlaylistItem());

        // Commented: This does not work for multiple screens. In this case QDesktopWidget().availableGeometry()
        // returns the size of the primary screen and not the size of the screen the application is on.
        // Also when in full screen this should not be done.
        // In general resizing the application without user interaction seems like a bug.

        //// resize player window to fit video size
        //QRect screenRect = QDesktopWidget().availableGeometry();
        //unsigned int newWidth = MIN( MAX( selectedPrimaryPlaylistItem()->displayObject()->width()+680, width() ), screenRect.width() );
        //unsigned int newHeight = MIN( MAX( selectedPrimaryPlaylistItem()->displayObject()->height()+140, height() ), screenRect.height() );
        //resize( newWidth, newHeight );
    }
}

void MainWindow::openRecentFile()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action)
    {
        QStringList fileList = QStringList(action->data().toString());
        loadFiles(fileList);
    }
}

void MainWindow::addTextFrame()
{

    EditTextDialog newTextObjectDialog;
    int done = newTextObjectDialog.exec();
    if (done==QDialog::Accepted)
    {
        PlaylistItemText* newPlayListItemText = new PlaylistItemText(newTextObjectDialog.getText(), p_playlistWidget);
        newPlayListItemText->displayObject()->setFont(newTextObjectDialog.getFont());
        newPlayListItemText->displayObject()->setDuration(newTextObjectDialog.getDuration());
        newPlayListItemText->displayObject()->setColor(newTextObjectDialog.getColor());
        p_playlistWidget->setCurrentItem(newPlayListItemText, 0, QItemSelectionModel::ClearAndSelect);
        p_playlistWidget->setIsSaved(false);
    }

}

void MainWindow::addDifferenceSequence()
{
    PlaylistItemDifference* newPlayListItemDiff = new PlaylistItemDifference("Difference", p_playlistWidget);

    QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();

    for( int i=0; i<MIN(selectedItems.count(),2); i++ )
    {
        QTreeWidgetItem* item = selectedItems[i];

        int index = p_playlistWidget->indexOfTopLevelItem(item);
        if( index != INT_INVALID )
        {
            item = p_playlistWidget->takeTopLevelItem(index);
            newPlayListItemDiff->addChild(item);
            newPlayListItemDiff->setExpanded(true);

        }
    }

    p_playlistWidget->setCurrentItem(newPlayListItemDiff, 0, QItemSelectionModel::ClearAndSelect);
    p_playlistWidget->setIsSaved(false);
    updateSelectedItems();

}

PlaylistItem* MainWindow::selectedPrimaryPlaylistItem()
{
    if( p_playlistWidget == NULL )
        return NULL;

    QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();
    PlaylistItem* selectedItemPrimary = NULL;

    bool allowAssociatedItem = selectedItems.count() == 1;  // if only one item is selected, it can also be a child (associated)
    bool found_parent = false;

    if( selectedItems.count() >= 1 )
    {
        foreach (QTreeWidgetItem* anItem, selectedItems)
        {
            // we search an item that does not have a parent
            if(allowAssociatedItem || !dynamic_cast<PlaylistItem*>(anItem->parent()))
            {
                found_parent = true;
                selectedItemPrimary = dynamic_cast<PlaylistItem*>(anItem);
                break;
            }
        }
        if(!found_parent)
        {
            selectedItemPrimary = dynamic_cast<PlaylistItem*>(selectedItems.first());
        }
    }
    return selectedItemPrimary;
}

PlaylistItem* MainWindow::selectedSecondaryPlaylistItem()
{
    if( p_playlistWidget == NULL )
        return NULL;

    QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();
    PlaylistItem* selectedItemSecondary = NULL;

    if( selectedItems.count() >= 2 )
    {
        PlaylistItem* selectedItemPrimary = selectedPrimaryPlaylistItem();

        foreach (QTreeWidgetItem* anItem, selectedItems)
        {
            // we search an item that does not have a parent and that is not the primary item
            //PlaylistItem* aPlaylistParentItem = dynamic_cast<PlaylistItem*>(anItem->parent());
            //if(!aPlaylistParentItem && anItem != selectedItemPrimary)
            if(anItem != selectedItemPrimary)
            {
                selectedItemSecondary = dynamic_cast<PlaylistItem*>(anItem);
                break;
            }
        }
    }

    return selectedItemSecondary;
}

void MainWindow::updateSelectedItems()
{
    PlaylistItem* selectedItemPrimary = selectedPrimaryPlaylistItem();
    PlaylistItem* selectedItemSecondary = selectedSecondaryPlaylistItem();

    ui->DifferencegroupBox->setVisible(false);
    ui->DifferencegroupBox->setEnabled(false);
     ui->differenceLabel->setVisible(false);
    if( selectedItemPrimary == NULL  || selectedItemPrimary->displayObject() == NULL)
    {
        setWindowTitle("YUView");

        ui->fileDockWidget->setEnabled(false);
        ui->displayDockWidget->setEnabled(true);
        ui->YUVMathdockWidget->setEnabled(false);
        ui->statsDockWidget->setEnabled(false);
        ui->displaySplitView->setActiveDisplayObjects(NULL, NULL);
        ui->displaySplitView->setActiveStatisticsObjects(NULL, NULL);

        // update model
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setStatisticsTypeList( StatisticsTypeList() );

        setCurrentFrame(0);
        setControlsEnabled(false);

        return;
    }

    // update window caption
    QString newCaption = "YUView - " + selectedItemPrimary->text(0);
    setWindowTitle(newCaption);

    PlaylistItemStats* statsItem = NULL;    // used for model as source

    // if the newly selected primary (!) item is of type statistics, use it as source for types
    if( selectedItemPrimary && selectedItemPrimary->itemType() == StatisticsItemType )
    {
        statsItem = dynamic_cast<PlaylistItemStats*>(selectedItemPrimary);
        Q_ASSERT(statsItem != NULL);
    }
    else if( selectedItemSecondary && selectedItemSecondary->itemType() == StatisticsItemType )
    {
        statsItem = dynamic_cast<PlaylistItemStats*>(selectedItemSecondary);
        Q_ASSERT(statsItem != NULL);
    }

    // if selected item is of type 'diff', update child items
    if( selectedItemPrimary && selectedItemPrimary->itemType() == DifferenceItemType && selectedItemPrimary->childCount() == 2 )
    {
        PlaylistItemDifference* diffItem = dynamic_cast<PlaylistItemDifference*>(selectedItemPrimary);
        PlaylistItemVid* firstVidItem = dynamic_cast<PlaylistItemVid*>(selectedItemPrimary->child(0));
        PlaylistItemVid* secondVidItem = dynamic_cast<PlaylistItemVid*>(selectedItemPrimary->child(1));
        //TO-Do: Implement this as a different function
        if( firstVidItem && secondVidItem )
            diffItem->displayObject()->setFrameObjects(firstVidItem->displayObject(), secondVidItem->displayObject());
        ui->DifferencegroupBox->setVisible(true);
        ui->DifferencegroupBox->setEnabled(true);
        bool isChecked = ui->markDifferenceCheckBox->isChecked();
        QSettings settings;
        QColor color = settings.value("Difference/Color").value<QColor>();
        bool diff = diffItem->displayObject()->markDifferences(isChecked, color);
            if(isChecked)
            {
                if (diff)
                {
                   ui->differenceLabel->setVisible(true);
                   ui->differenceLabel->setText("There are differences in the pixels");
                }
              else
                {
                  ui->differenceLabel->setVisible(true);
                  ui->differenceLabel->setText("There is no difference");
                }
            }
            else ui->differenceLabel->setVisible(false);
     }

    // check for associated statistics
    PlaylistItemStats* statsItemPrimary = NULL;
    PlaylistItemStats* statsItemSecondary = NULL;
    if( selectedItemPrimary && selectedItemPrimary->itemType() == VideoItemType && selectedItemPrimary->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedItemPrimary->child(0);
        statsItemPrimary = dynamic_cast<PlaylistItemStats*>(childItem);
        Q_ASSERT(statsItemPrimary != NULL);

        if( statsItem == NULL )
            statsItem = statsItemPrimary;
    }
    if( selectedItemSecondary && selectedItemSecondary->itemType() == VideoItemType && selectedItemSecondary->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedItemSecondary->child(0);
        statsItemSecondary = dynamic_cast<PlaylistItemStats*>(childItem);
        Q_ASSERT(statsItemSecondary != NULL);

        if( statsItem == NULL )
            statsItem = statsItemSecondary;
    }

    // update statistics mode, if statistics is selected or associated with a selected item
    if( statsItem != NULL )
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setStatisticsTypeList(statsItem->displayObject()->getStatisticsTypeList());

    // update display widget
    ui->displaySplitView->setActiveStatisticsObjects(statsItemPrimary?statsItemPrimary->displayObject():NULL, statsItemSecondary?statsItemSecondary->displayObject():NULL);

    if(selectedItemPrimary == NULL || selectedItemPrimary->displayObject() == NULL)
        return;

    // tell our display widget about new objects
    ui->displaySplitView->setActiveDisplayObjects(selectedItemPrimary?selectedItemPrimary->displayObject():NULL, selectedItemSecondary?selectedItemSecondary->displayObject():NULL);

    // update playback controls
    setControlsEnabled(true);
    ui->fileDockWidget->setEnabled( selectedItemPrimary->itemType() != TextItemType );
    ui->displayDockWidget->setEnabled( true );
    ui->YUVMathdockWidget->setEnabled( selectedItemPrimary->itemType() == VideoItemType || selectedItemPrimary->itemType() == DifferenceItemType );
    ui->statsDockWidget->setEnabled( statsItem != NULL );

    // update displayed information
    updateMetaInfo();

    // update playback widgets
    refreshPlaybackWidgets();
}

void MainWindow::onCustomContextMenu(const QPoint &point)
{
    QMenu menu;

    // first add generic items to context menu
    menu.addAction("Open File...", this, SLOT(openFile()));
    menu.addAction("Add Text Frame", this, SLOT(addTextFrame()));
    menu.addAction("Add Difference Sequence", this, SLOT(addDifferenceSequence()));

    QTreeWidgetItem* itemAtPoint = p_playlistWidget->itemAt(point);
    if (itemAtPoint)
    {
        menu.addSeparator();
        menu.addAction("Delete Item", this, SLOT(deleteItem()));

        PlaylistItemStats* testStats = dynamic_cast<PlaylistItemStats*>(itemAtPoint);
        if(testStats)
        {
            // TODO: special actions for statistics items
        }
        PlaylistItemVid* testVid = dynamic_cast<PlaylistItemVid*>(itemAtPoint);
        if(testVid)
        {
            // TODO: special actions for video items
        }
        PlaylistItemText* testText = dynamic_cast<PlaylistItemText*>(itemAtPoint);
        if(testText)
        {
            menu.addAction("Edit Properties",this,SLOT(editTextFrame()));
        }
        PlaylistItemDifference* testDiff = dynamic_cast<PlaylistItemDifference*>(itemAtPoint);
        if(testDiff)
        {
            // TODO: special actions for difference items
        }
    }

    QPoint globalPos = p_playlistWidget->viewport()->mapToGlobal(point);
    QAction* selectedAction= menu.exec(globalPos);
    if (selectedAction)
    {
        //TODO
        //printf("Do something \n");
    }
}

void MainWindow::onItemDoubleClicked(QTreeWidgetItem* item, int)
{
    PlaylistItemStats* testStats = dynamic_cast<PlaylistItemStats*>(item);
    if(testStats)
    {
        // TODO: Double Click Behavior for Stats
    }
    PlaylistItemVid* testVid = dynamic_cast<PlaylistItemVid*>(item);
    if(testVid)
    {
        // TODO: Double Click Behavior for Video
    }
    PlaylistItemText* testText = dynamic_cast<PlaylistItemText*>(item);
    if(testText)
    {
        editTextFrame();
    }
    PlaylistItemDifference* testDiff = dynamic_cast<PlaylistItemDifference*>(item);
    if(testDiff)
    {
        // TODO: Double Click Behavior for difference
    }
}

void MainWindow::editTextFrame()
{
    PlaylistItemText* current =(PlaylistItemText *)p_playlistWidget->currentItem();
    EditTextDialog newTextObjectDialog;

    newTextObjectDialog.loadItemStettings((PlaylistItemText*)current);
    int done = newTextObjectDialog.exec();
    if (done==QDialog::Accepted)
    {
        current->displayObject()->setText(newTextObjectDialog.getText());
        current->displayObject()->setFont(newTextObjectDialog.getFont());
        current->displayObject()->setDuration(newTextObjectDialog.getDuration());
        current->displayObject()->setColor(newTextObjectDialog.getColor());
        current->setText(0,newTextObjectDialog.getText().replace("\n", " "));
        p_playlistWidget->setIsSaved(false);
        updateSelectedItems();
    }
}

void MainWindow::setSelectedStats() {
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
    ui->opacitySlider->setValue(dynamic_cast<StatsListModel*>(ui->statsListView->model())->data(list.at(0), Qt::UserRole+1).toInt());
    ui->gridCheckBox->setChecked(dynamic_cast<StatsListModel*>(ui->statsListView->model())->data(list.at(0), Qt::UserRole+2).toBool());

    ui->opacitySlider->setEnabled(true);
    ui->gridCheckBox->setEnabled(true);
    ui->opacityGroupBox->setEnabled(true);

    statsTypesChanged();
}

void MainWindow::updateStatsOpacity(int val) {
    QModelIndexList list = ui->statsListView->selectionModel()->selectedIndexes();
    if (list.size() < 1)
        return;
    dynamic_cast<StatsListModel*>(ui->statsListView->model())->setData(list.at(0), val, Qt::UserRole+1);
}


void MainWindow::updateStatsGrid(bool val) {
    QModelIndexList list = ui->statsListView->selectionModel()->selectedIndexes();
    if (list.size() < 1)
        return;
    dynamic_cast<StatsListModel*>(ui->statsListView->model())->setData(list.at(0), val, Qt::UserRole+2);
}

void MainWindow::setCurrentFrame(int frame, bool forceRefresh)
{
    if (selectedPrimaryPlaylistItem() == NULL || selectedPrimaryPlaylistItem()->displayObject() == NULL)
    {
        p_currentFrame = 0;
        ui->displaySplitView->clear();
        return;
    }

    if (frame != p_currentFrame || forceRefresh)
    {
        // get real frame index
        if( frame < selectedPrimaryPlaylistItem()->displayObject()->startFrame() )
            frame = selectedPrimaryPlaylistItem()->displayObject()->startFrame();
        else if( frame > selectedPrimaryPlaylistItem()->displayObject()->endFrame() )
            // Don't change the frame.
            return;
        
        if(frame != p_currentFrame)
            p_FPSCounter++;
        p_currentFrame = frame;

        // update frame index in GUI
        ui->frameCounterSpinBox->setValue(p_currentFrame);
        ui->frameSlider->setValue(p_currentFrame);

        // draw new frame
        ui->displaySplitView->drawFrame(p_currentFrame);
    }
}

void MainWindow::updateMetaInfo()
{
    if (selectedPrimaryPlaylistItem() == NULL || selectedPrimaryPlaylistItem()->displayObject() == NULL)
        return;

    // update all selected YUVObjects from playlist, if signal comes from GUI
    if ((ui->widthSpinBox == QObject::sender()) || (ui->framesizeComboBox == QObject::sender()))
    {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
        {
            PlaylistItem* playlistItem = dynamic_cast<PlaylistItem*>(item);
            playlistItem->displayObject()->setWidth(ui->widthSpinBox->value());
            playlistItem->displayObject()->refreshNumberOfFrames();
        }
        ui->displaySplitView->resetViews();
    }
    else if ((ui->heightSpinBox == QObject::sender()) || (ui->framesizeComboBox == QObject::sender()))
    {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
        {
            PlaylistItem* playlistItem = dynamic_cast<PlaylistItem*>(item);
            int heightVal = ui->heightSpinBox->value();
            playlistItem->displayObject()->setHeight(heightVal);
            playlistItem->displayObject()->refreshNumberOfFrames();
        }
        ui->displaySplitView->resetViews();
    }
    else if (ui->startoffsetSpinBox == QObject::sender())
    {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setStartFrame(ui->startoffsetSpinBox->value());
        return;
    }
    else if (ui->endSpinBox == QObject::sender())
    {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
        {
            PlaylistItem* playlistItem = dynamic_cast<PlaylistItem*>(item);
            int endValue = ui->endSpinBox->value();
            playlistItem->displayObject()->setEndFrame(endValue);
        }
    }
    else if (ui->rateSpinBox == QObject::sender())
    {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setFrameRate(ui->rateSpinBox->value());

        // update playback timer
        p_playTimer->setInterval(1000.0/ui->rateSpinBox->value());
    }
    else if (ui->samplingSpinBox == QObject::sender())
    {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setSampling(ui->samplingSpinBox->value());
    }
    else if (ui->pixelFormatComboBox == QObject::sender())
    {
        YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)(ui->pixelFormatComboBox->currentIndex()+1);
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
        {
            PlaylistItemVid* vidItem = dynamic_cast<PlaylistItemVid*>(item);
            if(vidItem)
            {
                vidItem->displayObject()->setSrcPixelFormat(pixelFormat);
                vidItem->displayObject()->refreshNumberOfFrames();
            }
        }
    }
    else if (ui->markDifferenceCheckBox == QObject::sender())
    {
        bool isChecked = ui->markDifferenceCheckBox->isChecked();
        bool diff;
        QSettings settings;
        QColor color = settings.value("Difference/Color").value<QColor>();
            diff = dynamic_cast<PlaylistItemDifference*>(selectedPrimaryPlaylistItem())->displayObject()->markDifferences(isChecked,color);
            if(isChecked)
            {
              if (diff)
              {
                  ui->differenceLabel->setVisible(true);
                  ui->differenceLabel->setText("There are differences in the pixels");
              }
              else
              {
                  ui->differenceLabel->setVisible(true);
                  ui->differenceLabel->setText("There is no difference");
              }
            }
            else ui->differenceLabel->setVisible(false);
       }

    // Temporarily (!) disconnect slots/signals of info panel
    QObject::disconnect( ui->widthSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->heightSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->startoffsetSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->rateSpinBox, SIGNAL(valueChanged(double)), NULL, NULL );
    QObject::disconnect( ui->samplingSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL );
    QObject::disconnect( ui->widthSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->heightSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect(ui->markDifferenceCheckBox, SIGNAL(clicked(bool)),NULL,NULL);
    QObject::disconnect( selectedPrimaryPlaylistItem()->displayObject(), SIGNAL(informationChanged()), this, SLOT(currentSelectionInformationChanged()));


    if( selectedPrimaryPlaylistItem()->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(selectedPrimaryPlaylistItem());
        Q_ASSERT(viditem != NULL);

        ui->createdText->setText(viditem->displayObject()->createdtime());
        ui->modifiedText->setText(viditem->displayObject()->modifiedtime());
        ui->filepathText->setText(viditem->displayObject()->path());
        ui->nrBytesText->setText(QString::number(viditem->displayObject()->nrBytes()));
        ui->nrFramesText->setText(QString::number(viditem->displayObject()->numFrames()));
        ui->statusText->setText(viditem->displayObject()->getStatusAndInfo());
    }
    else if( selectedPrimaryPlaylistItem()->itemType() == StatisticsItemType )
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedPrimaryPlaylistItem());
        Q_ASSERT(statsItem != NULL);

        ui->createdText->setText(statsItem->displayObject()->createdtime());
        ui->modifiedText->setText(statsItem->displayObject()->modifiedtime());
        ui->filepathText->setText(statsItem->displayObject()->path());
        ui->nrBytesText->setText(QString::number(statsItem->displayObject()->nrBytes()));
        ui->nrFramesText->setText(QString::number(statsItem->displayObject()->numFrames()));
        ui->statusText->setText(statsItem->displayObject()->getStatusAndInfo());
    }
    else
    {
        // Other object
        ui->createdText->setText(QString(""));
        ui->modifiedText->setText(QString(""));
        ui->filepathText->setText(QString(""));
        ui->nrBytesText->setText(QString(""));
        ui->nrFramesText->setText(QString(""));
        ui->statusText->setText(QString(""));
    }

    // update GUI with information from primary selected playlist item
    ui->widthSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->width());
    ui->heightSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->height());
    ui->startoffsetSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->startFrame());
    ui->endSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->endFrame());
    ui->rateSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->frameRate());
    ui->samplingSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->sampling());

    updateFrameSizeComboBoxSelection();

    PlaylistItemVid* vidItem = dynamic_cast<PlaylistItemVid*>( selectedPrimaryPlaylistItem() );
    if(vidItem)
    {
        ui->pixelFormatComboBox->setCurrentIndex( vidItem->displayObject()->pixelFormat()-1 );
    }

    // Reconnect slots/signals of info panel
    QObject::connect( ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->startoffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateMetaInfo()) );

    QObject::connect( ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateFrameSizeComboBoxSelection()) );
    QObject::connect( ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateFrameSizeComboBoxSelection()) );
    QObject::connect(ui->markDifferenceCheckBox,SIGNAL(clicked(bool)),this, SLOT(updateMetaInfo()));
    QObject::connect( selectedPrimaryPlaylistItem()->displayObject(), SIGNAL(informationChanged()), this, SLOT(currentSelectionInformationChanged()));
}

void MainWindow::currentSelectionInformationChanged()
{
    // update displayed information
    updateMetaInfo();

    // Refresh the playback widget
    refreshPlaybackWidgets();
}

void MainWindow::refreshPlaybackWidgets()
{
    // don't do anything if not yet initialized
    if (selectedPrimaryPlaylistItem() == NULL)
        return;

    // update information about newly selected video

    // update our timer
    p_playTimer->setInterval(1000.0/selectedPrimaryPlaylistItem()->displayObject()->frameRate());

    int minFrameIdx = MAX( 0, selectedPrimaryPlaylistItem()->displayObject()->startFrame() );
    int maxFrameIdx = MAX(MIN( selectedPrimaryPlaylistItem()->displayObject()->endFrame(), selectedPrimaryPlaylistItem()->displayObject()->numFrames() ), minFrameIdx+1);
    ui->frameSlider->setMinimum( minFrameIdx );
    ui->frameSlider->setMaximum( maxFrameIdx );
    if (ui->endSpinBox->value() != selectedPrimaryPlaylistItem()->displayObject()->endFrame())
        ui->endSpinBox->setValue( selectedPrimaryPlaylistItem()->displayObject()->endFrame() );

    int modifiedFrame = p_currentFrame;

    if( p_currentFrame < selectedPrimaryPlaylistItem()->displayObject()->startFrame() )
        modifiedFrame = selectedPrimaryPlaylistItem()->displayObject()->startFrame();
    else if( p_currentFrame > selectedPrimaryPlaylistItem()->displayObject()->endFrame() )
        modifiedFrame = selectedPrimaryPlaylistItem()->displayObject()->endFrame();

    // make sure that changed info is resembled in display frame - might be due to changes to playback range
    setCurrentFrame(modifiedFrame, true);
}

void MainWindow::togglePlayback()
{
    if(p_playTimer->isActive())
        pause();
    else
    {
        if (selectedPrimaryPlaylistItem())
        {
            if (p_currentFrame>=selectedPrimaryPlaylistItem()->displayObject()->endFrame() && p_repeatMode==RepeatModeOff)
            {
                setCurrentFrame(selectedPrimaryPlaylistItem()->displayObject()->startFrame());
            }
            play();
        }
    }
}

void MainWindow::play()
{
    // check first if we are already playing
    if( p_playTimer->isActive() || !selectedPrimaryPlaylistItem() || !selectedPrimaryPlaylistItem()->displayObject() )
        return;

    p_FPSCounter = 0;
    p_lastHeartbeatTime = QTime::currentTime();

    // start playing with timer
    double frameRate = selectedPrimaryPlaylistItem()->displayObject()->frameRate();
    if(frameRate < 0.00001) frameRate = 1.0;
    p_playTimer->start( 1000.0/frameRate );

    // update our play/pause icon
    ui->playButton->setIcon(p_pauseIcon);
}

void MainWindow::pause()
{
    // stop the play timer loading new frames.
    p_playTimer->stop();

    // update our play/pause icon
    ui->playButton->setIcon(p_playIcon);
}

void MainWindow::stop()
{
    // stop the play timer loading new frames
    p_playTimer->stop();

    // reset our video
    if( isPlaylistItemSelected() )
        setCurrentFrame( selectedPrimaryPlaylistItem()->displayObject()->startFrame() );

    // update our play/pause icon
    ui->playButton->setIcon(p_playIcon);
}

void MainWindow::deleteItem()
{
    // stop playback first
    stop();

    QList<QTreeWidgetItem*> selectedList = p_playlistWidget->selectedItems();

    if( selectedList.count() == 0 )
        return;

    // now delete selected items
    for(int i = 0; i<selectedList.count(); i++)
    {
        QTreeWidgetItem *parentItem = selectedList.at(i)->parent();

        if( parentItem != NULL )    // is child of another item
        {
            int idx = parentItem->indexOfChild(selectedList.at(i));

            QTreeWidgetItem* itemToRemove;
            PlaylistItemDifference *diffObject = dynamic_cast<PlaylistItemDifference*>(parentItem);
            if (diffObject != NULL)
                // This is a difference item. Call the PlaylistItemDifference takeChild 
                // function which will also update the assiciated differenceobject.
                itemToRemove = diffObject->takeChild(idx);
            else
                itemToRemove = parentItem->takeChild(idx);

            delete itemToRemove;

            p_playlistWidget->setItemSelected(parentItem, true);
        }
        else
        {
            int idx = p_playlistWidget->indexOfTopLevelItem(selectedList.at(i));

            QTreeWidgetItem* itemToRemove = p_playlistWidget->takeTopLevelItem(idx);
            delete itemToRemove;

            int nextIdx = MAX(MIN(idx,p_playlistWidget->topLevelItemCount()-1), 0);

            QTreeWidgetItem* nextItem = p_playlistWidget->topLevelItem(nextIdx);
            if( nextItem )
                p_playlistWidget->setItemSelected(nextItem, true);
        }
    }
    if (p_playlistWidget->topLevelItemCount()==0)
    {
        // playlist empty, we dont need to save anymore
        p_playlistWidget->setIsSaved(true);
        ui->markDifferenceCheckBox->setChecked(false);
    }
}

void MainWindow::updateGrid() {
    bool enableGrid = ui->regularGridCheckBox->checkState() == Qt::Checked;
    QSettings settings;
    QColor color = settings.value("OverlayGrid/Color").value<QColor>();
    ui->displaySplitView->setRegularGridParameters(enableGrid, ui->gridSizeBox->value(), color);
}

void MainWindow::selectNextItem()
{
    QTreeWidgetItem* selectedItem = selectedPrimaryPlaylistItem();
    if ( selectedItem != NULL && p_playlistWidget->itemBelow(selectedItem) != NULL)
        p_playlistWidget->setCurrentItem(p_playlistWidget->itemBelow(selectedItem));
}

void MainWindow::selectPreviousItem()
{
    QTreeWidgetItem* selectedItem = selectedPrimaryPlaylistItem();
    if ( selectedItem != NULL && p_playlistWidget->itemAbove(selectedItem) != NULL)
        p_playlistWidget->setCurrentItem(p_playlistWidget->itemAbove(selectedItem));
}

// for debug only
bool MainWindow::eventFilter(QObject *target, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = (QKeyEvent*)event;
        qDebug()<<"Key: "<<keyEvent<<"Object: "<<target;
    }
    return QWidget::eventFilter(target,event);
}

void MainWindow::handleKeyPress(QKeyEvent *key)
{
    keyPressEvent(key);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // more keyboard shortcuts can be implemented here...
    switch(event->key())
    {
    case Qt::Key_Escape:
    {
        if(isFullScreen())
            toggleFullscreen();
        break;
    }
    case Qt::Key_F:
    {
        if (event->modifiers()==Qt::ControlModifier)
            toggleFullscreen();
        break;
    }
    case Qt::Key_1:
    {
        if (event->modifiers()==Qt::ControlModifier)
            enableSingleWindowMode();
        break;
    }
    case Qt::Key_2:
    {
        if (event->modifiers()==Qt::ControlModifier)
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
    case Qt::Key_Plus:
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
    }
    }
}

void MainWindow::toggleFullscreen()
{
    if(isFullScreen())
    {
        // show panels
        ui->fileDockWidget->show();
        ui->playlistDockWidget->show();
        ui->statsDockWidget->show();
        ui->displayDockWidget->show();
        ui->controlsDockWidget->show();
        ui->YUVMathdockWidget->show();

#ifndef QT_OS_MAC
        // show menu
        ui->menuBar->show();
#endif
        ui->displaySplitView->showNormal();
        showNormal();
    }
    else
    {
        if( p_windowMode == WindowModeSingle )
        {
            // hide panels
            ui->fileDockWidget->hide();
            ui->playlistDockWidget->hide();
            ui->statsDockWidget->hide();
            ui->displayDockWidget->hide();
            ui->YUVMathdockWidget->hide();
        }
#ifndef QT_OS_MAC
            // hide menu
            ui->menuBar->hide();
#endif
        // always hide playback controls in full screen mode
        ui->controlsDockWidget->hide();

        ui->displaySplitView->showFullScreen();

        showFullScreen();
    }
    ui->displaySplitView->resetViews();
}

void MainWindow::setControlsEnabled(bool flag)
{
    ui->playButton->setEnabled(flag);
    ui->stopButton->setEnabled(flag);
    ui->frameSlider->setEnabled(flag);
    ui->frameCounterSpinBox->setEnabled(flag);
}

void MainWindow::frameTimerEvent()
{
    if(!isPlaylistItemSelected())
        return stop();

    // if we reached the end of a sequence, react...
    int nextFrame = p_currentFrame + selectedPrimaryPlaylistItem()->displayObject()->sampling();
    if (nextFrame > selectedPrimaryPlaylistItem()->displayObject()->endFrame() )
    {
        switch(p_repeatMode)
        {
        case RepeatModeOff:
            pause();
            if (p_ClearFrame)
                ui->displaySplitView->drawFrame(INT_INVALID);
            break;
        case RepeatModeOne:
            setCurrentFrame( selectedPrimaryPlaylistItem()->displayObject()->startFrame() );
            break;
        case RepeatModeAll:
            // get next item in list
            QModelIndex curIdx = p_playlistWidget->indexForItem(selectedPrimaryPlaylistItem());
            int rowIdx = curIdx.row();
            PlaylistItem* nextItem = NULL;
            if(rowIdx == p_playlistWidget->topLevelItemCount()-1)
                nextItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(0));
            else
                nextItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(rowIdx+1));

            p_playlistWidget->setCurrentItem(nextItem, 0, QItemSelectionModel::ClearAndSelect);
            setCurrentFrame(nextItem->displayObject()->startFrame());
        }
    }
    else
    {
        // update current frame
        setCurrentFrame( nextFrame );
    }
}

void MainWindow::heartbeatTimerEvent()
{
    // update fps counter
    QTime newFrameTime = QTime::currentTime();
    float msecsSinceLastHeartbeat = (float)p_lastHeartbeatTime.msecsTo(newFrameTime);
    float numFramesSinceLastHeartbeat = (float)p_FPSCounter;
    int framesPerSec = (int)(numFramesSinceLastHeartbeat / (msecsSinceLastHeartbeat/1000.0));
    if (framesPerSec > 0)
        ui->frameRateLabel->setText(QString().setNum(framesPerSec));

    p_lastHeartbeatTime = newFrameTime;
    p_FPSCounter = 0;
}

void MainWindow::toggleRepeat()
{
    switch(p_repeatMode)
    {
    case RepeatModeOff:
        setRepeatMode(RepeatModeOne);
        break;
    case RepeatModeOne:
        setRepeatMode(RepeatModeAll);
        break;
    case RepeatModeAll:
        setRepeatMode(RepeatModeOff);
        break;
    }
}

void MainWindow::setRepeatMode(RepeatMode newMode)
{
    // save internally
    p_repeatMode = newMode;

    // update icon in GUI
    switch(p_repeatMode)
    {
    case RepeatModeOff:
        ui->repeatButton->setIcon(p_repeatOffIcon);
        break;
    case RepeatModeOne:
        ui->repeatButton->setIcon(p_repeatOneIcon);
        break;
    case RepeatModeAll:
        ui->repeatButton->setIcon(p_repeatAllIcon);
        break;
    }

    // save new repeat mode in user preferences
    QSettings settings;
    settings.setValue("RepeatMode", p_repeatMode);
}


/////////////////////////////////////////////////////////////////////////////////

void MainWindow::on_framesizeComboBox_currentIndexChanged(int index)
{
    switch (index)
    {
    case 0:
        ui->widthSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->width());
        ui->heightSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->height());
        break;
    case 1:
        ui->widthSpinBox->setValue(176);
        ui->heightSpinBox->setValue(144);
        break;
    case 2:
        ui->widthSpinBox->setValue(320);
        ui->heightSpinBox->setValue(240);
        break;
    case 3:
        ui->widthSpinBox->setValue(416);
        ui->heightSpinBox->setValue(240);
        break;
    case 4:
        ui->widthSpinBox->setValue(352);
        ui->heightSpinBox->setValue(288);
        break;
    case 5:
        ui->widthSpinBox->setValue(640);
        ui->heightSpinBox->setValue(480);
        break;
    case 6:
        ui->widthSpinBox->setValue(832);
        ui->heightSpinBox->setValue(480);
        break;
    case 7:
        ui->widthSpinBox->setValue(704);
        ui->heightSpinBox->setValue(576);
        break;
    case 8:
        ui->widthSpinBox->setValue(720);
        ui->heightSpinBox->setValue(576);
        break;
    case 9:
        ui->widthSpinBox->setValue(1280);
        ui->heightSpinBox->setValue(720);
        break;
    case 10:
        ui->widthSpinBox->setValue(1920);
        ui->heightSpinBox->setValue(1080);
        break;
    case 11:
        ui->widthSpinBox->setValue(3840);
        ui->heightSpinBox->setValue(2160);
        break;
    case 12:
        ui->widthSpinBox->setValue(1024);
        ui->heightSpinBox->setValue(768);
        break;
    case 13:
        ui->widthSpinBox->setValue(1280);
        ui->heightSpinBox->setValue(960);
        break;
    default:
        break;

    }

    refreshPlaybackWidgets();
}

void MainWindow::on_pixelFormatComboBox_currentIndexChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType) (index+1);
            viditem->displayObject()->setSrcPixelFormat(pixelFormat);
        }
    }
}

void MainWindow::on_interpolationComboBox_currentIndexChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setInterpolationMode((InterpolationMode)index);
        }
    }
}

void MainWindow::statsTypesChanged()
{
    // update all displayed statistics of primary item
    if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == StatisticsItemType)
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedPrimaryPlaylistItem());
        Q_ASSERT(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }
    else if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == VideoItemType && selectedPrimaryPlaylistItem()->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedPrimaryPlaylistItem()->child(0);
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(childItem);
        Q_ASSERT(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }

    // update all displayed statistics of secondary item
    if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == StatisticsItemType)
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedSecondaryPlaylistItem());
        Q_ASSERT(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }
    else if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == VideoItemType && selectedSecondaryPlaylistItem()->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedSecondaryPlaylistItem()->child(0);
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(childItem);
        Q_ASSERT(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }

    // refresh display widget
    ui->displaySplitView->drawFrame(p_currentFrame);
}

void MainWindow::updateFrameSizeComboBoxSelection()
{
    int W = ui->widthSpinBox->value();
    int H = ui->heightSpinBox->value();

    if ( W == 176 && H == 144)
        ui->framesizeComboBox->setCurrentIndex(1);
    else if ( W == 320 && H == 240 )
        ui->framesizeComboBox->setCurrentIndex(2);
    else if ( W == 416 && H == 240 )
        ui->framesizeComboBox->setCurrentIndex(3);
    else if ( W == 352 && H == 288 )
        ui->framesizeComboBox->setCurrentIndex(4);
    else if ( W == 640 && H == 480 )
        ui->framesizeComboBox->setCurrentIndex(5);
    else if ( W == 832 && H == 480 )
        ui->framesizeComboBox->setCurrentIndex(6);
    else if ( W == 704 && H == 576 )
        ui->framesizeComboBox->setCurrentIndex(7);
    else if ( W == 720 && H == 576 )
        ui->framesizeComboBox->setCurrentIndex(8);
    else if ( W == 1280 && H == 720 )
        ui->framesizeComboBox->setCurrentIndex(9);
    else if ( W == 1920 && H == 1080 )
        ui->framesizeComboBox->setCurrentIndex(10);
    else if ( W == 3840 && H == 2160 )
        ui->framesizeComboBox->setCurrentIndex(11);
    else if ( W == 1024 && H == 768 )
        ui->framesizeComboBox->setCurrentIndex(12);
    else if ( W == 1280 && H == 960 )
        ui->framesizeComboBox->setCurrentIndex(13);
    else
        ui->framesizeComboBox->setCurrentIndex(0);
}

void MainWindow::updatePixelFormatComboBoxSelection(PlaylistItem* selectedItem)
{
    PlaylistItem* item = dynamic_cast<PlaylistItem*>(selectedItem);
    if( item->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
        Q_ASSERT(viditem != NULL);

        YUVCPixelFormatType pixelFormat = viditem->displayObject()->pixelFormat();
        ui->pixelFormatComboBox->setCurrentIndex(pixelFormat-1);
    }
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

    if (currentReply->error() == QNetworkReply::NoError) {
        QString strReply = (QString)currentReply->readAll();
        //parse json
        QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());
        QJsonArray jsonArray = jsonResponse.array();
        QJsonObject jsonObject = jsonArray[0].toObject();
        QString currentHash = jsonObject["sha"].toString();
        QString buildHash = QString::fromUtf8(YUVIEW_HASH);
        QString buildVersion= QString::fromUtf8(YUVIEW_VERSION);
        if (QString::compare(currentHash,buildHash))
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
    flags |= Qt::WindowStaysOnTopHint;
    flags |= Qt::CustomizeWindowHint;

    about->setWindowFlags(flags);
    about->setReadOnly(true);

    QFile file(":/about.html");
    if (!file.open (QIODevice::ReadOnly))
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
    about->setFixedSize(QSize(500,400));

    about->show();
}

void MainWindow::openProjectWebsite()
{
    QDesktopServices::openUrl(QUrl("https://github.com/IENT/YUView"));
}

void MainWindow::saveScreenshot() {

    QSettings settings;

    QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), settings.value("LastScreenshotPath").toString(), tr("PNG Files (*.png);"));

    ui->displaySplitView->captureScreenshot().save(filename);

    filename = filename.section('/',0,-2);
    settings.setValue("LastScreenshotPath",filename);
}

void MainWindow::updateSettings()
{
    FrameObject::frameCache.setMaxCost(p_settingswindow.getCacheSizeInMB());

    updateGrid();

    p_ClearFrame = p_settingswindow.getClearFrameState();

    ui->displaySplitView->update();
}

QString MainWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void MainWindow::on_LumaScaleSpinBox_valueChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setLumaScale(index);
        }
        else if (item->itemType() == DifferenceItemType )
        {
            PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);
            Q_ASSERT(diffitem != NULL);

            diffitem->displayObject()->setLumaScale(index);
        }
    }
}

void MainWindow::on_ChromaScaleSpinBox_valueChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setChromaVScale(index);
            viditem->displayObject()->setChromaUScale(index);
        }
        else if (item->itemType() == DifferenceItemType )
        {
            PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);
            Q_ASSERT(diffitem != NULL);

            diffitem->displayObject()->setChromaVScale(index);
            diffitem->displayObject()->setChromaUScale(index);
        }
    }
}

void MainWindow::on_LumaOffsetSpinBox_valueChanged(int arg1)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setLumaOffset(arg1);
        }
        else if (item->itemType() == DifferenceItemType )
        {
            PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);
            Q_ASSERT(diffitem != NULL);

            diffitem->displayObject()->setLumaOffset(arg1);
        }
    }
}

void MainWindow::on_ChromaOffsetSpinBox_valueChanged(int arg1)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setChromaOffset(arg1);
        }
        else if (item->itemType() == DifferenceItemType )
        {
            PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);
            Q_ASSERT(diffitem != NULL);

            diffitem->displayObject()->setChromaOffset(arg1);
        }
    }
}

void MainWindow::on_LumaInvertCheckBox_toggled(bool checked)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setLumaInvert(checked);
        }
        else if (item->itemType() == DifferenceItemType )
        {
            PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);
            Q_ASSERT(diffitem != NULL);

            diffitem->displayObject()->setLumaInvert(checked);
        }
    }
}

void MainWindow::on_ChromaInvertCheckBox_toggled(bool checked)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setChromaInvert(checked);
        }
        else if (item->itemType() == DifferenceItemType )
        {
            PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);
            Q_ASSERT(diffitem != NULL);

            diffitem->displayObject()->setChromaInvert(checked);
        }
    }
}

void MainWindow::on_ColorComponentsComboBox_currentIndexChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
        PlaylistItemDifference* diffitem = dynamic_cast<PlaylistItemDifference*>(item);

        switch(index)
        {
        case 0:
            on_LumaScaleSpinBox_valueChanged(ui->LumaScaleSpinBox->value());
            on_ChromaScaleSpinBox_valueChanged(ui->ChromaScaleSpinBox->value());
            ui->ChromagroupBox->setDisabled(false);
            ui->LumagroupBox->setDisabled(false);
            break;

        case 1:
            on_LumaScaleSpinBox_valueChanged(ui->LumaScaleSpinBox->value());
            on_ChromaScaleSpinBox_valueChanged(0);
            on_ChromaOffsetSpinBox_valueChanged(0);
            ui->ChromagroupBox->setDisabled(true);
            ui->LumagroupBox->setDisabled(false);
            break;
        case 2:
            on_LumaScaleSpinBox_valueChanged(0);
            on_LumaOffsetSpinBox_valueChanged(0);
            if(viditem)
            {
                viditem->displayObject()->setChromaUScale(ui->ChromaScaleSpinBox->value());
                viditem->displayObject()->setChromaVScale(0);
            }
            if(diffitem)
            {
                diffitem->displayObject()->setChromaUScale(ui->ChromaScaleSpinBox->value());
                diffitem->displayObject()->setChromaVScale(0);
            }
            ui->ChromagroupBox->setDisabled(false);
            ui->LumagroupBox->setDisabled(true);
            break;
        case 3:
            on_LumaScaleSpinBox_valueChanged(0);
            on_LumaOffsetSpinBox_valueChanged(0);
            if(viditem)
            {
                viditem->displayObject()->setChromaUScale(0);
                viditem->displayObject()->setChromaVScale(ui->ChromaScaleSpinBox->value());
            }
            if(diffitem)
            {
                diffitem->displayObject()->setChromaUScale(0);
                diffitem->displayObject()->setChromaVScale(ui->ChromaScaleSpinBox->value());
            }
            ui->ChromagroupBox->setDisabled(false);
            ui->LumagroupBox->setDisabled(true);
            break;
        default:
            on_LumaScaleSpinBox_valueChanged(ui->LumaScaleSpinBox->value());
            on_ChromaScaleSpinBox_valueChanged(ui->ChromaScaleSpinBox->value());
            ui->ChromagroupBox->setDisabled(false);
            ui->LumagroupBox->setDisabled(false);
            break;
        }
    }
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
    ui->displaySplitView->resetViews();

}

void MainWindow::on_zoomBoxCheckBox_toggled(bool checked)
{
    ui->displaySplitView->setZoomBoxEnabled(checked);
}

void MainWindow::on_SplitViewgroupBox_toggled(bool checkState)
{
    ui->displaySplitView->setSplitEnabled(checkState);
    ui->SplitViewgroupBox->setChecked(checkState);
    ui->displaySplitView->setViewMode(SIDE_BY_SIDE);
    ui->viewComboBox->setCurrentIndex(0);
    QSettings settings;
    settings.setValue("SplitViewEnabled",checkState);
    ui->displaySplitView->resetViews();

}

void MainWindow::enableSeparateWindowsMode()
{
    // if we are in fullscreen, get back to windowed mode
    if(isFullScreen())
    {
        this->toggleFullscreen();
    }

    // show inspector window with default dockables
    p_inspectorWindow.hide();
    ui->fileDockWidget->show();
    p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->fileDockWidget);
    ui->displayDockWidget->show();
    p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->displayDockWidget);
    ui->YUVMathdockWidget->show();
    p_inspectorWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->YUVMathdockWidget);
    p_inspectorWindow.show();

    // show playlist with default dockables
    p_playlistWindow.hide();
    ui->playlistDockWidget->show();
    p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->playlistDockWidget);
    ui->statsDockWidget->show();
    p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea, ui->statsDockWidget);
    ui->controlsDockWidget->show();
    p_playlistWindow.addDockWidget(Qt::LeftDockWidgetArea,ui->controlsDockWidget);
    p_playlistWindow.show();
    ui->playlistTreeWidget->setFocus();
    p_windowMode = WindowModeSeparate;
}

// if we are in fullscreen, get back to windowed mode
void MainWindow::enableSingleWindowMode()
{
    // if we are in fullscreen, get back to windowed mode
    if(isFullScreen())
    {
        this->toggleFullscreen();
    }

    // hide inspector window and move dockables to main window
    p_inspectorWindow.hide();
    ui->fileDockWidget->show();
    this->addDockWidget(Qt::RightDockWidgetArea, ui->fileDockWidget);
    ui->displayDockWidget->show();
    this->addDockWidget(Qt::RightDockWidgetArea, ui->displayDockWidget);
    ui->YUVMathdockWidget->show();
    this->addDockWidget(Qt::RightDockWidgetArea, ui->YUVMathdockWidget);

    // hide playlist window and move dockables to main window
    p_playlistWindow.hide();
    ui->playlistDockWidget->show();
    this->addDockWidget(Qt::LeftDockWidgetArea, ui->playlistDockWidget);
    ui->statsDockWidget->show();
    this->addDockWidget(Qt::LeftDockWidgetArea, ui->statsDockWidget);
    ui->controlsDockWidget->show();
    this->addDockWidget(Qt::BottomDockWidgetArea,ui->controlsDockWidget);
    activateWindow();

    p_windowMode = WindowModeSingle;
}


void MainWindow::on_colorConversionComboBox_currentIndexChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == VideoItemType )
        {
            PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
            Q_ASSERT(viditem != NULL);

            viditem->displayObject()->setColorConversionMode((YUVCColorConversionType)index);
        }
    }
}

void MainWindow::on_markDifferenceCheckBox_clicked()
{
   // bool checked = false;
    if(ui->markDifferenceCheckBox->isChecked())
    {
       // checked = true;
    }
}
