// HDRRenderingManager.cpp - Fixed version with proper initialization timing

#include "HDRRenderingManager.h"
#include <QDebug>
#include <QApplication>
#include <QMainWindow>
#include <QTimer>

HDRRenderingManager::HDRRenderingManager(QObject* parent)
    : QObject(parent)
    , m_hdrWidget(nullptr)
    , m_hdrDetectionWorker(nullptr)
    , m_useHDRRendering(false)
{
    qDebug() << "=== HDRRenderingManager: Constructor called ===";
    qDebug() << "HDRRenderingManager: Initializing with parent:" << parent;
    
    // Initialize HDR capabilities to not supported by default
    m_hdrCapabilities.isHDRSupported = false;
    m_hdrCapabilities.errorMessage = "HDR detection not performed yet";
    qDebug() << "HDRRenderingManager: HDR capabilities initialized to NOT SUPPORTED";
    
    qDebug() << "HDRRenderingManager: Constructor completed successfully";
}

HDRRenderingManager::~HDRRenderingManager()
{
    cleanupHDRResources();
}

void HDRRenderingManager::setHDRRenderingEnabled(bool enabled)
{
    qDebug() << "=== HDRRenderingManager::setHDRRenderingEnabled() called ===";
    qDebug() << "HDRRenderingManager: HDR rendering enable request:" << enabled;
    qDebug() << "HDRRenderingManager: Current HDR state:" << m_useHDRRendering;
    
    if (m_useHDRRendering == enabled) {
        qDebug() << "HDRRenderingManager: HDR state unchanged, returning";
        return;
    }
    
    qDebug() << "HDRRenderingManager: Changing HDR state from" << m_useHDRRendering << "to" << enabled;
    m_useHDRRendering = enabled;
    
    if (enabled) {
        qDebug() << "HDRRenderingManager: === ENABLING HDR RENDERING ===";
        
        // Perform synchronous HDR detection for immediate feedback
        qDebug() << "HDRRenderingManager: Performing synchronous HDR detection for immediate feedback";
        HDRDetection* detector = HDRDetection::instance();
        HDRDetection::HDRCapabilities capabilities = detector->detectHDRCapabilities();
        
        if (capabilities.isHDRSupported) {
            qDebug() << "HDRRenderingManager: Immediate HDR detection - HDR supported";
            onHDRDetectionComplete(capabilities);
        } else {
            qDebug() << "HDRRenderingManager: Immediate HDR detection - HDR not supported";
            onHDRDetectionComplete(capabilities);
        }
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
    
    // CRITICAL FIX: Connect to the widgetInitialized signal
    connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
            this, [this]() {
        qDebug() << "HDRRenderingManager: HDR widget initialization complete";
        // Now it's safe to push frames to the widget
        emit hdrRenderingStateChanged(true, m_hdrWidget);
    });
    
    // Connect other HDR widget signals
    qDebug() << "HDRRenderingManager: Connecting HDR widget signals...";
    connect(m_hdrWidget, &HDR_VideoWidget::hdrNotSupported,
            this, &HDRRenderingManager::onHDRNotSupported);
    connect(m_hdrWidget, &HDR_VideoWidget::renderModeChanged,
            this, &HDRRenderingManager::onHDRModeChanged);
    qDebug() << "HDRRenderingManager: HDR widget signals connected successfully";
    
    // Set HDR capabilities on the widget
    qDebug() << "HDRRenderingManager: Setting HDR capabilities on widget...";
    qDebug() << "HDRRenderingManager: HDR Mode:" << m_hdrCapabilities.supportedMode;
    qDebug() << "HDRRenderingManager: Max Luminance:" << m_hdrCapabilities.maxLuminance << "nits";
    qDebug() << "HDRRenderingManager: Display Name:" << m_hdrCapabilities.displayName;
    
    m_hdrWidget->setHDRCapabilities(m_hdrCapabilities);
    
    // Set render mode based on HDR capabilities
    qDebug() << "HDRRenderingManager: Setting render mode based on capabilities...";
    if (m_hdrCapabilities.supportedMode == HDRDetection::BT2020_PQ_10bit) {
        qDebug() << "HDRRenderingManager: Setting BT2020 PQ 10-bit mode";
        m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT2020_PQ_10bit);
    } else if (m_hdrCapabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
        qDebug() << "HDRRenderingManager: Setting BT709 Linear 16-bit mode";
        m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT709_Linear_16bit);
    } else {
        qDebug() << "HDRRenderingManager: Setting SDR mode (fallback)";
        m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_SDR);
    }
    
    qDebug() << "HDRRenderingManager: HDR widget created successfully with parent integration";
    qDebug() << "HDRRenderingManager: Widget ready for display and frame updates";
    
    return m_hdrWidget;
}

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
    qDebug() << "=== HDRRenderingManager::updateHDRFrame() called ===";
    qDebug() << "HDRRenderingManager: Frame size:" << frame.size();
    qDebug() << "HDRRenderingManager: Frame format:" << frame.format();
    qDebug() << "HDRRenderingManager: HDR widget exists:" << (m_hdrWidget != nullptr);
    qDebug() << "HDRRenderingManager: HDR rendering active:" << m_useHDRRendering;

    if (!m_hdrWidget || !m_useHDRRendering) {
        if (!m_hdrWidget) {
            qDebug() << "HDRRenderingManager: ERROR - Cannot update frame: HDR widget is null";
        }
        if (!m_useHDRRendering) {
            qDebug() << "HDRRenderingManager: ERROR - Cannot update frame: HDR rendering is disabled";
        }
        return;
    }

    // CRITICAL FIX: Check if widget is ready for rendering
    if (!m_hdrWidget->isReadyForRendering()) {
        qDebug() << "HDRRenderingManager: Widget not ready for rendering, deferring frame update";
        // Connect to widgetInitialized signal if not already connected
        static bool deferredUpdateConnected = false;
        if (!deferredUpdateConnected) {
            connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
                    this, [this, frame]() {
                qDebug() << "HDRRenderingManager: Widget now initialized, pushing deferred frame";
                if (m_hdrWidget && m_useHDRRendering) {
                    m_hdrWidget->updateFrame(frame);
                }
            }, Qt::SingleShotConnection);  // Use SingleShot to avoid multiple connections
            deferredUpdateConnected = true;
        }
        return;
    }

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
    
    // Store HDR capabilities
    m_hdrCapabilities = capabilities;
    
    if (capabilities.isHDRSupported) {
        qDebug() << "HDRRenderingManager: === HDR IS SUPPORTED ===";
        qDebug() << "HDRRenderingManager: Storing HDR capabilities...";
        
        m_useHDRRendering = true;
        qDebug() << "HDRRenderingManager: HDR capabilities stored and rendering enabled";
        
        emit hdrDetectionCompleted(true);
        qDebug() << "HDRRenderingManager: HDR rendering enabled (integrated mode):" 
                 << HDRDetection::getHDRModeDescription(capabilities.supportedMode);
        
        // Find the split view widget to use as parent
        qDebug() << "HDRRenderingManager: Looking for split view widget...";
        splitViewWidget* splitView = nullptr;
        QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget* widget : topLevelWidgets) {
            if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(widget)) {
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
            // Fallback handling...
        }
    } else {
        // HDR not supported
        qDebug() << "HDRRenderingManager: === HDR IS NOT SUPPORTED ===";
        qDebug() << "HDRRenderingManager: Error:" << capabilities.errorMessage;
        
        m_useHDRRendering = false;
        m_hdrCapabilities = capabilities;
        
        if (m_hdrWidget) {
            emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
            m_hdrWidget->hide();
        }
        
        emit hdrRenderingStateChanged(false, m_hdrWidget);
        emit hdrDetectionFailed(capabilities.errorMessage.isEmpty() ? 
                               "HDR not supported on this display" : capabilities.errorMessage);
    }
}

// ... Other methods remain the same ...
