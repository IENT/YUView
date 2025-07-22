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

### 2.2 Platform HDR Detection Methods

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

### 3.1 Component Design

#### HDR Detection Module (`HDRDetection.h/cpp`)
```cpp
class HDRDetection {
public:
    struct HDRCapabilities {
        bool isHDRSupported;
        float maxLuminance;
        float minLuminance;
        int bitsPerChannel;
        QString displayName;
    };
    
    static HDRCapabilities detectHDRCapabilities(QWidget* parent = nullptr);
    static bool isHDRActiveOnScreen(QScreen* screen);
};
```

#### HDR Video Widget (`HDR_VideoWidget.h/cpp`)
```cpp
class HDR_VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
    
public slots:
    void updateFrame(const QImage &newFrame);
    
signals:
    void hdrNotSupported(const QString &reason);
    
private:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    
    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLTexture* m_videoTexture;
    QOpenGLBuffer m_vertexBuffer;
    bool m_hdrCapable;
};
```

### 3.2 Integration Strategy

#### Rendering Path Decision Logic
```cpp
// In FrameHandler::drawFrame() or videoHandlerYUV
if (enable10BitDisplay && HDRDetection::isHDRActiveOnScreen(screen)) {
    // Use HDR_VideoWidget OpenGL path
    m_hdrWidget->updateFrame(outputImage);
} else {
    // Fallback to existing QPainter path
    painter->drawImage(rect, outputImage);
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

### 5.1 Technical Risks

**High Risk**:
- Qt framework limitations with 10-bit rendering may require workarounds
- Platform-specific HDR detection complexity (Windows/macOS differences)
- OpenGL context compatibility across different hardware configurations

**Medium Risk**:
- Shader compilation failures on older graphics drivers
- Performance impact of real-time PQ EOTF calculations
- Color accuracy validation across different HDR displays

**Low Risk**:
- Integration with existing YUView architecture (well-structured codebase)
- Fallback to SDR path (existing implementation remains unchanged)

### 5.2 Mitigation Strategies

**Qt Limitations**:
- Implement custom `QOpenGLWidget` with manual texture format control
- Use `QSurfaceFormat::setDefaultFormat()` for proper 10-bit context setup
- Provide comprehensive fallback mechanisms

**Performance Optimization**:
- Implement optimized PQ EOTF approximation for older hardware
- Add GPU capability detection and adaptive quality settings
- Cache shader compilation results

**Cross-Platform Compatibility**:
- Implement platform-agnostic HDR detection using Qt APIs where possible
- Provide platform-specific implementations as fallbacks
- Comprehensive testing on different hardware configurations

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

The implementation of native 10-bit HDR rendering in YUView is technically feasible and addresses a critical limitation in the current architecture. The proposed solution leverages OpenGL's high-precision rendering capabilities while maintaining backward compatibility through intelligent fallback mechanisms.

**Key Benefits**:
- Eliminates color banding in 10-bit HDR content
- Future-proofs YUView for HDR display adoption
- Maintains existing functionality for SDR users
- Provides clear user feedback for HDR status

**Recommended Approach**:
1. Implement Windows-focused solution first (primary user base)
2. Use proven OpenGL + ST.2084 PQ EOTF shader pipeline
3. Maintain existing QPainter fallback for compatibility
4. Plan for cross-platform expansion in future releases

The 8-week timeline provides adequate buffer for handling Qt framework limitations and platform-specific challenges while ensuring a robust, production-ready implementation.

---

*Report prepared for YUView HDR implementation project*  
*Date: July 22, 2025*  
*Status: Ready for implementation approval*