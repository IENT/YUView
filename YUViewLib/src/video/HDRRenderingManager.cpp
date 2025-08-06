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
  qDebug() << "=== HDRRenderingManager: Constructor called ===";
  qDebug() << "HDRRenderingManager: Initializing with parent:" << parent;
  
  // Initialize HDR capabilities to default (not supported)
  m_hdrCapabilities.isHDRSupported = false;
  qDebug() << "HDRRenderingManager: HDR capabilities initialized to NOT SUPPORTED";
  qDebug() << "HDRRenderingManager: Constructor completed successfully";
}

HDRRenderingManager::~HDRRenderingManager()
{
  qDebug() << "=== HDRRenderingManager: Destructor called ===";
  qDebug() << "HDRRenderingManager: Cleaning up HDR resources...";
  cleanupHDRResources();
  qDebug() << "HDRRenderingManager: Destructor completed successfully";
}

void HDRRenderingManager::setHDRRenderingEnabled(bool enabled)
{
  qDebug() << "=== HDRRenderingManager::setHDRRenderingEnabled() called ===";
  qDebug() << "HDRRenderingManager: HDR rendering enable request:" << enabled;
  qDebug() << "HDRRenderingManager: Current HDR state:" << m_useHDRRendering;
  
  if (m_useHDRRendering == enabled) {
    qDebug() << "HDRRenderingManager: HDR state unchanged, no action needed";
    return;
  }
  
  qDebug() << "HDRRenderingManager: Changing HDR state from" << m_useHDRRendering << "to" << enabled;
  m_useHDRRendering = enabled;
  
  if (enabled) {
    qDebug() << "HDRRenderingManager: === ENABLING HDR RENDERING ===";
    
    // Check if HDR detection is already in progress or complete
    if (m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting()) {
      qDebug() << "HDRRenderingManager: HDR detection already in progress, ignoring duplicate request";
      return;
    }
    
    if (m_useHDRRendering && m_hdrCapabilities.isHDRSupported) {
      qDebug() << "HDRRenderingManager: HDR already supported and enabled";
      qDebug() << "HDRRenderingManager: HDR Mode:" << m_hdrCapabilities.supportedMode;
      qDebug() << "HDRRenderingManager: Max Luminance:" << m_hdrCapabilities.maxLuminance << "nits";
      return;
    }
    
    // Start HDR detection only if not already done
    qDebug() << "HDRRenderingManager: 10-bit display requested, starting HDR detection...";
    qDebug() << "HDRRenderingManager: Deferring HDR detection to next event loop iteration";
    
    // Use QTimer to defer HDR detection to next event loop iteration
    QTimer::singleShot(0, this, [this]() {
      qDebug() << "HDRRenderingManager: QTimer callback - starting HDR detection now";
      startHDRDetection();
    });
  } else {
    qDebug() << "HDRRenderingManager: === DISABLING HDR RENDERING ===";
    qDebug() << "HDRRenderingManager: HDR widget exists:" << (m_hdrWidget != nullptr);
    
    // Disable HDR rendering
    if (m_hdrWidget) {
      qDebug() << "HDRRenderingManager: Hiding HDR widget and emitting display signal";
      emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
      m_hdrWidget->hide();
      qDebug() << "HDRRenderingManager: HDR widget hidden successfully";
    }
    
    qDebug() << "HDRRenderingManager: Emitting HDR rendering state changed signal (disabled)";
    emit hdrRenderingStateChanged(false, m_hdrWidget);
    qDebug() << "HDRRenderingManager: HDR rendering disabled successfully";
  }
}

HDR_VideoWidget* HDRRenderingManager::createHDRWidget(QWidget* parent)
{
  qDebug() << "=== HDRRenderingManager::createHDRWidget() called ===";
  qDebug() << "HDRRenderingManager: HDR widget creation requested with parent:" << parent;
  qDebug() << "HDRRenderingManager: Current HDR rendering state:" << m_useHDRRendering;
  qDebug() << "HDRRenderingManager: HDR widget exists:" << (m_hdrWidget != nullptr);
  
  // Only create if HDR is supported and not already created
  if (!m_useHDRRendering || m_hdrWidget) {
    if (!m_useHDRRendering) {
      qDebug() << "HDRRenderingManager: Cannot create HDR widget - HDR not enabled";
    } else {
      qDebug() << "HDRRenderingManager: HDR widget already exists, returning existing widget";
    }
    return m_hdrWidget;
  }
  
  qDebug() << "HDRRenderingManager: === CREATING NEW HDR WIDGET ===";
  qDebug() << "HDRRenderingManager: Creating persistent HDR widget for push model architecture";
  qDebug() << "HDRRenderingManager: Parent widget pointer:" << parent;
  qDebug() << "HDRRenderingManager: HDR capabilities supported:" << m_hdrCapabilities.isHDRSupported;
  
  m_hdrWidget = new HDR_VideoWidget(parent);
  qDebug() << "HDRRenderingManager: HDR_VideoWidget object created at:" << m_hdrWidget;
  
  // Connect HDR widget signals
  qDebug() << "HDRRenderingManager: Connecting HDR widget signals...";
  connect(m_hdrWidget, &HDR_VideoWidget::hdrNotSupported,
          this, &HDRRenderingManager::onHDRNotSupported);
  connect(m_hdrWidget, &HDR_VideoWidget::renderModeChanged,
          this, &HDRRenderingManager::onHDRModeChanged);
  qDebug() << "HDRRenderingManager: HDR widget signals connected successfully";
  
  // Set HDR capabilities
  qDebug() << "HDRRenderingManager: Setting HDR capabilities on widget...";
  qDebug() << "HDRRenderingManager: HDR Mode:" << m_hdrCapabilities.supportedMode;
  qDebug() << "HDRRenderingManager: Max Luminance:" << m_hdrCapabilities.maxLuminance << "nits";
  qDebug() << "HDRRenderingManager: Display Name:" << m_hdrCapabilities.displayName;
  m_hdrWidget->setHDRCapabilities(m_hdrCapabilities);
  
  // Set appropriate render mode based on capabilities
  qDebug() << "HDRRenderingManager: Setting render mode based on capabilities...";
  if (m_hdrCapabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
    qDebug() << "HDRRenderingManager: Setting BT709 Linear 16-bit mode";
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT709_Linear_16bit);
  } else {
    qDebug() << "HDRRenderingManager: Setting BT2020 PQ 10-bit mode";
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT2020_PQ_10bit);
  }
  
  qDebug() << "HDRRenderingManager: HDR widget created successfully with parent integration";
  qDebug() << "HDRRenderingManager: Widget ready for display and frame updates";
  
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
  qDebug() << "=== HDRRenderingManager::updateHDRFrame() called ===";
  qDebug() << "HDRRenderingManager: Frame size:" << frame.size();
  qDebug() << "HDRRenderingManager: Frame format:" << frame.format();
  qDebug() << "HDRRenderingManager: HDR widget exists:" << (m_hdrWidget != nullptr);
  qDebug() << "HDRRenderingManager: HDR rendering active:" << m_useHDRRendering;

  if (m_hdrWidget && m_useHDRRendering) {
    qDebug() << "HDRRenderingManager: === PUSHING FRAME TO HDR WIDGET ===";

    // Ensure widget is visible
    if (!m_hdrWidget->isVisible()) {
      qDebug() << "HDRRenderingManager: HDR widget is hidden, showing it now";
      m_hdrWidget->show();
      m_hdrWidget->raise();
    }

    qDebug() << "HDRRenderingManager: Calling HDR_VideoWidget::updateFrame()...";
    m_hdrWidget->updateFrame(frame);
    qDebug() << "HDRRenderingManager: Frame data successfully pushed to HDR widget";

    // Force immediate update
    m_hdrWidget->update();
  } else {
    if (!m_hdrWidget) {
      qDebug() << "HDRRenderingManager: ERROR - Cannot update frame: HDR widget is null";
      // Try to create it if it doesn't exist
      onHDRDetectionComplete(m_hdrCapabilities);
    }
    if (!m_useHDRRendering) {
      qDebug() << "HDRRenderingManager: ERROR - Cannot update frame: HDR rendering is disabled";
    }
  }
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
          // Integrate with split view
          splitView->setHDROverlayWidget(m_hdrWidget);
          splitView->showHDROverlay(true);

          qDebug() << "HDRRenderingManager: HDR widget integrated with split view";
        }
      } else {
        qDebug() << "HDRRenderingManager: WARNING - Split view widget not found";
        // Fallback to creating widget with main window as parent
        QWidget* parentWidget = QApplication::activeWindow();
        if (parentWidget) {
          createHDRWidget(parentWidget);
        }
      }
    }
  }
}

void HDRRenderingManager::onHDRDetectionFailed(const QString& error)
{
  qDebug() << "YUView HDR: Detection failed, falling back to standard rendering:" << error;
  
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
