#include "HDRRenderingManager.h"
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
  
  if (enabled) {
    // Check if HDR detection is already in progress or complete
    if (m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting()) {
      qDebug() << "HDR detection already in progress, ignoring duplicate request";
      return;
    }
    
    if (m_useHDRRendering && m_hdrCapabilities.isHDRSupported) {
      qDebug() << "HDR already supported and enabled";
      return;
    }
    
    // Start HDR detection only if not already done
    qDebug() << "10-bit display requested, starting HDR detection...";
    
    // Use QTimer to defer HDR detection to next event loop iteration
    QTimer::singleShot(0, this, [this]() {
      startHDRDetection();
    });
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
    qDebug() << "HDR widget already exists or HDR not enabled";
    return m_hdrWidget;
  }
  
  qDebug() << "Creating persistent HDR widget for push model architecture";
  qDebug() << "Creating HDR widget with proper parent for UI integration";
  
  m_hdrWidget = new HDR_VideoWidget(parent);
  
  // Connect HDR widget signals
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
  
  qDebug() << "HDR widget created with parent integration, ready for display";
  
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
  qDebug() << "Starting background HDR detection...";
  
  // Create HDR detection worker if not already created
  if (!m_hdrDetectionWorker) {
    m_hdrDetectionWorker = new HDRDetectionWorker(this);
    
    // Connect signals
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionComplete,
            this, &HDRRenderingManager::onHDRDetectionComplete);
    connect(m_hdrDetectionWorker, &HDRDetectionWorker::detectionFailed,
            this, &HDRRenderingManager::onHDRDetectionFailed);
  }
  
  if (!m_hdrDetectionWorker->isDetecting()) {
    m_hdrDetectionWorker->startDetection();
    qDebug() << "HDR detection started in background thread";
  } else {
    qDebug() << "HDR detection already in progress";
  }
}

bool HDRRenderingManager::isHDRDetectionInProgress() const
{
  return m_hdrDetectionWorker && m_hdrDetectionWorker->isDetecting();
}

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
  if (m_hdrWidget && m_useHDRRendering) {
    m_hdrWidget->updateFrame(frame);
    qDebug() << "Frame data pushed to HDR widget (push model architecture)";
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
  qDebug() << "HDR detection completed in main thread. Supported:" << capabilities.isHDRSupported;
  
  if (capabilities.isHDRSupported) {
    // Store capabilities and enable HDR rendering
    m_hdrCapabilities = capabilities;
    m_useHDRRendering = true;
    
    qDebug() << "HDR rendering enabled (integrated mode):" << HDRDetection::getHDRModeDescription(capabilities.supportedMode);
    
    emit hdrRenderingStateChanged(true, m_hdrWidget);
    emit hdrDetectionCompleted(true);
    
    // If widget is already created, update its capabilities
    if (m_hdrWidget) {
      m_hdrWidget->setHDRCapabilities(m_hdrCapabilities);
    }
  } else {
    // HDR not supported, fall back to SDR with same handling as detection failed
    qDebug() << "HDR not supported, falling back to standard rendering";
    onHDRDetectionFailed(capabilities.errorMessage);
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