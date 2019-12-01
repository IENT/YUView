/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include <QBasicTimer>
#include <QPointer>
#include <QTime>
#include <QWidget>

#include "playlistTreeWidget.h"
#include "splitViewWidget.h"
#include "common/typedef.h"

#include "ui_playbackController.h"

class playlistItem;
class PlaylistTreeWidget;

class PlaybackController : public QWidget, private Ui::PlaybackController
{
  Q_OBJECT

public:

  PlaybackController();

  void setSplitViews(splitViewWidget *primary, splitViewWidget *separate) { splitViewPrimary = primary; splitViewSeparate = separate; }
  void setPlaylist (PlaylistTreeWidget *playlistWidget) { playlist = playlistWidget; }

  // If playback is running, stop it by pressing the playPauseButton.
  void pausePlayback() { if (playing()) on_playPauseButton_clicked(); }

  // What is the sate of the playback?
  bool playing() const { return playbackMode != PlaybackStopped; }
  bool isWaitingForCaching() const { return playbackMode == PlaybackWaitingForCache; }

  // Get the currently shown frame index
  int getCurrentFrame() const { return currentFrameIdx; }
  // Set the current frame in the controls and update the splitView without invoking more events from the controls.
  // Return if an update was performed.
  bool setCurrentFrame(int frame, bool updateView=true);

  // Using the currentFrameIdx and the repreat mode, calculate the next frame index.
  // -1: The next frame is the first fame of the next item.
  int getNextFrameIndex();

public slots:
  // Slots for the play/stop/toggleRepera buttons (these are automatically connected by the UI file (connectSlotsByName))
  void on_playPauseButton_clicked();
  void on_stopButton_clicked();
  void on_repeatModeButton_clicked();

  // Slots for skipping to the next/previous frame. There could be buttons connected to these.
  void nextFrame();
  void previousFrame();

  // Accept the signal from the playlistTreeWidget that signals if a new (or two) item was selected.
  // The playback controller will save a pointer to this in order to get playback info from the item later
  // like the sampling or the framerate. This will also update the slider and the spin box.
  // Playback will be stopped if chageByPlayback is false.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2, bool chageByPlayback);

  // One of the (possibly two) selected items finished loading the double bufffer.
  void currentSelectedItemsDoubleBufferLoad(int itemID);

  // The properties of the currently selected item(s) changed. Update the frame sliders and toggle an update()
  // in the splitView if necessary.
  void selectionPropertiesChanged(bool redraw);

  // Update the current settings fomr the QSettings
  void updateSettings();

signals:
  void ControllerStartCachingCurrentSelection(indexRange range);
  void ControllerRemoveFromCache(indexRange range);

  // This is connected to the videoCache and tells it to call itemCachingFinished when caching is finished.
  void waitForItemCaching(playlistItem *item);

  // The playback is now going to start
  void signalPlaybackStarting();

public slots:
  // The video cache calls this if caching of the item is finished
  void itemCachingFinished(playlistItem *item);

private slots:
  // The user is fiddeling with the slider/spinBox controls (automatically connected)
  void on_frameSlider_valueChanged(int val);
  void on_frameSpinBox_valueChanged(int val) { on_frameSlider_valueChanged(val); }

private:

  // Enable/disable all controls
  void enableControls(bool enable);
  bool controlsEnabled;

  // Update the frame range (minimum/maximum frame number)
  void updateFrameRange();

  // The current frame index. -1 means the frame index is invalid. In this case, lastValidFrameIdx
  // contains the last valid frame index which will be restored if a valid indexed item is selected.
  int currentFrameIdx;
  int lastValidFrameIdx;

  // Start the time if not running or update the timer interval. This is called when we jump to the next item, when the user presses 
  // play or when the rate of the current item changes.
  void startOrUpdateTimer();
  // Start playback. Start the timer (startOrUpdateTimer()), set the icons, inform the split views...
  void startPlayback(); 

  // Set the new repeat mode and save it into the settings. Update the control.
  // Always use this function to set the new repeat mode.
  typedef enum {
    RepeatModeOff,
    RepeatModeOne,
    RepeatModeAll
  } RepeatMode;
  RepeatMode repeatMode;
  void setRepeatMode(RepeatMode mode);

  QIcon iconPlay;
  QIcon iconStop;
  QIcon iconPause;
  QIcon iconRepeatOff;
  QIcon iconRepeatAll;
  QIcon iconRepeatOne;

  // Is playback currently running?
  typedef enum {
    PlaybackStopped,
    PlaybackRunning,
    PlaybackStalled,
    PlaybackWaitingForCache,
  } PlaybackMode;
  PlaybackMode playbackMode;

  // If playback mode is PlaybackStalled, which items are we waiting for?
  bool waitingForItem[2];

  // Was playback stalled recently? This is used to indicate stalling in the fps label.
  bool playbackWasStalled;

  // Before starting playback of an item, do we wait until caching is complete?
  bool waitForCachingOfItem;

  // The timer for playback
  QBasicTimer timer;
  int    timerInterval;        // The current timer interval in milli seconds. If it changes, update the running timer.
  int    timerFPSCounter;      // Every time the timer is toggled count this up. If it reaches 50, calculate FPS.
  QTime  timerLastFPSTime;     // The last time we updated the FPS counter. Used to calculate new FPS.
  int    timerStaticItemCountDown; // Also for static items we run the timer to update the slider.
  virtual void timerEvent(QTimerEvent *event) Q_DECL_OVERRIDE; // Overloaded from QObject. Called when the timer fires.

  // We keep a pointer to the currently selected item(s)
  QPointer<playlistItem> currentItem[2];

  // The playback controller has a pointer to the split view so it can toggle a redraw event when a new frame is selected.
  // This could also be done using signals/slots but the problem is that signals/slots have a small overhead. 
  // So when we are using a timer for high framerates, this is the faster option.
  QPointer<splitViewWidget> splitViewPrimary;
  QPointer<splitViewWidget> splitViewSeparate;

  // We keep a pointer to the playlist tree so we can select the next item, see if there is a next item and so on.
  QPointer<PlaylistTreeWidget> playlist;
};

#endif // PLAYBACKCONTROLLER_H
