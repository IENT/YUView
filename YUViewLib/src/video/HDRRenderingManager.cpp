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
  if (m_useHDRRendering == enabled) {
    return;
  }
  m_useHDRRendering = enabled;
  
  m_useHDRRendering = enabled;
  
  if (enabled) {
    // CRITICAL FIX: Perform HDR detection synchronously to provide immediate feedback
    HDRDetection* detector = HDRDetection::instance();
    HDRDetection::HDRCapabilities capabilities = detector->detectHDRCapabilities(nullptr);
    
    // Process detection results immediately
    onHDRDetectionComplete(capabilities);
  } else {
    
    // Disable HDR rendering
    if (m_hdrWidget) {
      emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
      m_hdrWidget->hide();
    }
    
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
  
  // CRITICAL FIX: Connect to the widgetInitialized signal
  connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
          this, [this]() {
      // Now it's safe to push frames to the widget
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
  qDebug() << "HDRRenderingManager: Starting background HDR detection...";
  qDebug() << "HDRRenderingManager: HDR detection worker exists:" << (m_hdrDetectionWorker != nullptr);
  
  // Create HDR detection worker if not already created
  if (!m_hdrDetectionWorker) {
    qDebug() << "HDRRenderingManager: Creating new HDRDetectionWorker...";
    m_hdrDetectionWorker = new HDRDetectionWorker(this);
    qDebug() << "HDRRenderingManager: HDRDetectionWorker created at:" << m_hdrDetectionWorker;
    
    // Connect signals
    qDebug() << "HDRRenderingManager: Connecting HDR detection signals...";
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionComplete,
            this, &HDRRenderingManager::onHDRDetectionComplete);
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionFailed,
            this, &HDRRenderingManager::onHDRDetectionFailed);
    qDebug() << "HDRRenderingManager: HDR detection signals connected successfully";
  } else {
    qDebug() << "HDRRenderingManager: Using existing HDRDetectionWorker";
  }
  
  qDebug() << "HDRRenderingManager: Checking if HDR detection is already in progress...";
  if (!m_hdrDetectionWorker->isDetecting()) {
    qDebug() << "HDRRenderingManager: Starting HDR detection in background thread...";
    m_hdrDetectionWorker->startDetection();
    qDebug() << "HDRRenderingManager: HDR detection started successfully in background thread";
  } else {
    qDebug() << "HDRRenderingManager: HDR detection already in progress, skipping";
  }
}

bool HDRRenderingManager::isHDRDetectionInProgress() const
{
  return m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting();
}

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
  if (!m_hdrWidget || !m_useHDRRendering) {
    return;
  }

  // CRITICAL FIX: Try to push frame even if widget isn't fully ready
  // This helps resolve initialization race conditions
  if (!m_hdrWidget->isReadyForRendering()) {
    qDebug() << "HDRRenderingManager: Widget not ready, attempting frame push anyway";
    
    // Try to push the frame - the widget's updateFrame will handle the unready state
    m_hdrWidget->updateFrame(frame);
    
    // Also connect to widgetInitialized signal as backup
    static bool deferredUpdateConnected = false;
    if (!deferredUpdateConnected) {
      connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
              this, [this, frame]() {
          qDebug() << "HDRRenderingManager: Widget initialized, pushing frame via signal";
          if (m_hdrWidget && m_useHDRRendering) {
            m_hdrWidget->updateFrame(frame);
          }
      }, Qt::SingleShotConnection);  // Use SingleShot to avoid multiple connections
      deferredUpdateConnected = true;
    }
    return;
  }

  // Ensure widget is visible
  if (!m_hdrWidget->isVisible()) {
    m_hdrWidget->show();
    m_hdrWidget->raise();
  }

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
          // CRITICAL FIX: Defer integration until widget is shown
          QTimer::singleShot(0, this, [this, splitView]() {
            // Integrate with split view
            splitView->setHDROverlayWidget(m_hdrWidget);
            splitView->showHDROverlay(true);
            qDebug() << "HDRRenderingManager: HDR widget integrated with split view";
            
            // Widget will emit widgetInitialized when ready
            // That signal will trigger hdrRenderingStateChanged
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
    // CRITICAL FIX: Handle HDR not supported case
    qDebug() << "HDRRenderingManager: === HDR IS NOT SUPPORTED ===";
    qDebug() << "HDRRenderingManager: Error:" << capabilities.errorMessage;
    
    // Disable HDR rendering since it's not supported
    m_useHDRRendering = false;
    m_hdrCapabilities = capabilities; // Store capabilities for reference
    
    // Clean up any existing HDR widget
    if (m_hdrWidget) {
      emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
      m_hdrWidget->hide();
    }
    
    // Notify that HDR is disabled
    emit hdrRenderingStateChanged(false, m_hdrWidget);
    emit hdrDetectionFailed(capabilities.errorMessage.isEmpty() ? 
                           "HDR not supported on current display" : 
                           capabilities.errorMessage);
  }
}

void HDRRenderingManager::onHDRDetectionFailed(const QString& error)
{
  m_useHDRRendering = false;
  
  if (m_hdrWidget) {
    emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
    m_hdrWidget->hide();
  }
  
  emit hdrRenderingStateChanged(false, m_hdrWidget);
  emit hdrDetectionFailed(error);
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
