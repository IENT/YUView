#include "DistortionPlaybackController.h"
#include <QApplication>
#include <QMainWindow>
#include <QDebug>
#include <ui/views/SplitViewWidget.h>
#include <ui/PlaybackController.h>

DistortionPlaybackController::DistortionPlaybackController(QObject* parent)
  : QObject(parent)
  , m_distortionTimer(new QTimer(this))
  , m_isDistortionActive(false)
  , m_currentDistortionLevel(0)
  , m_playbackFrameIndex(0)
  , m_revertFrameNumber(-1)
  , m_activeDistortionButton(nullptr)
{
  // Connect timer
  connect(m_distortionTimer, &QTimer::timeout, this, &DistortionPlaybackController::onDistortionTimerTimeout);
}

DistortionPlaybackController::~DistortionPlaybackController()
{
  if (m_distortionTimer) {
    m_distortionTimer->stop();
  }
}

void DistortionPlaybackController::startFirstLevelDistortion(QPushButton* button, int currentFrameIndex)
{
  // Check if this button is currently active
  if (m_activeDistortionButton == button) {
    // Second click on active button - pause and reset
    stopDistortion();
    resetAllButtons();
    return;
  }
  
  // Initialize distortion state for first level (30 FPS)
  initializeDistortionState(button, 1, currentFrameIndex);
  
  // Set view to 1x zoom
  setViewZoom(1.0);
  
  // Find PlaybackController and capture current frame as revert point, then enable loop mode
  PlaybackController* playbackController = findPlaybackController();
  if (playbackController) {
    // Capture current frame as revert point before starting playback
    m_revertFrameNumber = playbackController->getCurrentFrame();
    // Enable loop mode for continuous playback
    setPlaybackControllerRepeatMode(playbackController, PlaybackController::RepeatMode::One);
  }
  
  // Start playback at 30 FPS
  startDistortionPlayback(30.0);
}

void DistortionPlaybackController::startSecondLevelDistortion(QPushButton* button, int currentFrameIndex)
{
  // Check if this button is currently active
  if (m_activeDistortionButton == button) {
    // Second click on active button - pause and reset
    stopDistortion();
    resetAllButtons();
    return;
  }
  
  // Initialize distortion state for second level (1.5 FPS)
  initializeDistortionState(button, 2, currentFrameIndex);
  
  // Set view to 1x zoom
  setViewZoom(1.0);
  
  // Find PlaybackController and capture current frame as revert point, then enable loop mode
  PlaybackController* playbackController = findPlaybackController();
  if (playbackController) {
    // Capture current frame as revert point before starting playback
    m_revertFrameNumber = playbackController->getCurrentFrame();
    // Enable loop mode for continuous playback
    setPlaybackControllerRepeatMode(playbackController, PlaybackController::RepeatMode::One);
  }
  
  // Start playback at 1.5 FPS
  startDistortionPlayback(1.5);
}

void DistortionPlaybackController::stopDistortion()
{
  if (m_isDistortionActive) {
    m_distortionTimer->stop();
    m_distortionTimer->disconnect();
    m_isDistortionActive = false;
  }
  
  // Find PlaybackController and pause playback
  PlaybackController* playbackController = findPlaybackController();
  if (playbackController) {
    playbackController->pausePlayback();
    // Reset repeat mode to off
    setPlaybackControllerRepeatMode(playbackController, PlaybackController::RepeatMode::Off);
  }
}

void DistortionPlaybackController::revertToFirstLevel()
{
  if (m_revertFrameNumber < 0) {
    return;
  }
  
  // Stop any active distortion analysis
  stopDistortion();
  
  // Find PlaybackController and revert to the saved frame
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(QApplication::activeWindow());
  if (!mainWindow) {
    qDebug() << "ERROR: Could not find main window";
    return;
  }
  
  qDebug() << "Found main window, searching for PlaybackController...";
  auto playbackController = mainWindow->findChild<PlaybackController*>();
  if (!playbackController) {
    qDebug() << "ERROR: Could not find PlaybackController";
    return;
  }
  
  qDebug() << "Found PlaybackController, current frame:" << playbackController->getCurrentFrame();
  
  // Pause playback first
  playbackController->pausePlayback();
  qDebug() << "Playback paused";
  
  // Reset repeat mode to off
  setPlaybackControllerRepeatMode(playbackController, PlaybackController::RepeatMode::Off);
  qDebug() << "Repeat mode reset to Off";
  
  // Seek back to the revert point
  bool success = playbackController->setCurrentFrameAndUpdate(m_revertFrameNumber);
  qDebug() << "Frame revert result:" << success << "(target frame:" << m_revertFrameNumber << ", current frame:" << playbackController->getCurrentFrame() << ")";
  
  // Reset button appearance
  resetAllButtons();
}

void DistortionPlaybackController::revertToSecondLevel()
{
  if (m_revertFrameNumber < 0) {
    return;
  }
  
  // Stop any active distortion analysis
  stopDistortion();
  
  // Find PlaybackController and revert to the saved frame
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(QApplication::activeWindow());
  if (!mainWindow) {
    qDebug() << "ERROR: Could not find main window";
    return;
  }
  
  qDebug() << "Found main window, searching for PlaybackController...";
  auto playbackController = mainWindow->findChild<PlaybackController*>();
  if (!playbackController) {
    qDebug() << "ERROR: Could not find PlaybackController";
    return;
  }
  
  qDebug() << "Found PlaybackController, current frame:" << playbackController->getCurrentFrame();
  
  // Pause playback first
  playbackController->pausePlayback();
  qDebug() << "Playback paused";
  
  // Reset repeat mode to off
  setPlaybackControllerRepeatMode(playbackController, PlaybackController::RepeatMode::Off);
  qDebug() << "Repeat mode reset to Off";
  
  // Seek back to the revert point
  bool success = playbackController->setCurrentFrameAndUpdate(m_revertFrameNumber);
  qDebug() << "Frame revert result:" << success << "(target frame:" << m_revertFrameNumber << ", current frame:" << playbackController->getCurrentFrame() << ")";
  
  // Reset button appearance
  resetAllButtons();
}

void DistortionPlaybackController::setActiveButton(QPushButton* button)
{
  m_activeDistortionButton = button;
  setButtonActiveState(button, true);
}

void DistortionPlaybackController::resetAllButtons()
{
  if (m_activeDistortionButton) {
    setButtonActiveState(m_activeDistortionButton, false);
    m_activeDistortionButton = nullptr;
  }
}

void DistortionPlaybackController::onDistortionTimerTimeout()
{
  // Find PlaybackController to properly advance frames
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(QApplication::activeWindow());
  PlaybackController* playbackController = nullptr;
  
  if (mainWindow) {
    playbackController = mainWindow->findChild<PlaybackController*>();
  }
  
  if (playbackController) {
    // Use PlaybackController to advance to next frame properly
    playbackController->nextFrame();
  } else {
    // Fallback to manual advancement if PlaybackController not found
    m_playbackFrameIndex++;
    
    // Emit signal for manual frame advancement
    emit frameAdvanceRequested();
  }
}

void DistortionPlaybackController::initializeDistortionState(QPushButton* button, int level, int currentFrameIndex)
{
  // Stop any existing distortion activity and reset other buttons
  stopDistortion();
  resetAllButtons();
  
  // Set this button as active
  setActiveButton(button);
  
  // Initialize distortion state
  m_currentDistortionLevel = level;
  m_isDistortionActive = true;
  m_playbackFrameIndex = currentFrameIndex;
}

void DistortionPlaybackController::startDistortionPlayback(double fps)
{
  // For distortion analysis, we need precise timing control
  // The best approach is to use manual frame advancement with proper timing
  // since modifying the playlist item's frame rate is complex and can affect
  // the overall playback experience for the user
  
  startManualFrameAdvancement(fps);
}

void DistortionPlaybackController::startManualFrameAdvancement(double fps)
{
  // Calculate timer interval for the specified FPS
  int intervalMs = static_cast<int>(1000.0 / fps);
  
  // Stop any existing timer
  m_distortionTimer->stop();
  
  // Disconnect any existing connections
  m_distortionTimer->disconnect();
  
  // Connect timer to frame advancement
  connect(m_distortionTimer, &QTimer::timeout, this, &DistortionPlaybackController::onDistortionTimerTimeout);
  
  // Start the timer
  m_distortionTimer->start(intervalMs);
}

void DistortionPlaybackController::setButtonActiveState(QPushButton* button, bool active)
{
  if (!button) return;
  
  if (active) {
    button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
  } else {
    button->setStyleSheet("");
  }
}

void DistortionPlaybackController::setViewZoom(double zoomFactor)
{
  // Use PlaybackController to access the split view widgets
  PlaybackController* playbackController = findPlaybackController();
  if (!playbackController) {
    return;
  }
  
  // Get the main window and try to find split view widgets
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(QApplication::activeWindow());
  if (!mainWindow) {
    return;
  }
  
  // Try to find splitViewWidget objects - they might have different names
  QList<splitViewWidget*> splitViews = mainWindow->findChildren<splitViewWidget*>();
  
  if (splitViews.isEmpty()) {
    return;
  }
  
  // Apply zoom to all found split view widgets
  for (splitViewWidget* splitView : splitViews) {
    if (splitView) {
      // Call the appropriate zoom method based on the zoom factor
      if (zoomFactor == 1.0) {
        splitView->zoomTo100(true);
      } else if (zoomFactor == 2.0) {
        splitView->zoomTo200(true);
      } else if (zoomFactor == 0.5) {
        splitView->zoomTo50(true);
      }
    }
  }
}

PlaybackController* DistortionPlaybackController::findPlaybackController()
{
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(QApplication::activeWindow());
  if (mainWindow) {
    return mainWindow->findChild<PlaybackController*>();
  }
  return nullptr;
}

void DistortionPlaybackController::setPlaybackControllerRepeatMode(PlaybackController* controller, PlaybackController::RepeatMode targetMode)
{
  if (!controller) return;
  
  // Since we can't access the private repeatMode member, we need to assume the current state.
  // The RepeatMode cycles: Off -> One -> All -> Off
  // Default mode is Off, so we click the button once to get to RepeatMode::One
  // For RepeatMode::Off, we click 3 times (Off->One->All->Off)
  
  if (targetMode == PlaybackController::RepeatMode::One) {
    // From default Off state, click once to get to One
    controller->on_repeatModeButton_clicked();
  } else if (targetMode == PlaybackController::RepeatMode::All) {
    // From default Off state, click twice to get to All
    controller->on_repeatModeButton_clicked(); // Off -> One
    controller->on_repeatModeButton_clicked(); // One -> All
  } else if (targetMode == PlaybackController::RepeatMode::Off) {
    // Already at Off by default, but if we've changed it before, 
    // we need to cycle back. Since we don't know current state,
    // click 3 times to ensure we're back at Off regardless of current state
    controller->on_repeatModeButton_clicked(); // Current -> Next
    controller->on_repeatModeButton_clicked(); // Next -> Next+1
    controller->on_repeatModeButton_clicked(); // Next+1 -> Back to current (full cycle)
  }
  
  QString modeStr;
  switch (targetMode) {
    case PlaybackController::RepeatMode::Off: modeStr = "Off"; break;
    case PlaybackController::RepeatMode::One: modeStr = "One"; break;
    case PlaybackController::RepeatMode::All: modeStr = "All"; break;
  }
  qDebug() << "Repeat mode set to:" << modeStr;
}