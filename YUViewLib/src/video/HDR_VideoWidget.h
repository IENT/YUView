#ifndef HDR_VIDEOWIDGET_H
#define HDR_VIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QImage>
#include <QTimer>

#include "HDRDetection.h"

class HDR_VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    enum RenderMode {
        Mode_SDR_8bit = 0,           // Standard 8-bit SDR rendering
        Mode_BT2020_PQ_10bit = 1,    // 10-bit HDR10/BT.2020 PQ rendering  
        Mode_BT709_Linear_16bit = 2  // 16-bit scRGB/Rec.709 Linear rendering
    };
    
    enum TextureFormat {
        Format_RGBA8 = 0,     // Standard 8-bit RGBA
        Format_RGBA16F = 1,   // 16-bit floating point RGBA (HDR)
        Format_RGB10_A2 = 2   // 10-bit RGB + 2-bit Alpha (HDR10)
    };

    explicit HDR_VideoWidget(QWidget* parent = nullptr);
    virtual ~HDR_VideoWidget();

    // Configuration methods
    void setRenderMode(RenderMode mode);
    RenderMode getRenderMode() const { return m_renderMode; }
    
    void setTextureFormat(TextureFormat format);
    TextureFormat getTextureFormat() const { return m_textureFormat; }
    
    // HDR-specific settings
    void setHDRExposure(float exposure);
    float getHDRExposure() const { return m_hdrExposure; }
    
    void setHDRGamma(float gamma);  
    float getHDRGamma() const { return m_hdrGamma; }
    
    // Status queries
    bool isHDRCapable() const { return m_hdrCapable; }
    bool isInitialized() const { return m_initialized; }
    
    // HDR capability configuration
    void setHDRCapabilities(const HDRDetection::HDRCapabilities& capabilities);
    
public slots:
    // Main method for updating video frames
    void updateFrame(const QImage& newFrame);
    
    // Clear the current frame 
    void clearFrame();
    
    // HDR control slots
    void setHDRExposureSlot(double exposure) { setHDRExposure(static_cast<float>(exposure)); }
    void setHDRGammaSlot(double gamma) { setHDRGamma(static_cast<float>(gamma)); }

signals:
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

    // Event handling
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    // Initialization methods
    bool initializeShaders();
    bool initializeGeometry();
    bool initializeTexture();
    
    // Shader management
    bool loadShaderProgram();
    void updateShaderUniforms();
    
    // Texture management
    bool uploadTextureData(const QImage& image);
    void updateTextureParameters();
    
    // Rendering methods
    void renderFrame();
    void renderQuad();
    
    // HDR capability detection
    void detectHDRCapabilities();
    void validateRenderMode();
    
    // Utility methods
    QString getRenderModeString() const;
    QString getTextureFormatString() const;
    void logOpenGLError(const QString& operation);
    
    // Error handling
    void handleInitializationError(const QString& error);
    void handleRenderingError(const QString& error);

private:
    // Rendering state
    RenderMode m_renderMode;
    TextureFormat m_textureFormat;
    bool m_hdrCapable;
    bool m_initialized;
    
    // HDR parameters
    float m_hdrExposure;    // HDR exposure adjustment (-10.0 to +10.0)
    float m_hdrGamma;       // Gamma correction (0.1 to 5.0)
    
    // OpenGL objects
    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLTexture* m_videoTexture;
    QOpenGLBuffer* m_vertexBuffer;
    QOpenGLBuffer* m_indexBuffer;
    QOpenGLVertexArrayObject* m_vertexArrayObject;
    
    // Shader uniform locations
    int m_textureLocation;
    int m_renderModeLocation;
    int m_hdrExposureLocation;
    int m_hdrGammaLocation;
    int m_textureMatrixLocation;
    int m_projectionMatrixLocation;
    
    // Transformation matrices
    QMatrix4x4 m_textureMatrix;
    QMatrix4x4 m_projectionMatrix;
    
    // Frame data
    QImage m_currentFrame;
    bool m_frameUpdated;
    QSize m_frameSize;
    
    // Performance monitoring
    QTimer* m_fpsTimer;
    int m_frameCount;
    qint64 m_lastFpsUpdate;
    
    // Mouse interaction state
    bool m_dragging;
    QPoint m_lastMousePos;
    
    // Vertex data for full-screen quad
    static const float s_quadVertices[];
    static const unsigned int s_quadIndices[];
};

#endif // HDR_VIDEOWIDGET_H