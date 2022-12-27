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

#pragma once

#include <chrono>

#include <common/Typedef.h>
#include <ui/views/SplitViewWidget.h>
#include <ui/widgets/PlaylistTreeWidget.h>

#include <QBasicTimer>
#include <QPointer>
#include <QTime>
#include <QWidget>

#include "ui_playbackController.h"

class playlistItem;
class PlaylistTreeWidget;

class CountDown
{
public:
  CountDown() = default;
  CountDown(const int ticks);
  bool tickAndGetIsExpired();
  int  getTicks() const { return this->initialTicks; }
  int  getCurrentTick() const { return this->tick; }

private:
  int tick{};
  int initialTicks{};
};

class PlaybackController : public QWidget
{
  Q_OBJECT

public:
  PlaybackController();

  void setSplitViews(splitViewWidget *primary, splitViewWidget *separate);
  void setPlaylist(PlaylistTreeWidget *playlistWidget);

  void pausePlayback();

  bool playing() const;
  bool isWaitingForCaching() const;
  int  getCurrentFrame() const;

  bool setCurrentFrameAndUpdate(int frame, bool updateView = true);

public slots:
  void on_playPauseButton_clicked();
  void on_stopButton_clicked();
  void on_repeatModeButton_clicked();

  void nextFrame();
  void previousFrame();

  // Accept the signal from the playlistTreeWidget that signals if a new (or two) item was selected.
  // The playback controller will save a pointer to this in order to get playback info from the item
  // later like the sampling or the framerate. This will also update the slider and the spin box.
  // Playback will be stopped if chageByPlayback is false.
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2, bool chageByPlayback);

  // One of the (possibly two) selected items finished loading the double bufffer.
  void currentSelectedItemsDoubleBufferLoad(int itemID);

  // The properties of the currently selected item(s) changed. Update the frame sliders and toggle
  // an update() in the splitView if necessary.
  void selectionPropertiesChanged(bool redraw);

  void updateSettings();
  void loadButtonIcons();
  void updatePlayPauseButtonIcon();

signals:
  void ControllerStartCachingCurrentSelection(indexRange range);
  void ControllerRemoveFromCache(indexRange range);

  // This is connected to the VideoCache and tells it to call itemCachingFinished when caching is
  // finished.
  void waitForItemCaching(playlistItem *item);

  void signalPlaybackStarting();

public slots:
  void itemCachingFinished(playlistItem *item);

private slots:
  void on_frameSlider_valueChanged(int val);
  void on_frameSpinBox_valueChanged(int val) { this->on_frameSlider_valueChanged(val); }

private:
  std::optional<int> getNextFrameIndexInCurrentItem();

  void enableControls(bool enable);
  bool controlsEnabled{};

  void updateFrameRange();
  void goToNextItem();
  void goToNextFrame(const int nextFrameIndex);

  // The current frame index. -1 means the frame index is invalid. In this case, lastValidFrameIdx
  // contains the last valid frame index which will be restored if a valid indexed item is selected.
  int currentFrameIdx{-1};
  int lastValidFrameIdx{-1};

  void startOrUpdateTimer();
  void startPlayback();

  enum class RepeatMode
  {
    Off,
    One,
    All
  };
  EnumMapper<RepeatMode> RepeatModeMapper{
      {{RepeatMode::Off, "Off"}, {RepeatMode::One, "One"}, {RepeatMode::All, "All"}}};
  RepeatMode repeatMode{RepeatMode::Off};
  void       setRepeatModeAndUpdateIcons(const RepeatMode mode);

  void updateFrameSliderAndSpinBoxWithoutSignals(const int                value,
                                                 const std::optional<int> sliderMaximum = {});

  QIcon iconPlay;
  QIcon iconStop;
  QIcon iconPause;
  QIcon iconRepeatOff;
  QIcon iconRepeatAll;
  QIcon iconRepeatOne;

  // Is playback currently running?
  enum class PlaybackMode
  {
    Stopped,
    Running,
    Stalled,
    WaitingForCache,
  };
  PlaybackMode playbackMode{PlaybackMode::Stopped};

  bool waitingForItem[2]{false, false};
  bool playbackWasStalled{false};
  bool waitForCachingOfItem{};

  QBasicTimer                           timer;
  std::chrono::milliseconds             timerInterval{};
  CountDown                             countdownForFPSUpdate;
  std::chrono::steady_clock::time_point timerLastFPSTime;
  CountDown                             countDownForStaticItem;
  virtual void
  timerEvent(QTimerEvent *event) override; // Overloaded from QObject. Called when the timer fires.

  QPointer<playlistItem> currentItem[2];
  bool                   anyItemIndexedByFrame() const;
  double                 getCurrentItemsFrameRate() const;

  // The playback controller has a pointer to the split view so it can toggle a redraw event when a
  // new frame is selected. This could also be done using signals/slots but the problem is that
  // signals/slots have a small overhead. So when we are using a timer for high framerates, this is
  // the faster option.
  QPointer<splitViewWidget> splitViewPrimary;
  QPointer<splitViewWidget> splitViewSeparate;

  QPointer<PlaylistTreeWidget> playlist;

  Ui::PlaybackController ui;
};
