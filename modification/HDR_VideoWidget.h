// HDR_VideoWidget.h - Updated header with initialization improvements

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QImage>
#include <QMatrix4x4>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>

#include "HDRDetection.h"

/**
 * @brief OpenGL widget for HDR video rendering
 * 
 * This widget handles all OpenGL-based HDR rendering operations.
 * It supports multiple HDR modes including BT.2020 PQ and BT.709 Linear.
 */
class HDR_VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    // Texture formats supported by the widget
    enum TextureFormat {
        Format_SDR_8bit,      // Standard 8-bit RGBA
        Format_SDR_16bit,     // 16-bit floating point RGBA
        Format_RGB10_A2,      // 10-bit RGB with 2-bit alpha (HDR10)
        Format_RGBA16F        // 16-bit floating point RGBA (scRGB)
    };
    
    // Rendering modes
    enum RenderMode {
        Mode_SDR,                   // Standard Dynamic Range
        Mode_BT2020_PQ_10bit,      // BT.2020 with PQ transfer (HDR10)
        Mode_BT709_Linear_16bit     // BT.709 with linear transfer (scRGB)
    };

    explicit HDR_VideoWidget(QWidget* parent = nullptr);
    ~HDR_VideoWidget() override;

    // Frame management
    void updateFrame(const QImage& newFrame);
    void clearFrame();
    
    // HDR configuration
    void setRenderMode(RenderMode mode);
    RenderMode getRenderMode() const { return m_renderMode; }
    
    void setTextureFormat(TextureFormat format);
    TextureFormat getTextureFormat() const { return m_textureFormat; }
    
    // HDR parameters
    void setHDRExposure(float exposure);
    float getHDRExposure() const { return m_hdrExposure; }
    
    void setHDRGamma(float gamma);
    float getHDRGamma() const { return m_hdrGamma; }
    
    // Luminance mapping
    void setDisplayMaxLuminance(float maxLuminance);
    float getDisplayMaxLuminance() const { return m_displayMaxLuminance; }
    
    void setSourceMaxLuminance(float maxLuminance);
    float getSourceMaxLuminance() const { return m_sourceMaxLuminance; }
    
    // Status queries
    bool isHDRCapable() const { return m_hdrCapable; }
    bool isInitialized() const { return m_initialized; }
    bool isReadyForRendering() const;  // NEW: Check if widget is ready
    
    // HDR capability configuration
    void setHDRCapabilities(const HDRDetection::HDRCapabilities& capabilities);
    
public slots:
    // HDR control slots
    void setHDRExposureSlot(double exposure) { setHDRExposure(static_cast<float>(exposure)); }
    void setHDRGammaSlot(double gamma) { setHDRGamma(static_cast<float>(gamma)); }

signals:
    // NEW: Emitted when widget is fully initialized and ready
    void widgetInitialized();
    
    // Emitted when HDR is not supported and fallback is needed
    void hdrNotSupported(const QString& reason);
    
    // Emitted when rendering mode changes
    void renderModeChanged(RenderMode mode);
    
    // Emitted when frame is updated successfully
    void frameUpdated();
    
    // Emitted on OpenGL errors
    void openGLError(const QString& error);

protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    
    // Event handlers
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    
private:
    // Initialization helpers
    bool initializeShaders();
    bool initializeGeometry();
    bool initializeTexture();
    void validateHDRSupport();
    
    // Rendering helpers
    void renderFrame();
    void updateShaderUniforms();
    bool uploadTextureData(const QImage& image);
    
    // Error handling
    bool logOpenGLError(const QString& context);
    void handleInitializationError(const QString& error);
    void handleRenderingError(const QString& error);
    
    // OpenGL resources
    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLVertexArrayObject* m_vertexArrayObject;
    QOpenGLBuffer* m_vertexBuffer;
    QOpenGLBuffer* m_indexBuffer;
    QOpenGLTexture* m_videoTexture;
    
    // Shader uniform locations
    int m_textureLocation;
    int m_renderModeLocation;
    int m_hdrExposureLocation;
    int m_hdrGammaLocation;
    int m_displayMaxLuminanceLocation;
    int m_sourceMaxLuminanceLocation;
    int m_textureMatrixLocation;
    int m_projectionMatrixLocation;
    
    // Matrices
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_textureMatrix;
    
    // Frame data
    QImage m_currentFrame;
    QSize m_frameSize;
    bool m_frameUpdated;
    
    // HDR parameters
    TextureFormat m_textureFormat;
    RenderMode m_renderMode;
    bool m_hdrCapable;
    float m_displayMaxLuminance;
    float m_sourceMaxLuminance;
    float m_hdrExposure;
    float m_hdrGamma;
    
    // Initialization state
    bool m_initialized;
    
    // Mouse interaction
    bool m_dragging;
    QPoint m_lastMousePos;
    
    // FPS monitoring
    QTimer* m_fpsTimer;
    qint64 m_frameCount;
    qint64 m_lastFpsUpdate;
};
