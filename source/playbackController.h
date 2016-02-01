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

#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include "typedef.h"
#include <assert.h>
#include <QWidget>
#include <QIcon>
#include <QTime>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>

#include "playlistitem.h"
#include "splitViewWidget.h"

class PlaybackController : public QWidget
{
  Q_OBJECT

public:
  
  /* 
  */
  PlaybackController();
  virtual ~PlaybackController() {};

  void setSplitView(splitViewWidget *view) { splitView = view; }

  int getCurrentFrame() { return currentFrame; }

signals:

public slots:
  // Slots for the play/stop/toggleRepera buttons
  void playPause();
  void stop();
  void toggleRepeat();

  // Slots for skipping to the next/previous frame. There could be buttons connected to these.
  void nextFrame();
  void previousFrame();

  // Accept the signal from the playlisttreewidget that signals if a new (or two) item was selected.
  // The playback controller will save a pointer to this in order to get playback info from the item later
  // like the sampling or the framerate. This will also update the slider and the spin box and stop playback.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2);

  /* The properties of the currently selected item(s) changed. Update the frame sliders and toggle an update()
   * in the splitview.
  */
  void selectionPropertiesChanged();
 
protected:

private slots:
  // The user is fiddeling with the slider/spinBox controls
  void frameSliderValueChanged(int val);
  void frameSpinBoxValueChanged(int val);
 
private:

  // Create the widgets/layouts and connect all the signals/slots. Only call this once!!
  void createWidgetsAndLayout();

  // Enable/disable all controls
  void enableControls(bool enable);
  bool controlsEnabled;

  // The controls in this widget (play, stop , slider, frameSpinBox, FPS counter and repeatMode)
  QPushButton *playPauseButton;
  QPushButton *stopButton;
  QSlider     *frameSlider;
  QSpinBox    *frameSpinBox;
  QLabel      *fpsLabel;
  QLabel      *fpsTextLabel;
  QPushButton *repeatModeButton;

  typedef enum {
    RepeatModeOff,
    RepeatModeOne,
    RepeatModeAll
  } RepeatMode;
  RepeatMode repeatMode;
  /* Set the new repeat mode and save it into the settings. Update the control.
   * Always use this function to set the new repeat mode.
  */
  void setRepeatMode(RepeatMode mode);
  
  QIcon iconPlay;
  QIcon iconStop;
  QIcon iconPause;
  QIcon iconRepeatOff;
  QIcon iconRepeatAll;
  QIcon iconRepeatOne;
  
  int    currentFrame;
  int    timerId;           // If we call QObject::startTimer(...) we have to remember the ID so we can kill it later.
  int    timerInterval;		  // The current timer interval. If it changes, update the running timer.
  int    timerFPSCounter;	  // Every time the timer is toggeled count this up. If it reaches 50, calculate FPS.
  QTime  timerLastFPSTime;	// The last time we updated the FPS counter. Used to calculate new FPS.

  // We keep a pointer to the currently selected item (only the first)
  playlistItem *currentItem;

  // The playback controller has a pointer to the split view so it can toggle a redraw event when a new frame is selected.
  // This could also be done using signals/slots but the problem is that signals/slots are slow. So when we are using the
  // QTimer for high framerates, this is the faster option.
  splitViewWidget *splitView;

};

#endif // PLAYBACKCONTROLLER_H
