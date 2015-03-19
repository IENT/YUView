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
#include <QTime>
#include <QTreeWidget>
#include <QMouseEvent>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QDesktopWidget>
#include <QKeyEvent>

#include "settingswindow.h"
#include "edittextdialog.h"
#include "playlisttreewidget.h"

class PlaylistItem;

#include "displaywidget.h"

typedef enum {
    RepeatModeOff,
    RepeatModeOne,
    RepeatModeAll
} RepeatMode;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void keyPressEvent( QKeyEvent * event );
    //void moveEvent ( QMoveEvent * event );
    void closeEvent(QCloseEvent *event);
    //void resizeEvent(QResizeEvent *event);

private:
    PlaylistTreeWidget *p_playlistWidget;
    Ui::MainWindow *ui;

    QTimer *p_playTimer;
    int p_currentFrame;

    QTimer *p_heartbeatTimer;

    QIcon p_playIcon;
    QIcon p_pauseIcon;
    QIcon p_repeatOffIcon;
    QIcon p_repeatAllIcon;
    QIcon p_repeatOneIcon;

    RepeatMode p_repeatMode;
    int p_numFrames;

    QMessageBox *p_msg;
    QTime p_lastHeartbeatTime;
    int p_FPSCounter;
    bool p_ClearFrame;

public:
    //! loads a list of yuv/csv files
    void loadFiles(QStringList files);

    void loadPlaylistFile(QString filePath);

    bool isPlaylistItemSelected() { return selectedPrimaryPlaylistItem() != NULL; }

public slots:
    //! Toggle fullscreen playback
    void toggleFullscreen();

    //! Shows the file open dialog and loads all selected Files
    void openFile();

    //! Adds a new text frame
    void addTextFrame();

    void addDifferenceSequence();

    void savePlaylistToFile();

    //! Starts playback of selected video file
    void play();

    //! Pauses playback of selected video file
    void pause();

    //! Toggles playback of selected video file
    void togglePlayback();

    //! Stops playback of selected video file
    void stop();

    //! Toggle playback in endless loop
    void toggleRepeat();

    //! Deletes a group from playlist
    void deleteItem();

    //! update parameters of regular overlay grid
    void updateGrid();

    void updateSelectedItems();

    //! Select a Stats Type and update GUI
    void setSelectedStats();

	//! Slot for updating the opacity of the current selected stats type (via items model)
    void updateStatsOpacity(int val);

	//! Slot for updating the grid visibility of the current selected stats type (via items model)
    void updateStatsGrid(bool val);

    //! set current frame for playback
    void setCurrentFrame( int frame, bool forceRefresh = false );

    //! enables the playback controls
    void setControlsEnabled(bool flag);

    //! updates the YUV information GUI elements from the current Renderobject
    void updateMetaInfo();

    //! updates the Playback controls to fit the current YUV settings
    void refreshPlaybackWidgets();

    //! update selection of frame size ComboBox
    void updateFrameSizeComboBoxSelection();

    //! update selection of color format ComboBox
    void updateColorFormatComboBoxSelection(PlaylistItem *selectedItem);

    //! this event is called when the playback-timer is triggered. It will paint the next frame
    void frameTimerEvent();

    void heartbeatTimerEvent();

    void showAbout();

    void openProjectWebsite();

    void saveScreenshot();

    void updateSettings();

    void editTextFrame();


private slots:
    //! Timeout function for playback timer
    //void newFrameTimeout();

    void statsTypesChanged();

    void on_interpolationComboBox_currentIndexChanged(int index);
    void on_pixelFormatComboBox_currentIndexChanged(int index);
    void on_framesizeComboBox_currentIndexChanged(int index);
    void onCustomContextMenu(const QPoint &point);
    void onItemDoubleClicked(QTreeWidgetItem* item, int);

    void openRecentFile();

    void on_LumaScaleSpinBox_valueChanged(int index);

    void on_ChromaScaleSpinBox_valueChanged(int index);

    void on_LumaOffsetSpinBox_valueChanged(int arg1);

    void on_ChromaOffsetSpinBox_valueChanged(int arg1);

    void on_LumaInvertCheckBox_toggled(bool checked);

    void on_ChromaInvertCheckBox_toggled(bool checked);

    void on_ColorComponentsComboBox_currentIndexChanged(int index);

    void selectNextItem();
    void selectPreviousItem();
    void nextFrame() { setCurrentFrame( p_currentFrame+1 ); }
    void previousFrame() { setCurrentFrame( p_currentFrame-1 ); }
    void on_viewComboBox_currentIndexChanged(int index);

    void on_zoomBoxCheckBox_toggled(bool checked);

    void on_SplitviewCheckBox_clicked();

    void on_SplitViewgroupBox_toggled(bool arg1);

private:
    int findMaxNumFrames();
    PlaylistItem* selectedPrimaryPlaylistItem();
    PlaylistItem* selectedSecondaryPlaylistItem();

    SettingsWindow p_settingswindow;

    void createMenusAndActions();
    void updateRecentFileActions();

    QAction* openYUVFileAction;
    QAction* savePlaylistAction;
    QAction* addTextAction;
    QAction* addDifferenceAction;
    QAction* saveScreenshotAction;
    QAction* showSettingsAction;
    QAction* deleteItemAction;

    QAction* zoomToStandardAction;
    QAction* zoomToFitAction;
    QAction* zoomInAction;
    QAction* zoomOutAction;

    QAction* togglePlaylistAction;
    QAction* toggleStatisticsAction;
    QAction* toggleFileOptionsAction;
    QAction* toggleDisplayOptionsActions;
    QAction* toggleYUVMathActions;
    QAction* toggleControlsAction;
    QAction* toggleFullscreenAction;

    QAction* playPauseAction;
    QAction* nextItemAction;
    QAction* previousItemAction;
    QAction* nextFrameAction;
    QAction* previousFrameAction;

    QAction *aboutAction;
    QAction *bugReportAction;
    QAction *featureRequestAction;

    enum { MaxRecentFiles = 5 };
    QAction *recentFileActs[MaxRecentFiles];

    QString strippedName(const QString &fullFileName);
};

#endif // MAINWINDOW_H
