// HDR_VideoWidget.cpp - Fixed version with proper initialization handling

#include "HDR_VideoWidget.h"
#include "HDRDetection.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QThread>
#include <QApplication>
#include <QElapsedTimer>
#include <cmath>

HDR_VideoWidget::HDR_VideoWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , QOpenGLFunctions_3_3_Core()
    , m_initialized(false)
    , m_shaderProgram(nullptr)
    , m_vertexArrayObject(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_videoTexture(nullptr)
    , m_frameUpdated(false)
    , m_textureFormat(Format_SDR_8bit)
    , m_renderMode(Mode_SDR)
    , m_hdrCapable(false)
    , m_displayMaxLuminance(100.0f)
    , m_sourceMaxLuminance(1000.0f)
    , m_hdrExposure(0.0f)
    , m_hdrGamma(1.0f)
    , m_dragging(false)
    , m_lastMousePos(0, 0)
    , m_frameCount(0)
    , m_lastFpsUpdate(0)
{
    // Configure OpenGL surface format before anything else
    QSurfaceFormat format;
    format.setMajorVersion(3);
    format.setMinorVersion(3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(0);  // No multisampling for better performance
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    
    // Start with standard 8-bit format for stability
    qDebug() << "HDR Surface Format: Starting with standard 8-bit format for stability";
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(8);
    
    setFormat(format);
    
    // Set critical attributes for HDR rendering
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, false);
    setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_NoSystemBackground, false);
    
    // Set focus policy for interaction
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    
    // Initialize matrices
    m_projectionMatrix.setToIdentity();
    m_textureMatrix.setToIdentity();
    
    // CRITICAL FIX: Ensure widget is shown to trigger OpenGL initialization
    // Use a single-shot timer to defer any frame updates until after initialization
    QTimer::singleShot(0, this, [this]() {
        if (!isVisible()) {
            show();
            raise();
        }
        // Force OpenGL context creation by requesting an update
        update();
    });
    
    // Setup FPS monitoring timer
    m_fpsTimer = new QTimer(this);
    connect(m_fpsTimer, &QTimer::timeout, this, [this]() {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (m_lastFpsUpdate > 0) {
            qint64 elapsed = currentTime - m_lastFpsUpdate;
            float fps = (m_frameCount * 1000.0f) / elapsed;
            qDebug() << "HDR Video Widget FPS:" << fps << "frames/sec";
        }
        m_frameCount = 0;
        m_lastFpsUpdate = currentTime;
    });
}

HDR_VideoWidget::~HDR_VideoWidget()
{
    // Ensure proper cleanup in OpenGL context
    if (context() && context()->isValid()) {
        makeCurrent();
        
        // Clean up OpenGL resources
        delete m_shaderProgram;
        delete m_vertexArrayObject;
        delete m_vertexBuffer;
        delete m_indexBuffer;
        delete m_videoTexture;
        
        doneCurrent();
    }
}

void HDR_VideoWidget::initializeGL()
{
    if (!initializeOpenGLFunctions()) {
        handleInitializationError("Failed to initialize OpenGL functions");
        return;
    }
    
    qDebug() << "Initializing HDR Video Widget with OpenGL" 
             << context()->format().majorVersion() << "."
             << context()->format().minorVersion();
    
    // Validate surface format for HDR
    QSurfaceFormat format = context()->format();
    qDebug() << "HDR Surface Format validation:";
    qDebug() << "  Red buffer size:" << format.redBufferSize();
    qDebug() << "  Green buffer size:" << format.greenBufferSize();
    qDebug() << "  Blue buffer size:" << format.blueBufferSize();
    qDebug() << "  Alpha buffer size:" << format.alphaBufferSize();
    qDebug() << "  Color space:" << format.colorSpace();
    
    bool isHDRFormat = (format.redBufferSize() >= 10 || format.redBufferSize() == 16);
    qDebug() << "  Is HDR format:" << isHDRFormat;
    
    if (isHDRFormat) {
        qDebug() << "HDR format detected, enabling HDR-optimized rendering";
        // Enable HDR-optimized attributes now that we have a valid HDR context
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    } else {
        qDebug() << "HDR format requested but not available, will simulate HDR in shaders";
    }
    
    // Initialize OpenGL state with error checking
    glEnable(GL_BLEND);
    if (logOpenGLError("Enable blend")) return;
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (logOpenGLError("Set blend func")) return;
    
    glDisable(GL_DEPTH_TEST);
    if (logOpenGLError("Disable depth test")) return;
    
    glDisable(GL_CULL_FACE);
    if (logOpenGLError("Disable cull face")) return;
    
    // Initialize components with proper error checking
    if (!initializeShaders()) {
        handleInitializationError("Failed to initialize shaders");
        return;
    }
    
    if (!initializeGeometry()) {
        handleInitializationError("Failed to initialize geometry");
        return;
    }
    
    if (!initializeTexture()) {
        handleInitializationError("Failed to initialize texture");
        return;
    }
    
    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (logOpenGLError("Set clear color")) return;
    
    // CRITICAL: Set initialized flag BEFORE processing any pending frames
    m_initialized = true;
    qDebug() << "HDR Video Widget initialized successfully";
    
    // Process any pending frame that was queued before initialization
    if (m_frameUpdated && !m_currentFrame.isNull()) {
        qDebug() << "Processing pending frame after initialization";
        uploadTextureData(m_currentFrame);
        update();  // Trigger immediate repaint
    }
    
    // Emit signal to notify that widget is ready
    emit widgetInitialized();
    
    logOpenGLError("initializeGL complete");
}

void HDR_VideoWidget::paintGL()
{
    // CRITICAL FIX: Properly check initialization state
    if (!m_initialized) {
        // Widget not initialized yet, clear to black and return
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (!m_currentFrame.isNull() && m_videoTexture && m_videoTexture->isCreated()) {
        renderFrame();
        m_frameCount++;
        // qDebug() << "HDR_VideoWidget::paintGL: Frame rendered successfully";
    } else {
        // Only log in debug mode to avoid spam
        if (m_currentFrame.isNull()) {
            // qDebug() << "HDR_VideoWidget::paintGL: No frame to render (frame is null)";
        } else if (!m_videoTexture) {
            qDebug() << "HDR_VideoWidget::paintGL: No video texture available";
        } else if (!m_videoTexture->isCreated()) {
            qDebug() << "HDR_VideoWidget::paintGL: Video texture not created yet";
        }
    }
    
    logOpenGLError("paintGL");
}

void HDR_VideoWidget::updateFrame(const QImage& newFrame)
{
    if (newFrame.isNull()) {
        qWarning() << "HDR_VideoWidget::updateFrame: Received null frame";
        return;
    }
    
    // Store frame data (thread-safe)
    m_currentFrame = newFrame;
    m_frameSize = newFrame.size();
    m_frameUpdated = true;
    
    // CRITICAL FIX: Handle different initialization states properly
    if (!m_initialized) {
        // Widget not initialized yet, queue the frame for later
        qDebug() << "HDR_VideoWidget::updateFrame: Widget not initialized, queuing frame";
        // The frame will be processed in initializeGL() when it completes
        return;
    }
    
    // Widget is initialized, process the frame
    if (context() && context()->isValid()) {
        // Use direct invocation if we're in the GUI thread
        if (QThread::currentThread() == QApplication::instance()->thread()) {
            makeCurrent();
            if (uploadTextureData(newFrame)) {
                doneCurrent();
                update();  // Trigger repaint
                emit frameUpdated();
                // qDebug() << "HDR_VideoWidget::updateFrame: Texture uploaded, triggering repaint";
            } else {
                doneCurrent();
                handleRenderingError("Failed to upload texture data");
            }
        } else {
            // Use queued invocation for thread safety
            QMetaObject::invokeMethod(this, [this, newFrame]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    if (uploadTextureData(newFrame)) {
                        doneCurrent();
                        update();
                        emit frameUpdated();
                    } else {
                        doneCurrent();
                        handleRenderingError("Failed to upload texture data");
                    }
                }
            }, Qt::QueuedConnection);
        }
    } else {
        qWarning() << "HDR_VideoWidget::updateFrame: OpenGL context not valid";
    }
}

// Add this helper method to check if widget is ready for rendering
bool HDR_VideoWidget::isReadyForRendering() const
{
    return m_initialized && context() && context()->isValid();
}

// ... Rest of the implementation remains the same ...
// (Include all other methods like initializeShaders, initializeGeometry, etc.)
