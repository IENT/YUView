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

#include <QSettings>
#include "playlistItem.h"
#include "signalsSlots.h"

PlaybackController::PlaybackController()
{
  setupUi(this);

  // Default fps
  fpsLabel->setText("0");
    
  // Load the icons for the buttons
  iconPlay.addFile(":img_play.png");
  iconStop.addFile(":img_stop.png");
  iconPause.addFile(":img_pause.png");
  iconRepeatOff.addFile(":img_repeat.png");
  iconRepeatAll.addFile(":img_repeat_on.png");
  iconRepeatOne.addFile(":img_repeat_one.png");

  // Set button icons
  playPauseButton->setIcon( iconPlay );
  stopButton->setIcon( iconStop );

  // Load current repeat mode from settings (and set icon of the button)
  QSettings settings;
  setRepeatMode((RepeatMode)settings.value("RepeatMode", RepeatModeOff).toUInt());

  // Initialize variables
  currentFrameIdx = 0;
  timerInterval = -1;
  timerFPSCounter = 0;
  timerLastFPSTime = QTime::currentTime();

  // Initial state is disabled (until an item is selected in the playlist)
  enableControls(false);
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

void PlaybackController::on_stopButton_clicked()
{
  // Stop playback (if running) and go to frame 0.
  pausePlayback();

  // Goto frame 0 and update the splitView
  setCurrentFrame(0);
}

void PlaybackController::on_playPauseButton_clicked()
{
  // If no item is selected there is nothing to play back
  if (!currentItem)
    return;

  if (playing())
  {
    // The timer is running. Stop it.
    timer.stop();

    // update our play/pause icon
    playPauseButton->setIcon(iconPlay);

    // Set the fps text to 0
    fpsLabel->setText("0");

    // Unfreeze the primary view
    splitViewPrimary->freezeView(false);
  }
  else
  { 
    if (currentFrameIdx >= frameSlider->maximum() && repeatMode == RepeatModeOff)
    {
      // We are currently at the end of the sequence and the user pressed play.
      // If there is no next item to play, replay the current item from the beginning.
      if (!playlist->hasNextItem())
        setCurrentFrame( frameSlider->minimum() );
    }

    // Timer is not running. Start it up.
    startOrUpdateTimer();

    // update our play/pause icon
    playPauseButton->setIcon(iconPause);

    // Freeze the primary view
    splitViewPrimary->freezeView(true);
  }
}

void PlaybackController::startOrUpdateTimer()
{
  // Get the frame rate of the current item. Lower limit is 0.01 fps.
  if (currentItem->isIndexedByFrame())
  {
    double frameRate = currentItem->getFrameRate();
    if (frameRate < 0.01)
      frameRate = 0.01;

    timerInterval = 1000.0 / frameRate;
  }
  else
    timerInterval = int(currentItem->getDuration() * 1000);

  timer.start(timerInterval, Qt::PreciseTimer, this);
  timerLastFPSTime = QTime::currentTime();
  timerFPSCounter = 0;
}

void PlaybackController::nextFrame()
{
  // If the next Frame slot is toggled, abort playback (if running)
  pausePlayback();

  // Can we go to the next frame?
  if (currentFrameIdx >= frameSlider->maximum())
    return;

  // Go to the next frame and update the splitView
  setCurrentFrame( currentFrameIdx + 1 );
}

void PlaybackController::previousFrame()
{
  // If the previous Frame slot is toggled, abort playback (if running)
  pausePlayback();

  // Can we go to the previous frame?
  if (currentFrameIdx == frameSlider->minimum())
    return;

  // Go to the previous frame and update the splitView
  setCurrentFrame( currentFrameIdx - 1 );
}

void PlaybackController::on_frameSlider_valueChanged(int value)
{
  // Stop playback (if running)
  pausePlayback();

  // Go to the new frame and update the splitView
  setCurrentFrame( value );
}

void PlaybackController::on_frameSpinBox_valueChanged(int value)
{
  // Stop playback (if running)
  pausePlayback();

  // Go to the new frame and update the splitView
  setCurrentFrame( value );
}

/** Toggle the repeat mode (loop through the list)
  * The signal repeatModeButton->clicked() is connected to this slot
  */
void PlaybackController::on_repeatModeButton_clicked()
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

void PlaybackController::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2, bool chageByPlayback)
{
  // The playback controller only checks the first item
  // TODO: Is this correct? What if the second item has more frames than the first one? Should the user be able to navigate here? I would think yes!
  Q_UNUSED(item2);

  QSettings settings;
  bool continuePlayback = settings.value("ContinuePlaybackOnSequenceSelection",false).toBool();
  
  if (playing() && !chageByPlayback && !continuePlayback)
    // Stop playback (if running)
    pausePlayback();

  // Set the correct number of frames
  currentItem = item1;

  if (!item1 || !item1->isIndexedByFrame())
  {
    // No item selected or the selected item is not indexed by a frame (there is no navigation in the item)
    enableControls(false);

    if (item1 && (chageByPlayback || (continuePlayback && playing())))
    {
      // Update the timer
      startOrUpdateTimer();
    }

    // Also update the view to display an empty widget or the static item.
    splitViewPrimary->update(true);
    splitViewSeparate->update();
    
    return;
  }
 
  if (playing() && (chageByPlayback || continuePlayback))
  {
    // Update the timer
    startOrUpdateTimer();

    // Update the frame slider and spin boxes without emitting more signals
    const QSignalBlocker blocker1(frameSpinBox);
    const QSignalBlocker blocker2(frameSlider);

    enableControls(true);
    indexRange range = item1->getFrameIndexRange();
    frameSlider->setEnabled(range != indexRange(-1,-1));    // Disable slider if range == (-1,-1)
    frameSlider->setMaximum(range.second);
    frameSlider->setMinimum(range.first);
    frameSpinBox->setMinimum(range.first);
    frameSpinBox->setMaximum(range.second);

    if (!chageByPlayback && continuePlayback && currentFrameIdx >= range.second)
    {
      // The user changed this but we want playback to continue. Unfortunately the new selected sequence does not 
      // have as many frames as the previous one. So we start playback at the start.
      currentFrameIdx = range.first;
      frameSpinBox->setValue(currentFrameIdx);
      frameSlider->setValue(currentFrameIdx);
    }
  }
  else
  {
    enableControls(true);
    indexRange range = item1->getFrameIndexRange();
    frameSlider->setEnabled(range != indexRange(-1,-1));    // Disable slider if range == (-1,-1)
    frameSlider->setMaximum(range.second);
    frameSlider->setMinimum(range.first);
    frameSpinBox->setMinimum(range.first);
    frameSpinBox->setMaximum(range.second);
  }

  // Also update the view to display the new frame
  splitViewPrimary->update(true);
  splitViewSeparate->update();
}

void PlaybackController::selectionPropertiesChanged(bool redraw)
{
  if (controlsEnabled)
  {
    // Update min/max frame index values of the controls
    indexRange range = currentItem->getFrameIndexRange();
    frameSlider->setEnabled(range != indexRange(-1,-1));    // Disable slider if range == (-1,-1)
    frameSlider->setMaximum(range.second);
    frameSlider->setMinimum(range.first);
    frameSpinBox->setMinimum(range.first);
    frameSpinBox->setMaximum(range.second);
  }

  // Check if the current frame is outside of the (new) allowed range
  if (currentFrameIdx > frameSlider->maximum())
    setCurrentFrame(frameSlider->maximum());
  else if (currentFrameIdx < frameSlider->minimum())
    setCurrentFrame(frameSlider->minimum());
  else if (redraw)
  {
    splitViewPrimary->update();
    splitViewSeparate->update();
  }
}

void PlaybackController::enableControls(bool enable)
{
  frameSlider->setEnabled(enable);
  frameSpinBox->setEnabled(enable);
  fpsLabel->setEnabled(enable);

  // If disabling, also reset the controls but emit no signals
  if (!enable)
  {
    const QSignalBlocker blocker(frameSlider);
    frameSlider->setMaximum(0);
    fpsLabel->setText("0");
  }

  controlsEnabled = enable;
}

void PlaybackController::timerEvent(QTimerEvent *event)
{
  if (event->timerId() != timer.timerId())
    return QWidget::timerEvent(event);

  if (currentFrameIdx >= frameSlider->maximum() || !currentItem->isIndexedByFrame())
  {
    // The sequence is at the end. The behavior now depends on the set repeat mode.
    switch (repeatMode)
    {
      case RepeatModeOff:
        // Repeat is off. Goto the next item but don't goto the next item if the playlist is over.
        if (playlist->selectNextItem(false, true))
          // We jumped to the next item. Start at the first frame.
          setCurrentFrame(frameSlider->minimum());
        else
          // There is no next item. Stop playback
          on_playPauseButton_clicked();
        break;
      case RepeatModeOne:
        // Repeat the current item. So the next frame is the first frame of the currently selected item.
        setCurrentFrame(frameSlider->minimum());
        break;
      case RepeatModeAll:
        if (playlist->selectNextItem(true, true))
          // We jumped to the next item. Start at the first frame.
          setCurrentFrame(frameSlider->minimum());
        else
          // There is no next item. For repeat all this can only happen if the playlist is empty.
          // Anyways, stop playback.
          on_playPauseButton_clicked();
        break;
    }
  }
  else
  {
    // Go to the next frame and update the splitView
    setCurrentFrame(currentFrameIdx + 1);

    // Update the FPS counter every 50 frames
    timerFPSCounter++;
    if (timerFPSCounter >= 50)
    {
      QTime newFrameTime = QTime::currentTime();
      double msecsSinceLastUpdate = (double)timerLastFPSTime.msecsTo(newFrameTime);

      // Print the frames per second as float with one digit after the decimal dot.
      double framesPerSec = (50 / (msecsSinceLastUpdate / 1000.0));
      if (framesPerSec > 0)
        fpsLabel->setText(QString::number(framesPerSec, 'f', 1));

      timerLastFPSTime = QTime::currentTime();
      timerFPSCounter = 0;
    }

    // Check if the time interval changed (the user changed the rate of the item)
    if (currentItem->isIndexedByFrame())
    {
      double frameRate = currentItem->getFrameRate();
      if (frameRate < 0.01)
        frameRate = 0.01;

      int newtimerInterval = 1000.0 / frameRate;
      if (timerInterval != newtimerInterval)
        startOrUpdateTimer();
    }
  }
}

/* Set the value currentFrame to frame and update the value in the splinBox and the slider without
 * invoking any events from these controls. Also update the splitView.
*/
void PlaybackController::setCurrentFrame(int frame)
{
  if (frame == currentFrameIdx)
    return;

  // Set the new value in the controls without invoking another signal
  const QSignalBlocker blocker1(frameSpinBox);
  const QSignalBlocker blocker2(frameSlider);
  currentFrameIdx = frame;
  frameSpinBox->setValue(frame);
  frameSlider->setValue(frame);

  // Also update the view to display the new frame
  splitViewPrimary->update(true);
  splitViewSeparate->update();
}
