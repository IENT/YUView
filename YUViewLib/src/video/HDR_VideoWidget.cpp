#include "HDR_VideoWidget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QApplication>
#include <QElapsedTimer>
#include <cmath>

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
    , m_shaderProgram(nullptr)
    , m_videoTexture(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_vertexArrayObject(nullptr)
    , m_textureLocation(-1)
    , m_renderModeLocation(-1)
    , m_hdrExposureLocation(-1)
    , m_hdrGammaLocation(-1)
    , m_textureMatrixLocation(-1)
    , m_projectionMatrixLocation(-1)
    , m_frameUpdated(false)
    , m_fpsTimer(nullptr)
    , m_frameCount(0)
    , m_lastFpsUpdate(0)
    , m_dragging(false)
{
    // Set up the widget for OpenGL rendering
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    
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
            makeCurrent();
            updateShaderUniforms();
            doneCurrent();
            update();
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
            makeCurrent();
            updateTextureParameters();
            doneCurrent();
            update();
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
            makeCurrent();
            updateShaderUniforms();
            doneCurrent();
            update();
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
            makeCurrent();
            updateShaderUniforms();
            doneCurrent();
            update();
        }
        
        qDebug() << "HDR gamma set to:" << gamma;
    }
}

void HDR_VideoWidget::updateFrame(const QImage& newFrame)
{
    if (newFrame.isNull()) {
        qWarning() << "HDR_VideoWidget::updateFrame: Received null frame";
        return;
    }
    
    m_currentFrame = newFrame;
    m_frameSize = newFrame.size();
    m_frameUpdated = true;
    
    if (m_initialized) {
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
        // Queue the frame for when we're initialized
        update();
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
    
    // Detect HDR capabilities
    detectHDRCapabilities();
    
    // Initialize OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
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
uniform int renderMode;        // 0=SDR, 1=BT2020_PQ, 2=BT709_Linear
uniform float hdrExposure;     // HDR exposure adjustment
uniform float hdrGamma;        // Gamma correction

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

// Apply ST.2084 PQ transfer function (EOTF)
vec3 applyPQ(vec3 linear) {
    // Normalize to [0,1] range for PQ curve
    vec3 normalizedLinear = linear / 10000.0;
    
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
    
    // Set texture parameters based on current format
    updateTextureParameters();
    
    qDebug() << "HDR texture initialized with format:" << getTextureFormatString();
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
    
    // Allocate or update texture storage
    if (m_videoTexture->width() != textureImage.width() || 
        m_videoTexture->height() != textureImage.height()) {
        
        m_videoTexture->setSize(textureImage.width(), textureImage.height());
        m_videoTexture->setFormat(QOpenGLTexture::TextureFormat(internalFormat));
        m_videoTexture->allocateStorage();
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
    
    m_videoTexture->bind();
    
    // Set filtering based on content type
    m_videoTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_videoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    
    // Set wrap mode
    m_videoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    
    m_videoTexture->release();
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
    m_shaderProgram->setUniformValue(m_textureMatrixLocation, m_textureMatrix);
    m_shaderProgram->setUniformValue(m_projectionMatrixLocation, m_projectionMatrix);
}

void HDR_VideoWidget::detectHDRCapabilities()
{
    auto hdrDetection = HDRDetection::instance();
    auto capabilities = hdrDetection->detectHDRCapabilities(this);
    
    m_hdrCapable = capabilities.isHDRSupported;
    
    if (m_hdrCapable) {
        qDebug() << "HDR capabilities detected:"
                 << "Mode:" << HDRDetection::getHDRModeDescription(capabilities.supportedMode)
                 << "Max Luminance:" << capabilities.maxLuminance << "nits"
                 << "Bits per channel:" << capabilities.bitsPerChannel;
        
        // Set appropriate texture format based on detected capabilities
        if (capabilities.supportedMode == HDRDetection::BT2020_PQ_10bit) {
            setTextureFormat(Format_RGB10_A2);
            setRenderMode(Mode_BT2020_PQ_10bit);
        } else if (capabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
            setTextureFormat(Format_RGBA16F);
            setRenderMode(Mode_BT709_Linear_16bit);
        }
    } else {
        qDebug() << "HDR not supported:" << capabilities.errorMessage;
        setTextureFormat(Format_RGBA8);
        setRenderMode(Mode_SDR_8bit);
        emit hdrNotSupported(capabilities.errorMessage);
    }
}

void HDR_VideoWidget::validateRenderMode()
{
    if (!m_hdrCapable && m_renderMode != Mode_SDR_8bit) {
        qWarning() << "HDR render mode requested but HDR not supported, falling back to SDR";
        m_renderMode = Mode_SDR_8bit;
        emit hdrNotSupported("HDR render mode not supported on this display");
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

