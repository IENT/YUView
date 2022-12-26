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

using namespace std::chrono_literals;

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define PLAYBACKCONTROLLER_DEBUG 0
#if PLAYBACKCONTROLLER_DEBUG && !NDEBUG
#define DEBUG_PLAYBACK qDebug
#else
#define DEBUG_PLAYBACK(fmt, ...) ((void)0)
#endif

PlaybackController::PlaybackController()
{
  this->ui.setupUi(this);

  if (is_Q_OS_MAC)
    // There is a bug in Qt that significantly slows down drawing QSlider ticks on Mac
    this->ui.frameSlider->setTickPosition(QSlider::NoTicks);

  this->ui.fpsLabel->setText("0");
  this->ui.fpsLabel->setStyleSheet("");

  QSettings  settings;
  const auto repeatModeOffIndex = static_cast<int>(RepeatModeMapper.indexOf(RepeatMode::Off));
  auto       repeatModeIdx      = settings.value("RepeatMode", repeatModeOffIndex).toInt();
  if (auto newRepeatMode = RepeatModeMapper.at(repeatModeIdx))
    this->repeatMode = *newRepeatMode;

  this->timerLastFPSTime = QTime::currentTime();

  this->loadButtonIcons();
  this->updatePlayPauseButtonIcon();
  this->ui.stopButton->setIcon(this->iconStop);
  this->setRepeatModeAndUpdateIcons(this->repeatMode);

  this->updateSettings();
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
  return this->playbackMode != PlaybackMode::Stopped;
}

bool PlaybackController::isWaitingForCaching() const
{
  return this->playbackMode == PlaybackMode::WaitingForCache;
}

int PlaybackController::getCurrentFrame() const
{
  return this->currentFrameIdx;
}

void PlaybackController::setRepeatModeAndUpdateIcons(const RepeatMode mode)
{
  QSettings settings;
  settings.setValue("RepeatMode", static_cast<int>(RepeatModeMapper.indexOf(mode)));
  this->repeatMode = mode;

  if (mode == RepeatMode::Off)
    this->ui.repeatModeButton->setIcon(this->iconRepeatOff);
  else if (mode == RepeatMode::One)
    this->ui.repeatModeButton->setIcon(this->iconRepeatOne);
  else if (mode == RepeatMode::All)
    this->ui.repeatModeButton->setIcon(this->iconRepeatAll);
}

void PlaybackController::on_stopButton_clicked()
{
  this->pausePlayback();
  this->setCurrentFrameAndUpdate(0);
}

void PlaybackController::on_playPauseButton_clicked()
{
  if (!this->currentItem[0])
    return;

  if (this->playing())
  {
    DEBUG_PLAYBACK("PlaybackController::on_playPauseButton_clicked Stop");
    this->timer.stop();
    this->playbackMode = PlaybackMode::Stopped;
    emit(waitForItemCaching(nullptr));
    this->ui.fpsLabel->setText("0");
    this->ui.fpsLabel->setStyleSheet("");
    this->splitViewPrimary->freezeView(false);

    this->splitViewPrimary->update(false, true);
    this->splitViewSeparate->update(false, true);
  }
  else
  {
    DEBUG_PLAYBACK("PlaybackController::on_playPauseButton_clicked Start");
    if (this->currentFrameIdx >= this->ui.frameSlider->maximum() &&
        this->repeatMode == RepeatMode::Off)
    {
      if (!this->playlist->hasNextItem())
        this->setCurrentFrameAndUpdate(this->ui.frameSlider->minimum());
    }

    emit(signalPlaybackStarting());

    if (this->waitForCachingOfItem)
    {
      DEBUG_PLAYBACK("PlaybackController::on_playPauseButton_clicked waiting for caching...");
      this->playbackMode = PlaybackMode::WaitingForCache;
      this->splitViewPrimary->freezeView(true);
      this->playbackWasStalled = false;
      emit(waitForItemCaching(this->currentItem[0]));

      if (this->playbackMode == PlaybackMode::WaitingForCache)
      {
        // Update the views so that the "caching loading" hourglass indicator is drawn.
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

  this->updatePlayPauseButtonIcon();
}

void PlaybackController::itemCachingFinished(playlistItem *)
{
  if (this->playbackMode == PlaybackMode::WaitingForCache)
  {
    DEBUG_PLAYBACK("PlaybackController::itemCachingFinished");
    this->startPlayback();
  }
}

void PlaybackController::startPlayback()
{
  this->startOrUpdateTimer();

  // Tell the primary split view that playback just started. This will toggle loading
  // of the double buffer of the currently visible items (if required).
  if (auto nextFrameIndex = this->getNextFrameIndexInCurrentItem())
    this->splitViewPrimary->playbackStarted(*nextFrameIndex);
}

void PlaybackController::startOrUpdateTimer()
{
  // Get the frame rate of the current item. Lower limit is 0.01 fps (100 seconds per frame).
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
    this->timerInterval = std::chrono::duration_cast<std::chrono::milliseconds>(1000ms / frameRate);
    DEBUG_PLAYBACK("PlaybackController::startOrUpdateTimer framerate %f", frameRate);
  }
  else
  {
    // The item (or both items) are not indexed by frame.
    // Use the duration of item 0
    this->timerInterval            = 100ms;
    this->timerStaticItemCountDown = this->currentItem[0]->properties().duration * 10;
    DEBUG_PLAYBACK("PlaybackController::startOrUpdateTimer duration %d", this->timerInterval);
  }

  this->timer.start(this->timerInterval.count(), Qt::PreciseTimer, this);
  this->playbackMode     = PlaybackMode::Running;
  this->timerLastFPSTime = QTime::currentTime();
  this->timerFPSCounter  = 0;
}

void PlaybackController::nextFrame()
{
  this->pausePlayback();
  if (this->currentFrameIdx < this->ui.frameSlider->maximum())
    this->setCurrentFrameAndUpdate(this->currentFrameIdx + 1);
}

void PlaybackController::previousFrame()
{
  this->pausePlayback();
  if (this->currentFrameIdx != this->ui.frameSlider->minimum())
    this->setCurrentFrameAndUpdate(this->currentFrameIdx - 1);
}

void PlaybackController::on_frameSlider_valueChanged(int value)
{
  this->pausePlayback();
  this->setCurrentFrameAndUpdate(value);
}

void PlaybackController::on_repeatModeButton_clicked()
{
  if (this->repeatMode == RepeatMode::Off)
    this->setRepeatModeAndUpdateIcons(RepeatMode::One);
  else if (this->repeatMode == RepeatMode::One)
    this->setRepeatModeAndUpdateIcons(RepeatMode::All);
  else if (this->repeatMode == RepeatMode::All)
    this->setRepeatModeAndUpdateIcons(RepeatMode::Off);
}

void PlaybackController::currentSelectedItemsChanged(playlistItem *item1,
                                                     playlistItem *item2,
                                                     bool          chageByPlayback)
{
  QSettings settings;
  auto continuePlayback = settings.value("ContinuePlaybackOnSequenceSelection", false).toBool();

  if (this->playing() && !chageByPlayback && !continuePlayback)
    this->pausePlayback();

  this->currentItem[0] = item1;
  this->currentItem[1] = item2;

  const auto item1IndexedByFrame   = item1 && item1->properties().isIndexedByFrame();
  const auto item2IndexedByFrame   = item2 && item2->properties().isIndexedByFrame();
  const auto anyItemIndexedByFrame = (item1IndexedByFrame || item2IndexedByFrame);
  if (!anyItemIndexedByFrame)
  {
    this->enableControls(false);

    // Save the last valid frame index. Now the frame index is invalid.
    if (this->currentFrameIdx != -1)
      this->lastValidFrameIdx = this->currentFrameIdx;
    this->currentFrameIdx = -1;

    if (item1 && !item1->properties().isIndexedByFrame())
    {
      const auto sliderMax = item1->properties().duration * 10;
      this->updateFrameSliderAndSpinBoxWithoutSignals(0, sliderMax);
    }

    if (item1 && (chageByPlayback || (continuePlayback && this->playing())) &&
        this->playbackMode != PlaybackMode::WaitingForCache)
      // Update the timer
      this->startOrUpdateTimer();

    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsChanged No indexed items - "
                   "currentFrameIdx %d lastValidFrameIdx %d slider %d-%d",
                   this->currentFrameIdx,
                   this->lastValidFrameIdx,
                   this->frameSlider->minimum(),
                   this->frameSlider->maximum());

    // Also update the view to display the new frame
    this->splitViewPrimary->update(true);
    this->splitViewSeparate->update();
  }
  else if (this->playing() && (chageByPlayback || continuePlayback))
  {
    if (this->playbackMode != PlaybackMode::WaitingForCache)
      // Update the timer
      this->startOrUpdateTimer();

    this->updateFrameRange();

    if (!chageByPlayback && continuePlayback &&
        this->currentFrameIdx >= this->ui.frameSlider->maximum())
      // The user changed this but we want playback to continue. Unfortunately the new selected
      // sequence does not have as many frames as the previous one. So we start playback at the
      // start.
      this->setCurrentFrameAndUpdate(this->ui.frameSlider->minimum());

    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsChanged Playback next - "
                   "currentFrameIdx %d lastValidFrameIdx %d slider %d-%d",
                   this->currentFrameIdx,
                   this->lastValidFrameIdx,
                   this->frameSlider->minimum(),
                   this->frameSlider->maximum());
  }
  else
  {
    this->updateFrameRange();
    if (this->ui.frameSlider->minimum() == -1 && this->ui.frameSlider->maximum() == -1)
    {
      // The new item is indexed by frame by currently we can not navigate to any frame in it.
      // Save the last valid frame index. Now the frame index is invalid.
      if (this->currentFrameIdx != -1)
        this->lastValidFrameIdx = this->currentFrameIdx;
      this->setCurrentFrameAndUpdate(-1, false);
    }
    else
    {
      if (this->currentFrameIdx == -1)
      {
        // The current frame index is invalid. Set it to a valid value.
        if (this->lastValidFrameIdx != -1)
        {
          // Restore the last valid frame index and check if it is within the current items limits.
          if (this->lastValidFrameIdx < this->ui.frameSlider->minimum())
            this->lastValidFrameIdx = this->ui.frameSlider->minimum();
          else if (this->lastValidFrameIdx > this->ui.frameSlider->maximum())
            this->lastValidFrameIdx = this->ui.frameSlider->maximum();
          this->setCurrentFrameAndUpdate(this->lastValidFrameIdx, false);
        }
        else
          this->setCurrentFrameAndUpdate(this->ui.frameSlider->minimum(), false);
      }
      else
      {
        // Check if the current frame index is valid in the new limits.
        if (this->currentFrameIdx < this->ui.frameSlider->minimum())
          this->setCurrentFrameAndUpdate(this->ui.frameSlider->minimum(), false);
        else if (this->lastValidFrameIdx > this->ui.frameSlider->maximum())
          this->setCurrentFrameAndUpdate(this->ui.frameSlider->maximum(), false);
      }
    }

    this->splitViewPrimary->update(true);
    this->splitViewSeparate->update();

    DEBUG_PLAYBACK("PlaybackController::currentSelectedItemsChanged Indexed item - currentFrameIdx "
                   "%d lastValidFrameIdx %d slider %d-%d",
                   this->currentFrameIdx,
                   this->lastValidFrameIdx,
                   this->frameSlider->minimum(),
                   this->frameSlider->maximum());
  }
}

void PlaybackController::selectionPropertiesChanged(bool redraw)
{
  if (this->controlsEnabled)
    this->updateFrameRange();

  // Check if the current frame is outside of the (new) allowed range
  bool updated = false;
  if (this->currentFrameIdx > this->ui.frameSlider->maximum())
    updated = this->setCurrentFrameAndUpdate(this->ui.frameSlider->maximum());
  else if (this->currentFrameIdx < this->ui.frameSlider->minimum())
    updated = this->setCurrentFrameAndUpdate(this->ui.frameSlider->minimum());

  if (redraw && !updated)
  {
    this->splitViewPrimary->update(false, true);
    this->splitViewSeparate->update(false, true);
  }
}

void PlaybackController::updateSettings()
{
  QSettings settings;
  settings.beginGroup("VideoCache");
  auto caching               = settings.value("Enabled", true).toBool();
  auto wait                  = settings.value("PlaybackPauseCaching", false).toBool();
  this->waitForCachingOfItem = caching && wait;
}

void PlaybackController::loadButtonIcons()
{
  this->iconPlay      = functionsGui::convertIcon(":img_play.png");
  this->iconStop      = functionsGui::convertIcon(":img_stop.png");
  this->iconPause     = functionsGui::convertIcon(":img_pause.png");
  this->iconRepeatOff = functionsGui::convertIcon(":img_repeat.png");
  this->iconRepeatAll = functionsGui::convertIcon(":img_repeat_on.png");
  this->iconRepeatOne = functionsGui::convertIcon(":img_repeat_one.png");
}

void PlaybackController::updatePlayPauseButtonIcon()
{
  if (this->playbackMode == PlaybackMode::Stopped)
    this->ui.playPauseButton->setIcon(this->iconPlay);
  else
    this->ui.playPauseButton->setIcon(this->iconPause);
}

void PlaybackController::updateFrameRange()
{
  auto range =
      this->currentItem[0] ? this->currentItem[0]->properties().startEndRange : indexRange(-1, -1);
  if (this->currentItem[1])
  {
    // The index range is that of the longer sequence
    auto rangeItem2 = this->currentItem[1]->properties().startEndRange;
    range.first     = std::min(range.first, rangeItem2.first);
    range.second    = std::min(range.second, rangeItem2.second);
  }

  this->enableControls(true);
  const QSignalBlocker blocker1(this->ui.frameSpinBox);
  const QSignalBlocker blocker2(this->ui.frameSlider);
  this->ui.frameSlider->setEnabled(range !=
                                   indexRange(-1, -1)); // Disable slider if range == (-1,-1)
  this->ui.frameSlider->setMaximum(range.second);
  this->ui.frameSlider->setMinimum(range.first);
  this->ui.frameSpinBox->setMinimum(range.first);
  this->ui.frameSpinBox->setMaximum(range.second);

  DEBUG_PLAYBACK("PlaybackController::updateFrameRange - new range %d-%d",
                 this->frameSlider->minimum(),
                 this->frameSlider->maximum());
}

void PlaybackController::enableControls(bool enable)
{
  this->ui.frameSlider->setEnabled(enable);
  this->ui.frameSpinBox->setEnabled(enable);
  this->ui.fpsLabel->setEnabled(enable);

  const auto resetControls = !enable;
  if (resetControls)
  {
    const QSignalBlocker blocker(this->ui.frameSlider);
    this->ui.fpsLabel->setText("0");
    this->ui.fpsLabel->setStyleSheet("");
    this->playbackWasStalled = false;
  }

  this->controlsEnabled = enable;
}

std::optional<int> PlaybackController::getNextFrameIndexInCurrentItem()
{
  const auto isSliderAtEnd           = this->currentFrameIdx >= this->ui.frameSlider->maximum();
  const auto firstItemIndexedByFrame = this->currentItem[0]->properties().isIndexedByFrame();
  const auto secondItemSetAndIndexedByFrame =
      this->currentItem[1] && this->currentItem[1]->properties().isIndexedByFrame();
  const auto anyItemIndexedByFrame = firstItemIndexedByFrame || secondItemSetAndIndexedByFrame;

  if (isSliderAtEnd || !anyItemIndexedByFrame)
  {
    if (this->repeatMode == RepeatMode::One)
      return this->ui.frameSlider->minimum();

    return {};
  }
  return this->currentFrameIdx + 1;
}

void PlaybackController::timerEvent(QTimerEvent *event)
{
  if (event && event->timerId() != timer.timerId())
  {
    DEBUG_PLAYBACK("PlaybackController::timerEvent Different Timer IDs");
    QWidget::timerEvent(event);
    return;
  }

  const auto isCurrentItemStaticItem = this->timerStaticItemCountDown > 0;
  if (isCurrentItemStaticItem)
  {
    DEBUG_PLAYBACK("PlaybackController::timerEvent Showing Static Item");
    const QSignalBlocker blocker1(this->ui.frameSlider);
    const QSignalBlocker blocker2(this->ui.frameSpinBox);
    this->timerStaticItemCountDown--;
    this->ui.frameSlider->setValue(this->ui.frameSlider->value() + 1);
    this->ui.frameSpinBox->setValue((this->timerStaticItemCountDown / 10 + 1));
    return;
  }

  auto nextFrameIdx = this->getNextFrameIndexInCurrentItem();
  if (!nextFrameIdx)
  {
    if (this->waitForCachingOfItem)
    {
      // Set this before the next item is selected so that the timer is not updated
      this->playbackMode = PlaybackMode::WaitingForCache;
      // The event itemCachingFinished will restart the timer.
      this->timer.stop();
    }

    const auto wrapAround  = (this->repeatMode == RepeatMode::All);
    const auto hasNextItem = this->playlist->selectNextItem(wrapAround, true);
    if (!hasNextItem)
    {
      DEBUG_PLAYBACK("PlaybackController::timerEvent no next item. Stopping.");
      this->on_playPauseButton_clicked();
    }
    else
    {
      DEBUG_PLAYBACK("PlaybackController::timerEvent next item first frame %d",
                     this->frameSlider->minimum());
      this->setCurrentFrameAndUpdate(this->ui.frameSlider->minimum());

      if (this->waitForCachingOfItem)
      {
        DEBUG_PLAYBACK("PlaybackController::timerEvent waiting for caching...");
        emit(waitForItemCaching(this->currentItem[0]));

        if (this->playbackMode == PlaybackMode::WaitingForCache)
        {
          // Update the views so that the "caching loading" hourglass indicator is drawn.
          this->splitViewPrimary->update(false, false);
          this->splitViewSeparate->update(false, false);
        }
      }
    }
  }
  else
  {
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
      this->playbackMode       = PlaybackMode::Stalled;
      this->playbackWasStalled = true;
      DEBUG_PLAYBACK("PlaybackController::timerEvent playback stalled");
      return;
    }

    DEBUG_PLAYBACK("PlaybackController::timerEvent next frame %d", *nextFrameIdx);
    this->setCurrentFrameAndUpdate(*nextFrameIdx);

    // Update the FPS counter every 50 frames
    this->timerFPSCounter++;
    if (this->timerFPSCounter >= 50)
    {
      const auto newFrameTime         = QTime::currentTime();
      const auto msecsSinceLastUpdate = static_cast<double>(timerLastFPSTime.msecsTo(newFrameTime));

      // Print the frames per second as float with one digit after the decimal dot.
      auto framesPerSec = (50.0 / (msecsSinceLastUpdate / 1000.0));
      if (framesPerSec > 0)
        this->ui.fpsLabel->setText(QString::number(framesPerSec, 'f', 1));
      if (this->playbackWasStalled)
        this->ui.fpsLabel->setStyleSheet("QLabel { background-color: yellow }");
      else
        this->ui.fpsLabel->setStyleSheet("");
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

      const auto newtimerInterval =
          std::chrono::duration_cast<std::chrono::milliseconds>(1000ms / frameRate);
      if (this->timerInterval != newtimerInterval)
        this->startOrUpdateTimer();
    }
  }
}

void PlaybackController::currentSelectedItemsDoubleBufferLoad(int itemID)
{
  assert(itemID == 0 || itemID == 1);
  if (this->playbackMode == PlaybackMode::Stalled)
  {
    this->waitingForItem[itemID] = false;
    if (!this->waitingForItem[0] && !this->waitingForItem[1])
    {
      DEBUG_PLAYBACK(
          "PlaybackController::currentSelectedItemsDoubleBufferLoad - timer interval %dms",
          this->timerInterval.count());
      this->timer.start(this->timerInterval.count(), Qt::PreciseTimer, this);
      this->timerEvent(nullptr);
      this->playbackMode = PlaybackMode::Running;
    }
  }
}

bool PlaybackController::setCurrentFrameAndUpdate(int frame, bool updateView)
{
  if (frame == this->currentFrameIdx)
    return false;
  if (!this->currentItem[0] ||
      (!this->currentItem[0]->properties().isIndexedByFrame() &&
       (!this->currentItem[1] || !this->currentItem[1]->properties().isIndexedByFrame())))
    // Both items (that are selcted) are not indexed by frame.
    return false;

  DEBUG_PLAYBACK("PlaybackController::setCurrentFrameAndUpdate %d", frame);

  this->updateFrameSliderAndSpinBoxWithoutSignals(frame);
  this->currentFrameIdx = frame;

  if (updateView)
  {
    // Also update the view to display the new frame
    this->splitViewPrimary->update(true);
    this->splitViewSeparate->update();
    return true;
  }
  return false;
}

void PlaybackController::updateFrameSliderAndSpinBoxWithoutSignals(
    const int value, const std::optional<int> sliderMaximum)
{
  const QSignalBlocker blocker1(this->ui.frameSpinBox);
  const QSignalBlocker blocker2(this->ui.frameSlider);
  this->ui.frameSpinBox->setValue(value);
  this->ui.frameSlider->setValue(value);
  if (sliderMaximum)
    this->ui.frameSlider->setMaximum(*sliderMaximum);
}
