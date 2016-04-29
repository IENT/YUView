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
#include <QDebug>

PlaybackController::PlaybackController()
{
  setupUi(this);

  // Default fps
  fpsLabel->setText("0");

  // Load the icons for the buttons
  iconPlay = QIcon(":img_play.png");
  iconStop = QIcon(":img_stop.png");
  iconPause = QIcon(":img_pause.png");
  iconRepeatOff = QIcon(":img_repeat.png");
  iconRepeatAll = QIcon(":img_repeat_on.png");
  iconRepeatOne = QIcon(":img_repeat_one.png");

  // Set button icons
  playPauseButton->setIcon( iconPlay );
  stopButton->setIcon( iconStop );

  // Load current repeat mode from settings (and set icon of the button)
  QSettings settings;
  setRepeatMode ( (RepeatMode)settings.value("RepeatMode", RepeatModeOff).toUInt() );

  // Initialize variables
  currentFrameIdx = 0;
  timerFPSCounter = 0;
  timerId = -1;
  timerInterval = -1;
  timerFPSCounter = 0;
  timerLastFPSTime = QTime::currentTime();

  currentItem = NULL;

  splitView = NULL;

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

  if (timerId != -1)
  {
    // The timer is running. Stop it.
    killTimer(timerId);
    timerId = -1;

    // update our play/pause icon
    playPauseButton->setIcon(iconPlay);

    // Set the fps text to 0
    fpsLabel->setText("0");
  }
  else
  {
    // Timer is not running. Start it up.
    // Get the frame rate of the current item. Lower limit is 0.01 fps.
    if (currentItem->isIndexedByFrame())
    {
      double frameRate = currentItem->getFrameRate();
      if (frameRate < 0.01)
        frameRate = 0.01;

      timerInterval = 1000.0 / frameRate;
    }
    else
    {
      timerInterval = int(currentItem->getDuration() * 1000);
    }

    //// the user might have changed the currentFrame and hit play, start caching immediatly if we are outside of the last caching range
    //indexRange frameRange = currentItem->getFrameIndexRange();
    //if (currentFrameIdx < lastCacheRange.first || currentFrameIdx > lastCacheRange.second)
    //  {
    //    indexRange cacheRange;
    //    cacheRange.first = currentFrameIdx;
    //    cacheRange.second = ((currentFrameIdx+cacheSizeInFrames)>frameRange.second) ? frameRange.second : currentFrameIdx+cacheSizeInFrames;
    //    lastCacheRange = cacheRange;
    //    emit ControllerStartCachingCurrentSelection(cacheRange);
    //  }

    timerId = startTimer(timerInterval, Qt::PreciseTimer);
    timerLastFPSTime = QTime::currentTime();
    timerFPSCounter = 0;

    // update our play/pause icon
    playPauseButton->setIcon(iconPause);
  }
}

void PlaybackController::nextFrame()
{
  // If the next Frame slot is toggeled, abort playback (if running)
  pausePlayback();

  // Can we go to the next frame?
  if (currentFrameIdx >= frameSlider->maximum())
    return;

  // Go to the next frame and update the splitView
  setCurrentFrame( currentFrameIdx + 1 );
}

void PlaybackController::previousFrame()
{
  // If the previous Frame slot is toggeled, abort playback (if running)
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

void PlaybackController::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  // The playback controller only condiers the first item
  // TODO: Is this correct? What if the second item has more frames than the first one? Should the user be able to navigate here? I would think yes!
  Q_UNUSED(item2);

  // Stop playback (if running)
  pausePlayback();

  if (!item1 || !item1->isIndexedByFrame())
  {
    // No item selected or the selected item is not indexed by a frame (there is no navigation in the item)
    enableControls(false);

    // Also update the view to display an empty widget
    splitView->update();

    currentItem = NULL;
    return;
  }

  // Set the correct number of frames
  enableControls(true);
  indexRange range = item1->getFrameIndexRange();
  frameSlider->setEnabled( range != indexRange(-1,-1) );    // Disable slider if range == (-1,-1)
  frameSlider->setMaximum( range.second );
  frameSlider->setMinimum( range.first );
  frameSpinBox->setMinimum( range.first );
  frameSpinBox->setMaximum( range.second );

  currentItem = item1;

  //// Cache: start caching, starting from the current selected frame
  //indexRange cacheRange;
  //cacheRange.first=currentFrameIdx;
  //cacheRange.second=(currentFrameIdx + cacheSizeInFrames) > range.second ? range.second : currentFrameIdx + cacheSizeInFrames;
  //emit ControllerStartCachingCurrentSelection(cacheRange);
  //// remember the last cache range
  //// TODO: do it right in case of two or more items in the playlist
  //lastCacheRange = cacheRange;

  // Also update the view to display the new frame
  splitView->update();
}

void PlaybackController::selectionPropertiesChanged(bool redraw)
{
  if (controlsEnabled)
  {
    // Update min/max frame index values of the controls
    indexRange range = currentItem->getFrameIndexRange();
    frameSlider->setEnabled( range != indexRange(-1,-1) );    // Disable slider if range == (-1,-1)
    frameSlider->setMaximum( range.second );
    frameSlider->setMinimum( range.first );
    frameSpinBox->setMinimum( range.first );
    frameSpinBox->setMaximum( range.second );
  }

  // Check if the current frame is outside of the (new) allowed range
  if (currentFrameIdx > frameSlider->maximum())
    setCurrentFrame( frameSlider->maximum() );
  else if (currentFrameIdx < frameSlider->minimum())
    setCurrentFrame( frameSlider->minimum() );
  else if (redraw)
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

  controlsEnabled = enable;
}

void PlaybackController::timerEvent(QTimerEvent * event)
{
  Q_UNUSED(event);

  if (currentFrameIdx >= frameSlider->maximum() || !currentItem->isIndexedByFrame() )
  {
    // The sequence is at the end. The behavior now depends on the set repeat mode.
    switch (repeatMode)
    {
      case RepeatModeOff:
        // Repeat is off. Just stop playback.
        on_playPauseButton_clicked();
        break;
      case RepeatModeOne:
        // Repeat the current item. So the next frame is the first frame of the currently selected item.
        setCurrentFrame( frameSlider->minimum() );
        break;
      case RepeatModeAll:
        // TODO: The next item from the playlist has to be selected.
        break;
    }
  }
  else
  {

    /*if (repeatMode==RepeatModeOff || repeatMode==RepeatModeAll)
      emit ControllerRemoveFromCache(indexRange(currentFrameIdx,currentFrameIdx));*/

    // Go to the next frame and update the splitView
    setCurrentFrame( currentFrameIdx + 1 );

    //// trigger new cache signal, if we reached a threshold distance to the upper limit of our last caching operation
    //indexRange frameRange = currentItem->getFrameIndexRange();
    //if (currentFrameIdx > (lastCacheRange.second-cacheMargin) && lastCacheRange.second<frameRange.second )
    //  {
    //    indexRange cacheRange;
    //    cacheRange.first=lastCacheRange.second;
    //    cacheRange.second=(lastCacheRange.second+cacheSizeInFrames)>frameRange.second?frameRange.second : lastCacheRange.second+cacheSizeInFrames;
    //    qDebug() << "Cache event triggered" << endl;
    //    lastCacheRange = cacheRange;
    //    emit ControllerStartCachingCurrentSelection(cacheRange);
    //  }

    // Update the FPS counter every 50 frames
    timerFPSCounter++;
    if (timerFPSCounter > 50)
    {
      QTime newFrameTime = QTime::currentTime();
      double msecsSinceLastUpdate = (double)timerLastFPSTime.msecsTo(newFrameTime);

      int framesPerSec = (int)(50 / (msecsSinceLastUpdate / 1000.0));
      if (framesPerSec > 0)
        fpsLabel->setText(QString().setNum(framesPerSec));

      timerLastFPSTime = QTime::currentTime();
      timerFPSCounter = 0;
    }

    // Check if the time interval changed (the user changed the rate of the item)
    double frameRate = currentItem->getFrameRate();
    if (frameRate < 0.01)
      frameRate = 0.01;
    double newTimerInterval = 1000.0 / frameRate;
    if (newTimerInterval != timerInterval)
    {
      // The interval changed. We have to retsart the timer.
      timerInterval = newTimerInterval;
      killTimer(timerId);
      timerId = startTimer(timerInterval, Qt::PreciseTimer);
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
  QObject::disconnect(frameSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  QObject::disconnect(frameSlider, SIGNAL(valueChanged(int)), NULL, NULL);
  currentFrameIdx = frame;
  frameSpinBox->setValue(currentFrameIdx);
  frameSlider->setValue(currentFrameIdx);
  QObject::connect(frameSpinBox, SIGNAL(valueChanged(int)), this, SLOT(on_frameSpinBox_valueChanged(int)));
  QObject::connect(frameSlider, SIGNAL(valueChanged(int)), this, SLOT(on_frameSlider_valueChanged(int)));

  // Also update the view to display the new frame
  splitView->update();
}
