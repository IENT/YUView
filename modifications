// splitViewWidget.cpp - Fixed HDR overlay integration
// Only showing the modified methods

void splitViewWidget::setHDROverlayWidget(HDR_VideoWidget* hdrWidget)
{
  if (m_hdrOverlayWidget && m_hdrOverlayWidget != hdrWidget) {
    // Remove old widget
    m_hdrOverlayWidget->setParent(nullptr);
    m_hdrOverlayWidget->hide();
  }

  m_hdrOverlayWidget = hdrWidget;

  if (m_hdrOverlayWidget) {
    // CRITICAL FIX: Proper overlay setup with OpenGL widget
    m_hdrOverlayWidget->setParent(this);
    
    // Ensure geometry is set correctly for OpenGL context
    QRect targetGeometry = rect();
    if (targetGeometry.width() < 64 || targetGeometry.height() < 64) {
      targetGeometry = QRect(0, 0, 640, 480);  // Fallback minimum size
    }
    
    m_hdrOverlayWidget->setGeometry(targetGeometry);
    
    // Ensure the widget is properly configured for overlay rendering
    m_hdrOverlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);  // Accept mouse events
    m_hdrOverlayWidget->setFocusPolicy(Qt::StrongFocus);
    
    // CRITICAL FIX: Defer showing the widget to allow proper initialization
    QTimer::singleShot(0, this, [this, targetGeometry]() {
      if (m_hdrOverlayWidget) {
        // Show and raise to top
        m_hdrOverlayWidget->show();
        m_hdrOverlayWidget->raise();
        m_hdrOverlayWidget->activateWindow();  // Ensure OpenGL context activation
        
        // Force an update to trigger OpenGL initialization
        m_hdrOverlayWidget->update();
        
        qDebug() << "splitViewWidget: HDR overlay widget configured - geometry:" << targetGeometry;
        qDebug() << "splitViewWidget: HDR overlay widget visible:" << m_hdrOverlayWidget->isVisible();
        qDebug() << "splitViewWidget: HDR overlay widget size:" << m_hdrOverlayWidget->size();
      }
    });
  }
}

void splitViewWidget::showHDROverlay(bool show)
{
  if (m_hdrOverlayWidget) {
    if (show) {
      // CRITICAL FIX: Ensure widget is properly shown and initialized
      m_hdrOverlayWidget->setGeometry(rect());  // Update geometry first
      m_hdrOverlayWidget->show();
      m_hdrOverlayWidget->raise();  // Ensure it's on top
      
      // Force an update to trigger OpenGL initialization if needed
      m_hdrOverlayWidget->update();
    } else {
      m_hdrOverlayWidget->hide();
    }
    
    qDebug() << "splitViewWidget: HDR overlay visibility set to:" << show;
    
    // Trigger a repaint of the split view to update rendering
    update();
  }
}

void splitViewWidget::paintEvent(QPaintEvent *event)
{
  MoveAndZoomableView::updatePaletteIfNeeded();

  if (!playlist) {
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;
  }

  // CRITICAL FIX: Check HDR overlay state properly
  if (m_hdrOverlayWidget && m_hdrOverlayWidget->isVisible()) {
    // HDR OpenGL widget is handling all rendering
    // But we still need to handle non-video UI elements
    
    // Check if the HDR widget is ready
    HDR_VideoWidget* hdrWidget = qobject_cast<HDR_VideoWidget*>(m_hdrOverlayWidget);
    if (hdrWidget && hdrWidget->isReadyForRendering()) {
      // HDR widget is ready and rendering, skip QPainter video rendering
      // qDebug() << "SplitViewWidget: HDR overlay active and ready";
      return;
    } else {
      // HDR widget exists but not ready yet
      // Clear the background while waiting
      QPainter painter(this);
      painter.fillRect(rect(), Qt::black);
      
      // Optionally show a loading indicator
      painter.setPen(Qt::white);
      painter.drawText(rect(), Qt::AlignCenter, "Initializing HDR display...");
      return;
    }
  }

  // Standard QPainter rendering path
  QPainter painter(this);

  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width(), height());

  if (isViewFrozen)
  {
    QString text = "Playback is running in the separate view only.\nCheck 'Playback in primary "
                   "view' if you want playback to run here too.";

    // Set the QRect where to show the text
    QFont        displayFont = painter.font();
    QFontMetrics metrics(displayFont);
    QSize        textSize = metrics.size(0, text);

    QRect textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(drawArea_botR / 2);

    // Draw a rectangle around the text in white with a black border
    QRect boxRect = textRect + QMargins(5, 5, 5, 5);
    painter.setPen(QPen(Qt::black, 1));
    painter.fillRect(boxRect, Qt::white);
    painter.drawRect(boxRect);

    painter.drawText(textRect, Qt::AlignCenter, text);

    MoveAndZoomableView::updateMouseCursor();

    return;
  }

  // ... rest of the existing paintEvent implementation ...
  // (continue with the standard rendering code)
}

// Override resizeEvent to keep HDR widget sized correctly
void splitViewWidget::resizeEvent(QResizeEvent* event)
{
  MoveAndZoomableView::resizeEvent(event);

  if (m_hdrOverlayWidget && m_hdrOverlayWidget->isVisible()) {
    // Update HDR widget geometry to match
    m_hdrOverlayWidget->setGeometry(rect());
    
    // Force update to handle OpenGL viewport changes
    m_hdrOverlayWidget->update();
    
    qDebug() << "splitViewWidget: HDR overlay resized to match view";
  }
  
  // Check for display changes when window is resized (may indicate move to different screen)
  checkCurrentDisplayHDRSupport();
}

// Add event filter to handle widget stacking order
bool splitViewWidget::eventFilter(QObject* watched, QEvent* event)
{
  // Ensure HDR widget stays on top when needed
  if (m_hdrOverlayWidget && watched == m_hdrOverlayWidget) {
    if (event->type() == QEvent::ZOrderChange) {
      // Make sure HDR widget stays on top
      if (m_hdrOverlayWidget->isVisible()) {
        m_hdrOverlayWidget->raise();
      }
    }
  }
  
  return MoveAndZoomableView::eventFilter(watched, event);
}

// In the constructor or initialization:
void splitViewWidget::initializeHDRSupport()
{
  // Install event filter for HDR widget management
  if (m_hdrOverlayWidget) {
    m_hdrOverlayWidget->installEventFilter(this);
  }
  
  // Connect to display change signals
  connect(this, &splitViewWidget::signalDisplayHDRSupportChanged,
          this, [this](bool hdrSupported, const QString& displayName) {
    qDebug() << "Display HDR support changed:" << displayName << "HDR:" << hdrSupported;
    
    // If HDR was active but display no longer supports it, disable HDR
    if (!hdrSupported && m_hdrOverlayWidget && m_hdrOverlayWidget->isVisible()) {
      qDebug() << "HDR display lost, disabling HDR rendering";
      showHDROverlay(false);
      
      // Notify user
      QMessageBox::information(this, "HDR Display Changed",
                              "The current display does not support HDR.\n"
                              "HDR rendering has been disabled.");
    }
  });
}
