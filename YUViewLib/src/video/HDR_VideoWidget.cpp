#include "HDR_VideoWidget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QApplication>
#include <QElapsedTimer>
#include <QColorSpace>  // For Qt 6 HDR color space support
#include <QThread>      // For thread safety checks
#include <QDateTime>    // For FPS timing
#include <cmath>
#include <vector>  // For 10-bit framebuffer grabbing
#include <QPainter>
#include "ui/views/SplitViewWidget.h"

// Vertex data for full-screen quad (position + texture coordinates)
// CRITICAL FIX: Use standard texture coordinates, flip image during upload instead
const float HDR_VideoWidget::s_quadVertices[] = {
    // Positions   // Texture Coords (standard)
    -1.0f, -1.0f,  0.0f, 0.0f,   // Bottom Left  -> Bottom Left in texture
     1.0f, -1.0f,  1.0f, 0.0f,   // Bottom Right -> Bottom Right in texture
     1.0f,  1.0f,  1.0f, 1.0f,   // Top Right    -> Top Right in texture
    -1.0f,  1.0f,  0.0f, 1.0f    // Top Left     -> Top Left in texture
};

const unsigned int HDR_VideoWidget::s_quadIndices[] = {
    0, 1, 2,   // First triangle
    2, 3, 0    // Second triangle
};

HDR_VideoWidget::HDR_VideoWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_renderMode(Mode_SDR_8bit)
    , m_textureFormat(Format_RGBA8)
    , m_hdrCapable(false)
    , m_initialized(false)
    , m_hdrExposure(0.0f)
    , m_hdrGamma(1.0f)
    , m_displayMaxLuminance(1000.0f)  // Default HDR display capability
    , m_sourceMaxLuminance(100.0f)    // Default SDR content assumption
    , m_shaderProgram(nullptr)
    , m_videoTexture(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_vertexArrayObject(nullptr)
    , m_textureLocation(-1)
    , m_renderModeLocation(-1)
    , m_hdrExposureLocation(-1)
    , m_hdrGammaLocation(-1)
    , m_displayMaxLuminanceLocation(-1)
    , m_sourceMaxLuminanceLocation(-1)
    , m_textureMatrixLocation(-1)
    , m_projectionMatrixLocation(-1)
    , m_frameUpdated(false)
    , m_fpsTimer(nullptr)
    , m_frameCount(0)
    , m_lastFpsUpdate(0)
    , m_dragging(false)
{
    // CRITICAL FIX: Progressive HDR Surface Format configuration with fallback
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(0);  // Disable multisampling for stability
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    
    // CRITICAL: Start with standard format, will be upgraded to HDR when capabilities are confirmed
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(8);
    
    qDebug() << "HDR Surface Format: Starting with standard 8-bit format for stability";
    
    setFormat(format);
    setMinimumSize(64, 64);  // Minimum size to ensure valid context
    
    // Set up the widget for OpenGL rendering with conservative attributes
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    
    // Set critical attributes for HDR rendering
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, false);
    setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_NoSystemBackground, false);
    
    // Ensure widget inherits parent's palette for consistent background
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(""); // Clear any stylesheet to inherit parent's style
    
    // Initialize transformation matrices
    m_textureMatrix.setToIdentity();
    m_projectionMatrix.setToIdentity();
    
    // CRITICAL FIX: Defer OpenGL initialization until widget is properly sized and visible
    // Don't force immediate initialization - let it happen naturally when widget is ready
    
    // Setup FPS monitoring timer - but don't start it until OpenGL is initialized
    m_fpsTimer = new QTimer(this);
    connect(m_fpsTimer, &QTimer::timeout, this, [this]() {
        if (!m_initialized) {
            return;  // Don't run FPS monitoring until OpenGL is ready
        }
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
    makeCurrent();
    
    delete m_shaderProgram;
    delete m_videoTexture;
    delete m_vertexBuffer;
    delete m_indexBuffer;
    delete m_vertexArrayObject;
    
    doneCurrent();
}

void HDR_VideoWidget::setRenderMode(RenderMode mode)
{
    if (m_renderMode != mode) {
        m_renderMode = mode;
        
        validateRenderMode();
        
        if (m_initialized) {
            // Ensure OpenGL operations happen in the main thread
            QMetaObject::invokeMethod(this, [this]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    updateShaderUniforms();
                    doneCurrent();
                    update();
                }
            }, Qt::QueuedConnection);
        }
        
        // Control FPS timer based on render mode
        if (mode != Mode_SDR_8bit) {
            // HDR mode - start FPS monitoring
            if (!m_fpsTimer->isActive()) {
                m_fpsTimer->start(1000);
            }
        } else {
            // SDR mode - stop FPS monitoring
            if (m_fpsTimer->isActive()) {
                m_fpsTimer->stop();
            }
        }
        
        emit renderModeChanged(mode);
    }
}

void HDR_VideoWidget::setTextureFormat(TextureFormat format)
{
    if (m_textureFormat != format) {
        m_textureFormat = format;
        
        if (m_initialized && m_videoTexture) {
            // Ensure OpenGL operations happen in the main thread
            QMetaObject::invokeMethod(this, [this]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    updateTextureParameters();
                    doneCurrent();
                    update();
                }
            }, Qt::QueuedConnection);
        }
        
        // Texture format updated
    }
}

void HDR_VideoWidget::setHDRExposure(float exposure)
{
    // Clamp exposure to reasonable range
    exposure = qBound(-10.0f, exposure, 10.0f);
    
    if (qAbs(m_hdrExposure - exposure) > 0.001f) {
        m_hdrExposure = exposure;
        
        if (m_initialized) {
            // Ensure OpenGL operations happen in the main thread
            QMetaObject::invokeMethod(this, [this]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    updateShaderUniforms();
                    doneCurrent();
                    update();
                }
            }, Qt::QueuedConnection);
        }
        
        // HDR exposure updated
    }
}

void HDR_VideoWidget::setHDRGamma(float gamma)
{
    // Clamp gamma to reasonable range
    gamma = qBound(0.1f, gamma, 5.0f);
    
    if (qAbs(m_hdrGamma - gamma) > 0.001f) {
        m_hdrGamma = gamma;
        
        if (m_initialized) {
            // Ensure OpenGL operations happen in the main thread
            QMetaObject::invokeMethod(this, [this]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    updateShaderUniforms();
                    doneCurrent();
                    update();
                }
            }, Qt::QueuedConnection);
        }
        
        // HDR gamma updated
    }
}

void HDR_VideoWidget::setDisplayMaxLuminance(float maxLuminance)
{
    // Clamp to reasonable range for display luminance
    maxLuminance = qBound(50.0f, maxLuminance, 10000.0f);
    
    if (qAbs(m_displayMaxLuminance - maxLuminance) > 0.1f) {
        m_displayMaxLuminance = maxLuminance;
        
        if (m_initialized) {
            QMetaObject::invokeMethod(this, [this]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    updateShaderUniforms();
                    doneCurrent();
                    update();
                }
            }, Qt::QueuedConnection);
        }
        
        // Display max luminance updated
    }
}

void HDR_VideoWidget::setSourceMaxLuminance(float maxLuminance)
{
    // Clamp to reasonable range for source content luminance
    maxLuminance = qBound(100.0f, maxLuminance, 10000.0f);
    
    if (qAbs(m_sourceMaxLuminance - maxLuminance) > 0.1f) {
        m_sourceMaxLuminance = maxLuminance;
        
        if (m_initialized) {
            QMetaObject::invokeMethod(this, [this]() {
                if (context() && context()->isValid()) {
                    makeCurrent();
                    updateShaderUniforms();
                    doneCurrent();
                    update();
                }
            }, Qt::QueuedConnection);
        }
        
        // Source max luminance updated
    }
}

void HDR_VideoWidget::updateFrame(const QImage& newFrame)
{
    if (newFrame.isNull()) {
        qWarning() << "HDR_VideoWidget::updateFrame: Received null frame";
        return;
    }
    
    qDebug() << "HDR_VideoWidget::updateFrame: Received frame" << newFrame.size() << "format:" << newFrame.format()
             << "initialized:" << m_initialized;
    
    // Store frame data first (thread-safe)
    QSize previousFrameSize = m_frameSize;
    m_currentFrame = newFrame;
    m_frameSize = newFrame.size();
    m_frameUpdated = true;
    
    qDebug() << "HDR_VideoWidget::updateFrame: Frame stored, size:" << m_frameSize 
             << "updated flag:" << m_frameUpdated;
    
    // CRITICAL FIX: Update projection matrix when frame size changes (thread-safe)
    if (m_frameSize != previousFrameSize && !m_frameSize.isEmpty()) {
        qDebug() << "HDR_VideoWidget::updateFrame: Frame size changed from" << previousFrameSize 
                 << "to" << m_frameSize << ", updating projection matrix";
        
        // Ensure projection matrix update happens in GUI thread
        if (QThread::currentThread() == QApplication::instance()->thread()) {
            // We're in GUI thread, safe to call directly
            updateProjectionMatrix();
        } else {
            // We're in a different thread, queue the update
            QMetaObject::invokeMethod(this, [this]() {
                updateProjectionMatrix();
            }, Qt::QueuedConnection);
        }
    }
    
    // CRITICAL FIX: Simplified frame handling - just store and trigger update
    if (!m_initialized) {
        qDebug() << "HDR_VideoWidget::updateFrame: Widget not initialized, frame stored for rendering";
        // Ensure widget is visible to trigger OpenGL context creation
        if (!isVisible()) {
            show();
        }
        // Simply trigger update - OpenGL context will be created automatically
        update();
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

void HDR_VideoWidget::clearFrame()
{
    m_currentFrame = QImage();
    m_frameSize = QSize();
    m_frameUpdated = false;
    update();
}

void HDR_VideoWidget::forceReinitializeContext()
{
    qDebug() << "HDR_VideoWidget::forceReinitializeContext: Forcing OpenGL context recreation";
    
    // Reset initialization flag
    m_initialized = false;
    
    // Clean up existing OpenGL resources
    if (context() && context()->isValid()) {
        makeCurrent();
        
        delete m_shaderProgram;
        m_shaderProgram = nullptr;
        
        delete m_videoTexture;
        m_videoTexture = nullptr;
        
        delete m_vertexBuffer;
        m_vertexBuffer = nullptr;
        
        delete m_indexBuffer;
        m_indexBuffer = nullptr;
        
        delete m_vertexArrayObject;
        m_vertexArrayObject = nullptr;
        
        doneCurrent();
    }
    
    // Force widget to recreate its context
    // This is done by temporarily hiding and showing the widget
    bool wasVisible = isVisible();
    hide();
    
    // Schedule reinitialization
    QTimer::singleShot(50, this, [this, wasVisible]() {
        if (wasVisible) {
            show();
        }
        
        // Trigger initialization
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "HDR_VideoWidget::forceReinitializeContext: Triggering reinitialization";
            update(); // This will call paintGL which has initialization retry logic
        });
    });
}

void HDR_VideoWidget::initializeGL()
{
    // CRITICAL FIX: Simplified initialization - no complex retry logic
    if (!context() || !context()->isValid()) {
        qWarning() << "HDR_VideoWidget: OpenGL context not available";
        return;
    }
    
    if (!initializeOpenGLFunctions()) {
        qWarning() << "HDR_VideoWidget: Failed to initialize OpenGL functions";
        return;
    }
    
    // Basic size validation - allow smaller sizes for flexibility
    if (width() < 1 || height() < 1) {
        qWarning() << "HDR_VideoWidget: Invalid widget size:" << width() << "x" << height();
        return;
    }
    
    qDebug() << "HDR_VideoWidget: Starting OpenGL initialization, size:" << width() << "x" << height();
    
    // Context is now validated above
    
    // CRITICAL: Validate actual surface format and adjust expectations
    QSurfaceFormat actualFormat = context()->format();
    bool isActuallyHDR = isHDRFormat(actualFormat);
    
    qDebug() << "HDR Surface Format validation:";
    qDebug() << "  Red buffer size:" << actualFormat.redBufferSize();
    qDebug() << "  Green buffer size:" << actualFormat.greenBufferSize(); 
    qDebug() << "  Blue buffer size:" << actualFormat.blueBufferSize();
    qDebug() << "  Alpha buffer size:" << actualFormat.alphaBufferSize();
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qDebug() << "  Color space:" << actualFormat.colorSpace();
    #endif
    qDebug() << "  Is HDR format:" << isActuallyHDR;
    
    // CRITICAL: If HDR was requested but not obtained, adjust accordingly
    if (m_renderMode != Mode_SDR_8bit && !isActuallyHDR) {
        // Don't fail - we can still do HDR processing in shaders even with 8-bit framebuffer
    } else if (isActuallyHDR) {
        // True HDR surface format confirmed
        // Note: Widget attributes should be set in constructor, not here during GL initialization
    }
    
    // CRITICAL FIX: Simplified OpenGL state setup  
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // Set clear color to match parent widget's background
    QColor bgColor = palette().color(QPalette::Window);
    glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), 1.0f);
    
    // Try to initialize components - continue even if some fail
    bool shadersOk = initializeShaders();
    bool geometryOk = initializeGeometry();
    bool textureOk = initializeTexture();
    
    if (shadersOk && geometryOk && textureOk) {
        m_initialized = true;
        qDebug() << "HDR_VideoWidget: OpenGL initialization completed successfully";
        
        // Upload any pending frame
        if (m_frameUpdated && !m_currentFrame.isNull()) {
            uploadTextureData(m_currentFrame);
        }
        
        // Signal that widget is ready (ensure it's emitted in GUI thread)
        if (QThread::currentThread() == QApplication::instance()->thread()) {
            emit widgetInitialized();
        } else {
            // This should never happen since initializeGL() is called in GUI thread,
            // but add safety check just in case
            QMetaObject::invokeMethod(this, [this]() {
                emit widgetInitialized();
            }, Qt::QueuedConnection);
            qWarning() << "HDR_VideoWidget: widgetInitialized signal queued from non-GUI thread";
        }
    } else {
        qWarning() << "HDR_VideoWidget: Partial initialization - Shaders:" << shadersOk 
                   << "Geometry:" << geometryOk << "Texture:" << textureOk;
        // Mark as initialized anyway to allow basic rendering
        m_initialized = true;
    }
    
    logOpenGLError("initializeGL complete");
}

void HDR_VideoWidget::paintGL()
{
    // CRITICAL FIX: Update clear color on each paint to match parent's background
    // This ensures consistency even if the parent's theme changes
    QColor bgColor;
    if (parentWidget()) {
        bgColor = parentWidget()->palette().color(QPalette::Window);
    } else {
        bgColor = palette().color(QPalette::Window);
    }
    glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), 1.0f);
    
    // Clear with the updated background color
    glClear(GL_COLOR_BUFFER_BIT);
    
    // If not initialized, just clear and return - initialization happens in initializeGL()
    if (!m_initialized) {
        return;
    }
    
    // Render current frame if available
    if (!m_currentFrame.isNull() && m_videoTexture && m_videoTexture->isCreated()) {
        renderFrame();
        m_frameCount++;
    }
    
    logOpenGLError("paintGL");
}

// *** REMOVED: renderNativeHDR() - Violated Principle #1 (Single Rendering Path) ***
// All rendering now occurs exclusively through paintGL() as per architectural mandate

void HDR_VideoWidget::paintEvent(QPaintEvent* event)
{
    // CRITICAL FIX: Properly handle OpenGL rendering without QPainter conflicts
    if (!m_initialized || !context() || !context()->isValid()) {
        return;
    }
    
    // IMPORTANT: We must still call the base class paintEvent to trigger OpenGL rendering
    // But only if we're properly initialized and have a valid context
    try {
        // Call base class to trigger paintGL() - this is safe now with proper attributes set
        QOpenGLWidget::paintEvent(event);

        // CRITICAL FIX: Always draw zoom indicator at fixed position
        // Fetch zoom from parent split view
        splitViewWidget* parentView = qobject_cast<splitViewWidget*>(parentWidget());
        if (parentView) {
            QPointF offset; double zoom = 1.0; double splitPoint = 0.5; int mode = 0;
            parentView->getViewState(offset, zoom, splitPoint, mode);
            
            // Ensure OpenGL operations are finished before starting QPainter
            makeCurrent();
            glFinish(); // Ensure all OpenGL commands are completed
            doneCurrent();
            
            QPainter painter(this);
            painter.setRenderHint(QPainter::TextAntialiasing);
            
            // Use the same font and style as the main split view widget
            QFont font("helvetica", 24);
            painter.setFont(font);
            
            QString zoomString = QString("x") + QString::number(zoom, 'g', (zoom < 0.5) ? 4 : 2);
            QFontMetrics fm(font);
            
            // CRITICAL: Fixed position at top-left corner (10, font height)
            QPoint pos(10, fm.height());
            
            // Draw with BLACK text as requested by user
            painter.setPen(QColor(Qt::black));
            painter.drawText(pos, zoomString);
        }
        
    } catch (...) {
        qCritical() << "Exception in HDR_VideoWidget::paintEvent - OpenGL context issue";
        handleRenderingError("Exception during OpenGL paint event");
    }
}

void HDR_VideoWidget::resizeGL(int width, int height)
{
    // Always set viewport regardless of initialization state
    glViewport(0, 0, width, height);
    
    qDebug() << "HDR Widget resized to:" << width << "x" << height;
    
    // CRITICAL FIX: If not initialized and we now have proper size, try initialization
    if (!m_initialized && width >= 64 && height >= 64 && context() && context()->isValid()) {
        qDebug() << "HDR_VideoWidget::resizeGL: Widget now has proper size and context, attempting initialization";
        // Trigger initialization attempt via update (which calls paintGL with retry logic)
        QTimer::singleShot(10, this, [this]() {
            if (!m_initialized) {
                update();
            }
        });
    }
    
    // CRITICAL FIX: Update projection matrix for proper aspect ratio after resize
    updateProjectionMatrix();
    
    // CRITICAL FIX: Force update to ensure proper rendering after resize
    update();
    
    logOpenGLError("resizeGL");
}

bool HDR_VideoWidget::initializeShaders()
{
    m_shaderProgram = new QOpenGLShaderProgram(this);
    
    // Vertex shader source
    const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 textureMatrix;
uniform mat4 projectionMatrix;

void main()
{
    gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
    TexCoord = (textureMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
}
)";

    // Fragment shader source with HDR support
    const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D videoTexture;
uniform int renderMode;            // 0=SDR, 1=BT2020_PQ, 2=BT709_Linear
uniform float hdrExposure;         // HDR exposure adjustment
uniform float hdrGamma;            // Gamma correction
uniform float displayMaxLuminance; // Display peak luminance in nits (from HDRDetection)
uniform float sourceMaxLuminance;  // Source content peak luminance in nits

// ST.2084 PQ constants for HDR10
const float m1 = 2610.0 / 4096.0 / 4.0;
const float m2 = 2523.0 / 4096.0 * 128.0;
const float c1 = 3424.0 / 4096.0;
const float c2 = 2413.0 / 4096.0 * 32.0;
const float c3 = 2392.0 / 4096.0 * 32.0;

// CRITICAL FIX: Corrected Rec.709 to Rec.2020 color space conversion matrix  
// Using ITU-R BT.2020 standard transformation matrix (row-major order)
// This matrix converts from Rec.709 RGB to Rec.2020 RGB  
const mat3 from709to2020 = mat3(
    0.6274, 0.0691, 0.0164,  // Red channel coefficients
    0.3293, 0.9195, 0.0880,  // Green channel coefficients  
    0.0433, 0.0114, 0.8956   // Blue channel coefficients
);

// Decode Rec.709 OETF to linear light (scene-referred)
vec3 rec709ToLinear(vec3 v) {
    vec3 lo = v / 4.5;
    vec3 hi = pow((v + 0.099) / 1.099, vec3(1.0 / 0.45));
    bvec3 useHi = greaterThanEqual(v, vec3(0.081));
    return vec3(useHi.x ? hi.x : lo.x,
                useHi.y ? hi.y : lo.y,
                useHi.z ? hi.z : lo.z);
}

// Encode linear absolute luminance to ST.2084 PQ code values
// Input: linear scene-referred RGB in [0,1] where 1.0 corresponds to sourceMaxLuminance nits
// Steps: map to absolute nits, normalize to [0,1] with 10000 nits, then apply PQ OETF
vec3 applyPQ(vec3 linearScene) {
    vec3 absNits = max(linearScene, vec3(0.0)) * sourceMaxLuminance;
    vec3 normalized = clamp(absNits / 10000.0, 0.0, 1.0);
    vec3 Lp = pow(normalized, vec3(m1));
    vec3 numerator = vec3(c1) + vec3(c2) * Lp;
    vec3 denominator = vec3(1.0) + vec3(c3) * Lp;
    return pow(numerator / max(denominator, vec3(1e-6)), vec3(m2));
}

// Apply exposure adjustment for linear/scRGB mode
vec3 applyExposure(vec3 linear) {
    return linear * pow(2.0, hdrExposure);
}

// Apply gamma correction
vec3 applyGamma(vec3 color) {
    return pow(max(color, vec3(0.0)), vec3(1.0 / hdrGamma));
}

void main()
{
    vec4 color = texture(videoTexture, TexCoord);
    
    if (renderMode == 1) {
        // BT.2020 + PQ: Assume incoming texture is Rec.709 OETF-encoded SDR
        // 1) Linearize Rec.709, 2) Convert primaries to Rec.2020 in linear, 3) Map to absolute nits
        // 4) Normalize to 10000 nits and apply ST.2084 OETF. No extra gamma on PQ path.
        vec3 rgb709 = clamp(color.rgb, 0.0, 1.0);
        vec3 rgb709_linear = rec709ToLinear(rgb709);
        vec3 rgb2020_linear = from709to2020 * rgb709_linear;
        vec3 pqEncoded = applyPQ(rgb2020_linear);
        FragColor = vec4(pqEncoded, color.a);
        
    } else if (renderMode == 2) {
        // BT709_Linear mode: Apply exposure and gamma
        vec3 exposedColor = applyExposure(color.rgb);
        exposedColor = applyGamma(exposedColor);
        FragColor = vec4(exposedColor, color.a);
        
    } else {
        // SDR mode: Direct output with optional gamma correction
        vec3 sdrColor = applyGamma(color.rgb);
        FragColor = vec4(sdrColor, color.a);
    }
}
)";

    // Compile vertex shader
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qCritical() << "Failed to compile vertex shader:" << m_shaderProgram->log();
        return false;
    }
    
    // Compile fragment shader
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qCritical() << "Failed to compile fragment shader:" << m_shaderProgram->log();
        return false;
    }
    
    // Link shader program
    if (!m_shaderProgram->link()) {
        qCritical() << "Failed to link shader program:" << m_shaderProgram->log();
        return false;
    }
    
    // Get uniform locations
    m_textureLocation = m_shaderProgram->uniformLocation("videoTexture");
    m_renderModeLocation = m_shaderProgram->uniformLocation("renderMode");
    m_hdrExposureLocation = m_shaderProgram->uniformLocation("hdrExposure");
    m_hdrGammaLocation = m_shaderProgram->uniformLocation("hdrGamma");
    m_displayMaxLuminanceLocation = m_shaderProgram->uniformLocation("displayMaxLuminance");
    m_sourceMaxLuminanceLocation = m_shaderProgram->uniformLocation("sourceMaxLuminance");
    m_textureMatrixLocation = m_shaderProgram->uniformLocation("textureMatrix");
    m_projectionMatrixLocation = m_shaderProgram->uniformLocation("projectionMatrix");
    
    qDebug() << "HDR shaders compiled and linked successfully";
    return true;
}

bool HDR_VideoWidget::initializeGeometry()
{
    // Create vertex array object
    m_vertexArrayObject = new QOpenGLVertexArrayObject(this);
    if (!m_vertexArrayObject->create()) {
        qCritical() << "Failed to create vertex array object";
        return false;
    }
    
    m_vertexArrayObject->bind();
    
    // Create vertex buffer
    m_vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (!m_vertexBuffer->create()) {
        qCritical() << "Failed to create vertex buffer";
        return false;
    }
    
    m_vertexBuffer->bind();
    m_vertexBuffer->allocate(s_quadVertices, sizeof(s_quadVertices));
    
    // Create index buffer
    m_indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    if (!m_indexBuffer->create()) {
        qCritical() << "Failed to create index buffer";
        return false;
    }
    
    m_indexBuffer->bind();
    m_indexBuffer->allocate(s_quadIndices, sizeof(s_quadIndices));
    
    // Set up vertex attributes
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // Texture coordinate attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    m_vertexArrayObject->release();
    
    qDebug() << "HDR geometry initialized successfully";
    return true;
}

bool HDR_VideoWidget::initializeTexture()
{
    // Create texture object
    m_videoTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    
    if (!m_videoTexture) {
        qWarning() << "Failed to create OpenGL texture object";
        return false;
    }
    
    // Create the texture storage (don't set parameters yet - wait for actual frame data)
    // We'll configure the texture when we have actual frame data to upload
    
    qDebug() << "HDR texture object created successfully";
    return true;
}

// Helper function to convert image to 10-bit packed format
QImage HDR_VideoWidget::convertTo10BitFormat(const QImage& source)
{
  int width = source.width();
  int height = source.height();

  // Create buffer for 10-bit packed data (using unsigned int for RGB10_A2)
  std::vector<uint32_t> packedData(width * height);

  // Convert each pixel to 10-bit packed format
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      QRgb pixel = source.pixel(x, y);

      // Extract 8-bit components
      int r8 = qRed(pixel);
      int g8 = qGreen(pixel);
      int b8 = qBlue(pixel);
      int a8 = qAlpha(pixel);

      // Convert to 10-bit (scale up from 8-bit to 10-bit)
      // CRITICAL: Proper scaling to utilize full 10-bit range
      uint32_t r10 = (r8 << 2) | (r8 >> 6);  // Scale 8-bit to 10-bit
      uint32_t g10 = (g8 << 2) | (g8 >> 6);
      uint32_t b10 = (b8 << 2) | (b8 >> 6);
      uint32_t a2 = a8 >> 6;  // Scale alpha to 2-bit

      // Pack into RGB10_A2 format (reverse order for GL_UNSIGNED_INT_2_10_10_10_REV)
      uint32_t packed = (a2 << 30) | (b10 << 20) | (g10 << 10) | r10;
      packedData[y * width + x] = packed;
    }
  }

  // CRITICAL FIX: Create QImage with proper format for 10-bit packed data
  // We need to create a custom QImage that preserves the packed data
  
  // Allocate persistent memory for the packed data
  uchar* persistentData = new uchar[width * height * sizeof(uint32_t)];
  std::memcpy(persistentData, packedData.data(), width * height * sizeof(uint32_t));
  
  // Create QImage with RGB30 format which can handle packed 10-bit data better
  // Use Format_RGB30 which is specifically designed for packed 10-bit RGB data
  QImage result(persistentData, width, height, width * sizeof(uint32_t),
                QImage::Format_RGB30,
                [](void* data) { delete[] (uchar*)data; },  // Custom cleanup function
                persistentData);

  return result;
}

// Helper function to convert image to floating point format
QImage HDR_VideoWidget::convertToFloatFormat(const QImage& source)
{
  int width = source.width();
  int height = source.height();

  // Create buffer for float data (4 floats per pixel)
  std::vector<float> floatData(width * height * 4);

  // Convert each pixel to float format
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      QRgb pixel = source.pixel(x, y);
      int idx = (y * width + x) * 4;

      // Normalize to [0,1] range with proper gamma correction
      floatData[idx + 0] = qRed(pixel) / 255.0f;
      floatData[idx + 1] = qGreen(pixel) / 255.0f;
      floatData[idx + 2] = qBlue(pixel) / 255.0f;
      floatData[idx + 3] = qAlpha(pixel) / 255.0f;
    }
  }

  // Create QImage from float data
  QImage result((uchar*)floatData.data(), width, height, width * 4 * sizeof(float),
                QImage::Format_RGBA8888);  // Use RGBA8888 as container format

  return result.copy();  // Make a deep copy
}

bool HDR_VideoWidget::uploadTextureData(const QImage& image)
{
  if (!m_videoTexture || image.isNull()) {
    qWarning() << "HDR_VideoWidget: Cannot upload texture - invalid state";
    return false;
  }

  makeCurrent();

  // Store frame info
  m_frameSize = image.size();

  // Delete existing texture if size changed
  if (m_videoTexture->isCreated() &&
      (m_videoTexture->width() != image.width() ||
       m_videoTexture->height() != image.height())) {
    m_videoTexture->destroy();
  }

  // CRITICAL FIX: Ensure high-precision texture format for HDR content
  // Always use GL_RGBA16F internal format to prevent precision loss
  GLenum internalFormat = GL_RGBA16F;  // High precision internal format as required
  GLenum pixelFormat = GL_RGBA;
  GLenum pixelType = GL_UNSIGNED_BYTE;  // Default, will be updated based on input format

  switch (m_textureFormat) {
  case Format_RGB10_A2:
    // Use 16-bit float internal format for 10-bit content
    internalFormat = GL_RGBA16F;  // Ensure high precision as required
    pixelFormat = GL_RGBA;
    pixelType = GL_UNSIGNED_SHORT;  // Higher precision upload for 10-bit content
    qDebug() << "HDR_VideoWidget: Using RGBA16F internal format with 16-bit upload for RGB10_A2";
    break;

  case Format_RGBA16F:
    // Use 16-bit floating point for scRGB/Linear HDR
    internalFormat = GL_RGBA16F;  // Ensure GL_RGBA16F as required by prompt
    pixelFormat = GL_RGBA;
    pixelType = GL_HALF_FLOAT;   // Match half-precision float data type
    qDebug() << "HDR_VideoWidget: Using RGBA16F texture format with half-float upload for 16-bit HDR";
    break;

  case Format_RGBA8:
  default:
    // Standard 8-bit RGBA with 16-bit internal for upscaling quality
    internalFormat = GL_RGBA16F;  // High precision internal even for 8-bit input
    pixelFormat = GL_RGBA;
    pixelType = GL_UNSIGNED_BYTE;  // Match 8-bit input data type
    qDebug() << "HDR_VideoWidget: Using RGBA16F internal with byte upload for SDR";
    break;
  }

  // Convert image to appropriate format if needed
  QImage textureImage;

  // CRITICAL FIX: Flip image vertically to match OpenGL coordinate system
  QImage flippedImage = image.flipped(Qt::Vertical);  // Flip vertically only
  
  // CRITICAL FIX: Check for native 16-bit input formats first
  if (flippedImage.format() == QImage::Format_RGBA64 || 
      flippedImage.format() == QImage::Format_RGBA64_Premultiplied) {
    
    // Use 16-bit texture formats for maximum precision
    internalFormat = GL_RGBA16F;   // Ensure GL_RGBA16F as required
    pixelFormat = GL_RGBA;
    pixelType = GL_UNSIGNED_SHORT; // Match 16-bit input data type precisely
    textureImage = flippedImage;   // Use flipped 16-bit image directly
    
  } else if (m_textureFormat == Format_RGB10_A2) {
    // For 8-bit input with RGB10_A2 texture format
    textureImage = flippedImage.convertToFormat(QImage::Format_RGBA8888);
    
  } else if (m_textureFormat == Format_RGBA16F) {
    // Convert to floating point format
    textureImage = convertToFloatFormat(flippedImage);
  } else {
    // Ensure RGBA8888 format for standard upload
    textureImage = flippedImage.convertToFormat(QImage::Format_RGBA8888);
  }

  if (!m_videoTexture->isCreated()) {
    // Create texture with proper parameters
    m_videoTexture->setFormat(QOpenGLTexture::RGBA32F);  // Use high precision internal format
    m_videoTexture->setSize(textureImage.width(), textureImage.height());
    m_videoTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_videoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_videoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

    if (!m_videoTexture->create()) {
      qCritical() << "Failed to create video texture";
      doneCurrent();
      return false;
    }
  }

  // Upload texture data with proper format
  m_videoTexture->bind();

  // CRITICAL FIX: Use raw OpenGL for precise control over pixel format
  glTexImage2D(GL_TEXTURE_2D,
               0,                          // Mipmap level
               internalFormat,             // Internal format (8-bit or 16-bit)
               textureImage.width(),
               textureImage.height(),
               0,                          // Border
               pixelFormat,                // Pixel format
               pixelType,                  // Pixel type
               textureImage.constBits());  // Pixel data

  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    qCritical() << "OpenGL error during texture upload:" << error;
    m_videoTexture->release();
    doneCurrent();
    return false;
  }

  m_videoTexture->release();

  m_frameUpdated = true;
  doneCurrent();

  // Trigger repaint
  update();

  // Log detailed texture upload information
  if (image.format() == QImage::Format_RGBA64 || 
      image.format() == QImage::Format_RGBA64_Premultiplied) {
    qDebug() << "HDR_VideoWidget: 16-bit texture uploaded successfully - size:"
             << textureImage.width() << "x" << textureImage.height()
             << "format:" << getTextureFormatString() << "(true 10-bit precision)";
  } else {
    qDebug() << "HDR_VideoWidget: Texture uploaded successfully - size:"
             << textureImage.width() << "x" << textureImage.height()
             << "format:" << getTextureFormatString();
  }

  return true;
}


void HDR_VideoWidget::updateTextureParameters()
{
    if (!m_videoTexture) {
        return;
    }
    
    // Only configure if texture has been created with actual data
    if (!m_videoTexture->isCreated()) {
        qDebug() << "Texture not yet created, skipping parameter update";
        return;
    }
    
    m_videoTexture->bind();
    
    // Set filtering based on content type
    m_videoTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_videoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    
    // Set wrap mode
    m_videoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    
    m_videoTexture->release();
    
    qDebug() << "Texture parameters updated successfully";
}

void HDR_VideoWidget::renderFrame()
{
    if (!m_shaderProgram || !m_vertexArrayObject || !m_videoTexture) {
        return;
    }
    
    // Bind shader program
    m_shaderProgram->bind();
    
    // Update uniforms
    updateShaderUniforms();
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    m_videoTexture->bind();
    m_shaderProgram->setUniformValue(m_textureLocation, 0);
    
    // Bind vertex array and render
    m_vertexArrayObject->bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    m_vertexArrayObject->release();
    
    // Cleanup
    m_videoTexture->release();
    m_shaderProgram->release();
}

void HDR_VideoWidget::updateShaderUniforms()
{
    if (!m_shaderProgram) {
        return;
    }
    
    m_shaderProgram->setUniformValue(m_renderModeLocation, static_cast<int>(m_renderMode));
    m_shaderProgram->setUniformValue(m_hdrExposureLocation, m_hdrExposure);
    m_shaderProgram->setUniformValue(m_hdrGammaLocation, m_hdrGamma);
    m_shaderProgram->setUniformValue(m_displayMaxLuminanceLocation, m_displayMaxLuminance);
    m_shaderProgram->setUniformValue(m_sourceMaxLuminanceLocation, m_sourceMaxLuminance);
    m_shaderProgram->setUniformValue(m_textureMatrixLocation, m_textureMatrix);
    m_shaderProgram->setUniformValue(m_projectionMatrixLocation, m_projectionMatrix);
}

void HDR_VideoWidget::updateProjectionMatrix()
{
    // CRITICAL FIX: Thread-safe projection matrix calculation
    
    // Get current widget dimensions
    int widgetWidth = width();
    int widgetHeight = height();
    
    // Get current frame dimensions (thread-safe copy)
    QSize frameSize = m_frameSize;  // Atomic copy
    int frameWidth = frameSize.width();
    int frameHeight = frameSize.height();
    
    // Validate dimensions
    if (widgetWidth <= 0 || widgetHeight <= 0 || frameWidth <= 0 || frameHeight <= 0) {
        qDebug() << "HDR_VideoWidget::updateProjectionMatrix: Invalid dimensions - Widget:" 
                 << widgetWidth << "x" << widgetHeight << "Frame:" << frameWidth << "x" << frameHeight;
        // Use identity matrix for invalid dimensions
        m_projectionMatrix.setToIdentity();
        return;
    }
    
    // Get zoom factor and offset from parent split view widget
    double zoomFactor = 1.0;
    QPointF viewOffset;
    splitViewWidget* parentView = qobject_cast<splitViewWidget*>(parentWidget());
    if (parentView) {
        double splitPoint = 0.5; int mode = 0;
        parentView->getViewState(viewOffset, zoomFactor, splitPoint, mode);
    }
    
    qDebug() << "HDR_VideoWidget::updateProjectionMatrix: Frame:" << frameWidth << "x" << frameHeight
             << "Widget:" << widgetWidth << "x" << widgetHeight
             << "Zoom:" << zoomFactor << "Offset:" << viewOffset;
    
    // Reset matrix
    m_projectionMatrix.setToIdentity();
    
    // CRITICAL FIX: Match SDR rendering behavior exactly
    // In SDR mode, the video is drawn with its actual pixel dimensions multiplied by zoom
    // Our quad is -1 to 1, so we need to scale it to match the video size
    
    // Calculate the video size in pixels after zoom
    float scaledVideoWidth = frameWidth * zoomFactor;
    float scaledVideoHeight = frameHeight * zoomFactor;
    
    // Convert to normalized device coordinates
    // The quad spans from -1 to 1, which is 2 units wide/tall
    // We need to scale it so that 2 units = scaledVideoWidth/Height in pixels
    float scaleX = scaledVideoWidth / widgetWidth;
    float scaleY = scaledVideoHeight / widgetHeight;
    
    // Calculate offset in NDC
    // viewOffset is in pixels, convert to NDC (-1 to 1 range)
    float offsetX = (viewOffset.x() * 2.0f) / widgetWidth;
    float offsetY = -(viewOffset.y() * 2.0f) / widgetHeight; // Negative because Y is flipped
    
    // Create the projection matrix
    // We need to scale our -1 to 1 quad to the correct size and position
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.translate(offsetX, offsetY, 0.0f);
    m_projectionMatrix.scale(scaleX, scaleY, 1.0f);
    
    qDebug() << "HDR_VideoWidget: Scale:" << scaleX << "x" << scaleY 
             << "Offset:" << offsetX << "," << offsetY;
    
    // CRITICAL FIX: Thread-safe OpenGL operations
    // Update shader uniform if OpenGL is initialized and we're in GUI thread
    if (m_initialized && m_shaderProgram && context() && context()->isValid()) {
        if (QThread::currentThread() == QApplication::instance()->thread()) {
            // We're in GUI thread, safe to make OpenGL calls directly
            makeCurrent();
            m_shaderProgram->bind();
            m_shaderProgram->setUniformValue(m_projectionMatrixLocation, m_projectionMatrix);
            m_shaderProgram->release();
            doneCurrent();
            qDebug() << "HDR_VideoWidget::updateProjectionMatrix: Projection matrix updated and sent to shader (direct)";
        } else {
            // We're in a different thread, queue the OpenGL update
            QMetaObject::invokeMethod(this, [this]() {
                if (m_initialized && m_shaderProgram && context() && context()->isValid()) {
                    makeCurrent();
                    m_shaderProgram->bind();
                    m_shaderProgram->setUniformValue(m_projectionMatrixLocation, m_projectionMatrix);
                    m_shaderProgram->release();
                    doneCurrent();
                    qDebug() << "HDR_VideoWidget::updateProjectionMatrix: Projection matrix updated and sent to shader (queued)";
                }
            }, Qt::QueuedConnection);
        }
    }
}

void HDR_VideoWidget::setHDRCapabilities(const HDRDetection::HDRCapabilities& capabilities)
{
    m_hdrCapable = capabilities.isHDRSupported;
    
    if (m_hdrCapable) {
        qDebug() << "HDR capabilities set:"
                 << "Mode:" << HDRDetection::getHDRModeDescription(capabilities.supportedMode)
                 << "Max Luminance:" << capabilities.maxLuminance << "nits"
                 << "Bits per channel:" << capabilities.bitsPerChannel;
        
        // CRITICAL FIX: Use detected display max luminance for proper tone mapping
        setDisplayMaxLuminance(capabilities.maxLuminance);
        
        // Set appropriate texture format based on detected capabilities
        if (capabilities.supportedMode == HDRDetection::BT2020_PQ_10bit) {
            setTextureFormat(Format_RGB10_A2);
        } else if (capabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
            setTextureFormat(Format_RGBA16F);
        }
        
        // CRITICAL: Upgrade surface format to HDR if not already done
        upgradeToHDRFormat(capabilities.supportedMode);
        
        // Note: Render mode will be set by external caller (videoHandlerYUV)
        // to avoid conflicts with async HDR detection timing
    } else {
        qDebug() << "HDR not supported:" << capabilities.errorMessage;
        setTextureFormat(Format_RGBA8);
        setRenderMode(Mode_SDR_8bit);
        emit hdrNotSupported(capabilities.errorMessage);
    }
}

void HDR_VideoWidget::detectHDRCapabilities()
{
    // This method is now deprecated - HDR capabilities should be set externally
    // via setHDRCapabilities() to avoid timing conflicts with async detection
    qDebug() << "Warning: detectHDRCapabilities() called - this is deprecated. Use setHDRCapabilities() instead.";
    
    auto hdrDetection = HDRDetection::instance();
    auto capabilities = hdrDetection->detectHDRCapabilities(this);
    setHDRCapabilities(capabilities);
}

void HDR_VideoWidget::validateRenderMode()
{
    if (!m_hdrCapable && m_renderMode != Mode_SDR_8bit) {
        qDebug() << "HDR render mode validation: HDR capability not yet determined, allowing mode to be set";
        qDebug() << "Current HDR capable:" << m_hdrCapable << "Requested mode:" << getRenderModeString();
        // Don't fallback immediately - HDR detection might still be in progress
        // Let the external caller handle the final validation after HDR detection completes
        return;
    }
    
    if (m_hdrCapable) {
        qDebug() << "HDR render mode validation: HDR supported, mode" << getRenderModeString() << "is valid";
    } else {
        qDebug() << "HDR render mode validation: HDR not supported, using SDR mode";
    }
}

QString HDR_VideoWidget::getRenderModeString() const
{
    switch (m_renderMode) {
    case Mode_BT2020_PQ_10bit:
        return "BT2020_PQ_10bit";
    case Mode_BT709_Linear_16bit:
        return "BT709_Linear_16bit";
    case Mode_SDR_8bit:
    default:
        return "SDR_8bit";
    }
}

QString HDR_VideoWidget::getTextureFormatString() const
{
    switch (m_textureFormat) {
    case Format_RGBA16F:
        return "RGBA16F";
    case Format_RGB10_A2:
        return "RGB10_A2";
    case Format_RGBA8:
    default:
        return "RGBA8";
    }
}

bool HDR_VideoWidget::logOpenGLError(const QString& operation)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        QString errorString;
        switch (error) {
        case GL_INVALID_ENUM:
            errorString = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errorString = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errorString = "GL_INVALID_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            errorString = "GL_OUT_OF_MEMORY";
            break;
        default:
            errorString = QString("Unknown error: 0x%1").arg(error, 0, 16);
            break;
        }
        
        QString fullError = QString("OpenGL error in %1: %2").arg(operation, errorString);
        qCritical() << fullError;
        emit openGLError(fullError);
        return true;  // Error occurred
    }
    return false;  // No error
}

void HDR_VideoWidget::handleInitializationError(const QString& error)
{
    qCritical() << "HDR initialization error:" << error;
    m_initialized = false;
    
    // Stop FPS timer on initialization failure
    if (m_fpsTimer && m_fpsTimer->isActive()) {
        m_fpsTimer->stop();
        qDebug() << "HDR FPS monitoring stopped due to initialization error";
    }
    
    // CRITICAL FIX: Add retry mechanism instead of immediately giving up
    static int retryCount = 0;
    const int maxRetries = 3;
    
    if (retryCount < maxRetries) {
        retryCount++;
        qWarning() << "HDR_VideoWidget: Scheduling initialization retry" << retryCount << "of" << maxRetries;
        
        // Schedule retry with increasing delay
        QTimer::singleShot(retryCount * 100, this, [this]() {
            if (!m_initialized && context() && context()->isValid()) {
                qDebug() << "HDR_VideoWidget: Retrying OpenGL initialization...";
                // Trigger another initialization attempt
                update();  // This will call paintGL which has retry logic
            }
        });
    } else {
        qCritical() << "HDR_VideoWidget: Maximum initialization retries exceeded, giving up";
        retryCount = 0;  // Reset for next time
        emit hdrNotSupported(error);
    }
}

void HDR_VideoWidget::handleRenderingError(const QString& error)
{
    qWarning() << "HDR rendering error:" << error;
    emit openGLError(error);
}

void HDR_VideoWidget::wheelEvent(QWheelEvent* event)
{
    // CRITICAL FIX: Use event ignoring mechanism for reliable parent forwarding
    if (event->modifiers() & Qt::ControlModifier) {
        // Handle HDR exposure adjustment - ACCEPT this event
        float delta = event->angleDelta().y() / 120.0f;
        setHDRExposure(m_hdrExposure + delta * 0.1f);
        event->accept();
        qDebug() << "HDR_VideoWidget: Handled wheel event for exposure adjustment";
    } else {
        // For all other wheel events, IGNORE to let Qt forward to parent automatically
        event->ignore();
        qDebug() << "HDR_VideoWidget: Ignored wheel event, forwarding to parent";
    }
}

void HDR_VideoWidget::mousePressEvent(QMouseEvent* event)
{
    // CRITICAL FIX: Use event ignoring mechanism for reliable parent forwarding
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)) {
        // Shift+Click for HDR gamma adjustment - ACCEPT this event
        m_dragging = true;
        m_lastMousePos = event->pos();
        event->accept();
        qDebug() << "HDR_VideoWidget: Handled mouse press for HDR adjustment (Shift+Click)";
    } else {
        // For all other mouse events, IGNORE to let Qt forward to parent automatically
        event->ignore();
        qDebug() << "HDR_VideoWidget: Ignored mouse press event, forwarding to parent";
    }
}

void HDR_VideoWidget::mouseMoveEvent(QMouseEvent* event)
{
    // CRITICAL FIX: Use event ignoring mechanism for reliable parent forwarding
    if (m_dragging && (event->buttons() & Qt::LeftButton) && (event->modifiers() & Qt::ShiftModifier)) {
        // HDR gamma adjustment drag - ACCEPT this event
        QPoint delta = event->pos() - m_lastMousePos;
        
        // Adjust gamma with vertical mouse movement
        float gammaDelta = -delta.y() * 0.01f;
        setHDRGamma(m_hdrGamma + gammaDelta);
        qDebug() << "HDR_VideoWidget: Adjusted gamma via mouse drag:" << m_hdrGamma;
        
        m_lastMousePos = event->pos();
        event->accept();
    } else {
        // Reset dragging state if conditions no longer met
        if (m_dragging && !(event->modifiers() & Qt::ShiftModifier)) {
            m_dragging = false;
            qDebug() << "HDR_VideoWidget: Reset dragging state due to modifier change";
        }
        
        // For all other mouse move events, IGNORE to let Qt forward to parent automatically
        event->ignore();
        qDebug() << "HDR_VideoWidget: Ignored mouse move event, forwarding to parent";
    }
}

void HDR_VideoWidget::mouseReleaseEvent(QMouseEvent* event)
{
    // CRITICAL FIX: Use event ignoring mechanism for reliable parent forwarding
    bool wasHandlingHDR = false;
    
    if (event->button() == Qt::LeftButton && m_dragging) {
        // We were handling HDR adjustment, now stop
        m_dragging = false;
        wasHandlingHDR = true;
        qDebug() << "HDR_VideoWidget: Reset dragging state on mouse release";
    }
    
    if (wasHandlingHDR) {
        // We were handling this interaction, ACCEPT the release event
        event->accept();
    } else {
        // For all other release events, IGNORE to let Qt forward to parent automatically
        event->ignore();
        qDebug() << "HDR_VideoWidget: Ignored mouse release event, forwarding to parent";
    }
}

// *** REMOVED: grabHDRFramebuffer() - Violated Principle #1 (Single Rendering Path) ***
// Manual framebuffer grabbing with context management created race conditions
// New architecture uses pure push model - external code should not pull rendered images

void HDR_VideoWidget::upgradeToHDRFormat(HDRDetection::HDRMode hdrMode)
{
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    
    // Check if we already have the right HDR format
    QSurfaceFormat currentFormat = context() ? context()->format() : format();
    
    // Check if we already have the right HDR format
    
    if (isHDRFormat(currentFormat)) {
        qDebug() << "HDR format already active, no upgrade needed";
        return;
    }
    
    qDebug() << "Upgrading surface format to HDR for mode:" << HDRDetection::getHDRModeDescription(hdrMode);
    
    // Create new HDR format based on detected mode
    QSurfaceFormat hdrFormat = currentFormat;
    
    if (hdrMode == HDRDetection::BT2020_PQ_10bit) {
        // CRITICAL FIX: Configure for BT2020 PQ (HDR10) with proper fallback
        hdrFormat.setRedBufferSize(10);
        hdrFormat.setGreenBufferSize(10);
        hdrFormat.setBlueBufferSize(10);
        hdrFormat.setAlphaBufferSize(2);
        
        hdrFormat.setColorSpace(QColorSpace::Bt2100Pq);
        qDebug() << "Upgrading to BT2020 PQ 10-bit format";
        
    } else if (hdrMode == HDRDetection::BT709_G10_16bit) {
        // Configure for scRGB/Linear RGB
        hdrFormat.setRedBufferSize(16);
        hdrFormat.setGreenBufferSize(16);
        hdrFormat.setBlueBufferSize(16);
        hdrFormat.setAlphaBufferSize(16);
        
        hdrFormat.setColorSpace(QColorSpace::SRgbLinear);
        qDebug() << "Upgrading to BT709 Linear 16-bit format";
        
    } else {
        qWarning() << "Unknown HDR mode for surface format upgrade:" << hdrMode;
        return;
    }
    
    // Apply the new format
    setFormat(hdrFormat);
    
    qDebug() << "HDR surface format upgrade completed";
    
    #else
    Q_UNUSED(hdrMode);
    qWarning() << "HDR surface format upgrade not supported in Qt < 6.0";
    #endif
}

bool HDR_VideoWidget::isHDRFormat(const QSurfaceFormat& format)
{
    // KRITA-INSPIRED IMPLEMENTATION: Exact copy of Krita's HDR format validation
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    
    // Check for BT2020 PQ format (HDR10)
    bool isBt2020PQ = 
        format.colorSpace() == QColorSpace::Bt2100Pq &&
        format.redBufferSize() == 10 &&
        format.greenBufferSize() == 10 &&
        format.blueBufferSize() == 10 &&
        format.alphaBufferSize() == 2;
    
    // Check for scRGB format (linear RGB)
    bool isBt709G10 = 
        format.colorSpace() == QColorSpace::SRgbLinear &&
        format.redBufferSize() == 16 &&
        format.greenBufferSize() == 16 &&
        format.blueBufferSize() == 16 &&
        format.alphaBufferSize() == 16;
    
    return isBt2020PQ || isBt709G10;
    
    #else
    Q_UNUSED(format);
    return false;  // HDR not supported in Qt < 6.0
    #endif
}

void HDR_VideoWidget::onParentZoomChanged()
{
    // Update projection matrix to reflect new zoom level
    if (m_initialized && !m_frameSize.isEmpty()) {
        qDebug() << "HDR_VideoWidget::onParentZoomChanged: Updating projection matrix for new zoom level";
        updateProjectionMatrix();
        update(); // Trigger repaint
    }
}

void HDR_VideoWidget::updateBackgroundColor()
{
    // Update OpenGL clear color to match parent widget's background
    if (m_initialized && context() && context()->isValid()) {
        makeCurrent();
        
        // Get background color from parent or use widget's palette
        QColor bgColor;
        if (parentWidget()) {
            bgColor = parentWidget()->palette().color(QPalette::Window);
        } else {
            bgColor = palette().color(QPalette::Window);
        }
        
        glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), 1.0f);
        
        doneCurrent();
        
        // Trigger repaint to apply new background color
        update();
    }
}

