#include "mainwindow.h"
#include "displaywidget.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <math.h>
#include <QThread>
#include <QStringList>
#include <QInputDialog>
#include <QDesktopServices>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QByteArray>
#include <QDebug>

#include "playlistitemvid.h"
#include "playlistitemstats.h"
#include "playlistitemtext.h"
#include "statslistmodel.h"
#include "sliderdelegate.h"
#include "displaysplitwidget.h"
#include "plistparser.h"
#include "plistserializer.h"

#define BOX_YUV400      0
#define BOX_YUV411      1
#define BOX_YUV420_8    2
#define BOX_YUV420_10   3
#define BOX_YUV422      4
#define BOX_YUV444      5

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

#define GUI_INTERPOLATION_STEPS 20.0

QVector<StatisticsRenderItem> MainWindow::p_emptyTypes;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    QSettings settings;
    if(!settings.contains("Background/Color"))
        settings.setValue("Background/Color", QColor(128,128,128));
    if(!settings.contains("OverlayGrid/Color"))
        settings.setValue("OverlayGrid/Color", QColor(0,0,0));

    p_playlistWidget = NULL;
    ui->setupUi(this);

    statusBar()->hide();

    p_playlistWidget = ui->playlistTreeWidget;
    //connect(p_playlistWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(treeItemDoubleClicked(QTreeWidgetItem*, int)));
    p_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(p_playlistWidget, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
    p_playTimer = new QTimer(this);
    QObject::connect(p_playTimer, SIGNAL(timeout()), this, SLOT(frameTimerEvent()));
    p_playTimer->setSingleShot(false);
#if QT_VERSION > 0x050000
    p_playTimer->setTimerType(Qt::PreciseTimer);
#endif

    // change the cursor over the splitter to ArrowCursor
    ui->displaySplitView->handle(1)->setCursor(Qt::ArrowCursor);

    p_currentFrame = 0;

    p_playIcon = QIcon(":images/img_play.png");
    p_pauseIcon = QIcon(":images/img_pause.png");
    p_repeatIcon = QIcon(":images/img_repeat.png");
    p_repeatOnIcon = QIcon(":images/img_repeat_on.png");

    p_numFrames = 1;
    p_repeat = true;

    ui->playlistTreeWidget->header()->resizeSection(1, 45);

    createMenusAndActions();

    StatsListModel *model= new StatsListModel(this);
    this->ui->statsListView->setModel(model);
    QObject::connect(model, SIGNAL(signalStatsTypesChanged()), this, SLOT(statsTypesChanged()));

    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    ui->deleteButton->setEnabled(false);
    ui->statsGroupBox->setEnabled(false);
    ui->opacitySlider->setEnabled(false);
    ui->gridCheckBox->setEnabled(false);
    QObject::connect(&p_settingswindow, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
    updateSettings();
}

void MainWindow::createMenusAndActions()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    openYUVFileAction = fileMenu->addAction("&Open YUV File", this, SLOT(openFile()),Qt::CTRL + Qt::Key_O);
    openStatisticsFileAction = fileMenu->addAction("&Open Statistics File", this, SLOT(openStatsFile()) );
    addTextAction = fileMenu->addAction("&Add Text Frame",this,SLOT(addTextFrame()));
    fileMenu->addSeparator();
    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActs[i] = new QAction(this);
        recentFileActs[i]->setVisible(false);
        connect(recentFileActs[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
        fileMenu->addAction(recentFileActs[i]);
    }
    fileMenu->addSeparator();
    savePlaylistAction = fileMenu->addAction("&Save Playlist...", this, SLOT(savePlaylistToFile()),Qt::CTRL + Qt::Key_S);
    fileMenu->addSeparator();
    saveScreenshotAction = fileMenu->addAction("&Save Screenshot", this, SLOT(saveScreenshot()) );
    fileMenu->addSeparator();
    showSettingsAction = fileMenu->addAction("&Settings", &p_settingswindow, SLOT(show()) );

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    togglePlaylistAction = viewMenu->addAction("Hide/Show P&laylist", ui->playlistDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_L);
    toggleStatisticsAction = viewMenu->addAction("Hide/Show &Statistics", ui->statsDockWidget->toggleViewAction(), SLOT(trigger()));
    viewMenu->addSeparator();
    toggleFileOptionsAction = viewMenu->addAction("Hide/Show F&ile Options", ui->fileDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_I);
    toggleDisplayOptionsActions = viewMenu->addAction("Hide/Show &Display Options", ui->displayDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_D);
    viewMenu->addSeparator();
    toggleControlsAction = viewMenu->addAction("Hide/Show Playback &Controls", ui->controlsDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_P);
    viewMenu->addSeparator();
    toggleFullscreenAction = viewMenu->addAction("&Fullscreen", this, SLOT(toggleFullscreen()),Qt::CTRL + Qt::Key_F);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    aboutAction = helpMenu->addAction("About", this, SLOT(showAbout()));
    bugReportAction = helpMenu->addAction("Report a Bug", this, SLOT(bugreport()));
    featureRequestAction = helpMenu->addAction("Request a Feature", this, SLOT(bugreport()));

    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 %2").arg(i + 1).arg(strippedName(files[i]));
        recentFileActs[i]->setText(text);
        recentFileActs[i]->setData(files[i]);
        recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
        recentFileActs[j]->setVisible(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
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
            int fontSize = itemProps["fontSize"].toInt();
            QString text = itemProps["text"].toString();

            // create text item and set properties
            PlaylistItemText* newPlayListItemText = new PlaylistItemText(text, p_playlistWidget);
            //newPlayListItemText->displayObject()->setFontSize(fontSize);
            newPlayListItemText->displayObject()->setDuration(duration);
        }
        else if(itemInfo["Class"].toString() == "YUVFile")
        {
            QString fileURL = itemProps["URL"].toString();
            int frameCount = itemProps["frameCount"].toInt();
            int frameOffset = itemProps["frameOffset"].toInt();
            int frameSampling = itemProps["frameSampling"].toInt();
            float frameRate = itemProps["framerate"].toFloat();
            int pixelFormat = itemProps["pixelFormat"].toInt();
            int height = itemProps["height"].toInt();
            int width = itemProps["width"].toInt();

            QString filePath = QUrl(fileURL).path();

            // create video item and set properties
            PlaylistItemVid *newListItemVid = new PlaylistItemVid(filePath, p_playlistWidget);
            newListItemVid->displayObject()->setWidth(width);
            newListItemVid->displayObject()->setHeight(height);
            newListItemVid->displayObject()->setPixelFormat(pixelFormat);
            newListItemVid->displayObject()->setFrameRate(frameRate);
            newListItemVid->displayObject()->setSampling(frameSampling);
            newListItemVid->displayObject()->setStartFrame(frameOffset);
            newListItemVid->displayObject()->setNumFrames(frameCount);

            // load potentially associated statistics file
            if( itemProps.contains("statistics") )
            {
                QVariantMap itemInfoAssoc = itemProps["statistics"].toMap();
                QVariantMap itemPropsAssoc = itemInfoAssoc["Properties"].toMap();

                QString fileURL = itemPropsAssoc["URL"].toString();
                int frameCount = itemPropsAssoc["frameCount"].toInt();
                int frameOffset = itemPropsAssoc["frameOffset"].toInt();
                int frameSampling = itemPropsAssoc["frameSampling"].toInt();
                float frameRate = itemPropsAssoc["framerate"].toFloat();
                int height = itemPropsAssoc["height"].toInt();
                int width = itemPropsAssoc["width"].toInt();

                QString filePath = QUrl(fileURL).path();

                // create associated statistics item and set properties
                PlaylistItemStats *newListItemStats = new PlaylistItemStats(filePath, newListItemVid);
                newListItemStats->displayObject()->setWidth(width);
                newListItemStats->displayObject()->setHeight(height);
                newListItemStats->displayObject()->setFrameRate(frameRate);
                newListItemStats->displayObject()->setSampling(frameSampling);
                newListItemStats->displayObject()->setStartFrame(frameOffset);
                newListItemStats->displayObject()->setNumFrames(frameCount);
            }
        }
        else if(itemInfo["Class"].toString() == "StatisticsFile")
        {
            QString fileURL = itemProps["URL"].toString();
            int frameCount = itemProps["frameCount"].toInt();
            int frameOffset = itemProps["frameOffset"].toInt();
            int frameSampling = itemProps["frameSampling"].toInt();
            float frameRate = itemProps["framerate"].toFloat();
            int height = itemProps["height"].toInt();
            int width = itemProps["width"].toInt();

            QString filePath = QUrl(fileURL).path();

            // create statistics item and set properties
            PlaylistItemStats *newListItemStats = new PlaylistItemStats(filePath, p_playlistWidget);
            newListItemStats->displayObject()->setWidth(width);
            newListItemStats->displayObject()->setHeight(height);
            newListItemStats->displayObject()->setFrameRate(frameRate);
            newListItemStats->displayObject()->setSampling(frameSampling);
            newListItemStats->displayObject()->setStartFrame(frameOffset);
            newListItemStats->displayObject()->setNumFrames(frameCount);
        }
    }

    if( p_playlistWidget->topLevelItemCount() > 0 )
    {
        p_playlistWidget->clearSelection();
        p_playlistWidget->setItemSelected(p_playlistWidget->topLevelItem(0), true);
    }
}

void MainWindow::savePlaylistToFile()
{
    // ask user for file location
    QSettings settings;
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Playlist"), settings.value("LastPlaylistPath").toString(), tr("Playlist Files (*.yuvplaylist)"));

    if(filename.length() == 0)
        return;

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
            itemProps["frameCount"] = vidItem->displayObject()->numFrames();
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
                assert(statsItem);

                QVariantMap itemInfoAssoc;
                itemInfoAssoc["Class"] = "AssociatedStatisticsFile";

                QVariantMap itemPropsAssoc;
                QUrl fileURL(statsItem->displayObject()->path());
                fileURL.setScheme("file");
                itemPropsAssoc["URL"] = fileURL.toString();
                itemPropsAssoc["frameCount"] = statsItem->displayObject()->numFrames();
                itemPropsAssoc["frameOffset"] = statsItem->displayObject()->startFrame();
                itemPropsAssoc["frameSampling"] = statsItem->displayObject()->sampling();
                itemPropsAssoc["framerate"] = statsItem->displayObject()->frameRate();
                itemPropsAssoc["height"] = statsItem->displayObject()->height();
                itemPropsAssoc["width"] = statsItem->displayObject()->width();

                itemInfoAssoc["Properties"] = itemPropsAssoc;

                // link to video item
                itemProps["statistics"] = itemInfoAssoc;
            }
        }
        else if( anItem->itemType() == TextItemType )
        {
            PlaylistItemText* textItem = dynamic_cast<PlaylistItemText*>(anItem);

            itemInfo["Class"] = "TextFrameProvider";

            // TODO
//            itemProps["duration"] = textItem->displayObject()->duration();
//            itemProps["fontSize"] = textItem->displayObject()->fontSize();
//            itemProps["text"] = textItem->displayObject()->text();
        }
        else if( anItem->itemType() == StatisticsItemType )
        {
            PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(anItem);

            itemInfo["Class"] = "StatisticsFile";

            QUrl fileURL(statsItem->displayObject()->path());
            fileURL.setScheme("file");
            itemProps["URL"] = fileURL.toString();
            itemProps["frameCount"] = statsItem->displayObject()->numFrames();
            itemProps["frameOffset"] = statsItem->displayObject()->startFrame();
            itemProps["frameSampling"] = statsItem->displayObject()->sampling();
            itemProps["framerate"] = statsItem->displayObject()->frameRate();
            itemProps["height"] = statsItem->displayObject()->height();
            itemProps["width"] = statsItem->displayObject()->width();
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
}

void MainWindow::loadFiles(QStringList files)
{
    QStringList filter;

    // this might be used to associate a statistics item with a video item
    PlaylistItem* curItem = (PlaylistItem*)p_playlistWidget->currentItem();
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
                // TODO: check if sequence parameters are available in csv file

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
    p_playlistWidget->clearSelection();
    p_playlistWidget->setItemSelected(lastAddedItem, true);
}

void MainWindow::openFile()
{
    // load last used directory from QPreferences
    QSettings settings;
    QStringList filter;
    filter << "Video Files (*.yuv *.yuvplaylist)" << "All Files (*)";

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
        updateColorFormatComboBoxSelection(selectedPrimaryPlaylistItem());

        // resize player window to fit video size
        QRect screenRect = QDesktopWidget().availableGeometry();
        unsigned int newWidth = MIN( MAX( selectedPrimaryPlaylistItem()->displayObject()->width()+680, width() ), screenRect.width() );
        unsigned int newHeight = MIN( MAX( selectedPrimaryPlaylistItem()->displayObject()->height()+140, height() ), screenRect.height() );
        resize( newWidth, newHeight );
        /*
        QRect frect = frameGeometry();
        frect.moveCenter(screenRect.center());
        move(frect.topLeft());
        */
    }
}

void MainWindow::openStatsFile()
{
    // load last used directory from QPreferences
    QSettings settings;
    QStringList filter;
    filter << "CSV Files (*.csv)" << "All Files (*)";

    QFileDialog openDialog(this);
    openDialog.setDirectory(settings.value("LastCSVFilePath").toString());
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
        settings.setValue("LastCSVFilePath",filePath);
    }

    loadFiles(fileNames);
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
    bool ok;
     QString text = QInputDialog::getText(this, tr("Add Text Frame"),
                                          tr("Text:"), QLineEdit::Normal,
                                          tr("Text"), &ok);
     if (ok && !text.isEmpty())
     {
         PlaylistItemText* newPlayListItemText = new PlaylistItemText(text,p_playlistWidget);

         // select newly added item
         p_playlistWidget->clearSelection();
         p_playlistWidget->setItemSelected(newPlayListItemText, true);
     }
}

PlaylistItem* MainWindow::selectedPrimaryPlaylistItem()
{
    if( p_playlistWidget == NULL )
        return NULL;

    QList<QTreeWidgetItem*> selectedItems = p_playlistWidget->selectedItems();
    PlaylistItem* selectedItemPrimary = NULL;

    if( selectedItems.count() >= 1 )
    {
        foreach (QTreeWidgetItem* anItem, selectedItems)
        {
            // we search an item that does not have a parent
            if(!dynamic_cast<PlaylistItem*>(anItem->parent()))
            {
                selectedItemPrimary = dynamic_cast<PlaylistItem*>(anItem);
                break;
            }
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
            PlaylistItem* aPlaylistParentItem = dynamic_cast<PlaylistItem*>(anItem->parent());
            if(!aPlaylistParentItem && anItem != selectedItemPrimary)
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

    if( selectedItemPrimary == NULL  || selectedItemPrimary->displayObject() == NULL)
    {
        setWindowTitle("YUView");
        setCurrentFrame(0);
        setControlsEnabled(false);
        ui->fileDockWidget->setEnabled(false);
        ui->infoChromaBox->setEnabled(false);
        ui->gridBox->setEnabled(false);
        ui->deleteButton->setEnabled(false);

        ui->displaySplitView->setActiveDisplayObjects(NULL, NULL);
        ui->displaySplitView->setActiveStatisticsObjects(NULL, NULL);

        ui->displaySplitView->drawFrame(0);

        // update model
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setCurrentStatistics(NULL, p_emptyTypes);

        return;
    }

    // update window caption
    QString newCaption = "YUView - " + selectedItemPrimary->text(0);
    setWindowTitle(newCaption);

    PlaylistItemStats* statsItem = NULL;    // used for model as source

    // if the newly selected primary (!) item is of type statistics, update model of statistics list
    if( selectedItemPrimary && selectedItemPrimary->itemType() == StatisticsItemType )
    {
        statsItem = dynamic_cast<PlaylistItemStats*>(selectedItemPrimary);
        assert(statsItem != NULL);
    }
    else if( selectedItemSecondary && selectedItemSecondary->itemType() == StatisticsItemType )
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedItemSecondary);
        assert(statsItem != NULL);
    }

    // check for associated statistics
    PlaylistItemStats* statsItemPrimary = NULL;
    PlaylistItemStats* statsItemSecondary = NULL;
    if( selectedItemPrimary && selectedItemPrimary->itemType() == VideoItemType && selectedItemPrimary->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedItemPrimary->child(0);
        statsItemPrimary = dynamic_cast<PlaylistItemStats*>(childItem);
        assert(statsItemPrimary != NULL);

        if( statsItem == NULL )
            statsItem = statsItemPrimary;
    }
    if( selectedItemSecondary && selectedItemSecondary->itemType() == VideoItemType && selectedItemSecondary->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedItemSecondary->child(0);
        statsItemSecondary = dynamic_cast<PlaylistItemStats*>(childItem);
        assert(statsItemSecondary != NULL);

        if( statsItem == NULL )
            statsItem = statsItemSecondary;
    }

    // update statistics mode, if statistics is selected or associated with a selected item
    if( statsItem )
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setCurrentStatistics(statsItem->displayObject(), statsItem->displayObject()->getActiveStatsTypes());

    // update display widget
    ui->displaySplitView->setActiveStatisticsObjects(statsItemPrimary?statsItemPrimary->displayObject():NULL, statsItemSecondary?statsItemSecondary->displayObject():NULL);

    if(selectedItemPrimary == NULL || selectedItemPrimary->displayObject() == NULL)
        return;

    // tell our display widget about new objects
    ui->displaySplitView->setActiveDisplayObjects(selectedItemPrimary?selectedItemPrimary->displayObject():NULL, selectedItemSecondary?selectedItemSecondary->displayObject():NULL);

    // update playback controls
    setControlsEnabled(true);
    ui->fileDockWidget->setEnabled(true);
    ui->infoChromaBox->setEnabled(true);
    ui->gridBox->setEnabled(true);
    ui->interpolationComboBox->setEnabled(true);
    ui->deleteButton->setEnabled(true);

    // update displayed information
    updateMetaInfo();

    // update playback widgets
    refreshPlaybackWidgets();

    QObject::disconnect(ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_pixelFormatComboBox_currentIndexChanged(int)));
    updateColorFormatComboBoxSelection(selectedItemPrimary);
    QObject::connect(ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_pixelFormatComboBox_currentIndexChanged(int)));
}

void MainWindow::onCustomContextMenu(const QPoint &point)
{
    QTreeWidgetItem* item = p_playlistWidget->itemAt(point);
   if (item) {
   showContextMenu(item, p_playlistWidget->viewport()->mapToGlobal(point));
   }
}

void MainWindow::showContextMenu(QTreeWidgetItem* item, const QPoint& globalPos)
{
    QMenu menu;
    menu.addAction("Remove",this,SLOT(deleteItem()));

    PlaylistItemStats* testStats = dynamic_cast<PlaylistItemStats*>(item);
    if(testStats)
    {
        menu.addAction("Do Something");

    }
    PlaylistItemVid* testVid = dynamic_cast<PlaylistItemVid*>(item);
    if(testVid)
    {
        menu.addAction("Do Something");
    }
    PlaylistItemText* testText = dynamic_cast<PlaylistItemText*>(item);
    if(testText)
    {
        menu.addAction("Edit",this,SLOT(editTextFrame()));
    }

   QAction* selectedAction= menu.exec(globalPos);
   if (selectedAction)
   {
       printf("Do something \n");
   }
}

void MainWindow::editTextFrame()
{
    PlaylistItem* current =(PlaylistItem *)p_playlistWidget->currentItem();
    p_FrameObjectDialog.show();

}


void MainWindow::resizeEvent(QResizeEvent *event)
{
    // TODO?!
}

void MainWindow::setSelectedStats() {
    //deactivate all GUI elements
    ui->statsGroupBox->setEnabled(false);
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
    ui->statsGroupBox->setEnabled(true);

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
            p_currentFrame = selectedPrimaryPlaylistItem()->displayObject()->startFrame();
        else if( frame >= p_numFrames + selectedPrimaryPlaylistItem()->displayObject()->startFrame() )
            p_currentFrame = selectedPrimaryPlaylistItem()->displayObject()->startFrame() + p_numFrames - 1;
        else
            p_currentFrame = frame;

        // update fps counter - TODO: this can also be done with a lower frequency (e.g. heartbeat timer with 1 sec interval)
        if( p_FPSCounter%10 == 0 )
        {
            QTime newFrameTime = QTime::currentTime();
            int newSecondsPerFrame = p_lastFrameTime.msecsTo(newFrameTime);
            if (newSecondsPerFrame > 0)
                ui->frameRateLabel->setText(QString().setNum(int(1000*10 / newSecondsPerFrame)));
            p_lastFrameTime = newFrameTime;
        }
        p_FPSCounter++;

        // update frame index in GUI
        ui->frameCounter->setValue(p_currentFrame);
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
    if ((ui->widthSpinBox == QObject::sender()) || (ui->sizeComboBox == QObject::sender())) {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setWidth(ui->widthSpinBox->value());
        return;
    } else
    if ((ui->heightSpinBox == QObject::sender()) || (ui->sizeComboBox == QObject::sender())) {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setHeight(ui->heightSpinBox->value());
        return;
    } else
    if (ui->offsetSpinBox == QObject::sender()) {
        int maxFrames = findMaxNumFrames();

        if (ui->offsetSpinBox->value() >= maxFrames)
            ui->offsetSpinBox->setValue(maxFrames-1);

        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setStartFrame(ui->offsetSpinBox->value());

        if ((ui->framesSpinBox->value() != 0) && (ui->offsetSpinBox->value() + ui->framesSpinBox->value() > maxFrames))
            ui->framesSpinBox->setValue(maxFrames - ui->offsetSpinBox->value());
        if (ui->framesSpinBox->value() == 0) {
            p_numFrames = (ui->framesSpinBox->value() == 0) ? maxFrames - ui->offsetSpinBox->value() : ui->framesSpinBox->value();

            foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
                dynamic_cast<PlaylistItem*>(item)->displayObject()->setNumFrames(p_numFrames);
        }

        return;
    } else
    if (ui->framesSpinBox == QObject::sender()) {
        int maxFrames = findMaxNumFrames();

        if (ui->offsetSpinBox->value() + ui->framesSpinBox->value() > maxFrames) {
            ui->framesSpinBox->setValue(maxFrames - ui->offsetSpinBox->value());
        }
        p_numFrames = (ui->framesSpinBox->value() == 0) ? maxFrames - ui->offsetSpinBox->value() : ui->framesSpinBox->value();

        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems()) {
            PlaylistItem* YUVItem = dynamic_cast<PlaylistItem*>(item);
            YUVItem->displayObject()->setNumFrames(p_numFrames);
            YUVItem->displayObject()->setPlayUntilEnd(ui->framesSpinBox->value() == 0);
        }

        return;
    } else
    if (ui->rateSpinBox == QObject::sender()) {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setFrameRate(ui->rateSpinBox->value());
        // update timer
        if( p_playTimer->isActive() )
        {
            p_playTimer->stop();
            p_playTimer->start( 1000.0/( selectedPrimaryPlaylistItem()->displayObject()->frameRate() ) );
        }
        return;
    } else
    if (ui->samplingSpinBox == QObject::sender()) {
        foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
            dynamic_cast<PlaylistItem*>(item)->displayObject()->setSampling(ui->samplingSpinBox->value());
        return;
    }

    // Disconnect slots/signals of info panel
    QObject::disconnect( ui->widthSpinBox, SIGNAL(valueChanged(int)), 0, 0 );
    QObject::disconnect( ui->heightSpinBox, SIGNAL(valueChanged(int)), 0, 0 );
    QObject::disconnect( ui->offsetSpinBox, SIGNAL(valueChanged(int)), 0, 0 );
    QObject::disconnect( ui->framesSpinBox, SIGNAL(valueChanged(int)), 0, 0 );
    QObject::disconnect( ui->rateSpinBox, SIGNAL(valueChanged(double)), 0, 0 );
    QObject::disconnect( ui->samplingSpinBox, SIGNAL(valueChanged(int)), 0, 0 );

    if( selectedPrimaryPlaylistItem()->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(selectedPrimaryPlaylistItem());
        assert(viditem != NULL);

        ui->createdLineEdit->setText(viditem->displayObject()->createdtime());
        ui->modifiedLineEdit->setText(viditem->displayObject()->modifiedtime());
        ui->filePathEdit->setPlainText(viditem->displayObject()->path());
    }
    else if( selectedPrimaryPlaylistItem()->itemType() == StatisticsItemType )
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedPrimaryPlaylistItem());
        assert(statsItem != NULL);

        ui->createdLineEdit->setText(statsItem->displayObject()->createdtime());
        ui->modifiedLineEdit->setText(statsItem->displayObject()->modifiedtime());
        ui->filePathEdit->setPlainText(statsItem->displayObject()->path());
    }

    if (selectedPrimaryPlaylistItem()->displayObject()->playUntilEnd())
        ui->framesSpinBox->setValue(0);
    else
        ui->framesSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->numFrames());

    ui->widthSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->width());
    ui->heightSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->height());
    ui->rateSpinBox->setValue(selectedPrimaryPlaylistItem()->displayObject()->frameRate());

    // Connect slots/signals of info panel
    QObject::connect( ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->offsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->framesSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );

    QObject::connect( ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateFrameSizeComboBoxSelection()) );
    QObject::connect( ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateFrameSizeComboBoxSelection()) );
    QObject::connect(selectedPrimaryPlaylistItem()->displayObject(), SIGNAL(informationChanged(uint)), this, SLOT(refreshPlaybackWidgets()));

    updateFrameSizeComboBoxSelection();
}

void MainWindow::refreshPlaybackWidgets()
{
    // don't do anything if not yet initialized
    if (selectedPrimaryPlaylistItem() == NULL)
        return;

    // update information about newly selected video
    p_numFrames = (ui->framesSpinBox->value() == 0) ? findMaxNumFrames() - ui->offsetSpinBox->value() : ui->framesSpinBox->value();

    int minFrameIdx = MAX( 0, selectedPrimaryPlaylistItem()->displayObject()->startFrame() );
    int maxFrameIdx = MAX( minFrameIdx, selectedPrimaryPlaylistItem()->displayObject()->startFrame() + p_numFrames - 1 );
    ui->frameSlider->setMinimum( minFrameIdx );
    ui->frameSlider->setMaximum( maxFrameIdx );
    ui->framesSpinBox->setValue(p_numFrames);

    if( maxFrameIdx - minFrameIdx <= 0 )
    {
        // this is stupid, but the slider seems to have problems with a zero range!
        ui->frameSlider->setMaximum( maxFrameIdx+1 );
        ui->frameSlider->setEnabled(false);
        ui->frameCounter->setEnabled(false);
    }

    int modifiedFrame = p_currentFrame;

    if( p_currentFrame < selectedPrimaryPlaylistItem()->displayObject()->startFrame() )
        modifiedFrame = selectedPrimaryPlaylistItem()->displayObject()->startFrame();
    else if( p_currentFrame >= ( selectedPrimaryPlaylistItem()->displayObject()->startFrame() + p_numFrames ) )
        modifiedFrame = selectedPrimaryPlaylistItem()->displayObject()->startFrame() + p_numFrames - 1;

    // make sure that changed info is resembled in display frame
    setCurrentFrame(modifiedFrame, true);
}

void MainWindow::togglePlayback()
{
    if(p_playTimer->isActive())
        pause();
    else
        play();
}

void MainWindow::play()
{
    // check first if we are already playing
    if( p_playTimer->isActive() || !selectedPrimaryPlaylistItem() || !selectedPrimaryPlaylistItem()->displayObject() )
        return;

    p_FPSCounter = 0;

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

        if( parentItem != NULL )
        {
            int idx = parentItem->indexOfChild(selectedList.at(i));

            QTreeWidgetItem* itemToRemove = parentItem->takeChild(idx);
            delete itemToRemove;
        }
        else
        {
            int idx = p_playlistWidget->indexOfTopLevelItem(selectedList.at(i));

            QTreeWidgetItem* itemToRemove = p_playlistWidget->takeTopLevelItem(idx);
            delete itemToRemove;
        }
    }
}

void MainWindow::updateGrid() {
    bool enableGrid = ui->regularGridCheckBox->checkState() == Qt::Checked;
    unsigned char c[4];
    QSettings settings;
    QColor color = settings.value("OverlayGrid/Color").value<QColor>();
    c[0] = color.red();
    c[1] = color.green();
    c[2] = color.blue();
    c[3] = color.alpha();
    ui->displaySplitView->setRegularGridParameters(enableGrid, ui->gridSizeBox->value(), c);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // more keyboard shortcuts can be implemented here...
    switch(event->key())
    {
    case Qt::Key_F:
    {
        if (event->modifiers()==Qt::ControlModifier)
            toggleFullscreen();
        break;
    }
    case Qt::Key_L:
    case Qt::Key_P:
    {
        if (event->modifiers()==Qt::ControlModifier)
            ui->playlistDockWidget->toggleViewAction()->trigger();
        break;
    }
    case Qt::Key_R:
    case Qt::Key_I:
    {
        if (event->modifiers()==Qt::ControlModifier)
            ui->fileDockWidget->toggleViewAction()->trigger();
        break;
    }
    case Qt::Key_S:
    {
        if (event->modifiers()==Qt::ControlModifier)
            ui->statsDockWidget->toggleViewAction()->trigger();
        break;
    }
    case Qt::Key_Escape:
    {
        if(isFullScreen())
            toggleFullscreen();
        break;
    }
    case Qt::Key_Space:
    {
        togglePlayback();
        break;
    }
    case Qt::Key_Left:
    {
        setCurrentFrame( p_currentFrame-1 );
        break;
    }
    case Qt::Key_Right:
    {
        setCurrentFrame( p_currentFrame+1 );
        break;
    }
    /*
    case Qt::Key_Up:
    {
        QList<QTreeWidgetItem*> selectedList = p_playlistWidget->selectedItems();

        if( selectedList.count() == 0 || selectedList.count() > 1)
            return;
        else
        {

            if ( p_playlistWidget->itemAbove( selectedList.at(0)) == NULL)
                p_playlistWidget->setCurrentItem( selectedList.at(0) );
            else
                p_playlistWidget->setCurrentItem( p_playlistWidget->itemAbove( selectedList.at(0) ));
        }
        break;
    }
    case Qt::Key_Down:
    {
        QList<QTreeWidgetItem*> selectedList = p_playlistWidget->selectedItems();

        if( selectedList.count() == 0 || selectedList.count() > 1)
            return;
        else
        {
            if ( p_playlistWidget->itemBelow( selectedList.at(0)) == NULL)
                p_playlistWidget->setCurrentItem( selectedList.at(0) );
            else
                p_playlistWidget->setCurrentItem( p_playlistWidget->itemBelow( selectedList.at(0) ));
        }
            break;
    }
    */
    case Qt::Key_Plus:
    {
//        if (event->modifiers()==Qt::ControlModifier)
//        {
//            if( ui->renderWidget->isVisible() )
//                ui->renderWidget->zoomIn();
//            if( ui->renderWidget2->isVisible() )
//                ui->renderWidget2->zoomIn();
//            break;
//        }
    }
    case Qt::Key_Minus:
    {
//        if (event->modifiers()==Qt::ControlModifier)
//        {
//            if( ui->renderWidget->isVisible() )
//                ui->renderWidget->zoomOut();
//            if( ui->renderWidget2->isVisible() )
//                ui->renderWidget2->zoomOut();
//            break;
//        }
    }
    case Qt::Key_0:
    {
//        if (event->modifiers()==Qt::ControlModifier)
//        {
//            if( ui->renderWidget->isVisible() )
//                ui->renderWidget->zoomToStandard();
//            if( ui->renderWidget2->isVisible() )
//                ui->renderWidget2->zoomToStandard();
//            break;
//        }
    }
    case Qt::Key_9:
    {
//        if (event->modifiers()==Qt::ControlModifier)
//        {
//            if( ui->renderWidget->isVisible() )
//                ui->renderWidget->zoomToFit();
//            if( ui->renderWidget2->isVisible() )
//                ui->renderWidget2->zoomToFit();
//            break;
//        }
    }

    }
}

void MainWindow::moveEvent ( QMoveEvent* )
{
    // refresh
    //ui->displaySplitView->drawFrame(p_currentFrame);
}

void MainWindow::toggleFullscreen()
{
    if(isFullScreen())
    {
        // go back to windowed mode
        ui->centralLayout->setMargin(0);

        // show panels
        ui->fileDockWidget->show();
        ui->playlistDockWidget->show();
        ui->statsDockWidget->show();
        ui->displayDockWidget->show();

#ifndef QT_OS_MAC
        // show menu
        ui->menuBar->show();
#endif

        showNormal();

        // this is just stupid, but necessary!
//        QSize newSize = QSize(ui->renderWidget->size());
//        if( ui->renderWidget->isVisible() )
//        {
//            ui->renderWidget->resize( newSize - QSize(1,1) );
//            ui->renderWidget->resize( newSize );
//        }
    }
    else
    {
        // change to fullscreen mode
        ui->centralLayout->setMargin(0);

        // hide panels
        ui->fileDockWidget->hide();
        ui->playlistDockWidget->hide();
        ui->statsDockWidget->hide();
        ui->displayDockWidget->hide();

#ifndef QT_OS_MAC
        // hide menu
        ui->menuBar->hide();
#endif

        showFullScreen();

        // this is just stupid, but necessary!
//        QSize newSize = QSize(ui->renderWidget->size());
//        if( ui->renderWidget->isVisible() )
//        {
//            ui->renderWidget->resize( newSize - QSize(1,1) );
//            ui->renderWidget->resize( newSize );
//        }
    }
}

void MainWindow::setControlsEnabled(bool flag)
{
    ui->playButton->setEnabled(flag);
    ui->stopButton->setEnabled(flag);
    ui->frameSlider->setEnabled(flag);
    ui->frameCounter->setEnabled(flag);
}

void MainWindow::frameTimerEvent()
{
    if(!isPlaylistItemSelected())
        return stop();

    if (p_currentFrame >= selectedPrimaryPlaylistItem()->displayObject()->startFrame() + p_numFrames-1 )
    {
        // TODO: allow third 'loop' mode: loop playlist--> change to next item in playlist now.
        if(!p_repeat)
            pause();
        else
            setCurrentFrame( selectedPrimaryPlaylistItem()->displayObject()->startFrame() );
    }
    else
    {
        // update current frame
        setCurrentFrame( p_currentFrame + selectedPrimaryPlaylistItem()->displayObject()->sampling() );
    }
}

void MainWindow::toggleRepeat()
{
    p_repeat = !p_repeat;

    if(p_repeat)
    {
        ui->repeatButton->setIcon(p_repeatOnIcon);
    }
    else
    {
        ui->repeatButton->setIcon(p_repeatIcon);
    }
}


/////////////////////////////////////////////////////////////////////////////////

void MainWindow::on_sizeComboBox_currentIndexChanged(int index)
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
            assert(viditem != NULL);

            switch (index)
            {
            case BOX_YUV400:
                viditem->displayObject()->setPixelFormat(YUVC_8GrayPixelFormat);
                break;
            case BOX_YUV411:
                viditem->displayObject()->setPixelFormat(YUVC_411YpCbCr8PlanarPixelFormat);
                break;
            case BOX_YUV420_8:
                viditem->displayObject()->setPixelFormat(YUVC_420YpCbCr8PlanarPixelFormat);
                break;
            case BOX_YUV420_10:
                viditem->displayObject()->setPixelFormat(YUVC_420YpCbCr10LEPlanarPixelFormat);
                break;
            case BOX_YUV422:
                viditem->displayObject()->setPixelFormat(YUVC_422YpCbCr8PlanarPixelFormat);
                break;
            case BOX_YUV444:
                viditem->displayObject()->setPixelFormat(YUVC_444YpCbCr8PlanarPixelFormat);
                break;
            }
        }
    }
}

void MainWindow::on_interpolationComboBox_currentIndexChanged(int index)
{
    if (selectedPrimaryPlaylistItem() != NULL && selectedPrimaryPlaylistItem()->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(selectedPrimaryPlaylistItem());
        assert(viditem != NULL);

        viditem->displayObject()->setInterpolationMode(index);
    }
}

void MainWindow::statsTypesChanged()
{
    // update all displayed statistics of primary item
    if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == StatisticsItemType)
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedPrimaryPlaylistItem());
        assert(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setActiveStatsTypes(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatistics());
    }
    else if (selectedPrimaryPlaylistItem() && selectedPrimaryPlaylistItem()->itemType() == VideoItemType && selectedPrimaryPlaylistItem()->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedPrimaryPlaylistItem()->child(0);
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(childItem);
        assert(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setActiveStatsTypes(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatistics());
    }

    // update all displayed statistics of secondary item
    if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == StatisticsItemType)
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedSecondaryPlaylistItem());
        assert(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setActiveStatsTypes(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatistics());
    }
    else if (selectedSecondaryPlaylistItem() && selectedSecondaryPlaylistItem()->itemType() == VideoItemType && selectedSecondaryPlaylistItem()->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedSecondaryPlaylistItem()->child(0);
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(childItem);
        assert(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setActiveStatsTypes(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatistics());
    }

    // refresh display widget
    ui->displaySplitView->drawFrame(p_currentFrame);
}

void MainWindow::updateFrameSizeComboBoxSelection()
{
    int W = ui->widthSpinBox->value();
    int H = ui->heightSpinBox->value();

    if ( W == 176 && H == 144)
        ui->sizeComboBox->setCurrentIndex(1);
    else if ( W == 320 && H == 240 )
        ui->sizeComboBox->setCurrentIndex(2);
    else if ( W == 416 && H == 240 )
        ui->sizeComboBox->setCurrentIndex(3);
    else if ( W == 352 && H == 288 )
        ui->sizeComboBox->setCurrentIndex(4);
    else if ( W == 640 && H == 480 )
        ui->sizeComboBox->setCurrentIndex(5);
    else if ( W == 832 && H == 480 )
        ui->sizeComboBox->setCurrentIndex(6);
    else if ( W == 704 && H == 576 )
        ui->sizeComboBox->setCurrentIndex(7);
    else if ( W == 720 && H == 576 )
        ui->sizeComboBox->setCurrentIndex(8);
    else if ( W == 1280 && H == 720 )
        ui->sizeComboBox->setCurrentIndex(9);
    else if ( W == 1920 && H == 1080 )
        ui->sizeComboBox->setCurrentIndex(10);
    else if ( W == 3840 && H == 2160 )
        ui->sizeComboBox->setCurrentIndex(11);
    else if ( W == 1024 && H == 768 )
        ui->sizeComboBox->setCurrentIndex(12);
    else if ( W == 1280 && H == 960 )
        ui->sizeComboBox->setCurrentIndex(13);
    else
        ui->sizeComboBox->setCurrentIndex(0);
}

void MainWindow::updateColorFormatComboBoxSelection(PlaylistItem* selectedItem)
{
    PlaylistItem* item = dynamic_cast<PlaylistItem*>(selectedItem);
    if( item->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(item);
        assert(viditem != NULL);

        YUVCPixelFormatType pixelFormat = viditem->displayObject()->pixelFormat();

        if ( pixelFormat == YUVC_8GrayPixelFormat)
            ui->pixelFormatComboBox->setCurrentIndex(BOX_YUV400);
        else if ( pixelFormat == YUVC_411YpCbCr8PlanarPixelFormat)
            ui->pixelFormatComboBox->setCurrentIndex(BOX_YUV411);
        else if ( pixelFormat == YUVC_420YpCbCr8PlanarPixelFormat)
            ui->pixelFormatComboBox->setCurrentIndex(BOX_YUV420_8);
        else if ( pixelFormat == YUVC_420YpCbCr10LEPlanarPixelFormat)
            ui->pixelFormatComboBox->setCurrentIndex(BOX_YUV420_10);
        else if ( pixelFormat == YUVC_422YpCbCr8PlanarPixelFormat)
            ui->pixelFormatComboBox->setCurrentIndex(BOX_YUV422);
        else if ( pixelFormat == YUVC_444YpCbCr8PlanarPixelFormat)
            ui->pixelFormatComboBox->setCurrentIndex(BOX_YUV444);
    }

}

void MainWindow::showAbout()
{
    QTextEdit *about = new QTextEdit(this);
    about->setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
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
    about->setFixedSize(QSize(240,200));

    about->show();
}

void MainWindow::bugreport()
{
     QDesktopServices::openUrl(QUrl("http://host1:8000/YUView/newticket"));
}

void MainWindow::saveScreenshot() {

    QSettings settings;

    QString filename = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), settings.value("LastScreenshotPath").toString(), tr("Image Files (*.jpg *.png);;All Files (*)"));
    //ui->renderWidget->grabFrameBuffer().save(filename);

    filename = filename.section('/',0,-2);
    settings.setValue("LastScreenshotPath",filename);
}

void MainWindow::updateSettings()
{
    VideoFile::frameCache.setMaxCost(p_settingswindow.getCacheSizeInMB());

    updateGrid();

    ui->displaySplitView->clear();
}

int MainWindow::findMaxNumFrames()
{
    // check max # of frames
    int maxFrames = INT_MAX;
    foreach(QTreeWidgetItem* item, p_playlistWidget->selectedItems())
    {
        PlaylistItem* yuvItem = dynamic_cast<PlaylistItem*>(item);

        if (yuvItem->displayObject()->numFrames() < maxFrames)
            maxFrames = yuvItem->displayObject()->numFrames();
    }
    if(maxFrames == INT_MAX)
        maxFrames = 1;

    return maxFrames;
}

QString MainWindow::strippedName(const QString &fullFileName)
 {
     return QFileInfo(fullFileName).fileName();
 }
