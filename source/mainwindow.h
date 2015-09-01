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
#include "playlistitem.h"

#include "displaywidget.h"

typedef enum {
    RepeatModeOff,
    RepeatModeOne,
    RepeatModeAll
} RepeatMode;

typedef enum {
    WindowModeSingle,
    WindowModeSeparate
} WindowMode;

namespace Ui {
    class MainWindow;
}

// A frameSizePreset has a name and a size
typedef QPair<QString, QSize> frameSizePreset;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void keyPressEvent( QKeyEvent * event );
    bool eventFilter(QObject * target, QEvent * event);
    //void moveEvent ( QMoveEvent * event );
    void closeEvent(QCloseEvent *event);
    //void resizeEvent(QResizeEvent *event);

private:
    PlaylistTreeWidget *p_playlistWidget;
    Ui::MainWindow *ui;

    int    p_currentFrame;
	bool   p_timerRunning;		// Is the playback timer running?
	int    p_timerId;           // If we call QObject::startTimer(...) we have to remember the ID so we can kill it later.
	int    p_timerInterval;		// The current timer interval. If it changes, update the running timer.
	int    p_timerFPSCounter;	// Every time the timer is toggeled count this up. If it reaches 50, calculate FPS.
	QTime  p_timerLastFPSTime;	// The last time we updated the FPS counter. Used to calculate new FPS.

    QIcon p_playIcon;
    QIcon p_pauseIcon;
    QIcon p_repeatOffIcon;
    QIcon p_repeatAllIcon;
    QIcon p_repeatOneIcon;

    RepeatMode p_repeatMode;

    WindowMode p_windowMode;

    QMessageBox *p_msg;
    bool p_ClearFrame;

    QMenu* fileMenu;
    QMenu* viewMenu;
    QMenu* playbackMenu;
    QMenu* helpMenu;

public:
    //! loads a list of yuv/csv files
    void loadFiles(QStringList files);

    void loadPlaylistFile(QString filePath);

    bool isPlaylistItemSelected() { return selectedPrimaryPlaylistItem() != NULL; }

	// Get the static list of preset frame sizes
	static QList<frameSizePreset> presetFrameSizesList();

public slots:

    void enableSingleWindowMode();
    void enableSeparateWindowsMode();

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
    void setCurrentFrame( int frame, bool bForceRefresh=false );

    //! enables the playback controls
    void setControlsEnabled(bool flag);

    //! The display objects information changed. Update.
    void currentSelectionInformationChanged();

    //! updates the Playback controls to fit the current YUV settings
    void refreshPlaybackWidgets();

    //! update selection of frame size ComboBox
    void updateFrameSizeComboBoxSelection();

    //! update selection of color format ComboBox
    void updatePixelFormatComboBoxSelection(PlaylistItem *selectedItem);

    void showAbout();

    void openProjectWebsite();

    void saveScreenshot();

    void updateSettings();

    void editTextFrame();

    void handleKeyPress(QKeyEvent* key);

    void checkNewVersion();

private slots:
    //! Timeout function for playback timer
    //void newFrameTimeout();

    void setRepeatMode(RepeatMode newMode);

    void statsTypesChanged();

    void on_interpolationComboBox_currentIndexChanged(int index);
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
    void nextFrame() { setCurrentFrame( p_currentFrame + selectedPrimaryPlaylistItem()->displayObject()->sampling() ); }
    void previousFrame() { setCurrentFrame( p_currentFrame - selectedPrimaryPlaylistItem()->displayObject()->sampling() ); }
    void on_viewComboBox_currentIndexChanged(int index);

    void on_zoomBoxCheckBox_toggled(bool checked);

    void on_SplitViewgroupBox_toggled(bool arg1);

    void on_colorConversionComboBox_currentIndexChanged(int index);

    void on_markDifferenceCheckBox_clicked();

	/// A file option was changed by the user (width/height/start/end/rate/sampling/frameSize/pixelFormat).
	void on_fileOptionValueChanged();

private:
	//! updates the YUV information GUI elements from the current Renderobject
	void updateSelectionMetaInfo();

	//! update the frames slider and frame selection box without toggeling any signals
	void updateFrameControls();

	//! this event is called when the playback-timer is triggered. It will paint the next frame
	void timerEvent(QTimerEvent * event);

	/// Return the primary and secondary playlist item that is currently selected
    PlaylistItem* selectedPrimaryPlaylistItem();
    PlaylistItem* selectedSecondaryPlaylistItem();

	/// Stores the previously selected display object
	DisplayObject* previouslySelectedDisplayObject;

	/// Get the width/height for the current frameSize selection (in frameSizeComboBox)
	void convertFrameSizeComboBoxIndexToSize(int *width, int*height);

	/// The static list of preset frame sizes. Use presetFrameSizesList() to get the list
	static QList<frameSizePreset> g_presetFrameSizes;

    SettingsWindow p_settingswindow;

    void createMenusAndActions();
	void populateComboBoxes();			//< Populate the frameSizeCombobox and the pixelFormatComboBox
    void updateRecentFileActions();

    // variables related to alternative window mode (YUV Checker)
    QMainWindow p_inspectorWindow;
    QMainWindow p_playlistWindow;


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
    QAction* enableSingleWindowModeAction;
    QAction* enableSeparateWindowModeAction;

    QAction* playPauseAction;
    QAction* nextItemAction;
    QAction* previousItemAction;
    QAction* nextFrameAction;
    QAction* previousFrameAction;

    QAction *aboutAction;
    QAction *bugReportAction;
    QAction *featureRequestAction;
    QAction *checkNewVersionAction;

    enum { MaxRecentFiles = 5 };
    QAction *recentFileActs[MaxRecentFiles];

    QString strippedName(const QString &fullFileName);
};

#endif // MAINWINDOW_H
