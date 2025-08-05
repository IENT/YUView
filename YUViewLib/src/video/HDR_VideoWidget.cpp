#include "HDR_VideoWidget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QApplication>
#include <QElapsedTimer>
#include <QColorSpace>  // For Qt 6 HDR color space support
#include <cmath>
#include <vector>  // For 10-bit framebuffer grabbing

// Vertex data for full-screen quad (position + texture coordinates)
const float HDR_VideoWidget::s_quadVertices[] = {
    // Positions   // Texture Coords
    -1.0f, -1.0f,  0.0f, 0.0f,   // Bottom Left
     1.0f, -1.0f,  1.0f, 0.0f,   // Bottom Right
     1.0f,  1.0f,  1.0f, 1.0f,   // Top Right
    -1.0f,  1.0f,  0.0f, 1.0f    // Top Left
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
    , m_displayMaxLuminance(100.0f)  // Default SDR display
    , m_sourceMaxLuminance(1000.0f)  // Default HDR content assumption
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
    // CRITICAL FIX: Proper HDR Surface Format configuration based on Krita implementation
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(0);  // Disable multisampling for stability
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    
    // KRITA-INSPIRED FIX: Configure for true HDR10 (BT.2020 PQ) support
    // This matches Krita's BT2020_PQ configuration exactly
    format.setRedBufferSize(10);
    format.setGreenBufferSize(10);
    format.setBlueBufferSize(10);
    format.setAlphaBufferSize(2);
    
    // CRITICAL: Set HDR color space for true 10-bit display (Qt 6)
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    format.setColorSpace(QColorSpace::Bt2100Pq);  // HDR10/BT.2020 PQ
    qDebug() << "HDR Surface Format: BT2100Pq color space configured";
    #else
    qWarning() << "Qt version < 6.0, HDR color space not available";
    #endif
    
    setFormat(format);
    setMinimumSize(64, 64);  // Minimum size to ensure valid context
    
    // Set up the widget for OpenGL rendering
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    
    // Configure for HDR rendering (modified attributes)
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    // REMOVED: setAttribute(Qt::WA_PaintOnScreen, false) to allow proper context
    
    // Initialize transformation matrices
    m_textureMatrix.setToIdentity();
    m_projectionMatrix.setToIdentity();
    
    // Set up FPS monitoring timer (but don't start it yet)
    m_fpsTimer = new QTimer(this);
    connect(m_fpsTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "HDR Video Widget FPS:" << m_frameCount << "frames/sec";
        m_frameCount = 0;
    });
    // Note: Timer will be started only when HDR rendering is successfully initialized
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
                qDebug() << "HDR FPS monitoring started";
            }
        } else {
            // SDR mode - stop FPS monitoring
            if (m_fpsTimer->isActive()) {
                m_fpsTimer->stop();
                qDebug() << "HDR FPS monitoring stopped";
            }
        }
        
        emit renderModeChanged(mode);
        qDebug() << "HDR render mode changed to:" << getRenderModeString();
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
        
        qDebug() << "HDR texture format changed to:" << getTextureFormatString();
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
        
        qDebug() << "HDR exposure set to:" << exposure;
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
        
        qDebug() << "HDR gamma set to:" << gamma;
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
        
        qDebug() << "Display max luminance set to:" << maxLuminance << "nits";
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
        
        qDebug() << "Source max luminance set to:" << maxLuminance << "nits";
    }
}

void HDR_VideoWidget::updateFrame(const QImage& newFrame)
{
    if (newFrame.isNull()) {
        qWarning() << "HDR_VideoWidget::updateFrame: Received null frame";
        return;
    }
    
    // Store frame data first (thread-safe)
    m_currentFrame = newFrame;
    m_frameSize = newFrame.size();
    m_frameUpdated = true;
    
    // Use QMetaObject::invokeMethod to ensure OpenGL operations happen in the main thread
    if (m_initialized) {
        // Queue the texture upload for the main thread to avoid OpenGL context issues
        QMetaObject::invokeMethod(this, [this, newFrame]() {
            if (context() && context()->isValid()) {
                makeCurrent();
                if (uploadTextureData(newFrame)) {
                    doneCurrent();
                    update();
                    emit frameUpdated();
                    m_frameCount++;
                } else {
                    doneCurrent();
                    handleRenderingError("Failed to upload texture data");
                }
            } else {
                handleRenderingError("OpenGL context is not valid for HDR frame update");
            }
        }, Qt::QueuedConnection);
    } else {
        // Queue the frame for when we're initialized
        QMetaObject::invokeMethod(this, [this]() {
            update();
        }, Qt::QueuedConnection);
    }
}

void HDR_VideoWidget::clearFrame()
{
    m_currentFrame = QImage();
    m_frameSize = QSize();
    m_frameUpdated = false;
    update();
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
    
    // KRITA-INSPIRED FIX: Validate actual HDR surface format
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
    
    if (m_renderMode != Mode_SDR_8bit) {
        if (isActuallyHDR) {
            qDebug() << "SUCCESS: True HDR surface format confirmed!";
        } else {
            qWarning() << "WARNING: HDR mode requested but surface format is not HDR-capable";
            qWarning() << "This may result in 8-bit display despite 10-bit content";
        }
    }
    
    // Initialize OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // Check for OpenGL errors after state setup
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        handleInitializationError(QString("OpenGL state setup failed: %1").arg(error));
        return;
    }
    
    // Initialize shaders
    if (!initializeShaders()) {
        handleInitializationError("Failed to initialize shaders");
        return;
    }
    
    // Initialize geometry
    if (!initializeGeometry()) {
        handleInitializationError("Failed to initialize geometry");
        return;
    }
    
    // Initialize texture
    if (!initializeTexture()) {
        handleInitializationError("Failed to initialize texture");
        return;
    }
    
    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    m_initialized = true;
    qDebug() << "HDR Video Widget initialized successfully";
    
    // Upload any pending frame
    if (m_frameUpdated && !m_currentFrame.isNull()) {
        uploadTextureData(m_currentFrame);
    }
    
    logOpenGLError("initializeGL");
}

void HDR_VideoWidget::paintGL()
{
    if (!m_initialized) {
        return;
    }
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (!m_currentFrame.isNull() && m_videoTexture) {
        renderFrame();
    }
    
    logOpenGLError("paintGL");
}

// *** REMOVED: renderNativeHDR() - Violated Principle #1 (Single Rendering Path) ***
// All rendering now occurs exclusively through paintGL() as per architectural mandate

void HDR_VideoWidget::paintEvent(QPaintEvent* event)
{
    // STABILITY FIX: Minimal paintEvent to avoid recursion
    Q_UNUSED(event);
    
    // Only use standard OpenGL rendering, no custom behavior
    if (m_initialized) {
        QOpenGLWidget::paintEvent(event);
    }
}

void HDR_VideoWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
    
    // Update projection matrix for proper aspect ratio
    m_projectionMatrix.setToIdentity();
    
    if (m_initialized && m_shaderProgram) {
        m_shaderProgram->bind();
        m_shaderProgram->setUniformValue(m_projectionMatrixLocation, m_projectionMatrix);
        m_shaderProgram->release();
    }
    
    qDebug() << "HDR Widget resized to:" << width << "x" << height;
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

// Rec.709 to Rec.2020 color space conversion matrix
const mat3 from709to2020 = mat3(
    0.6274040, 0.3292820, 0.0433136,
    0.0690970, 0.9195400, 0.0113612,
    0.0163916, 0.0880132, 0.8955950
);

// Apply ST.2084 PQ transfer function with proper physical tone mapping
vec3 applyPQ(vec3 linear) {
    // First, tone map from source peak luminance to display peak luminance
    vec3 toneMappedLinear = linear * (displayMaxLuminance / sourceMaxLuminance);
    
    // Normalize to [0,1] range for PQ curve using display peak luminance
    vec3 normalizedLinear = toneMappedLinear / displayMaxLuminance;
    
    // Apply PQ curve
    vec3 Lp = pow(max(normalizedLinear, vec3(0.0)), vec3(m1));
    vec3 numerator = c1 + c2 * Lp;
    vec3 denominator = 1.0 + c3 * Lp;
    
    return pow(max(numerator / max(denominator, vec3(0.0001)), vec3(0.0)), vec3(m2));
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
        // BT2020_PQ mode: Convert to Rec.2020 and apply PQ curve
        vec3 rec2020Color = from709to2020 * color.rgb;
        vec3 pqColor = applyPQ(rec2020Color);
        pqColor = applyGamma(pqColor);
        FragColor = vec4(pqColor, color.a);
        
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

bool HDR_VideoWidget::uploadTextureData(const QImage& image)
{
    if (!m_videoTexture || image.isNull()) {
        return false;
    }
    
    // Convert image to appropriate format
    QImage textureImage;
    
    switch (m_textureFormat) {
    case Format_RGBA16F:
        // For HDR modes, ensure we have high precision format
        if (image.format() != QImage::Format_RGBA64 && 
            image.format() != QImage::Format_RGBA64_Premultiplied) {
            textureImage = image.convertToFormat(QImage::Format_RGBA64);
        } else {
            textureImage = image;
        }
        break;
        
    case Format_RGB10_A2:
        // Convert to RGB30 if available, otherwise RGBA64
        if (image.format() != QImage::Format_RGB30) {
            textureImage = image.convertToFormat(QImage::Format_RGBA64);
        } else {
            textureImage = image;
        }
        break;
        
    case Format_RGBA8:
    default:
        // Standard 8-bit RGBA
        textureImage = image.convertToFormat(QImage::Format_RGBA8888);
        break;
    }
    
    // Bind texture and upload data
    m_videoTexture->bind();
    
    GLenum internalFormat, format, type;
    
    switch (m_textureFormat) {
    case Format_RGBA16F:
        internalFormat = GL_RGBA16F;
        format = GL_RGBA;
        type = GL_UNSIGNED_SHORT;
        break;
        
    case Format_RGB10_A2:
        internalFormat = GL_RGB10_A2;
        format = GL_RGBA;
        type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
        
    case Format_RGBA8:
    default:
        internalFormat = GL_RGBA8;
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        break;
    }
    
    // Create or recreate texture storage as needed
    bool needsReallocation = !m_videoTexture->isCreated() ||
                            m_videoTexture->width() != textureImage.width() || 
                            m_videoTexture->height() != textureImage.height();
    
    if (needsReallocation) {
        qDebug() << "Creating/recreating texture storage:" 
                 << textureImage.width() << "x" << textureImage.height()
                 << "format:" << internalFormat;
        
        // Destroy old texture if it exists
        if (m_videoTexture->isCreated()) {
            m_videoTexture->destroy();
        }
        
        m_videoTexture->setSize(textureImage.width(), textureImage.height());
        m_videoTexture->setFormat(QOpenGLTexture::TextureFormat(internalFormat));
        
        if (!m_videoTexture->create()) {
            qWarning() << "Failed to create OpenGL texture";
            m_videoTexture->release();
            return false;
        }
        
        // Allocate texture storage (allocateStorage() returns void)
        m_videoTexture->allocateStorage();
        
        // Check for OpenGL errors after allocation
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            qWarning() << "OpenGL error during texture allocation:" << error;
            m_videoTexture->release();
            return false;
        }
        
        // Set texture parameters now that it's created
        updateTextureParameters();
    }
    
    // Upload pixel data
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    textureImage.width(), textureImage.height(),
                    format, type, textureImage.constBits());
    
    m_videoTexture->release();
    
    logOpenGLError("uploadTextureData");
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

void HDR_VideoWidget::logOpenGLError(const QString& operation)
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
    }
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
    
    emit hdrNotSupported(error);
}

void HDR_VideoWidget::handleRenderingError(const QString& error)
{
    qWarning() << "HDR rendering error:" << error;
    emit openGLError(error);
}

void HDR_VideoWidget::wheelEvent(QWheelEvent* event)
{
    // Handle mouse wheel for exposure adjustment
    if (event->modifiers() & Qt::ControlModifier) {
        float delta = event->angleDelta().y() / 120.0f; // Standard wheel step
        setHDRExposure(m_hdrExposure + delta * 0.1f);
        event->accept();
    } else {
        QOpenGLWidget::wheelEvent(event);
    }
}

void HDR_VideoWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        event->accept();
    } else {
        QOpenGLWidget::mousePressEvent(event);
    }
}

void HDR_VideoWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->pos() - m_lastMousePos;
        
        // Adjust gamma with vertical mouse movement
        if (event->modifiers() & Qt::ShiftModifier) {
            float gammaDelta = -delta.y() * 0.01f;
            setHDRGamma(m_hdrGamma + gammaDelta);
        }
        
        m_lastMousePos = event->pos();
        event->accept();
    } else {
        QOpenGLWidget::mouseMoveEvent(event);
    }
}

// *** REMOVED: grabHDRFramebuffer() - Violated Principle #1 (Single Rendering Path) ***
// Manual framebuffer grabbing with context management created race conditions
// New architecture uses pure push model - external code should not pull rendered images

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

