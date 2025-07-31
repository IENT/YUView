# YUView HDR 10-bit Display Implementation Research Report

## Executive Summary

This research report analyzes the implementation requirements for native 10-bit HDR video rendering in YUView, a C++ Qt-based video player. The current implementation suffers from color banding due to QPainter's 8-bit SDR pipeline limitation. This report provides a comprehensive technical analysis, implementation strategy, and project timeline for replacing the flawed rendering path with a complete OpenGL-based HDR solution.

## 1. Current Implementation Analysis

### 1.1 Existing Architecture
- **Build System**: qmake-based with YUViewLib (static library) and YUViewApp (application)
- **Qt Modules**: Uses Qt OpenGL module with basic QOpenGLWidget support
- **Current 10-bit Path**: Lines 2550-2600 in `videoHandlerYUV.cpp`
- **Fatal Flaw**: Uses `QPainter::drawImage()` which downsamples 16-bit data to 8-bit SDR

### 1.2 Technical Limitations
```cpp
// Current flawed implementation (videoHandlerYUV.cpp:2550-2600)
if (enable10BitDisplay && yuvFormat.getBitsPerSample() == 10) {
    // Creates QImage::Format_RGBA64_Premultiplied
    // Later: painter->drawImage(rect, outputImage); // ← Causes banding
}
```

**Root Cause**: QPainter's rendering pipeline is fundamentally limited to 8-bit SDR, causing unavoidable color banding when displaying 10-bit HDR content.

### 1.3 Existing Infrastructure Assessment
**Advantages**:
- Qt OpenGL module already included
- 10-bit YUV to 16-bit RGBA conversion logic exists
- UI toggle for 10-bit display implemented (`checkBoxEnable10BitDisplay`)
- Well-structured video handler architecture

**Gaps**:
- No HDR display detection mechanism
- No OpenGL HDR rendering pipeline
- Incomplete `configureHighBitDepthRendering()` function
- No ST.2084 PQ EOTF shader implementation

## 2. Technical Research Findings

### 2.1 Qt HDR Support Status (2024-2025)
**Current Limitations**:
- Qt Bug QTBUG-62071: QOpenGLWidget does not support 10-bit color depth
- QOpenGLFrameBufferObject uses GL_RGBA8 internally by default
- Limited native HDR support in Qt framework

**Workarounds Available**:
- Custom OpenGL context with `QSurfaceFormat::setRedBufferSize(10)`
- Manual texture format override to `GL_RGBA16F` or `GL_RGB10_A2`
- Projects like Krita have successfully implemented HDR through Qt modifications

### 2.2 Krita HDR Implementation Analysis

#### Overview
Krita, an open-source painting application, has successfully implemented native 10-bit/16-bit HDR display support that works on HDMI 2.1/HDR monitors without precision loss. Their implementation provides a proven blueprint for YUView.

#### Key Implementation Details

**1. HDR Format Support**:
- **BT2020_PQ Mode**: 10-bit per channel (R:10, G:10, B:10, A:2)
  - Uses BT.2020 color space with PQ transfer function
  - Suitable for HDR10 displays
- **BT709_G10 Mode (scRGB/Rec709 Linear)**: 16-bit per channel (R:16, G:16, B:16, A:16)
  - Linear color space with extended range
  - User-visible as "Rec709 Linear (16bit)" in Display->HDR settings

**2. QSurfaceFormat Configuration**:
```cpp
// From KisOpenGLModeProber::initSurfaceFormatFromConfig
if (config == KisConfig::BT2020_PQ) {
    format->setRedBufferSize(10);
    format->setGreenBufferSize(10);
    format->setBlueBufferSize(10);
    format->setAlphaBufferSize(2);
    format->setColorSpace(KisSurfaceColorSpaceWrapper::bt2020PQColorSpace);
} else if (config == KisConfig::BT709_G10) {
    format->setRedBufferSize(16);
    format->setGreenBufferSize(16);
    format->setBlueBufferSize(16);
    format->setAlphaBufferSize(16);
    format->setColorSpace(KisSurfaceColorSpaceWrapper::scRGBColorSpace);
}
```

**3. OpenGL Texture Format**:
- Consistently uses `GL_RGBA16F` for HDR content storage
- Texture format set in `KisOpenGLCanvas2` constructor:
```cpp
if (KisOpenGLModeProber::instance()->useHDRMode()) {
    setTextureFormat(GL_RGBA16F);
}
```

**4. HDR Detection Architecture**:
- `KisOpenGLModeProber` class handles HDR capability detection
- Platform-specific implementations for Windows/macOS
- `isFormatHDR()` checks both color space and bit depth:
  - BT2020_PQ: 10-bit buffers + BT.2020 PQ color space
  - scRGB: 16-bit buffers + scRGB color space

**5. HDR Exposure Control**:
- Implements `HdrExposure` resource for dynamic range adjustment
- Exposed in UI through LUT docker
- Applied in shaders for fine-tuning display

**6. Build Configuration**:
- Uses `HAVE_HDR` preprocessor flag for conditional compilation
- HDR support can be enabled/disabled at build time

#### Architecture Insights

**Rendering Pipeline**:
1. Image data → 16-bit internal representation
2. Upload to GPU as `GL_RGBA16F` texture
3. Custom shaders apply color space conversion and exposure
4. Direct rendering to HDR-configured framebuffer
5. Bypasses QPainter's 8-bit limitation entirely

**Key Classes**:
- `KisOpenGLModeProber`: HDR detection and configuration
- `KisOpenGLCanvas2`: Main rendering widget (inherits QOpenGLWidget)
- `KisOpenGLCanvasRenderer`: Handles texture upload and rendering
- `KisDisplayColorConverter`: Color space conversions
- `KisScreenInformationAdapter`: Platform-specific screen info

### 2.3 Platform HDR Detection Methods

#### Windows (Primary Platform)
**DXGI Method (Recommended)**:
```cpp
IDXGIOutput6::GetDesc1() // Returns DXGI_OUTPUT_DESC1
// Check: BitsPerColor = 10
// Check: ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
// Check: MaxLuminance > 100 nits
```

**WinRT Advanced Color API**:
```cpp
Windows::Graphics::Display::AdvancedColorInfo
// Properties: AdvancedColorSupported, AdvancedColorEnabled
// Luminance values: MaxLuminanceInNits, MinLuminanceInNits
```

#### Cross-Platform Qt Method
```cpp
QScreen* screen = QApplication::primaryScreen();
// Check depth() >= 30 (10-bit per channel)
// Verify QSurfaceFormat capabilities
```

### 2.3 OpenGL HDR Rendering Pipeline

#### Required QSurfaceFormat Configuration
```cpp
QSurfaceFormat format;
format.setRedBufferSize(10);
format.setGreenBufferSize(10);
format.setBlueBufferSize(10);
format.setAlphaBufferSize(2);
format.setProfile(QSurfaceFormat::CoreProfile);
format.setVersion(3, 3);
QSurfaceFormat::setDefaultFormat(format); // Must be called before QApplication
```

#### ST.2084 PQ EOTF Shader Implementation
**Fragment Shader Constants**:
```glsl
// ST.2084 (PQ curve) constants for HDR10
const float m1 = 2610.0 / 4096.0 / 4.0;
const float m2 = 2523.0 / 4096.0 * 128.0;
const float c1 = 3424.0 / 4096.0;
const float c2 = 2413.0 / 4096.0 * 32.0;
const float c3 = 2392.0 / 4096.0 * 32.0;

// Rec.709 to Rec.2020 color space conversion
const mat3 from709to2020 = mat3(
    0.6274040, 0.3292820, 0.0433136,
    0.0690970, 0.9195400, 0.0113612,
    0.0163916, 0.0880132, 0.8955950
);
```

## 3. Implementation Architecture

### 3.1 Component Design (Based on Krita Architecture)

#### HDR Detection Module (`HDRDetection.h/cpp`)
```cpp
class HDRDetection {
public:
    enum HDRMode {
        SDR_Only,
        BT2020_PQ_10bit,      // 10-bit HDR10 mode
        BT709_G10_16bit       // 16-bit scRGB/Linear mode
    };
    
    struct HDRCapabilities {
        bool isHDRSupported;
        HDRMode supportedMode;
        float maxLuminance;
        float minLuminance;
        int bitsPerChannel;
        QString displayName;
    };
    
    static HDRCapabilities detectHDRCapabilities(QWidget* parent = nullptr);
    static bool isHDRActiveOnScreen(QScreen* screen);
    static QSurfaceFormat getHDRSurfaceFormat(HDRMode mode);
};
```

#### HDR Video Widget (`HDR_VideoWidget.h/cpp`)
```cpp
class HDR_VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
    
public:
    enum RenderMode {
        Mode_SDR_8bit,
        Mode_BT2020_PQ_10bit,
        Mode_BT709_Linear_16bit
    };
    
    explicit HDR_VideoWidget(QWidget* parent = nullptr);
    void setRenderMode(RenderMode mode);
    
public slots:
    void updateFrame(const QImage &newFrame);
    void setHDRExposure(float exposure);
    
signals:
    void hdrNotSupported(const QString &reason);
    
private:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    
    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLTexture* m_videoTexture;
    QOpenGLBuffer m_vertexBuffer;
    RenderMode m_renderMode;
    float m_hdrExposure;
    bool m_hdrCapable;
};
```

### 3.2 Integration Strategy (Following Krita's Approach)

#### Application Initialization
```cpp
// In main.cpp, before QApplication construction
int main(int argc, char *argv[]) {
    // Detect HDR capabilities early
    auto hdrCaps = HDRDetection::detectHDRCapabilities();
    
    if (hdrCaps.isHDRSupported) {
        // Set HDR surface format as default
        QSurfaceFormat hdrFormat = HDRDetection::getHDRSurfaceFormat(hdrCaps.supportedMode);
        QSurfaceFormat::setDefaultFormat(hdrFormat);
    }
    
    QApplication app(argc, argv);
    // ... rest of application
}
```

#### Rendering Path Decision Logic
```cpp
// In videoHandlerYUV::slot10BitDisplayChanged()
void videoHandlerYUV::slot10BitDisplayChanged() {
    bool enable10Bit = ui.checkBoxEnable10BitDisplay->isChecked();
    
    if (enable10Bit) {
        auto hdrCaps = HDRDetection::detectHDRCapabilities(this);
        
        if (hdrCaps.isHDRSupported) {
            // Initialize HDR widget if not already created
            if (!m_hdrWidget) {
                m_hdrWidget = new HDR_VideoWidget(this);
                connect(m_hdrWidget, &HDR_VideoWidget::hdrNotSupported,
                        this, &videoHandlerYUV::onHDRNotSupported);
            }
            
            // Set appropriate render mode based on capabilities
            if (hdrCaps.supportedMode == HDRDetection::BT709_G10_16bit) {
                m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT709_Linear_16bit);
            } else {
                m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT2020_PQ_10bit);
            }
            
            m_useHDRRendering = true;
        } else {
            // Show user notification
            QMessageBox::information(this, 
                tr("HDR Not Available"),
                tr("Your display does not support HDR. Using standard 8-bit rendering."));
            m_useHDRRendering = false;
        }
    } else {
        m_useHDRRendering = false;
    }
    
    // Save setting
    QSettings settings;
    settings.setValue("Enable10BitDisplay", enable10Bit);
}
```

### 3.3 Shader Architecture (Based on Krita's Approach)

#### Vertex Shader (hdr_vertex.vert)
```glsl
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 textureMatrix;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = (textureMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
}
```

#### Fragment Shader (hdr_fragment.frag)
```glsl
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D videoTexture;
uniform int renderMode;  // 0=SDR, 1=BT2020_PQ, 2=BT709_Linear
uniform float hdrExposure;

// ST.2084 PQ constants
const float m1 = 2610.0 / 4096.0 / 4.0;
const float m2 = 2523.0 / 4096.0 * 128.0;
const float c1 = 3424.0 / 4096.0;
const float c2 = 2413.0 / 4096.0 * 32.0;
const float c3 = 2392.0 / 4096.0 * 32.0;

// Apply PQ transfer function for BT2020_PQ mode
vec3 applyPQ(vec3 linear) {
    vec3 Lp = pow(linear / 10000.0, vec3(m1));
    return pow((c1 + c2 * Lp) / (1.0 + c3 * Lp), vec3(m2));
}

// Apply exposure for linear/scRGB mode
vec3 applyExposure(vec3 linear) {
    return linear * pow(2.0, hdrExposure);
}

void main() {
    vec4 color = texture(videoTexture, TexCoord);
    
    if (renderMode == 1) {
        // BT2020_PQ mode: Apply PQ curve
        FragColor = vec4(applyPQ(color.rgb), color.a);
    } else if (renderMode == 2) {
        // BT709_Linear mode: Apply exposure adjustment
        FragColor = vec4(applyExposure(color.rgb), color.a);
    } else {
        // SDR mode: Direct output
        FragColor = color;
    }
}
```

## 4. File Modification Plan

### 4.1 New Files to Create

**Core HDR Implementation**:
- `YUViewLib/src/video/HDRDetection.h`
- `YUViewLib/src/video/HDRDetection.cpp` 
- `YUViewLib/src/video/HDR_VideoWidget.h`
- `YUViewLib/src/video/HDR_VideoWidget.cpp`

**Shader Resources**:
- `YUViewLib/resources/shaders/hdr_vertex.vert`
- `YUViewLib/resources/shaders/hdr_fragment.frag`

**Windows Platform Support**:
- `YUViewLib/src/video/HDRDetection_win.cpp` (Windows-specific DXGI/WinRT)

### 4.2 Files to Modify

**Application Entry Point**:
- `YUViewApp/src/yuviewapp.cpp`
  - Add `QSurfaceFormat::setDefaultFormat()` before `QApplication` construction
  - Include 10-bit buffer configuration

**Video Handler Integration**:
- `YUViewLib/src/video/yuv/videoHandlerYUV.h`
  - Add `HDR_VideoWidget* m_hdrWidget` member
  - Add HDR-related signals and slots

- `YUViewLib/src/video/yuv/videoHandlerYUV.cpp`
  - Modify `slot10BitDisplayChanged()` to handle HDR detection
  - Replace QPainter path with HDR widget when appropriate
  - Add fallback notification system

**Build System**:
- `YUViewLib/YUViewLib.pro`
  - Add new source files to SOURCES and HEADERS
  - Add shader resource files to RESOURCES
  - Conditionally link Windows-specific libraries (dxgi.lib, user32.lib)

**Frame Handler**:
- `YUViewLib/src/video/FrameHandler.cpp`
  - Complete the `configureHighBitDepthRendering()` implementation
  - Add HDR widget integration points

### 4.3 Dependencies and Frameworks

**Qt Modules** (already available):
- `Qt::OpenGL` ✅
- `Qt::Widgets` ✅  
- `Qt::Gui` ✅

**Additional Dependencies**:
- OpenGL 3.3 Core Profile (standard)
- Windows: `dxgi.dll`, `user32.dll` (system libraries)
- macOS: `CoreGraphics.framework` (for future HDR detection)

**Shader Management**:
- Qt Resource System for embedding GLSL shaders
- `QOpenGLShaderProgram` for shader compilation and linking

## 5. Risk Assessment

### 5.1 Technical Risks (Mitigated by Krita Reference)

**Low Risk (Thanks to Krita Precedent)**:
- Qt framework limitations - Krita has proven workarounds
- OpenGL HDR rendering - Krita's implementation works reliably
- Platform-specific HDR detection - Can adapt Krita's approach
- Color accuracy - Krita's professional-grade implementation validates approach

**Medium Risk**:
- Integration complexity with YUView's existing architecture
- Performance optimization for video playback vs. static images
- Testing across diverse hardware configurations

**Minimal Risk**:
- Shader compilation - Using proven shader patterns from Krita
- Fallback to SDR path - Well-established pattern

### 5.2 Mitigation Strategies

**Leverage Krita's Solutions**:
- Use Krita's QSurfaceFormat configuration approach directly
- Adapt KisOpenGLModeProber pattern for HDR detection
- Follow Krita's GL_RGBA16F texture format strategy
- Implement similar HDR exposure control mechanism

**YUView-Specific Adaptations**:
- Optimize texture upload for video frame rates
- Implement frame buffering for smooth playback
- Add video-specific color space conversions

**Testing Strategy**:
- Test on same hardware configurations where Krita works
- Validate against Krita's rendering output
- Ensure compatibility with Krita-proven HDR displays

## 6. Project Timeline

### Phase 1: Foundation (Week 1-2)
**Week 1**:
- [ ] Implement `HDRDetection` class with Windows DXGI support
- [ ] Create basic `HDR_VideoWidget` class structure
- [ ] Add `QSurfaceFormat` setup to main.cpp

**Week 2**:
- [ ] Implement OpenGL texture management in HDR widget
- [ ] Create and test basic vertex/fragment shaders
- [ ] Add HDR detection to video handler initialization

### Phase 2: Core Implementation (Week 3-4)
**Week 3**:
- [ ] Complete ST.2084 PQ EOTF fragment shader implementation
- [ ] Implement `updateFrame()` method with texture upload
- [ ] Add rendering pipeline in `paintGL()`

**Week 4**:
- [ ] Integrate HDR widget into video handler rendering path
- [ ] Implement fallback notification system
- [ ] Add error handling and recovery mechanisms

### Phase 3: Integration & Testing (Week 5-6)
**Week 5**:
- [ ] Complete integration with existing 10-bit display toggle
- [ ] Implement automatic HDR/SDR path selection
- [ ] Add comprehensive logging and debugging support

**Week 6**:
- [ ] Performance optimization and shader tuning
- [ ] Cross-platform testing (Windows primary, macOS secondary)
- [ ] User acceptance testing with real HDR displays

### Phase 4: Polish & Documentation (Week 7-8)
**Week 7**:
- [ ] UI/UX improvements for HDR status indication
- [ ] Error message localization and user guidance
- [ ] Code review and refactoring

**Week 8**:
- [ ] Documentation updates
- [ ] Final integration testing
- [ ] Deployment preparation

## 7. Success Criteria

### 7.1 Functional Requirements
✅ **HDR Display Detection**: Automatically detect HDR-capable displays on Windows
✅ **10-bit Rendering**: Display 10-bit YUV content without color banding
✅ **Fallback Mechanism**: Graceful degradation to SDR on non-HDR systems
✅ **User Notification**: Clear indication of HDR status to users

### 7.2 Performance Requirements
- **Frame Rate**: Maintain 60fps playback on modern hardware
- **Startup Time**: HDR detection < 500ms
- **Memory Usage**: < 50MB additional memory for HDR pipeline
- **GPU Compatibility**: Support OpenGL 3.3+ hardware (DirectX 11 equivalent)

### 7.3 Quality Requirements
- **Color Accuracy**: Proper ST.2084 PQ EOTF implementation
- **No Banding**: Eliminate visible color banding in 10-bit content
- **HDR Range**: Support 100-4000 nits luminance range
- **Color Space**: Proper Rec.2020 wide color gamut handling

## 8. Conclusion

The implementation of native 10-bit HDR rendering in YUView is not only technically feasible but has a proven reference implementation in Krita. By analyzing Krita's source code, we have identified a robust architecture that successfully delivers 10-bit/16-bit content to HDR displays without precision loss.

**Key Learnings from Krita**:
- Dual HDR mode support (10-bit BT2020_PQ and 16-bit scRGB) provides flexibility
- Direct OpenGL rendering with GL_RGBA16F textures bypasses Qt's limitations
- Platform-specific HDR detection ensures compatibility
- HDR exposure control enhances user experience
- Proven architecture that works in production

**Key Benefits**:
- Eliminates color banding in 10-bit HDR content
- Future-proofs YUView for HDR display adoption
- Maintains existing functionality for SDR users
- Provides clear user feedback for HDR status
- Follows a proven implementation pattern from Krita

**Recommended Approach**:
1. Implement Krita-inspired dual-mode HDR support (BT2020_PQ and scRGB)
2. Use GL_RGBA16F texture format consistently for HDR content
3. Adopt Krita's QSurfaceFormat configuration strategy
4. Implement platform-specific HDR detection following Krita's model
5. Add HDR exposure control for enhanced usability
6. Maintain existing QPainter fallback for compatibility

The 8-week timeline provides adequate buffer for implementation while leveraging Krita's proven solutions to common challenges. By following Krita's architectural patterns, we can significantly reduce implementation risk and deliver a robust HDR solution.

---

*Report prepared for YUView HDR implementation project*  
*Date: December 2024*  
*Status: Ready for implementation with Krita reference architecture*
