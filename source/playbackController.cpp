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

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define PLAYBACKCONTROLLER_DEBUG 1
#if PLAYBACKCONTROLLER_DEBUG && !NDEBUG
#define DEBUG_PLAYBACK qDebug
#else
#define DEBUG_PLAYBACK(fmt,...) ((void)0)
#endif

PlaybackController::PlaybackController()
{
  setupUi(this);

  // Default fps
  fpsLabel->setText("0");
  fpsLabel->setStyleSheet("");

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
  playbackMode = PlaybackStopped;
  playbackWasStalled = false;
  waitingForItem[0] = false;
  waitingForItem[1] = false;

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
  if (repeatMode == RepeatModeOff)
    repeatModeButton->setIcon(iconRepeatOff);
  else if (repeatMode == RepeatModeOne)
    repeatModeButton->setIcon(iconRepeatOne);
  else if (repeatMode == RepeatModeAll)
    repeatModeButton->setIcon(iconRepeatAll);
}

void PlaybackController::on_stopButton_clicked()
{
  // Stop playback (if running) and go to frame 0.
  pausePlayback();
  setCurrentFrame(0);
}

void PlaybackController::on_playPauseButton_clicked()
{
  // If no item is selected there is nothing to play back
  if (!currentItem[0])
    return;

  if (playing())
  {
    // Stop the timer, update the icon and fps label text and unfreeze the primary view (maype it was frozen).
    timer.stop();
    playbackMode = PlaybackStopped;
    playPauseButton->setIcon(iconPlay);
    fpsLabel->setText("0");
    fpsLabel->setStyleSheet("");
    splitViewPrimary->freezeView(false);
  }
  else
  {
    // Playback is not running. Start it.
    if (currentFrameIdx >= frameSlider->maximum() && repeatMode == RepeatModeOff)
    {
      // We are currently at the end of the sequence and the user pressed play.
      // If there is no next item to play, replay the current item from the beginning.
      if (!playlist->hasNextItem())
        setCurrentFrame(frameSlider->minimum());
    }

    // Start the timer, update the icon and (possibly) freeze the primary view.
    startOrUpdateTimer();
    playPauseButton->setIcon(iconPause);
    splitViewPrimary->freezeView(true);
    playbackWasStalled = false;
    
    // Tell the primary split view that playback just started. This will toggle loading
    // of the double buffer of the currently visible items (if required).
    splitViewPrimary->playbackStarted(getNextFrameIndex());
  }
}

void PlaybackController::startOrUpdateTimer()
{
  // Get the frame rate of the current item. Lower limit is 0.01 fps (100 seconds per frame).
  if (currentItem[0]->isIndexedByFrame() || (currentItem[1] && currentItem[1]->isIndexedByFrame()))
  {
    // One (of the possibly two items) is indexed by frame. Get and set the frame rate
    double frameRate = currentItem[0]->isIndexedByFrame() ? currentItem[0]->getFrameRate() : currentItem[1]->getFrameRate();
    if (frameRate < 0.01)
      frameRate = 0.01;

    timerInterval = 1000.0 / frameRate;
  }
  else
    // The item (or both items) are not indexed by frame.
    // Use the duration of item 0
    timerInterval = int(currentItem[0]->getDuration() * 1000);

  timer.start(timerInterval, Qt::PreciseTimer, this);
  playbackMode = PlaybackRunning;
  timerLastFPSTime = QTime::currentTime();
  timerFPSCounter = 0;
}

void PlaybackController::nextFrame()
{
  // Abort playback (if running) and go to the next frame (if possible).
  pausePlayback();
  if (currentFrameIdx < frameSlider->maximum())
    setCurrentFrame(currentFrameIdx + 1);
}

void PlaybackController::previousFrame()
{
  // Abort playback (if running) and go to the previous frame (if possible).
  pausePlayback();
  if (currentFrameIdx != frameSlider->minimum())
    setCurrentFrame(currentFrameIdx - 1);
}

void PlaybackController::on_frameSlider_valueChanged(int value)
{
  // Stop playback (if running) and go to the new frame.
  pausePlayback();
  setCurrentFrame(value);
}

/** Toggle the repeat mode (loop through the list)
  * The signal repeatModeButton->clicked() is connected to this slot
  */
void PlaybackController::on_repeatModeButton_clicked()
{
  if (repeatMode == RepeatModeOff)
    setRepeatMode(RepeatModeOne);
  else if (repeatMode == RepeatModeOne)
    setRepeatMode(RepeatModeAll);
  else if (repeatMode == RepeatModeAll)
    setRepeatMode(RepeatModeOff);
}

void PlaybackController::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2, bool chageByPlayback)
{
  QSettings settings;
  bool continuePlayback = settings.value("ContinuePlaybackOnSequenceSelection",false).toBool();
  
  if (playing() && !chageByPlayback && !continuePlayback)
    // Stop playback (if running)
    pausePlayback();

  // Set the correct number of frames
  currentItem[0] = item1;
  currentItem[1] = item2;

  if (!(item1 && item1->isIndexedByFrame()) && !(item2 && item2->isIndexedByFrame()))
  {
    // No item selected or the selected item(s) is/are not indexed by a frame (there is no navigation in the item)
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

    updateFrameRange();

    if (!chageByPlayback && continuePlayback && currentFrameIdx >= frameSlider->maximum())
    {
      // The user changed this but we want playback to continue. Unfortunately the new selected sequence does not 
      // have as many frames as the previous one. So we start playback at the start.
      currentFrameIdx = frameSlider->minimum();
      frameSpinBox->setValue(currentFrameIdx);
      frameSlider->setValue(currentFrameIdx);
    }
  }
  else
    updateFrameRange();
  
  // Also update the view to display the new frame
  splitViewPrimary->update(true);
  splitViewSeparate->update();
}

void PlaybackController::selectionPropertiesChanged(bool redraw)
{
  if (controlsEnabled)
    updateFrameRange();

  // Check if the current frame is outside of the (new) allowed range
  if (currentFrameIdx > frameSlider->maximum())
    setCurrentFrame(frameSlider->maximum());
  else if (currentFrameIdx < frameSlider->minimum())
    setCurrentFrame(frameSlider->minimum());
  else if (redraw)
  {
    splitViewPrimary->update(false, true);
    splitViewSeparate->update(false, true);
  }
}

void PlaybackController::updateFrameRange()
{
  indexRange range1 = currentItem[0] ? currentItem[0]->getFrameIndexRange() : indexRange(-1,-1);
  indexRange range = range1;
  if (currentItem[1])
  {
    // The index range is that of the longer sequence
    indexRange range2 = currentItem[1]->getFrameIndexRange();
    range = indexRange(qMin(range1.first, range2.first), qMax(range1.second, range2.second));
  }
  enableControls(true);
  frameSlider->setEnabled(range != indexRange(-1,-1));    // Disable slider if range == (-1,-1)
  frameSlider->setMaximum(range.second);
  frameSlider->setMinimum(range.first);
  frameSpinBox->setMinimum(range.first);
  frameSpinBox->setMaximum(range.second);
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
    fpsLabel->setStyleSheet("");
    playbackWasStalled = false;
  }

  controlsEnabled = enable;
}

int PlaybackController::getNextFrameIndex()
{
  if (currentFrameIdx >= frameSlider->maximum() || (!currentItem[0]->isIndexedByFrame() && (!currentItem[1] || !currentItem[1]->isIndexedByFrame())))
  {
    // The sequence is at the end. Check the repeat mode to see what the next frame index is
    if (repeatMode == RepeatModeOne)
      // The next frame is the first frame of the current item
      return frameSlider->minimum();
    else
      // The next frame is the first frame of the next item
      return -1;
  }
  return currentFrameIdx + 1;
}

void PlaybackController::timerEvent(QTimerEvent *event)
{
  if (event && event->timerId() != timer.timerId())
    return QWidget::timerEvent(event);

  int nextFrameIdx = getNextFrameIndex();
  if (nextFrameIdx == -1)
  {
    // The next frame is the first frame of the next item.
    bool wrapAround = (repeatMode == RepeatModeAll);
    if (playlist->selectNextItem(wrapAround, true))
    {
      // We jumped to the next item. Start at the first frame.
      DEBUG_PLAYBACK("PlaybackController::timerEvent next item frame %d", frameSlider->minimum());
      setCurrentFrame(frameSlider->minimum());
    }
    else
    {
      // There is no next item. Stop playback
      DEBUG_PLAYBACK("PlaybackController::timerEvent playback done");
      on_playPauseButton_clicked();
    }
  }
  else
  {
    // Do we have to wait for one of the (possibly two) items to load until we can display it/them?
    waitingForItem[0] = currentItem[0]->isLoading() || currentItem[0]->isLoadingDoubleBuffer();
    waitingForItem[1] = splitViewPrimary->isSplitting() && currentItem[1] && (currentItem[1]->isLoading() || currentItem[1]->isLoadingDoubleBuffer());
    if (waitingForItem[0] || waitingForItem[1])
    {
      // The double buffer of the current item or the second item is still loading. Playback is not fast enough.
      // We must wait until the next frame was loaded (in both items) successfully until we can display it.
      // We must pause the timer until this happens.
      timer.stop();
      playbackMode = PlaybackStalled;
      playbackWasStalled = true;
      DEBUG_PLAYBACK("PlaybackController::timerEvent playback stalled");
      return;
    }

    // Go to the next frame and update the splitView
    DEBUG_PLAYBACK("PlaybackController::timerEvent next frame %d", nextFrameIdx);
    setCurrentFrame(nextFrameIdx);
    
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
      if (playbackWasStalled)
        fpsLabel->setStyleSheet("QLabel { background-color: yellow }");
      else
        fpsLabel->setStyleSheet("");
      playbackWasStalled = false;

      timerLastFPSTime = QTime::currentTime();
      timerFPSCounter = 0;
    }

    // Check if the time interval changed (the user changed the rate of the item)
    if (currentItem[0]->isIndexedByFrame() || (currentItem[1] && currentItem[1]->isIndexedByFrame()))
    {
      // One (of the possibly two items) is indexed by frame. Get and set the frame rate
      double frameRate = currentItem[0]->isIndexedByFrame() ? currentItem[0]->getFrameRate() : currentItem[1]->getFrameRate();
      if (frameRate < 0.01)
        frameRate = 0.01;

      int newtimerInterval = 1000.0 / frameRate;
      if (timerInterval != newtimerInterval)
        startOrUpdateTimer();
    }
  }
}

void PlaybackController::currentSelectedItemsDoubleBufferLoad(int itemID)
{
  assert(itemID == 0 || itemID == 1);
  if (playbackMode == PlaybackStalled)
  {
    waitingForItem[itemID] = false;
    if (!waitingForItem[0] && !waitingForItem[1])
    {
      // Playback was stalled because we were waiting for the double buffer to load. 
      // We can go on now.
      DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsDoubleBufferLoad");
      timer.start(timerInterval, Qt::PreciseTimer, this);
      timerEvent(nullptr);
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

  DEBUG_PLAYBACK("PlaybackController::setCurrentFrame %d", frame);

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
