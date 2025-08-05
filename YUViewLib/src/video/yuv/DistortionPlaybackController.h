#pragma once

#include <QObject>
#include <QTimer>
#include <QPushButton>
#include <QWidget>

#include <ui/PlaybackController.h>

class splitViewWidget;
class videoHandlerYUV;

/**
 * @brief Controls distortion analysis playback functionality
 * 
 * This class encapsulates all distortion-related operations including:
 * - First and second level distortion playback
 * - Playback timing control and frame advancement
 * - UI button state management
 * - Revert functionality
 * - Zoom control integration
 */
class DistortionPlaybackController : public QObject
{
  Q_OBJECT

public:
  explicit DistortionPlaybackController(QObject* parent = nullptr);
  ~DistortionPlaybackController();

  // Distortion state management
  bool isDistortionActive() const { return m_isDistortionActive; }
  int getCurrentDistortionLevel() const { return m_currentDistortionLevel; }
  int getCurrentPlaybackFrameIndex() const { return m_playbackFrameIndex; }
  
  // Playback control
  void startFirstLevelDistortion(QPushButton* button, int currentFrameIndex);
  void startSecondLevelDistortion(QPushButton* button, int currentFrameIndex);
  void stopDistortion();
  
  // Revert functionality
  void revertToFirstLevel();
  void revertToSecondLevel();
  void setRevertFrameNumber(int frameNumber) { m_revertFrameNumber = frameNumber; }
  
  // UI state management
  void setActiveButton(QPushButton* button);
  void resetAllButtons();

signals:
  // Request frame changes
  void frameAdvanceRequested();
  void frameRevertRequested(int frameNumber);
  
  // Zoom control
  void zoomChangeRequested(double zoomFactor);
  
  // Playback control signals
  void playbackControlRequested(bool pause);
  void repeatModeChangeRequested(PlaybackController::RepeatMode mode);

private slots:
  void onDistortionTimerTimeout();

private:
  // Distortion state
  QTimer* m_distortionTimer;
  bool m_isDistortionActive;
  int m_currentDistortionLevel;
  int m_playbackFrameIndex;
  int m_revertFrameNumber;
  
  // UI state management
  QPushButton* m_activeDistortionButton;
  
  // Helper methods
  void initializeDistortionState(QPushButton* button, int level, int currentFrameIndex);
  void startDistortionPlayback(double fps);
  void startManualFrameAdvancement(double fps);
  void setButtonActiveState(QPushButton* button, bool active);
  void setViewZoom(double zoomFactor);
  PlaybackController* findPlaybackController();
  void setPlaybackControllerRepeatMode(PlaybackController* controller, PlaybackController::RepeatMode targetMode);
};