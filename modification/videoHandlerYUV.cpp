// videoHandlerYUV.cpp - Fixed version with proper frame update timing
// Only showing the modified methods

void videoHandlerYUV::slot10BitDisplayChanged(bool enable10Bit)
{
  qDebug() << "=== videoHandlerYUV::slot10BitDisplayChanged() called ===";
  qDebug() << "videoHandlerYUV: 10-bit display checkbox state:" << enable10Bit;
  
  if (enable10Bit) {
    // Check if the current display supports HDR before proceeding
    HDRDetection* detector = HDRDetection::instance();
    HDRDetection::HDRCapabilities capabilities = detector->detectHDRCapabilities();
    
    if (!capabilities.isHDRSupported) {
      qDebug() << "videoHandlerYUV: HDR not supported on current display";
      
      // Show error message to user
      QMessageBox::warning(nullptr, 
                          "HDR Not Supported",
                          QString("HDR is not supported on the current display.\n\n%1")
                            .arg(capabilities.errorMessage.isEmpty() ? 
                                "Display does not support HDR" : capabilities.errorMessage));
      
      // Uncheck the checkbox and return without enabling HDR
      ui.checkBoxEnable10BitDisplay->setChecked(false);
      return;
    }
    
    qDebug() << "videoHandlerYUV: HDR supported - proceeding with activation";
  }
  
  // Save the 10-bit display setting to QSettings
  QSettings settings;
  settings.setValue("Enable10BitDisplay", enable10Bit);
  qDebug() << "videoHandlerYUV: Saved Enable10BitDisplay setting to:" << enable10Bit;
  
  // Delegate to HDRRenderingManager
  qDebug() << "videoHandlerYUV: Delegating HDR enable request to HDRRenderingManager";
  m_hdrRenderingManager->setHDRRenderingEnabled(enable10Bit);
  qDebug() << "videoHandlerYUV: HDR enable request completed";
}

void videoHandlerYUV::onHDRRenderingStateChanged(bool enabled, HDR_VideoWidget* widget)
{
  qDebug() << "=== videoHandlerYUV::onHDRRenderingStateChanged() called ===";
  qDebug() << "videoHandlerYUV: HDR rendering state changed to:" << enabled;
  qDebug() << "videoHandlerYUV: HDR widget pointer:" << widget;
  
  if (enabled && widget) {
    qDebug() << "videoHandlerYUV: HDR enabled - will push frame when widget is ready";
    
    // CRITICAL FIX: Don't push frame immediately, wait for widget to be ready
    // The widget will be initialized asynchronously
    QSettings settings;
    bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();
    
    if (enable10BitDisplay && srcPixelFormat.getBitsPerSample() == 10) {
      qDebug() << "videoHandlerYUV: Conditions met for HDR rendering";
      
      // Get current frame but don't push it yet
      QImage currentFrameImage = getCurrentFrameAsImage();
      if (currentFrameImage.isNull() && currentImageIndex >= 0) {
        qDebug() << "videoHandlerYUV: Current frame is null, loading frame" << currentImageIndex;
        loadFrame(currentImageIndex);
        currentFrameImage = getCurrentFrameAsImage();
      }
      
      if (!currentFrameImage.isNull()) {
        // Store the frame for later update
        m_pendingHDRFrame = currentFrameImage;
        
        // Connect to widget's initialized signal if not already connected
        if (!m_hdrWidgetInitConnection) {
          m_hdrWidgetInitConnection = connect(widget, &HDR_VideoWidget::widgetInitialized,
                                             this, [this]() {
            qDebug() << "videoHandlerYUV: HDR widget initialized, pushing pending frame";
            if (!m_pendingHDRFrame.isNull() && m_hdrRenderingManager) {
              m_hdrRenderingManager->updateHDRFrame(m_pendingHDRFrame);
              m_pendingHDRFrame = QImage(); // Clear pending frame
            }
          });
        }
        
        // Also try to push the frame in case widget is already initialized
        // The HDRRenderingManager will handle the case where widget is not ready
        QTimer::singleShot(100, this, [this, currentFrameImage]() {
          if (m_hdrRenderingManager && m_hdrRenderingManager->isHDRRenderingActive()) {
            qDebug() << "videoHandlerYUV: Attempting to push frame to HDR widget";
            m_hdrRenderingManager->updateHDRFrame(currentFrameImage);
          }
        });
      } else {
        qDebug() << "videoHandlerYUV: No frame available to push to HDR widget";
      }
    }
  } else if (!enabled && m_hdrWidgetInitConnection) {
    // Disconnect the signal when HDR is disabled
    disconnect(m_hdrWidgetInitConnection);
    m_hdrWidgetInitConnection = QMetaObject::Connection();
    m_pendingHDRFrame = QImage();
  }
  
  qDebug() << "videoHandlerYUV: HDR state change handling completed";
}

QImage videoHandlerYUV::getCurrentFrameAsImage()
{
  // CRITICAL FIX: Ensure we always return a valid image for HDR rendering
  if (currentImage.isNull() && currentImageIndex >= 0) {
    // Try to load the current frame if not already loaded
    loadFrame(currentImageIndex);
  }
  
  if (!currentImage.isNull()) {
    // Check if we need to convert the format for HDR
    QSettings settings;
    bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();
    
    if (enable10BitDisplay && srcPixelFormat.getBitsPerSample() == 10) {
      // For 10-bit content, ensure we have the right format
      if (currentImage.format() != QImage::Format_ARGB32_Premultiplied &&
          currentImage.format() != QImage::Format_RGB32) {
        return currentImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
      }
    }
  }
  
  return currentImage;
}

void videoHandlerYUV::drawFrame(PlaylistItem* item, int frameIdx, double zoomFactor)
{
  // Check HDR rendering conditions first
  QSettings settings;
  bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();
  
  qDebug() << "videoHandlerYUV: HDR rendering active:" << 
              (m_hdrRenderingManager ? m_hdrRenderingManager->isHDRRenderingActive() : false);
  qDebug() << "videoHandlerYUV: 10-bit display enabled:" << enable10BitDisplay;
  qDebug() << "videoHandlerYUV: Pixel format bits per sample:" << srcPixelFormat.getBitsPerSample();
  
  // Check if we should use HDR rendering
  if (m_hdrRenderingManager && 
      m_hdrRenderingManager->isHDRRenderingActive() && 
      enable10BitDisplay && 
      srcPixelFormat.getBitsPerSample() == 10) {
    
    qDebug() << "videoHandlerYUV: === USING HDR RENDERING PATH ===";
    
    // Load the frame if needed
    if (frameIdx != currentImageIndex) {
      loadFrame(frameIdx);
    }
    
    // Get current frame as QImage
    QImage frameImage = getCurrentFrameAsImage();
    
    if (!frameImage.isNull()) {
      // Update HDR frame - the manager will handle initialization state
      m_hdrRenderingManager->updateHDRFrame(frameImage);
      qDebug() << "videoHandlerYUV: HDR frame update completed, skipping QPainter rendering";
    } else {
      qDebug() << "videoHandlerYUV: ERROR - Failed to get current frame for HDR rendering";
    }
    
    // Skip standard QPainter rendering when HDR is active
    return;
  }
  
  qDebug() << "videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)";
  
  // Standard rendering path (existing code)
  videoHandler::drawFrame(item, frameIdx, zoomFactor);
  
  // ... rest of the existing drawFrame implementation ...
}

// Add these member variables to the videoHandlerYUV class header:
// private:
//   QImage m_pendingHDRFrame;
//   QMetaObject::Connection m_hdrWidgetInitConnection;
