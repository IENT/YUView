#include "mainwindow.h"
#include "displaywidget.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <math.h>
#include <QThread>
#include <QStringList>
#include <QInputDialog>
#include <QDesktopServices>

#include "playlistitemvid.h"
#include "playlistitemstats.h"
#include "playlistitemtext.h"
#include "statslistmodel.h"
#include "sliderdelegate.h"
#include "displaysplitwidget.h"

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
    p_playlistWidget = 0;
    ui->setupUi(this);

    statusBar()->hide();

    p_playlistWidget = ui->playlistTreeWidget;

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

    QMenu *file;
    file = menuBar()->addMenu(tr("&File"));
    file->addAction("&Open YUV File", this, SLOT(openFile()),Qt::CTRL + Qt::Key_O);
    file->addAction("&Open Statistics File", this, SLOT(openStatsFile()) );
    file->addAction("&Add Text Frame",this,SLOT(addTextFrame()));
    file->addSeparator();
    file->addAction("&Save Screenshot", this, SLOT(saveScreenshot()) );
    file->addSeparator();
    file->addAction("&Settings", &p_settingswindow, SLOT(show()) );

    file = menuBar()->addMenu(tr("&View"));
    file->addAction("Hide/Show P&laylist", ui->playlistDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_L);
    file->addAction("Hide/Show &Statistics", ui->statsDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_S);
    file->addSeparator();
    file->addAction("Hide/Show F&ile Options", ui->fileDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_I);
    file->addAction("Hide/Show &Display Options", ui->displayDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_D);
    file->addSeparator();
    file->addAction("Hide/Show Playback &Controls", ui->controlsDockWidget->toggleViewAction(), SLOT(trigger()),Qt::CTRL + Qt::Key_P);
    file->addSeparator();
    file->addAction("&Fullscreen", this, SLOT(toggleFullscreen()),Qt::CTRL + Qt::Key_F);

    file = menuBar()->addMenu(tr("&Help"));
    file->addAction("About", this, SLOT(showAbout()));
    file->addAction("Report a Bug", this, SLOT(bugreport()));
    file->addAction("Request a Feature", this, SLOT(bugreport()));

    StatsListModel *model= new StatsListModel(this);
    this->ui->statsListView->setModel(model);
    QObject::connect(model, SIGNAL(signalStatsTypesChanged()), this, SLOT(statsTypesChanged()));

    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    ui->deleteButton->setEnabled(false);
    ui->statsGroupBox->setEnabled(false);
    ui->opacitySlider->setEnabled(false);
    ui->gridCheckBox->setEnabled(false);
    QObject::connect(&p_settingswindow, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
    updateSettings();

    setSelectedPlaylistItem(NULL);
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

PlaylistItem* MainWindow::selectedPlaylistItem()
{
    if (p_playlistWidget!=0)
        return (PlaylistItem*)p_playlistWidget->currentItem();
    else
        return NULL;
}


void MainWindow::loadFiles(QStringList files)
{
    QStringList filter;

    // this might be used to associate a statistics item with a video item
    PlaylistItem* curItem = (PlaylistItem*)p_playlistWidget->currentItem();

    QStringList::Iterator it = files.begin();
    while(it != files.end())
    {
        QString fileName = *it;

        if( !(QFile(fileName).exists()) )
            continue;

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

                // select newly inserted file
                setSelectedPlaylistItem(newListItemVid);
            }
            else if( ext == "csv" )
            {
                PlaylistItemStats *newListItemStats = new PlaylistItemStats(fileName, p_playlistWidget);

                // select newly inserted file
                setSelectedPlaylistItem(newListItemStats);
            }
        }

        ++it;
    }
}

void MainWindow::openFile()
{
    // load last used directory from QPreferences
    QSettings settings;
    QStringList filter;
    filter << "Video Files (*.yuv *.xml)" << "All Files (*)";

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

    if(selectedPlaylistItem())
    {
        updateFrameSizeComboBoxSelection();
        updateColorFormatComboBoxSelection(selectedPlaylistItem());

        // resize player window to fit video size
        QRect screenRect = QDesktopWidget().availableGeometry();
        unsigned int newWidth = MIN( MAX( selectedPlaylistItem()->displayObject()->width()+680, width() ), screenRect.width() );
        unsigned int newHeight = MIN( MAX( selectedPlaylistItem()->displayObject()->height()+140, height() ), screenRect.height() );
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

void MainWindow::addTextFrame()
{
    bool ok;
     QString text = QInputDialog::getText(this, tr("Add Text Frame"),
                                          tr("Text:"), QLineEdit::Normal,
                                          tr("Text"), &ok);
     if (ok && !text.isEmpty())
     {
         PlaylistItemText* newPlayListItemText = new PlaylistItemText(text,p_playlistWidget);
         setSelectedPlaylistItem(newPlayListItemText);
     }


}

void MainWindow::setSelectedPlaylistItem(QTreeWidgetItem* newSelectedItem)
{
    PlaylistItem* selectedItem = dynamic_cast<PlaylistItem*>(newSelectedItem);

    if( newSelectedItem == NULL  || selectedItem->displayObject() == NULL)
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

        // make sure item is currently selected
        p_playlistWidget->setCurrentItem(newSelectedItem);

        return;
    }

    // make sure item is currently selected
    if (newSelectedItem != p_playlistWidget->currentItem())
        p_playlistWidget->setCurrentItem(newSelectedItem);

    // update window caption
    QString newCaption = "YUView - " + selectedItem->text(0);
    setWindowTitle(newCaption);

    // if the newly selected item is of type statistics, update model of statistics list
    if( selectedItem->itemType() == StatisticsItemType )
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedItem);
        assert(statsItem != NULL);

        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setCurrentStatistics(statsItem->displayObject(), statsItem->displayObject()->getActiveStatsTypes());
    }
    else if( selectedItem->itemType() == VideoItemType && selectedItem->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedItem->child(0);
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(childItem);
        assert(statsItem != NULL);

        // update model from selected item
        dynamic_cast<StatsListModel*>(ui->statsListView->model())->setCurrentStatistics(statsItem->displayObject(), statsItem->displayObject()->getActiveStatsTypes());

        // update display widget
        ui->displaySplitView->setActiveStatisticsObjects(statsItem->displayObject(), NULL);
    }

    if(selectedItem->displayObject() == NULL)
        return;

    // tell our display widget about new objects
    ui->displaySplitView->setActiveDisplayObjects(selectedPlaylistItem()->displayObject(), NULL);

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

    QObject::disconnect(ui->colorFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_colorFormatComboBox_currentIndexChanged(int)));
    updateColorFormatComboBoxSelection(selectedItem);
    QObject::connect(ui->colorFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_colorFormatComboBox_currentIndexChanged(int)));
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
    if (selectedPlaylistItem() == NULL || selectedPlaylistItem()->displayObject() == NULL)
    {
        p_currentFrame = -1;
        ui->displaySplitView->clear();
        return;
    }

    if (frame != p_currentFrame || forceRefresh)
    {
        // get real frame index
        if( frame < selectedPlaylistItem()->displayObject()->startFrame() )
            p_currentFrame = selectedPlaylistItem()->displayObject()->startFrame();
        else if( frame >= p_numFrames + selectedPlaylistItem()->displayObject()->startFrame() )
            p_currentFrame = selectedPlaylistItem()->displayObject()->startFrame() + p_numFrames - 1;
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
    if (selectedPlaylistItem() == NULL || selectedPlaylistItem()->displayObject() == NULL)
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
            p_playTimer->start( 1000.0/( selectedPlaylistItem()->displayObject()->frameRate() ) );
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

    if( selectedPlaylistItem()->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(selectedPlaylistItem());
        assert(viditem != NULL);

        ui->createdLineEdit->setText(viditem->displayObject()->createdtime());
        ui->modifiedLineEdit->setText(viditem->displayObject()->modifiedtime());
        ui->filePathEdit->setPlainText(viditem->displayObject()->path());
    }

    if (selectedPlaylistItem()->displayObject()->playUntilEnd())
        ui->framesSpinBox->setValue(0);
    else
        ui->framesSpinBox->setValue(selectedPlaylistItem()->displayObject()->numFrames());
    ui->widthSpinBox->setValue(selectedPlaylistItem()->displayObject()->width());
    ui->heightSpinBox->setValue(selectedPlaylistItem()->displayObject()->height());
    ui->rateSpinBox->setValue(selectedPlaylistItem()->displayObject()->frameRate());

    // Connect slots/signals of info panel
    QObject::connect( ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->offsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->framesSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateMetaInfo()) );
    QObject::connect( ui->samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateMetaInfo()) );

    QObject::connect( ui->widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateFrameSizeComboBoxSelection()) );
    QObject::connect( ui->heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateFrameSizeComboBoxSelection()) );
    QObject::connect(selectedPlaylistItem()->displayObject(), SIGNAL(informationChanged(uint)), this, SLOT(refreshPlaybackWidgets()));
}

void MainWindow::refreshPlaybackWidgets()
{
    // don't do anything if not yet initialized
    if (selectedPlaylistItem() == NULL)
        return;

    // update information about newly selected video
    p_numFrames = (ui->framesSpinBox->value() == 0) ? findMaxNumFrames() - selectedPlaylistItem()->displayObject()->startFrame() : selectedPlaylistItem()->displayObject()->numFrames();
    // TODO: check why app crashes for files with only a single frame: p_numFrames = 1
    int minFrameIdx = MAX( 0, selectedPlaylistItem()->displayObject()->startFrame() );
    int maxFrameIdx = MAX( minFrameIdx, selectedPlaylistItem()->displayObject()->startFrame() + p_numFrames );
    ui->frameSlider->setMinimum( minFrameIdx );
    ui->frameSlider->setMaximum( maxFrameIdx );
    //ui->framesSpinBox->setValue(p_numFrames);

    int modifiedFrame = p_currentFrame;

    if( p_currentFrame < selectedPlaylistItem()->displayObject()->startFrame() )
        modifiedFrame = selectedPlaylistItem()->displayObject()->startFrame();
    else if( p_currentFrame >= ( selectedPlaylistItem()->displayObject()->startFrame() + p_numFrames ) )
        modifiedFrame = selectedPlaylistItem()->displayObject()->startFrame() + p_numFrames - 1;

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
    if( p_playTimer->isActive() || !selectedPlaylistItem() || !selectedPlaylistItem()->displayObject() )
        return;

    p_FPSCounter = 0;

    // start playing with timer
    p_playTimer->start( 1000.0/( selectedPlaylistItem()->displayObject()->frameRate() ) );

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
        setCurrentFrame( selectedPlaylistItem()->displayObject()->startFrame() );

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
            if (p_playlistWidget->selectedItems().count() == 0)
                setSelectedPlaylistItem(NULL);
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
        ui->centralLayout->setMargin(-1);

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

    if (p_currentFrame >= selectedPlaylistItem()->displayObject()->startFrame() + p_numFrames-1 )
    {
        if(!p_repeat)
            pause();
        else
            setCurrentFrame( selectedPlaylistItem()->displayObject()->startFrame() );
    }
    else
    {
        // update current frame
        setCurrentFrame( p_currentFrame + selectedPlaylistItem()->displayObject()->sampling() );
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
        ui->widthSpinBox->setValue(selectedPlaylistItem()->displayObject()->width());
        ui->heightSpinBox->setValue(selectedPlaylistItem()->displayObject()->height());
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

void MainWindow::on_colorFormatComboBox_currentIndexChanged(int index)
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
                viditem->displayObject()->setColorFormat(YUV400);
                viditem->displayObject()->setBitPerPixel(8);
                break;
            case BOX_YUV411:
                viditem->displayObject()->setColorFormat(YUV411);
                viditem->displayObject()->setBitPerPixel(8);
                break;
            case BOX_YUV420_8:
                viditem->displayObject()->setColorFormat(YUV420);
                viditem->displayObject()->setBitPerPixel(8);
                break;
            case BOX_YUV420_10:
                viditem->displayObject()->setColorFormat(YUV420);
                viditem->displayObject()->setBitPerPixel(10);
                break;
            case BOX_YUV422:
                viditem->displayObject()->setColorFormat(YUV422);
                viditem->displayObject()->setBitPerPixel(8);
                break;
            case BOX_YUV444:
                viditem->displayObject()->setColorFormat(YUV444);
                viditem->displayObject()->setBitPerPixel(8);
                break;
            }
        }
    }
}

void MainWindow::on_interpolationComboBox_currentIndexChanged(int index)
{
    if (selectedPlaylistItem() != NULL && selectedPlaylistItem()->itemType() == VideoItemType )
    {
        PlaylistItemVid* viditem = dynamic_cast<PlaylistItemVid*>(selectedPlaylistItem());
        assert(viditem != NULL);

        viditem->displayObject()->setInterpolationMode(index);
    }
}

void MainWindow::statsTypesChanged()
{
    if (selectedPlaylistItem() && selectedPlaylistItem()->itemType() == StatisticsItemType)
    {
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(selectedPlaylistItem());
        assert(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setActiveStatsTypes(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatistics());

        // refresh display widget
        ui->displaySplitView->drawFrame(p_currentFrame);
    }
    else if (selectedPlaylistItem() && selectedPlaylistItem()->itemType() == VideoItemType && selectedPlaylistItem()->childCount() > 0 )
    {
        QTreeWidgetItem* childItem = selectedPlaylistItem()->child(0);
        PlaylistItemStats* statsItem = dynamic_cast<PlaylistItemStats*>(childItem);
        assert(statsItem != NULL);

        // update list of activated types
        statsItem->displayObject()->setActiveStatsTypes(dynamic_cast<StatsListModel*>(ui->statsListView->model())->getStatistics());

        // refresh display widget
        ui->displaySplitView->drawFrame(p_currentFrame);
    }
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

        ColorFormat colorFormat = viditem->displayObject()->colorFormat();
        int bitPerPixel = viditem->displayObject()->bitPerPixel();

        if ( colorFormat == YUV400 && bitPerPixel == 8)
            ui->colorFormatComboBox->setCurrentIndex(BOX_YUV400);
        else if ( colorFormat == YUV411 && bitPerPixel == 8)
            ui->colorFormatComboBox->setCurrentIndex(BOX_YUV411);
        else if ( colorFormat == YUV420 && bitPerPixel == 8)
            ui->colorFormatComboBox->setCurrentIndex(BOX_YUV420_8);
        else if ( colorFormat == YUV420 && bitPerPixel == 10)
            ui->colorFormatComboBox->setCurrentIndex(BOX_YUV420_10);
        else if ( colorFormat == YUV422 && bitPerPixel == 8)
            ui->colorFormatComboBox->setCurrentIndex(BOX_YUV422);
        else if ( colorFormat == YUV444 && bitPerPixel == 8)
            ui->colorFormatComboBox->setCurrentIndex(BOX_YUV444);
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
