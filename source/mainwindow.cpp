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
#include <QCache>
#include "playlistitem.h"
#include "statslistmodel.h"
#include "displaysplitwidget.h"
#include "plistparser.h"
#include "plistserializer.h"
#include "differenceobject.h"
#include "de265File.h"

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

// initialize the preset frame sizes to an empty list
QList<frameSizePreset> MainWindow::g_presetFrameSizes = QList<frameSizePreset>();

/* Return the list of preset fram sizes.
 * This is the one place to add additional default frame sizes for the dropdown list.
 */
QList<frameSizePreset> MainWindow::presetFrameSizesList()
{
  if (g_presetFrameSizes.empty()) {
    // Add all the presets
    g_presetFrameSizes.append(frameSizePreset("QCIF", QSize(176,144)));
    g_presetFrameSizes.append(frameSizePreset("QVGA", QSize(320, 240)));
    g_presetFrameSizes.append(frameSizePreset("WQVGA", QSize(416, 240)));
    g_presetFrameSizes.append(frameSizePreset("CIF", QSize(352, 288)));
    g_presetFrameSizes.append(frameSizePreset("VGA", QSize(640, 480)));
    g_presetFrameSizes.append(frameSizePreset("WVGA", QSize(832, 480)));
    g_presetFrameSizes.append(frameSizePreset("4CIF", QSize(704, 576)));
    g_presetFrameSizes.append(frameSizePreset("ITU R.BT601", QSize(720, 576)));
    g_presetFrameSizes.append(frameSizePreset("720i/p", QSize(1280, 720)));	
    g_presetFrameSizes.append(frameSizePreset("1080i/p", QSize(1920, 1080)));
    g_presetFrameSizes.append(frameSizePreset("4k", QSize(3840, 2160)));
    g_presetFrameSizes.append(frameSizePreset("XGA", QSize(1024, 768)));
    g_presetFrameSizes.append(frameSizePreset("XGA+", QSize(1280, 960)));
  }

  return g_presetFrameSizes;
}

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
    p_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(p_playlistWidget, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
    connect(p_playlistWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onItemDoubleClicked(QTreeWidgetItem*,int)));
    
    ui->displaySplitView->setAttribute(Qt::WA_AcceptTouchEvents);
    
    p_currentFrame = 0;
    p_timerRunning = false;
    p_timerFPSCounter = 0;
    previouslySelectedDisplayObject;  

    p_playIcon = QIcon(":img_play.png");
    p_pauseIcon = QIcon(":img_pause.png");
    p_repeatOffIcon = QIcon(":img_repeat.png");
    p_repeatAllIcon = QIcon(":img_repeat_on.png");
    p_repeatOneIcon = QIcon(":img_repeat_one.png");

    setRepeatMode((RepeatMode)settings.value("RepeatMode", RepeatModeOff).toUInt());   // load parameter from user preferences
    if (!settings.value("SplitViewEnabled",true).toBool())
        on_SplitViewgroupBox_toggled(false);
    
    // populate combo box for pixel formats and frame sizes
    populateComboBoxes();

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

    // Connect all the file options controls to on_fileOptionValueChanged()
    QObject::connect(ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->startoffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->framesizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_fileOptionValueChanged()));

    // Connect the frame slider and the frame spin box to the function 
    QObject::connect(ui->frameCounterSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));
    QObject::connect(ui->frameSlider, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));

    // Update the selected item. Nothing is selected but the function will then set some default values.
    updateSelectedItems();
    // Call this once to init FrameCache and other settings
    updateSettings();
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

/// Populate the frameSizeCombobox and the pixelFormatComboBox
void MainWindow::populateComboBoxes()
{
  // frameSizeCombobox
  if (ui->framesizeComboBox->count() != 0)
    ui->framesizeComboBox->clear();
  // Append "Custom Size" entry (index 0)
  ui->framesizeComboBox->addItem("Custom Size");
  foreach(frameSizePreset p, MainWindow::presetFrameSizesList()) {
    // Convert the frameSizePreset to string and add it
    QString str = QString("%1 (%2,%3)").arg(p.first).arg(p.second.width()).arg(p.second.height());
    ui->framesizeComboBox->addItem(str);
  }

  // pixelFormatComboBox
  if (ui->pixelFormatComboBox->count() != 0)
    ui->pixelFormatComboBox->clear();
  for (unsigned int i = 0; i < YUVFile::pixelFormatList().size(); i++) {
    YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)i;
    if (pixelFormat != YUVC_UnknownPixelFormat && YUVFile::pixelFormatList().count(pixelFormat))
    {
      ui->pixelFormatComboBox->addItem(YUVFile::pixelFormatList().at(pixelFormat).name());
    }
  }
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
    QFileInfo fileInfo(file);

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
            PlaylistItem *newPlayListItemText = new PlaylistItem(PlaylistItem_Text, text, p_playlistWidget);
            QSharedPointer<TextObject> newText = newPlayListItemText->getTextObject();
            newText->setFont(font);
            newText->setDuration(duration);
            newText->setColor(color);
        }
        else if(itemInfo["Class"].toString() == "YUVFile")
        {
            QString fileURL = itemProps["URL"].toString();
            QString filePath = QUrl(fileURL).path();

            QString relativeURL = itemProps["rURL"].toString();
            QFileInfo checkAbsoluteFile(filePath);
            // check if file with absolute path exists, otherwise check relative path
            if (!checkAbsoluteFile.exists())
            {
                QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
                QFileInfo checkRelativeFile(combinePath);
                if (checkRelativeFile.exists() && checkRelativeFile.isFile())
                {
                    filePath = QDir::cleanPath(combinePath);
                }
                else
                {
                    QMessageBox msgBox;
                    msgBox.setTextFormat(Qt::RichText);
                    msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
                    msgBox.exec();
                    break;
                }
            }

            int endFrame = itemProps["endFrame"].toInt();
            int frameOffset = itemProps["frameOffset"].toInt();
            int frameSampling = itemProps["frameSampling"].toInt();
            float frameRate = itemProps["framerate"].toFloat();
            YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)itemProps["pixelFormat"].toInt();
            int height = itemProps["height"].toInt();
            int width = itemProps["width"].toInt();


            // create video item and set properties
            PlaylistItem *newPlayListItemVideo = new PlaylistItem(PlaylistItem_Video, filePath, p_playlistWidget);
            QSharedPointer<FrameObject> newVideo = newPlayListItemVideo->getFrameObject();
            newVideo->setSize(width, height);
            newVideo->setSrcPixelFormat(pixelFormat);
            newVideo->setFrameRate(frameRate);
            newVideo->setSampling(frameSampling);
            newVideo->setStartFrame(frameOffset);
            newVideo->setEndFrame(endFrame);

            // load potentially associated statistics file
            if( itemProps.contains("statistics") )
            {
                QVariantMap itemInfoAssoc = itemProps["statistics"].toMap();
                QVariantMap itemPropsAssoc = itemInfoAssoc["Properties"].toMap();

                QString fileURL = itemProps["URL"].toString();
                QString filePath = QUrl(fileURL).path();

                QString relativeURL = itemProps["rURL"].toString();
                QFileInfo checkAbsoluteFile(filePath);
                // check if file with absolute path exists, otherwise check relative path
                if (!checkAbsoluteFile.exists())
                {
                    QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
                    QFileInfo checkRelativeFile(combinePath);
                    if (checkRelativeFile.exists() && checkRelativeFile.isFile())
                    {
                        filePath = QDir::cleanPath(combinePath);
                    }
                    else
                    {
                        QMessageBox msgBox;
                        msgBox.setTextFormat(Qt::RichText);
                        msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
                        msgBox.exec();
                        break;
                    }
                }

                int endFrame = itemPropsAssoc["endFrame"].toInt();
                int frameOffset = itemPropsAssoc["frameOffset"].toInt();
                int frameSampling = itemPropsAssoc["frameSampling"].toInt();
                float frameRate = itemPropsAssoc["framerate"].toFloat();
                int height = itemPropsAssoc["height"].toInt();
                int width = itemPropsAssoc["width"].toInt();
                QVariantList activeStatsTypeList = itemPropsAssoc["typesChecked"].toList();

                // create associated statistics item and set properties
                PlaylistItem *newPlayListItemStat = new PlaylistItem(PlaylistItem_Statistics, filePath, newPlayListItemVideo);
                QSharedPointer<StatisticsObject> newStat = newPlayListItemStat->getStatisticsObject();
                newStat->setSize(width, height);
                newStat->setFrameRate(frameRate);
                newStat->setSampling(frameSampling);
                newStat->setStartFrame(frameOffset);
                newStat->setEndFrame(endFrame);

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
                    newStat->setStatisticsTypeList(statsTypeList);
            }
        }
        else if(itemInfo["Class"].toString() == "StatisticsFile")
        {
            QString fileURL = itemProps["URL"].toString();
            QString filePath = QUrl(fileURL).path();

            QString relativeURL = itemProps["rURL"].toString();
            QFileInfo checkAbsoluteFile(filePath);
            // check if file with absolute path exists, otherwise check relative path
            if (!checkAbsoluteFile.exists())
            {
                QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
                QFileInfo checkRelativeFile(combinePath);
                if (checkRelativeFile.exists() && checkRelativeFile.isFile())
                {
                    filePath = QDir::cleanPath(combinePath);
                }
                else
                {
                    QMessageBox msgBox;
                    msgBox.setTextFormat(Qt::RichText);
                    msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
                    msgBox.exec();
                    break;
                }
            }

            int endFrame = itemProps["endFrame"].toInt();
            int frameOffset = itemProps["frameOffset"].toInt();
            int frameSampling = itemProps["frameSampling"].toInt();
            float frameRate = itemProps["framerate"].toFloat();
            int height = itemProps["height"].toInt();
            int width = itemProps["width"].toInt();
            QVariantList activeStatsTypeList = itemProps["typesChecked"].toList();

            // create statistics item and set properties
            PlaylistItem *newPlayListItemStat = new PlaylistItem(PlaylistItem_Statistics, filePath, p_playlistWidget);
            QSharedPointer<StatisticsObject> newStat = newPlayListItemStat->getStatisticsObject();
            newStat->setSize(width, height);
            newStat->setFrameRate(frameRate);
            newStat->setSampling(frameSampling);
            newStat->setStartFrame(frameOffset);
            newStat->setEndFrame(endFrame);

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
                newStat->setStatisticsTypeList(statsTypeList);
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
    QDir dirName(filename.section('/',0,-2));
    settings.setValue("LastPlaylistPath",dirName.path());

    QVariantList itemList;

    for(int i=0; i<p_playlistWidget->topLevelItemCount();i++)
    {
        PlaylistItem* anItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(i));

        QVariantMap itemInfo;
        QVariantMap itemProps;

        if( anItem->itemType() == PlaylistItem_Video )
        {
            QSharedPointer<FrameObject> vidItem = anItem->getFrameObject();
      
            itemInfo["Class"] = "YUVFile";

            QUrl fileURL(vidItem->path());
            fileURL.setScheme("file");
            QString relativePath = dirName.relativeFilePath(vidItem->path());
            itemProps["URL"] = fileURL.toString();
            itemProps["rURL"] = relativePath;
            itemProps["endFrame"] = vidItem->endFrame();
            itemProps["frameOffset"] = vidItem->startFrame();
            itemProps["frameSampling"] = vidItem->sampling();
            itemProps["framerate"] = vidItem->frameRate();
            itemProps["pixelFormat"] = vidItem->pixelFormat();
            itemProps["height"] = vidItem->height();
            itemProps["width"] = vidItem->width();

            // store potentially associated statistics file
            if (anItem->childCount() == 1)
            {
                QSharedPointer<StatisticsObject> statsItem = anItem->getStatisticsObject();
                
                Q_ASSERT(statsItem);

                QVariantMap itemInfoAssoc;
                itemInfoAssoc["Class"] = "AssociatedStatisticsFile";

                QVariantMap itemPropsAssoc;
                QUrl fileURL(statsItem->path());
                QString relativePath = dirName.relativeFilePath(statsItem->path());

                fileURL.setScheme("file");
                itemPropsAssoc["URL"] = fileURL.toString();
                itemProps["rURL"] = relativePath;
                itemPropsAssoc["endFrame"] = statsItem->endFrame();
                itemPropsAssoc["frameOffset"] = statsItem->startFrame();
                itemPropsAssoc["frameSampling"] = statsItem->sampling();
                itemPropsAssoc["framerate"] = statsItem->frameRate();
                itemPropsAssoc["height"] = statsItem->height();
                itemPropsAssoc["width"] = statsItem->width();

                // save active statistics types
                StatisticsTypeList statsTypeList = statsItem->getStatisticsTypeList();

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
        else if( anItem->itemType() == PlaylistItem_Text )
        {
            QSharedPointer<TextObject> textItem = anItem->getTextObject();
            
            itemInfo["Class"] = "TextFrameProvider";

            itemProps["duration"] = textItem->duration();
            itemProps["fontSize"] = textItem->font().pointSize();
            itemProps["fontName"] = textItem->font().family();
            itemProps["fontColor"]= textItem->color().name();
            itemProps["text"] = textItem->text();
        }
        else if( anItem->itemType() == PlaylistItem_Statistics )
        {
            QSharedPointer<StatisticsObject> statsItem = anItem->getStatisticsObject();
            
            itemInfo["Class"] = "StatisticsFile";

            QUrl fileURL(statsItem->path());
            QString relativePath = dirName.relativeFilePath(statsItem->path());

            fileURL.setScheme("file");
            itemProps["URL"] = fileURL.toString();
            itemProps["rURL"] = relativePath;
            itemProps["endFrame"] = statsItem->endFrame();
            itemProps["frameOffset"] = statsItem->startFrame();
            itemProps["frameSampling"] = statsItem->sampling();
            itemProps["framerate"] = statsItem->frameRate();
            itemProps["height"] = statsItem->height();
            itemProps["width"] = statsItem->width();

            // save active statistics types
            StatisticsTypeList statsTypeList = statsItem->getStatisticsTypeList();

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
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::loadFiles()";

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

      if (ext == "hevc")
      {
          // Open an hevc file
          PlaylistItem *newListItemVid = new PlaylistItem(PlaylistItem_Video, fileName, p_playlistWidget);
          lastAddedItem = newListItemVid;

          QSharedPointer<FrameObject> frmObj = newListItemVid->getFrameObject();
          QSharedPointer<de265File> dec = qSharedPointerDynamicCast<de265File>( frmObj->getSource() );
          if (dec->getStatisticsEnabled()) {
              // The library supports statistics.
              PlaylistItem *newListItemStats = new PlaylistItem(dec, newListItemVid);
              // Do not issue unused variable warning.
              // This is actually intentional. The new list item goes into the playlist
              // and just needs a pointer to the decoder.
              (void)newListItemStats;
          }

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
      if (ext == "yuv")
            {
                PlaylistItem *newListItemVid = new PlaylistItem(PlaylistItem_Video, fileName, p_playlistWidget);
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
                PlaylistItem *newListItemStats = new PlaylistItem(PlaylistItem_Statistics, fileName, p_playlistWidget);
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

/* Show the file open dialog.
 * The signal openButton->clicked is connected to this slot.
 */
void MainWindow::openFile()
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
        updatePixelFormatComboBoxSelectionCurrentSelection();

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
    PlaylistItem* newPlayListItemText = new PlaylistItem(PlaylistItem_Text, newTextObjectDialog.getText(), p_playlistWidget);
    QSharedPointer<TextObject> newText = newPlayListItemText->getTextObject();
        newText->setFont(newTextObjectDialog.getFont());
        newText->setDuration(newTextObjectDialog.getDuration());
        newText->setColor(newTextObjectDialog.getColor());
        p_playlistWidget->setCurrentItem(newPlayListItemText, 0, QItemSelectionModel::ClearAndSelect);
        p_playlistWidget->setIsSaved(false);
    }

}

void MainWindow::addDifferenceSequence()
{
    PlaylistItem* newPlayListItemDiff = new PlaylistItem(PlaylistItem_Difference, "Difference", p_playlistWidget);

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

    if( selectedItems.count() >= 1 )
    {
        bool found_parent = false;
        bool allowAssociatedItem = selectedItems.count() == 1;  // if only one item is selected, it can also be a child (associated)

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

/** A new item has been selected. Update all the controls (some might be enabled/disabled for this
  * type of object and the values probably changed). 
  * The signal playlistTreeWidget->itemSelectionChanged is connected to this slot.
  */
void MainWindow::updateSelectedItems()
{
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::updateSelectedItems()";

    // Get the selected item(s)
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
        ui->displaySplitView->setActiveDisplayObjects(QSharedPointer<DisplayObject>(), QSharedPointer<DisplayObject>());
        ui->displaySplitView->setActiveStatisticsObjects(QSharedPointer<StatisticsObject>(), QSharedPointer<StatisticsObject>());

        // update model
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setStatisticsTypeList( StatisticsTypeList() );

        setCurrentFrame(0);
        setControlsEnabled(false);

        return;
    }

    // Update the objects signal that something changed in the background
    if (selectedItemPrimary->displayObject() != previouslySelectedDisplayObject) {
        // New item was selected
        if (previouslySelectedDisplayObject != NULL) {
          // Disconnect old playlist Item
          QObject::disconnect(previouslySelectedDisplayObject.data(), SIGNAL(signal_objectInformationChanged()), NULL, NULL);
        }
        // Update last object
        previouslySelectedDisplayObject = selectedItemPrimary->displayObject();
        // Connect new object
        QObject::connect(previouslySelectedDisplayObject.data(), SIGNAL(signal_objectInformationChanged()), this, SLOT(currentSelectionInformationChanged()));
    }

    // update window caption
    QString newCaption = "YUView - " + selectedItemPrimary->text(0);
    setWindowTitle(newCaption);

    QSharedPointer<StatisticsObject> statsObject;    // used for model as source

    // if the newly selected primary (!) item is of type statistics, use it as source for types
    if( selectedItemPrimary && selectedItemPrimary->itemType() == PlaylistItem_Statistics )
    {
        statsObject = selectedItemPrimary->getStatisticsObject();
        Q_ASSERT(!statsObject.isNull());
    }
    else if (selectedItemSecondary && selectedItemSecondary->itemType() == PlaylistItem_Statistics)
    {
        statsObject = selectedItemSecondary->getStatisticsObject();
        Q_ASSERT(!statsObject.isNull());
    }

    // if selected item is of type 'diff', update child items
    if( selectedItemPrimary && selectedItemPrimary->itemType() == PlaylistItem_Difference && selectedItemPrimary->childCount() == 2 )
    {
        QSharedPointer<DifferenceObject> diffObject = selectedItemPrimary->getDifferenceObject();
        PlaylistItem* firstChild  = dynamic_cast<PlaylistItem*>(selectedItemPrimary->child(0));
        PlaylistItem* secondChild = dynamic_cast<PlaylistItem*>(selectedItemPrimary->child(1));
        QSharedPointer<FrameObject> firstVidObject = firstChild->getFrameObject();
        QSharedPointer<FrameObject> secondVidObject = secondChild->getFrameObject();

        //TO-Do: Implement this as a different function
        if (firstVidObject && secondVidObject)
            diffObject->setFrameObjects(firstVidObject, secondVidObject);
        ui->DifferencegroupBox->setVisible(true);
        ui->DifferencegroupBox->setEnabled(true);
        bool isChecked = ui->markDifferenceCheckBox->isChecked();
        QSettings settings;
        QColor color = settings.value("Difference/Color").value<QColor>();
        bool diff = diffObject->markDifferences(isChecked, color);
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
        else 
            ui->differenceLabel->setVisible(false);
     }

    // check for associated statistics
    QSharedPointer<StatisticsObject> statsItemPrimary;
    QSharedPointer<StatisticsObject> statsItemSecondary;
    if( selectedItemPrimary && selectedItemPrimary->itemType() == PlaylistItem_Video && selectedItemPrimary->childCount() > 0 )
    {
        PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedItemPrimary->child(0));
        statsItemPrimary = childItem->getStatisticsObject();

        Q_ASSERT(statsItemPrimary != NULL);

        if (statsObject == NULL)
            statsObject = statsItemPrimary;
    }
    if (selectedItemSecondary && selectedItemSecondary->itemType() == PlaylistItem_Video && selectedItemSecondary->childCount() > 0)
    {
        PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedItemSecondary->child(0));
        statsItemSecondary = childItem->getStatisticsObject();
    
        Q_ASSERT(statsItemSecondary != NULL);

        if (statsObject == NULL)
            statsObject = statsItemSecondary;
    }

    // update statistics mode, if statistics is selected or associated with a selected item
    if (statsObject != NULL)
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setStatisticsTypeList(statsObject->getStatisticsTypeList());

    // update display widget
    ui->displaySplitView->setActiveStatisticsObjects(statsItemPrimary, statsItemSecondary);

    if(selectedItemPrimary == NULL || selectedItemPrimary->displayObject() == NULL)
        return;

    // tell our display widget about new objects
    ui->displaySplitView->setActiveDisplayObjects(
      selectedItemPrimary ? selectedItemPrimary->displayObject() : QSharedPointer<DisplayObject>(), 
      selectedItemSecondary ? selectedItemSecondary->displayObject() : QSharedPointer<DisplayObject>() );

    // update playback controls
    setControlsEnabled(true);
    ui->fileDockWidget->setEnabled( selectedItemPrimary->itemType() != PlaylistItem_Text );
    ui->displayDockWidget->setEnabled( true );
    ui->YUVMathdockWidget->setEnabled(selectedItemPrimary->itemType() == PlaylistItem_Video || selectedItemPrimary->itemType() == PlaylistItem_Difference);
    ui->statsDockWidget->setEnabled(statsObject != NULL);

    // update displayed information
    updateSelectionMetaInfo();

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
    
    PlaylistItem* item = dynamic_cast<PlaylistItem*>(itemAtPoint);

    if (item->itemType() == PlaylistItem_Statistics)
        {
            // TODO: special actions for statistics items
        }
    if (item->itemType() == PlaylistItem_Video)
        {
            // TODO: special actions for video items
        }
    if (item->itemType() == PlaylistItem_Text)
        {
            menu.addAction("Edit Properties",this,SLOT(editTextFrame()));
        }
    if (item->itemType() == PlaylistItem_Difference)
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
    PlaylistItem* listItem = dynamic_cast<PlaylistItem*>(item);

    if (listItem->itemType() == PlaylistItem_Statistics)
    {
        // TODO: Double Click Behavior for Stats
    }
    if (listItem->itemType() == PlaylistItem_Video)
    {
        // TODO: Double Click Behavior for Video
    }
    if (listItem->itemType() == PlaylistItem_Text)
    {
        editTextFrame();
    }
    if (listItem->itemType() == PlaylistItem_Difference)
    {
        // TODO: Double Click Behavior for difference
    }
}

void MainWindow::editTextFrame()
{
    PlaylistItem* current = dynamic_cast<PlaylistItem*>(p_playlistWidget->currentItem());
    if (current->itemType() != PlaylistItem_Text)
        return;

    QSharedPointer<TextObject> text = current->getTextObject();

    EditTextDialog newTextObjectDialog;

    newTextObjectDialog.loadItemStettings(text);
    int done = newTextObjectDialog.exec();
    if (done==QDialog::Accepted)
    {
        text->setText(newTextObjectDialog.getText());
        text->setFont(newTextObjectDialog.getFont());
        text->setDuration(newTextObjectDialog.getDuration());
        text->setColor(newTextObjectDialog.getColor());
        current->setText(0,newTextObjectDialog.getText().replace("\n", " "));
        p_playlistWidget->setIsSaved(false);
        updateSelectedItems();
    }
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
    ui->opacitySlider->setValue(dynamic_cast<StatsListModel*>(ui->statsListView->model())->data(list.at(0), Qt::UserRole+1).toInt());
    ui->gridCheckBox->setChecked(dynamic_cast<StatsListModel*>(ui->statsListView->model())->data(list.at(0), Qt::UserRole+2).toBool());

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
    dynamic_cast<StatsListModel*>(ui->statsListView->model())->setData(list.at(0), val, Qt::UserRole+1);
}

/* Update the selected statistics item's option to draw a grid.
 * The signal gridCheckbox->toggle is connected to this slot.
*/
void MainWindow::updateStatsGrid(bool val)
{
    QModelIndexList list = ui->statsListView->selectionModel()->selectedIndexes();
    if (list.size() < 1)
        return;
    dynamic_cast<StatsListModel*>(ui->statsListView->model())->setData(list.at(0), val, Qt::UserRole+2);
}

/* Set the current frame to show.
 * The signal frameSLider->valueChanged is connected to this slot.
 * The signal frameCounterSpinBox->valueChanged is connected to this slot.
 */
void MainWindow::setCurrentFrame(int frame, bool bForceRefresh)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::setCurrentFrame()";

    if (selectedPrimaryPlaylistItem() == NULL || selectedPrimaryPlaylistItem()->displayObject() == NULL)
    {
        p_currentFrame = 0;
        ui->displaySplitView->clear();
        return;
    }

    if (frame != p_currentFrame || bForceRefresh)
    {
        // get real frame index
        int objEndFrame = selectedPrimaryPlaylistItem()->displayObject()->endFrame();
        if( frame < selectedPrimaryPlaylistItem()->displayObject()->startFrame() )
            frame = selectedPrimaryPlaylistItem()->displayObject()->startFrame();
        else if (objEndFrame >= 0 && frame > objEndFrame)
            // Don't change the frame.
            return;	// TODO: Shouldn't this set it to the last frame?
        
        p_currentFrame = frame;

        // update frame index in GUI without toggeling more signals
        updateFrameControls();

        // draw new frame
        ui->displaySplitView->drawFrame(p_currentFrame);
    }
}

/* Update the frame controls (spin box and slider) to p_currentFrame without toggeling more signals/slots.
*/
void MainWindow::updateFrameControls()
{
    // Temporarily disconnect signal/slots
    QObject::disconnect(ui->frameCounterSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    QObject::disconnect(ui->frameSlider, SIGNAL(valueChanged(int)), NULL, NULL);

    // This will cause two additional calls to this function which will not do much since (frame == p_currentFrame).
    ui->frameCounterSpinBox->setValue(p_currentFrame);
    ui->frameSlider->setValue(p_currentFrame);

    // Reconnect signals from the controls
    QObject::connect(ui->frameCounterSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));
    QObject::connect(ui->frameSlider, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));
}

/* Update the GUI controls for the selected item.
 * This function will disconnect all the signals from the GUI controls, update their values
 * and then reconnect everything. It will also update the fileInfo froup box.
 */
void MainWindow::updateSelectionMetaInfo()
{
    if (selectedPrimaryPlaylistItem() == NULL || selectedPrimaryPlaylistItem()->displayObject() == NULL)
    // Nothing selected.
        return;

    // Temporarily (!) disconnect slots/signals of file options panel
    QObject::disconnect( ui->widthSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->heightSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->startoffsetSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->rateSpinBox, SIGNAL(valueChanged(double)), NULL, NULL );
    QObject::disconnect( ui->samplingSpinBox, SIGNAL(valueChanged(int)), NULL, NULL );
    QObject::disconnect( ui->framesizeComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL );
    QObject::disconnect( ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL );

    // Update the file info labels from the selected item
    PlaylistItem *plItem = selectedPrimaryPlaylistItem();
    ui->fileInfo->setFileInfo(plItem->getInfoTitel(), plItem->getInfoList());
  
    // update GUI with information from primary selected playlist item
    ui->widthSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->width());
    ui->heightSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->height());
    ui->startoffsetSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->startFrame());
    ui->endSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->endFrame());
    ui->rateSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->frameRate());
    ui->samplingSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->sampling());
    updateFrameSizeComboBoxSelection();
    PlaylistItem* item = dynamic_cast<PlaylistItem*>(selectedPrimaryPlaylistItem());
    if (item->itemType() == PlaylistItem_Video)
    {
        QSharedPointer<FrameObject> video = item->getFrameObject();
        ui->pixelFormatComboBox->setCurrentIndex(video->pixelFormat() - 1);
    }

    // Reconnect slots/signals of info panel
    QObject::connect(ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->startoffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->framesizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_fileOptionValueChanged()));
    QObject::connect(ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_fileOptionValueChanged()));
}

/* A file option was changed by the user (width/height/start/end/rate/sampling/frameSize/pixelFormat).
 * All of these controls are connected to this slot. This slot should only be called when the user changed
 * something by using the GUI. If the value of one of these controls is to be changed (by software), don't
 * forget to disconnect the signal/slot first so this slot is not called (see function updateSelectionMetaInfo).
 *
 * This function changes the value in the currently selected displayObject and then updates all controls in
 * the file options.
 */
void MainWindow::on_fileOptionValueChanged()
{
  if (selectedPrimaryPlaylistItem() == NULL || selectedPrimaryPlaylistItem()->displayObject() == NULL)
    // Nothing selected.
    return;

  foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems()) {
    PlaylistItem* playlistItem = dynamic_cast<PlaylistItem*>(item);
    if ((ui->widthSpinBox == QObject::sender()) || (ui->heightSpinBox == QObject::sender()))
      playlistItem->displayObject()->setSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
    else if ((ui->startoffsetSpinBox == QObject::sender()))
      playlistItem->displayObject()->setStartFrame(ui->startoffsetSpinBox->value());
    else if (ui->endSpinBox == QObject::sender())
      playlistItem->displayObject()->setEndFrame(ui->endSpinBox->value());
    else if(ui->rateSpinBox == QObject::sender())
      playlistItem->displayObject()->setFrameRate(ui->rateSpinBox->value());
    else if (ui->samplingSpinBox == QObject::sender())
      playlistItem->displayObject()->setSampling(ui->samplingSpinBox->value());
    else if (ui->framesizeComboBox == QObject::sender()) {
      int width, height;
      convertFrameSizeComboBoxIndexToSize(&width, &height);
      playlistItem->displayObject()->setSize(width, height);
    }
    else if (ui->pixelFormatComboBox == QObject::sender()) {
      PlaylistItem* plItem = dynamic_cast<PlaylistItem*>(item);
      if (plItem->itemType() == PlaylistItem_Video) {
        YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)(ui->pixelFormatComboBox->currentIndex() + 1);
        QSharedPointer<FrameObject> video = plItem->getFrameObject();
        video->setSrcPixelFormat(pixelFormat);
      }
    }
  }

  // Call updateSelectionMetaInfo to update all the file option controls without calling this function again.
  updateSelectionMetaInfo();

  // Update playback widgets. The number of frames or start/end frame could have changed.
  // This will also force an update of the current frame.
  refreshPlaybackWidgets();
}

/* The information of the currently selected item changed in the background.
 * We need to update the metadata of the selected item.
*/
void MainWindow::currentSelectionInformationChanged()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::currentSelectionInformationChanged()";

    // update displayed information
  updateSelectionMetaInfo();

    // Refresh the playback widget
    refreshPlaybackWidgets();
}

void MainWindow::refreshPlaybackWidgets()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::refreshPlaybackWidgets()";

  // don't do anything if not yet initialized
  if (selectedPrimaryPlaylistItem() == NULL)
      return;

  // update information about newly selected video

  // update our timer
  int newInterval = 1000.0 / selectedPrimaryPlaylistItem()->displayObject()->frameRate();
  if (p_timerRunning && newInterval != p_timerInterval) {
    // The timer interval changed while the timer is running. Update it.
    killTimer(p_timerId);
    p_timerInterval = newInterval;
    p_timerId = startTimer(p_timerInterval, Qt::PreciseTimer);
  }

  int minFrameIdx, maxFrameIdx;
  selectedPrimaryPlaylistItem()->displayObject()->frameIndexLimits(minFrameIdx, maxFrameIdx);
  
    ui->frameSlider->setMinimum( minFrameIdx );
    ui->frameSlider->setMaximum( maxFrameIdx );

  int objEndFrame = selectedPrimaryPlaylistItem()->displayObject()->endFrame();
  if (ui->endSpinBox->value() != objEndFrame)
    ui->endSpinBox->setValue(objEndFrame);

    int modifiedFrame = p_currentFrame;

  if (p_currentFrame < minFrameIdx)
    modifiedFrame = minFrameIdx;
  else if (p_currentFrame > maxFrameIdx)
    modifiedFrame = maxFrameIdx;

    // make sure that changed info is resembled in display frame - might be due to changes to playback range
    setCurrentFrame(modifiedFrame, true);
}

/* Toggle play/pause
 * The signal playButton->clicked is connected to this slot.
 */
void MainWindow::togglePlayback()
{
    if (p_timerRunning)
        pause();
    else {
        if (selectedPrimaryPlaylistItem()) {
            int minIdx, maxIdx;
            selectedPrimaryPlaylistItem()->displayObject()->frameIndexLimits(minIdx, maxIdx);
            if (p_currentFrame >= maxIdx && p_repeatMode == RepeatModeOff) {
              setCurrentFrame(minIdx);
            }
            play();
        }
    }
}

void MainWindow::play()
{
    // check first if we are already playing or if there is something selected to play
    if ( p_timerRunning || !selectedPrimaryPlaylistItem() || !selectedPrimaryPlaylistItem()->displayObject())
        return;

    // start playing with timer
    double frameRate = selectedPrimaryPlaylistItem()->displayObject()->frameRate();
    if(frameRate < 0.00001) frameRate = 1.0;
    p_timerInterval = 1000.0 / frameRate;
    p_timerId = startTimer(p_timerInterval, Qt::PreciseTimer);
    p_timerRunning = true;
    p_timerLastFPSTime = QTime::currentTime();
    p_timerFPSCounter = 0;

    // update our play/pause icon
    ui->playButton->setIcon(p_pauseIcon);
}

void MainWindow::pause()
{
    // stop the play timer
    if (p_timerRunning) {
        killTimer(p_timerId);
        p_timerRunning = false;
    }

    // update our play/pause icon
    ui->playButton->setIcon(p_playIcon);
}

/** Stop playback and rewind to the start of the selected sequence.
  * stopButton->clicked() is connected here.
  */
void MainWindow::stop()
{
    // stop the play timer
    if (p_timerRunning) {
        killTimer(p_timerId);
        p_timerRunning = false;
    }

    // reset our video
    if( isPlaylistItemSelected() )
        setCurrentFrame( selectedPrimaryPlaylistItem()->displayObject()->startFrame() );

    // update our play/pause icon
    ui->playButton->setIcon(p_playIcon);
}

void MainWindow::deleteItem()
{
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::deleteItem()";

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
            itemToRemove = parentItem->takeChild(idx);
            delete itemToRemove;
            previouslySelectedDisplayObject = QSharedPointer<DisplayObject>();
            p_playlistWidget->setItemSelected(parentItem, true);
        }
        else
        {
            int idx = p_playlistWidget->indexOfTopLevelItem(selectedList.at(i));

            QTreeWidgetItem* itemToRemove = p_playlistWidget->takeTopLevelItem(idx);
            delete itemToRemove;
            previouslySelectedDisplayObject = QSharedPointer<DisplayObject>();
            int nextIdx = MAX(MIN(idx,p_playlistWidget->topLevelItemCount()-1), 0);

            QTreeWidgetItem* nextItem = p_playlistWidget->topLevelItem(nextIdx);
            if( nextItem )
            {
                p_playlistWidget->setItemSelected(nextItem, true);
            }
        }
    }
    if (p_playlistWidget->topLevelItemCount()==0)
    {
        // playlist empty, we dont need to save anymore
        p_playlistWidget->setIsSaved(true);
        ui->markDifferenceCheckBox->setChecked(false);
    }
}

/** Update (activate/deactivate) the grid (Draw Grid).
  * The signal regularGridCheckBox->clicked is connected to this slot.
  */
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
        //QKeyEvent* keyEvent = (QKeyEvent*)event;
        //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<<keyEvent<<"Object: "<<target;
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
    QSettings settings;

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
        restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
        restoreState(settings.value("mainWindow/windowState").toByteArray());
    }
    else
    {
        settings.setValue("mainWindow/geometry", saveGeometry());
        settings.setValue("mainWindow/windowState", saveState());

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

/* The playback timer has fired. Update the frame counter.
 * Also update the frame rate label every second.
 */
void MainWindow::timerEvent(QTimerEvent * event)
{
  if (event->timerId() != p_timerId)
    return;
    //qDebug() << "Other timer event!!";

    if(!isPlaylistItemSelected())
        return stop();

    // if we reached the end of a sequence, react...
    int nextFrame = p_currentFrame + selectedPrimaryPlaylistItem()->displayObject()->sampling();
    int minIdx, maxIdx;
    selectedPrimaryPlaylistItem()->displayObject()->frameIndexLimits(minIdx, maxIdx);
    if (nextFrame > maxIdx)
    {
        switch(p_repeatMode)
        {
        case RepeatModeOff:
            pause();
            if (p_ClearFrame)
                ui->displaySplitView->drawFrame(INT_INVALID);
            break;
        case RepeatModeOne:
            setCurrentFrame(minIdx);
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

  // FPS counter. Every 50th call of this function update the FPS counter.
  p_timerFPSCounter++;
  if (p_timerFPSCounter > 50) {
    QTime newFrameTime = QTime::currentTime();
    float msecsSinceLastUpdate = (float)p_timerLastFPSTime.msecsTo(newFrameTime);

    int framesPerSec = (int)(50 / (msecsSinceLastUpdate / 1000.0));
    if (framesPerSec > 0)
      ui->frameRateLabel->setText(QString().setNum(framesPerSec));

    p_timerLastFPSTime = QTime::currentTime();
    p_timerFPSCounter = 0;
  }
}

/** Toggle the repeat mode (loop through the list)
  * The signal repeatButton->clicked() is connected to this slot
  */
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

void MainWindow::convertFrameSizeComboBoxIndexToSize(int *width, int*height)
{
  int index = ui->framesizeComboBox->currentIndex();
  
  if (index <= 0 || (index-1) >= presetFrameSizesList().size()) {
    // "Custom Size" or non valid index
    *width = -1;
    *height = -1;
    return;
  }

  // Convert the index to width/height using the presetFrameSizesList
  frameSizePreset p = presetFrameSizesList().at(index - 1);
  *width = p.second.width();
  *height = p.second.height();
}

/// TODO: Should this also be in updateSelectionMetaInfo and the signal/slot?
void MainWindow::on_interpolationComboBox_currentIndexChanged(int index)
{
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::on_interpolationComboBox_currentIndexChanged(int " << index << ")";

    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == PlaylistItem_Video )
        {
            QSharedPointer<FrameObject> viditem = item->getFrameObject();
            Q_ASSERT(viditem);

            viditem->setInterpolationMode((InterpolationMode)index);
        }
    }
}

void MainWindow::statsTypesChanged()
{
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::statsTypesChanged()";

    // update all displayed statistics of primary item
    bool bUpdateNeeded = false;
    if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == PlaylistItem_Statistics)
    {
        QSharedPointer<StatisticsObject> statsItem = selectedPrimaryPlaylistItem()->getStatisticsObject();
        Q_ASSERT(statsItem);

        // update list of activated types
        bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }
    else if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == PlaylistItem_Video && selectedPrimaryPlaylistItem()->childCount() > 0 )
    {
        PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedPrimaryPlaylistItem()->child(0));
        Q_ASSERT(childItem->itemType() == PlaylistItem_Statistics);

        // update list of activated types
        QSharedPointer<StatisticsObject> statsItem = childItem->getStatisticsObject();
        bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }

    // update all displayed statistics of secondary item
    if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == PlaylistItem_Statistics)
    {
        QSharedPointer<StatisticsObject> statsItem = selectedSecondaryPlaylistItem()->getStatisticsObject();
        Q_ASSERT(statsItem);

        // update list of activated types
        bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }
    else if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == PlaylistItem_Video && selectedSecondaryPlaylistItem()->childCount() > 0 )
    {
        PlaylistItem* childItem = dynamic_cast<PlaylistItem*>(selectedSecondaryPlaylistItem()->child(0));
        Q_ASSERT(childItem->itemType() == PlaylistItem_Statistics);
            
        // update list of activated types
        QSharedPointer<StatisticsObject> statsItem = childItem->getStatisticsObject();
        bUpdateNeeded |= statsItem->setStatisticsTypeList(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatisticsTypeList());
    }

  if (bUpdateNeeded) {
    // refresh display widget
    ui->displaySplitView->drawFrame(p_currentFrame);
  }
}

/* Update the frame size combobox using the values that are set in the width/height spinboxes.
*/
void MainWindow::updateFrameSizeComboBoxSelection()
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::updateFrameSizeComboBoxSelection()";

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

void MainWindow::updatePixelFormatComboBoxSelectionCurrentSelection()
{
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::updatePixelFormatComboBoxSelectionCurrentSelection()";

    PlaylistItem *item = selectedPrimaryPlaylistItem();

    if( item->itemType() == PlaylistItem_Video )
    {
        QSharedPointer<FrameObject> viditem = item->getFrameObject();
        Q_ASSERT(!viditem.isNull());

        YUVCPixelFormatType pixelFormat = viditem->pixelFormat();
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
    //flags |= Qt::WindowStaysOnTopHint;
    flags |= Qt::CustomizeWindowHint;

    about->setWindowFlags(flags);
    about->setReadOnly(true);
    about->setOpenExternalLinks(true);

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
    about->setFixedSize(QSize(900,800));

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
    ui->displaySplitView->clear();
    updateSelectedItems();
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
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setLumaScale(index);
        }
        else if (item->itemType() == PlaylistItem_Difference )
        {
            item->getDifferenceObject()->setLumaScale(index);
        }
    }
    refreshPlaybackWidgets();
}

void MainWindow::on_ChromaScaleSpinBox_valueChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setChromaVScale(index);
            item->getFrameObject()->setChromaUScale(index);
        }
        else if (item->itemType() == PlaylistItem_Difference )
        {
            item->getDifferenceObject()->setChromaVScale(index);
            item->getDifferenceObject()->setChromaUScale(index);
        }
    }
    refreshPlaybackWidgets();
}

void MainWindow::on_LumaOffsetSpinBox_valueChanged(int arg1)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setLumaOffset(arg1);
        }
        else if (item->itemType() == PlaylistItem_Difference )
        {
            item->getDifferenceObject()->setLumaOffset(arg1);
        }
    }
    refreshPlaybackWidgets();
}

void MainWindow::on_ChromaOffsetSpinBox_valueChanged(int arg1)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setChromaOffset(arg1);
        }
        else if (item->itemType() == PlaylistItem_Difference )
        {
            item->getDifferenceObject()->setChromaOffset(arg1);
        }
    }
    refreshPlaybackWidgets();
}

void MainWindow::on_LumaInvertCheckBox_toggled(bool checked)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setLumaInvert(checked);
        }
        else if (item->itemType() == PlaylistItem_Difference )
        {
            item->getDifferenceObject()->setLumaInvert(checked);
        }
    }
    refreshPlaybackWidgets();
}

void MainWindow::on_ChromaInvertCheckBox_toggled(bool checked)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setChromaInvert(checked);
        }
        else if (item->itemType() == PlaylistItem_Difference )
        {
            item->getDifferenceObject()->setChromaInvert(checked);
        }
    }
    refreshPlaybackWidgets();
}

void MainWindow::on_ColorComponentsComboBox_currentIndexChanged(int index)
{
    foreach(QTreeWidgetItem* treeitem, p_playlistWidget->selectedItems())
    {
        PlaylistItem* item = dynamic_cast<PlaylistItem*>(treeitem);

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
            if (item->itemType() == PlaylistItem_Video)
            {
                item->getFrameObject()->setChromaUScale(ui->ChromaScaleSpinBox->value());
                item->getFrameObject()->setChromaVScale(0);
            }
            if (item->itemType() == PlaylistItem_Difference)
            {
                item->getDifferenceObject()->setChromaUScale(ui->ChromaScaleSpinBox->value());
                item->getDifferenceObject()->setChromaVScale(0);
            }
            ui->ChromagroupBox->setDisabled(false);
            ui->LumagroupBox->setDisabled(true);
            break;
        case 3:
            on_LumaScaleSpinBox_valueChanged(0);
            on_LumaOffsetSpinBox_valueChanged(0);
            if (item->itemType() == PlaylistItem_Video)
            {
                item->getFrameObject()->setChromaUScale(0);
                item->getFrameObject()->setChromaVScale(ui->ChromaScaleSpinBox->value());
            }
            if (item->itemType() == PlaylistItem_Difference)
            {
                item->getDifferenceObject()->setChromaUScale(0);
                item->getDifferenceObject()->setChromaVScale(ui->ChromaScaleSpinBox->value());
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
    refreshPlaybackWidgets();
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
        if( item->itemType() == PlaylistItem_Video )
        {
            item->getFrameObject()->setColorConversionMode((YUVCColorConversionType)index);
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
