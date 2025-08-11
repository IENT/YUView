#include "HDRRenderingManager.h"
#include "qmainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QTimer>


HDRRenderingManager::HDRRenderingManager(QObject* parent)
  : QObject(parent)
  , m_hdrWidget(nullptr)
  , m_hdrDetectionWorker(nullptr)
  , m_useHDRRendering(false)
{
  // Initialize HDR capabilities to default (not supported)
  m_hdrCapabilities.isHDRSupported = false;
}

HDRRenderingManager::~HDRRenderingManager()
{
  cleanupHDRResources();
}

void HDRRenderingManager::setHDRRenderingEnabled(bool enabled)
{
  if (enabled) {
    // CRITICAL FIX: Always attempt HDR detection when user tries to enable HDR
    // This allows retry after previous failures (e.g., after display driver updates)
    qDebug() << "HDRRenderingManager: User requested HDR enable, starting detection...";
    
    // Set state optimistically, will be corrected if detection fails
    m_useHDRRendering = true;
    
    // Start asynchronous HDR detection
    startHDRDetection();
    
  } else {
    // User explicitly disabled HDR
    if (m_useHDRRendering == false) {
      // Already disabled, but ensure UI feedback is provided
      qDebug() << "HDRRenderingManager: HDR already disabled, confirming state";
    } else {
      qDebug() << "HDRRenderingManager: User disabled HDR rendering";
    }
    
    m_useHDRRendering = false;
    
    // Clean up HDR widget if exists
    if (m_hdrWidget) {
      emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
      m_hdrWidget->hide();
    }
    
    // Notify that HDR is disabled
    emit hdrRenderingStateChanged(false, m_hdrWidget);
  }
}

HDR_VideoWidget* HDRRenderingManager::createHDRWidget(QWidget* parent)
{
  // Only create if HDR is supported and not already created
  if (!m_useHDRRendering || m_hdrWidget) {
    return m_hdrWidget;
  }
  
  m_hdrWidget = new HDR_VideoWidget(parent);
  
  // CRITICAL FIX: Connect widgetInitialized signal
  // Only when widget completes OpenGL initialization, we consider HDR ready
  connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
          this, [this]() {
      // Only after receiving this signal, HDR rendering is truly ready
      qDebug() << "HDRRenderingManager: Received widgetInitialized signal. HDR rendering is now ready.";
      // Notify external components that HDR rendering is now ready
      emit hdrRenderingStateChanged(true, m_hdrWidget);
  });
  
  // Connect other HDR widget signals
  connect(m_hdrWidget, &HDR_VideoWidget::hdrNotSupported,
          this, &HDRRenderingManager::onHDRNotSupported);
  connect(m_hdrWidget, &HDR_VideoWidget::renderModeChanged,
          this, &HDRRenderingManager::onHDRModeChanged);
  
  // Set HDR capabilities
  m_hdrWidget->setHDRCapabilities(m_hdrCapabilities);
  
  // Set appropriate render mode based on capabilities
  if (m_hdrCapabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT709_Linear_16bit);
  } else {
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT2020_PQ_10bit);
  }
  
  return m_hdrWidget;
}

void HDRRenderingManager::hideHDRWidget()
{
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
    m_hdrWidget->hide();
  }
}

void HDRRenderingManager::showHDRWidget()
{
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, true);
  }
}

void HDRRenderingManager::startHDRDetection()
{
  qDebug() << "=== HDRRenderingManager::startHDRDetection() called ===";
  qDebug() << "HDRRenderingManager: Starting HDR detection...";
  
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
  
  qDebug() << "HDRRenderingManager: Starting HDR detection (attempt)...";
  m_hdrDetectionWorker->startDetection();
  qDebug() << "HDRRenderingManager: HDR detection start request sent";
}

bool HDRRenderingManager::isHDRDetectionInProgress() const
{
  return m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting();
}

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
  // If HDR is not active or widget doesn't exist, return early
  if (!m_hdrWidget || !m_useHDRRendering) {
    return;
  }
  
  // Check if widget has confirmed readiness via widgetInitialized signal
  // isReadyForRendering provides a more reliable check than just m_initialized
  if (!m_hdrWidget->isReadyForRendering()) {
    qDebug() << "HDRRenderingManager: Widget not ready for rendering. Caching frame for later.";
    // Pass frame data to widget - it will cache it internally
    // Widget's paintGL will use this cached frame after initialization completes
    m_hdrWidget->updateFrame(frame); 
    return;
  }

  // Ensure widget is visible (keep as double insurance)
  if (!m_hdrWidget->isVisible()) {
    m_hdrWidget->show();
  }

  // Push frame data to ready widget
  m_hdrWidget->updateFrame(frame);
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
  
  // Fall back to standard rendering
  m_useHDRRendering = false;
  
  // Clean up HDR widget if exists
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
    m_hdrWidget->hide();
  }
  
  // Notify that HDR rendering failed
  emit hdrRenderingStateChanged(false, m_hdrWidget);
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

    // Store capabilities and enable HDR rendering
    qDebug() << "HDRRenderingManager: Storing HDR capabilities...";
    m_hdrCapabilities = capabilities;
    m_useHDRRendering = true;
    qDebug() << "HDRRenderingManager: HDR capabilities stored and rendering enabled";

    QString modeDescription = HDRDetection::getHDRModeDescription(capabilities.supportedMode);
    qDebug() << "HDRRenderingManager: HDR rendering enabled (integrated mode):" << modeDescription;
    if (!m_hdrWidget) {
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
        // Create HDR widget with split view as parent
        createHDRWidget(splitView);

        if (m_hdrWidget) {
          // Use QTimer::singleShot to ensure integration happens after current event loop
          // This gives Qt time to process the newly created widget's initial display events
          QTimer::singleShot(0, this, [this, splitView]() {
            splitView->setHDROverlayWidget(m_hdrWidget);
            splitView->showHDROverlay(true); // showHDROverlay will handle widget display
            qDebug() << "HDRRenderingManager: HDR widget integration with split view requested.";
            
            // NOTE: We don't emit hdrRenderingStateChanged here anymore.
            // That signal will be emitted by the widgetInitialized slot when widget is truly ready.
          });
        }
      } else {
        qDebug() << "HDRRenderingManager: WARNING - Split view widget not found";
        // Fallback to creating widget with main window as parent
        QWidget* parentWidget = QApplication::activeWindow();
        if (parentWidget) {
          createHDRWidget(parentWidget);
          if (m_hdrWidget) {
            // CRITICAL FIX: Emit signal to trigger immediate frame update for fallback case too
            emit hdrRenderingStateChanged(true, m_hdrWidget);
          }
        }
      }
    }
  } else {
    // HDR is not supported on current display
    qDebug() << "HDRRenderingManager: === HDR IS NOT SUPPORTED ===";
    qDebug() << "HDRRenderingManager: Error:" << capabilities.errorMessage;
    
    // Store capabilities for reference (even if unsupported)
    m_hdrCapabilities = capabilities;
    
    // Reset HDR rendering state
    m_useHDRRendering = false;
    
    // Clean up any existing HDR widget
    if (m_hdrWidget) {
      emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
      m_hdrWidget->hide();
    }
    
    // Notify that HDR detection failed with appropriate error message
    QString errorMsg = capabilities.errorMessage.isEmpty() ? 
                      "HDR not supported on current display configuration" : 
                      capabilities.errorMessage;
    
    emit hdrRenderingStateChanged(false, m_hdrWidget);
    emit hdrDetectionFailed(errorMsg);
    
    qDebug() << "HDRRenderingManager: HDR detection failed, error message sent to UI:" << errorMsg;
  }
}

void HDRRenderingManager::onHDRDetectionFailed(const QString& error)
{
  qDebug() << "HDRRenderingManager: HDR detection failed:" << error;
  
  // Reset HDR state
  m_useHDRRendering = false;
  
  // Clean up HDR widget if exists
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
    m_hdrWidget->hide();
  }
  
  // Notify about the failure
  emit hdrRenderingStateChanged(false, m_hdrWidget);
  emit hdrDetectionFailed(error);
  
  qDebug() << "HDRRenderingManager: HDR detection failure handled, error sent to UI";
}

void HDRRenderingManager::cleanupHDRResources()
{
  if (m_hdrWidget) {
    delete m_hdrWidget;
    m_hdrWidget = nullptr;
  }
  
  if (m_hdrDetectionWorker) {
    delete m_hdrDetectionWorker;
    m_hdrDetectionWorker = nullptr;
  }
}
