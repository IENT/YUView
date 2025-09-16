#include "HDRRenderingManager.h"
#include "HDRGlobalState.h"
#include "qmainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QMutexLocker>


HDRRenderingManager::HDRRenderingManager(QObject* parent)
  : QObject(parent)
  , m_hdrWidget(nullptr)
  , m_hdrDetectionWorker(nullptr)
  , m_useHDRRendering(false)
  , m_isHDRWidgetReady(false)  // CRITICAL FIX: Initialize widget readiness flag
{
  // Initialize HDR capabilities to default (not supported)
  m_hdrCapabilities.isHDRSupported = false;
  
  // Check global HDR state on initialization
  if (HDRGlobalState::instance()->isHDRModeEnabled()) {
    qDebug() << "HDRRenderingManager: Global HDR mode is enabled, preparing for HDR rendering";
    // Note: HDR will be fully activated when startHDRDetection is called
  }
}

HDRRenderingManager::~HDRRenderingManager()
{
  cleanupHDRResources();
}

void HDRRenderingManager::setHDRRenderingEnabled(bool enabled)
{
  if (enabled) {
    {
      QMutexLocker locker(&m_stateMutex);
      // CRITICAL FIX: Always attempt HDR detection when user tries to enable HDR
      // This allows retry after previous failures (e.g., after display driver updates)
      qDebug() << "HDRRenderingManager: User requested HDR enable, starting detection...";
      
      // Set state optimistically, will be corrected if detection fails
      m_useHDRRendering = true;
    }
    
    // Start asynchronous HDR detection (outside lock to avoid deadlock)
    startHDRDetection();
    
  } else {
    {
      QMutexLocker locker(&m_stateMutex);
      // User explicitly disabled HDR
      if (m_useHDRRendering == false) {
        // Already disabled, but ensure UI feedback is provided
        qDebug() << "HDRRenderingManager: HDR already disabled, confirming state";
      } else {
        qDebug() << "HDRRenderingManager: User disabled HDR rendering";
      }
      
      m_useHDRRendering = false;
      m_isHDRWidgetReady = false;  // CRITICAL FIX: Reset readiness flag when HDR is disabled
    }
    
    // Clean up HDR widget if exists (cleanupHDRResources() has its own locking)
    cleanupHDRResources();
    
    // Notify that HDR is disabled
    emit hdrRenderingStateChanged(false, nullptr);
  }
}

HDR_VideoWidget* HDRRenderingManager::createHDRWidget(QWidget* parent)
{
  QMutexLocker locker(&m_stateMutex);
  
  // Only create if HDR is supported and not already created
  if (!m_useHDRRendering || m_hdrWidget) {
    return m_hdrWidget;
  }
  
  m_hdrWidget = new HDR_VideoWidget(parent);
  
  // CRITICAL FIX: Connect widgetInitialized signal with thread-safe readiness update
  // Only when widget completes OpenGL initialization, we consider HDR ready
  connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
          this, [this]() {
      {
        QMutexLocker readyLocker(&m_stateMutex);
        // Set readiness flag - this is the key fix for the race condition
        m_isHDRWidgetReady = true;
        qDebug() << "HDRRenderingManager: Widget initialization complete. HDR rendering is now ready.";
      }
      
      // Notify external components that HDR rendering is now ready (outside lock)
      emit hdrRenderingStateChanged(true, m_hdrWidget);
  });
  
  // Connect other HDR widget signals
  connect(m_hdrWidget, &HDR_VideoWidget::hdrNotSupported,
          this, &HDRRenderingManager::onHDRNotSupported);
  connect(m_hdrWidget, &HDR_VideoWidget::renderModeChanged,
          this, &HDRRenderingManager::onHDRModeChanged);
  
  // Set HDR capabilities (using local copy to avoid holding lock too long)
  HDRDetection::HDRCapabilities capabilities = m_hdrCapabilities;
  locker.unlock();  // Release lock before potentially expensive widget operations
  
  // Set HDR capabilities
  m_hdrWidget->setHDRCapabilities(capabilities);
  
  // Always use PQ 10-bit mode when HDR is supported; otherwise SDR
  if (capabilities.isHDRSupported && capabilities.supportedMode == HDRDetection::BT2020_PQ_10bit) {
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT2020_PQ_10bit);
  } else {
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_SDR_8bit);
  }
  
  return m_hdrWidget;
}

void HDRRenderingManager::hideHDRWidget()
{
  QMutexLocker locker(&m_stateMutex);
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
    m_hdrWidget->hide();
  }
}

void HDRRenderingManager::showHDRWidget()
{
  QMutexLocker locker(&m_stateMutex);
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, true);
  }
}

void HDRRenderingManager::startHDRDetection()
{
  qDebug() << "=== HDRRenderingManager::startHDRDetection() called ===";
  qDebug() << "HDRRenderingManager: Starting HDR detection...";
  
  QMutexLocker locker(&m_stateMutex);
  
  // Create HDR detection worker if not already created
  if (!m_hdrDetectionWorker) {
    qDebug() << "HDRRenderingManager: Creating new HDRDetectionWorker...";
    m_hdrDetectionWorker = new HDRDetectionWorker(this);
    
    // Connect signals
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionComplete,
            this, &HDRRenderingManager::onHDRDetectionComplete);
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionFailed,
            this, &HDRRenderingManager::onHDRDetectionFailed);
    qDebug() << "HDRRenderingManager: HDRDetectionWorker created and connected";
  }
  
  // CRITICAL FIX: Always allow new detection attempts
  // This fixes the "odd/even click" bug by ensuring user can retry HDR detection
  if (m_hdrDetectionWorker->isDetecting()) {
    qDebug() << "HDRRenderingManager: Previous detection still running, allowing concurrent attempt";
    // Note: HDRDetectionWorker's mutex will handle concurrent access safely
  }
  
  // Get reference to worker and release lock before starting detection
  HDRDetectionWorker* worker = m_hdrDetectionWorker;
  locker.unlock();
  
  qDebug() << "HDRRenderingManager: Starting HDR detection (attempt)...";
  worker->startDetection();
  qDebug() << "HDRRenderingManager: HDR detection start request sent";
}

bool HDRRenderingManager::isHDRDetectionInProgress() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting();
}

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
  // CRITICAL FIX: Thread-safe check of readiness flags to prevent race condition
  HDR_VideoWidget* widget = nullptr;
  {
    QMutexLocker locker(&m_stateMutex);
    
    if (!m_hdrWidget || !m_useHDRRendering || !m_isHDRWidgetReady) {
      if (!m_isHDRWidgetReady && m_hdrWidget) {
        qDebug() << "HDRRenderingManager: Widget not ready for rendering. Waiting for initialization signal.";
      }
      return;  // Don't send frames until widget is fully ready
    }
    
    widget = m_hdrWidget;  // Get reference while holding lock
  }
  
  // Double-check widget's internal readiness state as additional safety (outside lock)
  if (!widget->isReadyForRendering()) {
    // Do not spam logs; keep one concise line
    qDebug() << "HDRRenderingManager: Widget not ready; deferring frame to avoid black screen";
    return;
  }

  // Ensure widget is visible (keep as double insurance)
  if (!widget->isVisible()) {
    widget->show();
  }

  // Push frame data to ready widget
  widget->updateFrame(frame);
}
QImage HDRRenderingManager::getHDRRenderedImage()
{
  if (m_hdrWidget) {
    // This would need to be implemented in HDR_VideoWidget
    // For now, return empty image
    return QImage();
  }
  return QImage();
}

void HDRRenderingManager::onHDRNotSupported(const QString& reason)
{
  qDebug() << "YUView HDR: HDR not supported:" << reason;
  
  HDR_VideoWidget* widget = nullptr;
  {
    QMutexLocker locker(&m_stateMutex);
    // Fall back to standard rendering
    m_useHDRRendering = false;
    m_isHDRWidgetReady = false;  // CRITICAL FIX: Reset readiness flag on error
    widget = m_hdrWidget;
  }
  
  // Clean up HDR widget if exists (outside lock to avoid deadlock with signals)
  if (widget) {
    emit hdrWidgetNeedsDisplay(widget, false);
    widget->hide();
  }
  
  // Notify that HDR rendering failed
  emit hdrRenderingStateChanged(false, widget);
  emit hdrDetectionFailed(reason);
}

void HDRRenderingManager::onHDRModeChanged(HDR_VideoWidget::RenderMode mode)
{
  Q_UNUSED(mode)
  qDebug() << "YUView HDR: Render mode changed";
  // Handle mode changes if needed
}

void HDRRenderingManager::onHDRDetectionComplete(const HDRDetection::HDRCapabilities& capabilities)
{
  qDebug() << "=== HDRRenderingManager::onHDRDetectionComplete() called ===";
  qDebug() << "HDRRenderingManager: HDR detection completed in main thread";
  qDebug() << "HDRRenderingManager: HDR supported:" << capabilities.isHDRSupported;
  qDebug() << "HDRRenderingManager: HDR mode:" << capabilities.supportedMode;
  qDebug() << "HDRRenderingManager: Max luminance:" << capabilities.maxLuminance << "nits";
  qDebug() << "HDRRenderingManager: Min luminance:" << capabilities.minLuminance << "nits";
  qDebug() << "HDRRenderingManager: Bits per channel:" << capabilities.bitsPerChannel;
  qDebug() << "HDRRenderingManager: Display name:" << capabilities.displayName;

  if (capabilities.isHDRSupported) {
    qDebug() << "HDRRenderingManager: === HDR IS SUPPORTED ===";

    // Store capabilities and enable HDR rendering atomically
    {
      QMutexLocker locker(&m_stateMutex);
      qDebug() << "HDRRenderingManager: Storing HDR capabilities...";
      m_hdrCapabilities = capabilities;
      m_useHDRRendering = true;
      qDebug() << "HDRRenderingManager: HDR capabilities stored and rendering enabled";
    }

    QString modeDescription = HDRDetection::getHDRModeDescription(capabilities.supportedMode);
    qDebug() << "HDRRenderingManager: HDR rendering enabled (integrated mode):" << modeDescription;
    
    bool needsWidgetCreation = false;
    {
      QMutexLocker locker(&m_stateMutex);
      needsWidgetCreation = !m_hdrWidget;
    }
    
    if (needsWidgetCreation) {
      qDebug() << "HDRRenderingManager: Looking for split view widget...";

      // Find the split view widget
      splitViewWidget* splitView = nullptr;
      QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
      for (QWidget* widget : topLevelWidgets) {
        if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(widget)) {
          // Find split view widget in main window
          splitView = mainWindow->findChild<splitViewWidget*>();
          if (splitView) {
            qDebug() << "HDRRenderingManager: Found split view widget:" << splitView;
            break;
          }
        }
      }

      if (splitView) {
        // Create HDR widget with split view as parent (createHDRWidget has its own locking)
        HDR_VideoWidget* createdWidget = createHDRWidget(splitView);

        if (createdWidget) {
          // CRITICAL FIX: Synchronous UI integration to prevent race condition
          // Integrate widget immediately to ensure it has proper parent before frames arrive
          splitView->setHDROverlayWidget(createdWidget);
          // Defer showing the overlay until the widget signals it's initialized
          QObject::connect(createdWidget, &HDR_VideoWidget::widgetInitialized, splitView, [splitView]() {
            splitView->showHDROverlay(true);
          });
          qDebug() << "HDRRenderingManager: HDR widget integrated with split view; waiting for initialization before showing.";
          
          // NOTE: hdrRenderingStateChanged will be emitted by the widgetInitialized slot
          // when widget completes OpenGL initialization and is truly ready for frames.
        }
      } else {
        qDebug() << "HDRRenderingManager: WARNING - Split view widget not found";
        // Fallback to creating widget with main window as parent
        QWidget* parentWidget = QApplication::activeWindow();
        if (parentWidget) {
          HDR_VideoWidget* createdWidget = createHDRWidget(parentWidget);
          if (createdWidget) {
            // CRITICAL FIX: Emit signal to trigger immediate frame update for fallback case too
            emit hdrRenderingStateChanged(true, createdWidget);
          }
        }
      }
    }
  } else {
    // HDR is not supported on current display
    qDebug() << "HDRRenderingManager: === HDR IS NOT SUPPORTED ===";
    qDebug() << "HDRRenderingManager: Error:" << capabilities.errorMessage;
    
    {
      QMutexLocker locker(&m_stateMutex);
      // Store capabilities for reference (even if unsupported)
      m_hdrCapabilities = capabilities;
      
      // Reset HDR rendering state
      m_useHDRRendering = false;
      m_isHDRWidgetReady = false;  // CRITICAL FIX: Reset readiness flag when HDR not supported
    }
    
    // Clean up any existing HDR widget (cleanupHDRResources() has its own locking)
    cleanupHDRResources();
    
    // Notify that HDR detection failed with appropriate error message
    QString errorMsg = capabilities.errorMessage.isEmpty() ? 
                      "HDR not supported on current display configuration" : 
                      capabilities.errorMessage;
    
    emit hdrRenderingStateChanged(false, nullptr);
    emit hdrDetectionFailed(errorMsg);
    
    qDebug() << "HDRRenderingManager: HDR detection failed, error message sent to UI:" << errorMsg;
  }
}

void HDRRenderingManager::onHDRDetectionFailed(const QString& error)
{
  qDebug() << "HDRRenderingManager: HDR detection failed:" << error;
  
  {
    QMutexLocker locker(&m_stateMutex);
    // Reset HDR state
    m_useHDRRendering = false;
    m_isHDRWidgetReady = false;  // CRITICAL FIX: Reset readiness flag on detection failure
  }
  
  // Clean up HDR widget if exists (cleanupHDRResources() has its own locking)
  cleanupHDRResources();
  
  // Notify about the failure
  emit hdrRenderingStateChanged(false, nullptr);
  emit hdrDetectionFailed(error);
  
  qDebug() << "HDRRenderingManager: HDR detection failure handled, error sent to UI";
}

void HDRRenderingManager::cleanupHDRResources()
{
  QMutexLocker locker(&m_stateMutex);
  
  // CRITICAL FIX: Reset readiness flag during cleanup
  m_isHDRWidgetReady = false;
  
  if (m_hdrWidget) {
    delete m_hdrWidget;
    m_hdrWidget = nullptr;
  }
  
  if (m_hdrDetectionWorker) {
    delete m_hdrDetectionWorker;
    m_hdrDetectionWorker = nullptr;
  }
}

// CRITICAL FIX: Thread-safe public interface methods
bool HDRRenderingManager::isHDRRenderingActive() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_useHDRRendering;
}

HDR_VideoWidget* HDRRenderingManager::getHDRWidget() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_hdrWidget;
}

HDRDetection::HDRCapabilities HDRRenderingManager::getHDRCapabilities() const
{
  QMutexLocker locker(&m_stateMutex);
  return m_hdrCapabilities;  // Deep copy to avoid reference issues
}

// CRITICAL FIX: Thread-safe internal helper methods
bool HDRRenderingManager::isHDRRenderingActive_locked() const
{
  // Assumes caller already holds m_stateMutex
  return m_useHDRRendering;
}

void HDRRenderingManager::setHDRWidgetReady_locked(bool ready)
{
  // Assumes caller already holds m_stateMutex
  m_isHDRWidgetReady = ready;
}

bool HDRRenderingManager::isHDRWidgetReady_locked() const
{
  // Assumes caller already holds m_stateMutex  
  return m_isHDRWidgetReady;
}
