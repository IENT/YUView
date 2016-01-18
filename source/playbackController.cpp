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

#include "playbackController.h"
#include <assert.h>
#include <QSettings>
#include <QHBoxLayout>

PlaybackController::PlaybackController()
{
  // Load the icons for the buttons
  iconPlay = QIcon(":img_play.png");
  iconStop = QIcon(":img_stop.png");
  iconPause = QIcon(":img_pause.png");
  iconRepeatOff = QIcon(":img_repeat.png");
  iconRepeatAll = QIcon(":img_repeat_on.png");
  iconRepeatOne = QIcon(":img_repeat_one.png");

  createWidgetsAndLayout();

  // Load current repear mode from settings
  QSettings settings;
  setRepeatMode ( (RepeatMode)settings.value("RepeatMode", RepeatModeOff).toUInt() );
  
  // Initialize variables
  currentFrame = 0;
  timerFPSCounter = 0;
  timerId = -1;
  timerInterval = -1;
  timerFPSCounter = 0;
  timerLastFPSTime = QTime::currentTime();

  splitView = NULL;

  // Initial state is disabled (until an item is selected in the playlist)
  enableControls(false);
}

void PlaybackController::createWidgetsAndLayout()
{
  // Create the controls and the layouts
  
  // Everything is layed out horizontally
  QHBoxLayout *hLayout = new QHBoxLayout;
  //hLayout->setContentsMargins( 0, 0, 0, 0 );

  // Create all controls
  playPauseButton = new QPushButton(iconPlay, "", this);
  stopButton = new QPushButton(iconStop, "", this);
  frameSlider = new QSlider(Qt::Horizontal ,this);
  frameSlider->setTickPosition( QSlider::TicksBothSides );
  frameSpinBox = new QSpinBox(this);
  fpsLabel = new QLabel("0", this);
  fpsTextLabel = new QLabel("fps", this);
  repeatModeButton = new QPushButton(iconRepeatOff, "", this);

  // Add all widgets to the layout
  hLayout->addWidget( playPauseButton );
  hLayout->addWidget( stopButton );
  hLayout->addWidget( frameSlider );
  hLayout->addWidget( frameSpinBox );
  hLayout->addWidget( fpsLabel );
  hLayout->addWidget( fpsTextLabel );
  hLayout->addWidget( repeatModeButton );

  setLayout( hLayout );

  // Connect signals/slots
  QObject::connect(repeatModeButton, SIGNAL(clicked(bool)), this, SLOT(toggleRepeat()));
  QObject::connect(frameSlider, SIGNAL(valueChanged(int)), this, SLOT(frameSliderValueChanged(int)));
  QObject::connect(frameSpinBox, SIGNAL(valueChanged(int)), this, SLOT(frameSpinBoxValueChanged(int)));
}

void PlaybackController::setRepeatMode(RepeatMode mode)
{
  // save new repeat mode in user preferences
  QSettings settings;
  settings.setValue("RepeatMode", mode);
  repeatMode = mode;

  // Update the icon on the push button
  switch (repeatMode)
  {
    case RepeatModeOff:
      repeatModeButton->setIcon(iconRepeatOff);
      repeatModeButton->setToolTip("Repeat Off");
      break;
    case RepeatModeOne:
      repeatModeButton->setIcon(iconRepeatOne);
      repeatModeButton->setToolTip("Repeat One");
      break;
    case RepeatModeAll:
      repeatModeButton->setIcon(iconRepeatAll);
      repeatModeButton->setToolTip("Repeat All");
      break;
  }
}

void PlaybackController::stop()
{
  if (timerId != -1)
  {
    // Stop the timer
    killTimer(timerId);
    timerId = -1;
  }

  // Set icon to play
  playPauseButton->setIcon(iconPlay);
}

void PlaybackController::playPause()
{
  if (timerId != -1)
    return;

  // start playing with timer
  /*double frameRate = selectedPrimaryPlaylistItem()->getFrameRate();
  if (frameRate < 0.00001) frameRate = 1.0;
  p_timerInterval = 1000.0 / frameRate;
  p_timerId = startTimer(p_timerInterval, Qt::PreciseTimer);
  p_timerRunning = true;
  p_timerLastFPSTime = QTime::currentTime();
  p_timerFPSCounter = 0;*/

  // update our play/pause icon
  playPauseButton->setIcon(iconPause);
}

void PlaybackController::frameSliderValueChanged(int value)
{
  // Stop playback (if running)
  stop();

  // Set the new value in the spinBox without invoking another signal
  QObject::disconnect(frameSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  frameSpinBox->setValue(value);
  currentFrame = value;
  QObject::connect(frameSpinBox, SIGNAL(valueChanged(int)), this, SLOT(frameSpinBoxValueChanged(int)));

  // Also update the view to display the new frame
  splitView->update();
}

void PlaybackController::frameSpinBoxValueChanged(int value)
{
  // Stop playback (if running)
  stop();

    // Set the new value in the frameSlider without invoking another signal
  QObject::disconnect(frameSlider, SIGNAL(valueChanged(int)), NULL, NULL);
  frameSlider->setValue(value);
  currentFrame = value;
  QObject::connect(frameSlider, SIGNAL(valueChanged(int)), this, SLOT(frameSliderValueChanged(int)));

  // Also update the view to display the new frame
  splitView->update();
}

/** Toggle the repeat mode (loop through the list)
  * The signal repeatModeButton->clicked() is connected to this slot
  */
void PlaybackController::toggleRepeat()
{
  switch (repeatMode)
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

void PlaybackController::setControlsEnabled(bool flag)
{
  playPauseButton->setEnabled(flag);
  stopButton->setEnabled(flag);
  frameSlider->setEnabled(flag);
  frameSpinBox->setEnabled(flag);
}

void PlaybackController::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  // Stop playback (if running)
  stop();

  if (!item1 || !item1->isIndexedByFrame())
  {
    // No item selected or the selected item is not indexed by a frame (there is no navigation in the item)
    enableControls(false);

    // Also update the view to display an empty widget
    splitView->update();

    return;
  }

  // Set the correct number of frames
  enableControls(true);
  frameSlider->setMaximum( item1->getNumberFrames() - 1 );
  frameSpinBox->setMaximum( item1->getNumberFrames() - 1 );

  // Also update the view to display the new frame
  splitView->update();
}

void PlaybackController::enableControls(bool enable)
{
  playPauseButton->setEnabled(enable);
  stopButton->setEnabled(enable);
  frameSlider->setEnabled(enable);
  frameSpinBox->setEnabled(enable);
  fpsLabel->setEnabled(enable);
  fpsTextLabel->setEnabled(enable);
  repeatModeButton->setEnabled(enable);

  // If disabling, also reset the controls
  if (!enable)
  {
    frameSlider->setMaximum(0);
    fpsLabel->setText("0");
  }
}