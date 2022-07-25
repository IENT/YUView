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

#include "PlaybackController.h"

#include <QSettings>

#include <common/FunctionsGui.h>
#include <common/Typedef.h>
#include <playlistitem/playlistItem.h>

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define PLAYBACKCONTROLLER_DEBUG 1
#if PLAYBACKCONTROLLER_DEBUG
#define DEBUG_PLAYBACK qDebug
#else
#define DEBUG_PLAYBACK(fmt, ...) ((void)0)
#endif

namespace
{

const auto RepeatModeMapper =
    EnumMapper<PlaybackController::RepeatMode>({{PlaybackController::RepeatMode::Off, "Off"},
                                                {PlaybackController::RepeatMode::One, "One"},
                                                {PlaybackController::RepeatMode::All, "All"}});

}

PlaybackController::PlaybackController()
{
  this->setupUi(this);

  if (is_Q_OS_MAC)
    // There is a bug in Qt that significantly slows down drawing QSlider ticks on Mac
    this->frameSlider->setTickPosition(QSlider::NoTicks);

  // Default fps
  this->fpsLabel->setText("0");
  this->fpsLabel->setStyleSheet("");

  // Load current repeat mode from settings
  QSettings settings;
  auto      repeatModeIdx =
      settings.value("RepeatMode", RepeatModeMapper.indexOf(RepeatMode::Off)).toInt();
  this->repeatMode = RepeatMode::Off;

  if (auto mode = RepeatModeMapper.at(repeatModeIdx))
    this->repeatMode = *mode;
  else
    DEBUG_PLAYBACK("PlaybackController::PlaybackController Error parsing repeat mode index");

  this->timerLastFPSTime = QTime::currentTime();
  this->updateSettings();

  // Initial state is disabled (until an item is selected in the playlist)
  this->enableControls(false);
}

void PlaybackController::setSplitViews(splitViewWidget *primary, splitViewWidget *separate)
{
  this->splitViewPrimary  = primary;
  this->splitViewSeparate = separate;
}

void PlaybackController::setPlaylist(PlaylistTreeWidget *playlistWidget)
{
  this->playlist = playlistWidget;
}

void PlaybackController::pausePlayback()
{
  if (this->playing())
    this->on_playPauseButton_clicked();
}

bool PlaybackController::playing() const
{
  return this->playbackState != PlaybackState::Stopped;
}

bool PlaybackController::isWaitingForCaching() const
{
  return this->playbackState == PlaybackState::WaitingForCache;
}

int PlaybackController::getCurrentFrame() const
{
  return this->currentFrameIdx;
}

void PlaybackController::setRepeatMode(RepeatMode mode)
{
  QSettings settings;
  settings.setValue("RepeatMode", RepeatModeMapper.indexOf(mode));
  this->repeatMode = mode;

  if (repeatMode == RepeatMode::Off)
    repeatModeButton->setIcon(iconRepeatOff);
  else if (repeatMode == RepeatMode::One)
    repeatModeButton->setIcon(iconRepeatOne);
  else if (repeatMode == RepeatMode::All)
    repeatModeButton->setIcon(iconRepeatAll);
}

void PlaybackController::on_stopButton_clicked()
{
  this->pausePlayback();
  this->setCurrentFrame(0);
}

void PlaybackController::on_playPauseButton_clicked()
{
  if (!this->currentItem[0])
    return;

  if (this->playing())
  {
    // Stop the timer, update the icon and fps label text and unfreeze the primary view (maype it
    // was frozen).
    DEBUG_PLAYBACK("PlaybackController::on_playPauseButton_clicked Stop");
    this->timer.stop();
    this->playbackState = PlaybackState::Stopped;
    emit(waitForItemCaching(nullptr));
    this->playPauseButton->setIcon(iconPlay);
    this->fpsLabel->setText("0");
    this->fpsLabel->setStyleSheet("");
    this->splitViewPrimary->freezeView(false);

    this->splitViewPrimary->update(false, true);
    this->splitViewSeparate->update(false, true);
  }
  else
  {
    DEBUG_PLAYBACK("PlaybackController::on_playPauseButton_clicked Start");
    if (this->currentFrameIdx >= this->frameSlider->maximum() &&
        this->repeatMode == RepeatMode::Off)
    {
      // We are currently at the end of the sequence and the user pressed play.
      // If there is no next item to play, replay the current item from the beginning.
      if (!this->playlist->hasNextItem())
        this->setCurrentFrame(this->frameSlider->minimum());
    }

    emit(signalPlaybackStarting());

    if (this->waitForCachingOfItem)
    {
      // Caching is enabled and we shall wait for caching of the current item to complete before
      // starting playback.
      DEBUG_PLAYBACK("PlaybackController::on_playPauseButton_clicked waiting for caching...");
      this->playbackState = PlaybackState::WaitingForCache;
      this->playPauseButton->setIcon(iconPause);
      this->splitViewPrimary->freezeView(true);
      this->playbackWasStalled = false;
      emit(waitForItemCaching(currentItem[0]));

      if (this->playbackState == PlaybackState::WaitingForCache)
      {
        // If we are really waiting, update the views so that the "caching loading" hourglass
        // indicator is drawn.
        this->splitViewPrimary->update(false, false);
        this->splitViewSeparate->update(false, false);
      }
    }
    else
    {
      this->splitViewPrimary->freezeView(true);
      this->startPlayback();
    }
  }
}

void PlaybackController::itemCachingFinished(playlistItem *)
{
  if (this->playbackState == PlaybackState::WaitingForCache)
  {
    // We were waiting and playback can now start
    DEBUG_PLAYBACK("PlaybackController::itemCachingFinished");
    this->startPlayback();
  }
}

void PlaybackController::startPlayback()
{
  this->startOrUpdateTimer();

  // Tell the primary split view that playback just started. This will toggle loading
  // of the double buffer of the currently visible items (if required).
  splitViewPrimary->playbackStarted(getNextFrameIndex());
}

void PlaybackController::startOrUpdateTimer()
{
  if (this->currentItem[0]->properties().isIndexedByFrame() ||
      (this->currentItem[1] && this->currentItem[1]->properties().isIndexedByFrame()))
  {
    // One (of the possibly two items) is indexed by frame. Get and set the frame rate
    auto frameRate = this->currentItem[0]->properties().isIndexedByFrame()
                         ? this->currentItem[0]->properties().frameRate
                         : this->currentItem[1]->properties().frameRate;
    if (frameRate < 0.01)
      frameRate = 0.01;
    this->timerStaticItemCountDown = -1;
    this->timerInterval            = 1000.0 / frameRate;
    DEBUG_PLAYBACK("PlaybackController::startOrUpdateTimer framerate %f", frameRate);
  }
  else
  {
    // The item (or both items) are not indexed by frame.
    // Use the duration of item 0
    this->timerInterval            = 100;
    this->timerStaticItemCountDown = this->currentItem[0]->properties().duration * 10;
    DEBUG_PLAYBACK("PlaybackController::startOrUpdateTimer duration %d", this->timerInterval);
  }

  this->timer.start(timerInterval, Qt::PreciseTimer, this);
  this->playbackState    = PlaybackState::Running;
  this->timerLastFPSTime = QTime::currentTime();
  this->timerFPSCounter  = 0;
}

void PlaybackController::nextFrame()
{
  this->pausePlayback();
  if (this->currentFrameIdx < this->frameSlider->maximum())
    this->setCurrentFrame(this->currentFrameIdx + 1);
}

void PlaybackController::previousFrame()
{
  this->pausePlayback();
  if (this->currentFrameIdx != this->frameSlider->minimum())
    this->setCurrentFrame(this->currentFrameIdx - 1);
}

void PlaybackController::on_frameSlider_valueChanged(int value)
{
  this->pausePlayback();
  this->setCurrentFrame(value);
}

/** Toggle the repeat mode (loop through the list)
 * The signal repeatModeButton->clicked() is connected to this slot
 */
void PlaybackController::on_repeatModeButton_clicked()
{
  if (this->repeatMode == RepeatMode::Off)
    this->setRepeatMode(RepeatMode::One);
  else if (this->repeatMode == RepeatMode::One)
    this->setRepeatMode(RepeatMode::All);
  else if (this->repeatMode == RepeatMode::All)
    this->setRepeatMode(RepeatMode::Off);
}

void PlaybackController::currentSelectedItemsChanged(playlistItem *item1,
                                                     playlistItem *item2,
                                                     bool          changedByPlayback)
{
  QSettings settings;
  auto continuePlayback = settings.value("ContinuePlaybackOnSequenceSelection", false).toBool();

  if (this->playing() && !changedByPlayback && !continuePlayback)
    this->pausePlayback();

  this->currentItem[0] = item1;
  this->currentItem[1] = item2;

  auto frameRange = Range<int>({this->frameSlider->minimum(), this->frameSlider->maximum()});

  if (!(item1 && item1->properties().isIndexedByFrame()) &&
      !(item2 && item2->properties().isIndexedByFrame()))
  {
    // No item selected or the selected item(s) is/are not indexed by a frame (there is no
    // navigation in the item)
    this->enableControls(false);

    // Save the last valid frame index. Now the frame index is invalid.
    if (this->currentFrameIdx != -1)
      this->lastValidFrameIdx = this->currentFrameIdx;
    this->currentFrameIdx = -1;

    if (item1 && !item1->properties().isIndexedByFrame())
    {
      // Setup the frame slider (it is disabled but will display how long we still have to wait)
      // Update the frame slider and spin boxes without emitting more signals
      const QSignalBlocker blocker1(frameSpinBox);
      const QSignalBlocker blocker2(frameSlider);

      auto newMaximum = item1->properties().duration * 10;
      this->frameSlider->setMaximum(newMaximum);
      frameRange.max = newMaximum;

      this->frameSlider->setValue(0);
      this->frameSpinBox->setValue(0);
    }

    if (item1 && (changedByPlayback || (continuePlayback && this->playing())) &&
        this->playbackState != PlaybackState::WaitingForCache)
      // Update the timer
      this->startOrUpdateTimer();

    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsChanged No indexed items - "
                   "currentFrameIdx %d lastValidFrameIdx %d slider %d-%d",
                   this->currentFrameIdx,
                   this->lastValidFrameIdx,
                   frameRange.min,
                   frameRange.max);

    this->splitViewPrimary->update(true);
    this->splitViewSeparate->update();
  }
  else if (this->playing() && (changedByPlayback || continuePlayback))
  {
    if (this->playbackState != PlaybackState::WaitingForCache)
      // Update the timer
      this->startOrUpdateTimer();

    this->updateFrameRange();

    if (!changedByPlayback && continuePlayback && this->currentFrameIdx >= frameRange.max)
      // The user changed this but we want playback to continue. Unfortunately the new selected
      // sequence does not have as many frames as the previous one. So we start playback at the
      // start.
      this->setCurrentFrame(this->frameSlider->minimum());

    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsChanged Playback next - "
                   "currentFrameIdx %d lastValidFrameIdx %d slider %d-%d",
                   this->currentFrameIdx,
                   this->lastValidFrameIdx,
                   frameRange.min,
                   frameRange.max);
  }
  else
  {
    this->updateFrameRange();
    if (frameRange.min == -1 && frameRange.max == -1)
    {
      // The new item is indexed by frame by currently we can not navigate to any frame in it.
      // Save the last valid frame index. Now the frame index is invalid.
      if (this->currentFrameIdx != -1)
        this->lastValidFrameIdx = this->currentFrameIdx;
      this->setCurrentFrame(-1, false);
    }
    else
    {
      if (this->currentFrameIdx == -1)
      {
        // The current frame index is invalid. Set it to a valid value.
        if (this->lastValidFrameIdx != -1)
        {
          // Restore the last valid frame index and check if it is within the current items limits.
          if (this->lastValidFrameIdx < frameRange.min)
            this->lastValidFrameIdx = frameRange.min;
          else if (this->lastValidFrameIdx > frameRange.max)
            this->lastValidFrameIdx = frameRange.max;
          this->setCurrentFrame(this->lastValidFrameIdx, false);
        }
        else
          this->setCurrentFrame(frameRange.min, false);
      }
      else
      {
        // Check if the current frame index is valid in the new limits.
        if (this->currentFrameIdx < frameRange.min)
          this->setCurrentFrame(frameRange.min, false);
        else if (this->lastValidFrameIdx > frameRange.max)
          this->setCurrentFrame(frameRange.max, false);
      }
    }

    this->splitViewPrimary->update(true);
    this->splitViewSeparate->update();

    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsChanged Indexed item - currentFrameIdx "
                   "%d lastValidFrameIdx %d slider %d-%d",
                   this->currentFrameIdx,
                   this->lastValidFrameIdx,
                   frameRange.min,
                   frameRange.max);
  }
}

void PlaybackController::selectionPropertiesChanged(bool redraw)
{
  DEBUG_PLAYBACK("PlaybackController::selectionPropertiesChanged - %s", redraw ? "redraw" : "");

  if (this->controlsEnabled)
    this->updateFrameRange();

  // Check if the current frame is outside of the (new) allowed range
  bool updated = false;
  if (currentFrameIdx > this->frameSlider->maximum())
    updated = this->setCurrentFrame(this->frameSlider->maximum());
  else if (currentFrameIdx < this->frameSlider->minimum())
    updated = this->setCurrentFrame(this->frameSlider->minimum());

  if (redraw && !updated)
  {
    this->splitViewPrimary->update(false, true);
    this->splitViewSeparate->update(false, true);
  }
}

void PlaybackController::updateSettings()
{
  // Is caching active and do we wait for the caching to complete before playing back?
  QSettings settings;
  settings.beginGroup("VideoCache");
  bool caching               = settings.value("Enabled", true).toBool();
  bool wait                  = settings.value("PlaybackPauseCaching", false).toBool();
  this->waitForCachingOfItem = caching && wait;

  // Load the icons for the buttons
  this->iconPlay      = functionsGui::convertIcon(":img_play.png");
  this->iconStop      = functionsGui::convertIcon(":img_stop.png");
  this->iconPause     = functionsGui::convertIcon(":img_pause.png");
  this->iconRepeatOff = functionsGui::convertIcon(":img_repeat.png");
  this->iconRepeatAll = functionsGui::convertIcon(":img_repeat_on.png");
  this->iconRepeatOne = functionsGui::convertIcon(":img_repeat_one.png");

  // Set button icons
  if (this->playing())
    this->playPauseButton->setIcon(this->iconPlay);
  else
    this->playPauseButton->setIcon(this->iconPause);
  this->stopButton->setIcon(this->iconStop);

  // Don't change the repeat mode but set the icons
  this->setRepeatMode(this->repeatMode);
}

void PlaybackController::updateFrameRange()
{
  // Update the frame slider and spin boxes without emitting more signals
  const QSignalBlocker blocker1(this->frameSpinBox);
  const QSignalBlocker blocker2(this->frameSlider);

  auto range1 = IndexRange(-1, -1);
  if (this->currentItem[0])
    range1 = currentItem[0]->properties().startEndRange;
  auto range = range1;
  if (this->currentItem[1])
  {
    // The index range is that of the longer sequence
    auto range2 = currentItem[1]->properties().startEndRange;
    range       = {std::min(range1.first, range2.first), std::max(range1.second, range2.second)};
  }

  this->enableControls(true);
  this->frameSlider->setEnabled(range !=
                                IndexRange({-1, -1})); // Disable slider if range == (-1,-1)
  this->frameSlider->setMaximum(range.second);
  this->frameSlider->setMinimum(range.first);
  this->frameSpinBox->setMinimum(range.first);
  this->frameSpinBox->setMaximum(range.second);

  DEBUG_PLAYBACK("PlaybackController::updateFrameRange - new range %d-%d",
                 this->frameSlider->minimum(),
                 this->frameSlider->maximum());
}

void PlaybackController::enableControls(bool enable)
{
  this->frameSlider->setEnabled(enable);
  this->frameSpinBox->setEnabled(enable);
  this->fpsLabel->setEnabled(enable);

  // If disabling, also reset the controls but emit no signals
  if (!enable)
  {
    const QSignalBlocker blocker(this->frameSlider);
    this->fpsLabel->setText("0");
    this->fpsLabel->setStyleSheet("");
    this->playbackWasStalled = false;
  }

  this->controlsEnabled = enable;
}

int PlaybackController::getNextFrameIndex()
{
  if (this->currentFrameIdx >= this->frameSlider->maximum() ||
      (!this->currentItem[0]->properties().isIndexedByFrame() &&
       (!this->currentItem[1] || !this->currentItem[1]->properties().isIndexedByFrame())))
  {
    // The sequence is at the end. Check the repeat mode to see what the next frame index is
    if (this->repeatMode == RepeatMode::One)
      // The next frame is the first frame of the current item
      return this->frameSlider->minimum();
    else
      // The next frame is the first frame of the next item
      return -1;
  }
  return this->currentFrameIdx + 1;
}

void PlaybackController::timerEvent(QTimerEvent *event)
{
  if (event && event->timerId() != this->timer.timerId())
  {
    DEBUG_PLAYBACK("PlaybackController::timerEvent Different Timer IDs");
    return QWidget::timerEvent(event);
  }
  if (this->timerStaticItemCountDown > 0)
  {
    // We are currently displaying a static item (until timerStaticItemCountDown reaches 0)
    // Update the frame slider.
    DEBUG_PLAYBACK("PlaybackController::timerEvent Showing Static Item");
    const QSignalBlocker blocker1(this->frameSlider);
    const QSignalBlocker blocker2(this->frameSpinBox);
    this->timerStaticItemCountDown--;
    this->frameSlider->setValue(this->frameSlider->value() + 1);
    this->frameSpinBox->setValue((this->timerStaticItemCountDown / 10 + 1));
    return;
  }

  int nextFrameIdx = this->getNextFrameIndex();
  if (nextFrameIdx == -1)
  {
    if (this->waitForCachingOfItem)
    {
      // Set this before the next item is selected so that the timer is not updated
      this->playbackState = PlaybackState::WaitingForCache;
      // Stop the timer. The event itemCachingFinished will restart it.
      this->timer.stop();
    }

    bool wrapAround = (this->repeatMode == RepeatMode::All);
    if (this->playlist->selectNextItem(wrapAround, true))
    {
      // We jumped to the next item. Start at the first frame.
      DEBUG_PLAYBACK("PlaybackController::timerEvent next item frame %d",
                     this->frameSlider->minimum());
      this->setCurrentFrame(this->frameSlider->minimum());

      if (this->waitForCachingOfItem)
      {
        DEBUG_PLAYBACK("PlaybackController::timerEvent waiting for caching...");
        emit(this->waitForItemCaching(this->currentItem[0]));

        if (this->playbackState == PlaybackState::WaitingForCache)
        {
          // If we are really waiting, update the views so that the "caching loading" hourglass
          // indicator is drawn.
          this->splitViewPrimary->update(false, false);
          this->splitViewSeparate->update(false, false);
        }
      }
    }
    else
    {
      // There is no next item. Stop playback
      DEBUG_PLAYBACK("PlaybackController::timerEvent playback done");
      this->on_playPauseButton_clicked();
    }
  }
  else
  {
    // Do we have to wait for one of the (possibly two) items to load until we can display it/them?
    this->waitingForItem[0] =
        this->currentItem[0]->isLoading() || this->currentItem[0]->isLoadingDoubleBuffer();
    this->waitingForItem[1] =
        this->splitViewPrimary->isSplitting() && this->currentItem[1] &&
        (this->currentItem[1]->isLoading() || this->currentItem[1]->isLoadingDoubleBuffer());
    if (this->waitingForItem[0] || this->waitingForItem[1])
    {
      // The double buffer of the current item or the second item is still loading. Playback is not
      // fast enough. We must wait until the next frame was loaded (in both items) successfully
      // until we can display it. We must pause the timer until this happens.
      this->timer.stop();
      this->playbackState      = PlaybackState::Stalled;
      this->playbackWasStalled = true;
      DEBUG_PLAYBACK("PlaybackController::timerEvent playback stalled");
      return;
    }

    // Go to the next frame and update the splitView
    DEBUG_PLAYBACK("PlaybackController::timerEvent next frame %d", nextFrameIdx);
    this->setCurrentFrame(nextFrameIdx);

    this->timerFPSCounter++;
    if (this->timerFPSCounter >= 50)
    {
      auto newFrameTime         = QTime::currentTime();
      auto msecsSinceLastUpdate = this->timerLastFPSTime.msecsTo(newFrameTime);

      // Print the frames per second as float with one digit after the decimal dot.
      auto framesPerSec = (50.0 / (double(msecsSinceLastUpdate) / 1000.0));
      if (framesPerSec > 0)
        this->fpsLabel->setText(QString::number(framesPerSec, 'f', 1));
      if (playbackWasStalled)
        this->fpsLabel->setStyleSheet("QLabel { background-color: yellow }");
      else
        this->fpsLabel->setStyleSheet("");
      this->playbackWasStalled = false;

      this->timerLastFPSTime = QTime::currentTime();
      this->timerFPSCounter  = 0;
    }

    // Check if the time interval changed (the user changed the rate of the item)
    if (this->currentItem[0]->properties().isIndexedByFrame() ||
        (this->currentItem[1] && this->currentItem[1]->properties().isIndexedByFrame()))
    {
      // One (of the possibly two items) is indexed by frame. Get and set the frame rate
      auto frameRate = this->currentItem[0]->properties().isIndexedByFrame()
                           ? this->currentItem[0]->properties().frameRate
                           : this->currentItem[1]->properties().frameRate;
      if (frameRate < 0.01)
        frameRate = 0.01;

      auto newtimerInterval = int(1000.0 / frameRate);
      if (timerInterval != newtimerInterval)
        this->startOrUpdateTimer();
    }
  }
}

void PlaybackController::currentSelectedItemsLoadFinished(int itemID, BufferSelection buffer)
{
  assert(itemID == 0 || itemID == 1);
  if (this->playbackState == PlaybackState::Stalled)
  {
    this->waitingForItem[itemID] = false;
    auto waitFinished            = (!this->waitingForItem[0] && !this->waitingForItem[1]);
    if (waitFinished)
    {
      DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsLoadFinished - timer interval %d",
                     this->timerInterval);
      if (buffer != BufferSelection::Primary)
        this->timerEvent(nullptr);
      this->timer.start(this->timerInterval, Qt::PreciseTimer, this);
      this->playbackState = PlaybackState::Running;
    }
  }

  if (buffer == BufferSelection::Primary)
  {
    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsLoadFinished refreshing item");
    this->splitViewPrimary->update(true, false);
    this->splitViewSeparate->update(true, false);
  }
}

/* Set the value currentFrame to frame and update the value in the splinBox and the slider without
 * invoking any events from these controls. Also update the splitView.
 */
bool PlaybackController::setCurrentFrame(int frame, bool updateView)
{
  if (frame == this->currentFrameIdx)
    return false;
  if (!this->currentItem[0] ||
      (!this->currentItem[0]->properties().isIndexedByFrame() &&
       (!this->currentItem[1] || !this->currentItem[1]->properties().isIndexedByFrame())))
    // Both items (that are selcted) are not indexed by frame.
    return false;

  DEBUG_PLAYBACK("PlaybackController::setCurrentFrame %d", frame);

  // Set the new value in the controls without invoking another signal
  const QSignalBlocker blocker1(frameSpinBox);
  const QSignalBlocker blocker2(frameSlider);
  this->currentFrameIdx = frame;
  this->frameSpinBox->setValue(frame);
  this->frameSlider->setValue(frame);

  if (updateView)
  {
    // Also update the view to display the new frame
    this->splitViewPrimary->update(true);
    this->splitViewSeparate->update();
    DEBUG_PLAYBACK("PlaybackController::setCurrentFrame refreshing item");
  }
  return updateView;
}
