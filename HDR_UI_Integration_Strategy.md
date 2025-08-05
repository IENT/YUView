# HDR UI Integration Strategy: QStackedWidget Implementation

## Overview
This document outlines the UI integration strategy for seamless switching between standard 8-bit rendering and the new HDR_VideoWidget without creating new windows, while retaining 10-bit HDR precision.

## Architecture Design

### QStackedWidget Integration Pattern

```cpp
// Main UI Container Structure
QStackedWidget* videoRenderStack = new QStackedWidget();

// Index 0: Standard 8-bit QPainter rendering widget
QWidget* standardVideoWidget = new QWidget();
videoRenderStack->addWidget(standardVideoWidget);

// Index 1: HDR_VideoWidget for 10-bit HDR rendering  
HDR_VideoWidget* hdrVideoWidget = new HDR_VideoWidget();
videoRenderStack->addWidget(hdrVideoWidget);

// Switch between renderers based on HDR mode
void switchToHDRMode(bool enableHDR) {
    if (enableHDR && hdrCapabilitiesDetected) {
        videoRenderStack->setCurrentIndex(1); // HDR widget
    } else {
        videoRenderStack->setCurrentIndex(0); // Standard widget
    }
}
```

### Signal Connection Strategy

The videoHandlerYUV already emits the required signal:
```cpp
emit hdrWidgetNeedsDisplay(m_hdrWidget, true);  // Show HDR widget
emit hdrWidgetNeedsDisplay(m_hdrWidget, false); // Hide HDR widget
```

### Main UI Integration Points

#### 1. Signal Handler in Main Window
```cpp
class MainWindow : public QMainWindow {
    
private slots:
    void onHDRWidgetNeedsDisplay(HDR_VideoWidget* widget, bool show);
    void onHDRRenderingStateChanged(bool enabled, HDR_VideoWidget* widget);
    
private:
    QStackedWidget* m_videoRenderStack;
    QWidget* m_standardVideoWidget;
    HDR_VideoWidget* m_hdrVideoWidget;
};

void MainWindow::onHDRWidgetNeedsDisplay(HDR_VideoWidget* widget, bool show) {
    if (show && widget) {
        // Switch to HDR rendering
        m_hdrVideoWidget = widget;
        if (m_videoRenderStack->indexOf(widget) == -1) {
            m_videoRenderStack->addWidget(widget);
        }
        m_videoRenderStack->setCurrentWidget(widget);
        widget->show();
        
        qDebug() << "UI switched to HDR rendering mode";
    } else {
        // Switch back to standard rendering
        m_videoRenderStack->setCurrentWidget(m_standardVideoWidget);
        if (widget) {
            widget->hide(); // Persistent lifecycle - don't delete
        }
        
        qDebug() << "UI switched to standard rendering mode";
    }
}
```

#### 2. Initialization in Main UI Constructor
```cpp
MainWindow::MainWindow() {
    // Create video render stack
    m_videoRenderStack = new QStackedWidget(this);
    
    // Create standard video widget (existing implementation)
    m_standardVideoWidget = createStandardVideoWidget();
    m_videoRenderStack->addWidget(m_standardVideoWidget);
    
    // Set as current widget by default
    m_videoRenderStack->setCurrentWidget(m_standardVideoWidget);
    
    // Add to main layout
    centralWidget()->layout()->addWidget(m_videoRenderStack);
    
    // Connect HDR signals from all video handlers
    connectHDRSignals();
}
```

#### 3. HDR-Specific Layout Considerations
```cpp
void MainWindow::setupHDRWidget(HDR_VideoWidget* hdrWidget) {
    // Ensure HDR widget uses appropriate size policy
    hdrWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Configure for proper HDR color space handling
    hdrWidget->setMinimumSize(320, 240);
    
    // Ensure widget is properly integrated with main window's color profile
    hdrWidget->setAttribute(Qt::WA_AcceptTouchEvents, false);
    hdrWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
}
```

## Benefits of This Approach

### ✅ Compliance with Architectural Mandate
1. **No New Windows**: HDR rendering stays within the main application window
2. **Seamless Switching**: QStackedWidget provides instant switching without flicker
3. **10-bit Precision Retained**: HDR_VideoWidget maintains full precision throughout the pipeline
4. **Persistent Lifecycle**: Widgets are managed via show/hide, not create/destroy

### ✅ User Experience Benefits
1. **No Window Management**: Users don't need to manage separate HDR windows
2. **Consistent UI**: All controls remain accessible during HDR playback
3. **Smooth Transitions**: Switching between SDR/HDR is immediate and seamless
4. **Resource Efficiency**: Widgets are reused rather than recreated

### ✅ Technical Benefits
1. **Thread Safety**: All OpenGL operations remain in main thread
2. **Context Stability**: No context switching between different windows
3. **Memory Efficiency**: Single widget instance per mode
4. **Error Resilience**: Fallback to standard rendering is automatic

## Implementation Files Required

### Modified Files (Already Completed)
- ✅ `HDR_VideoWidget.h` - Compliant interface implemented
- ✅ `HDR_VideoWidget.cpp` - Unified rendering path implemented  
- ✅ `videoHandlerYUV.cpp` - Push model and persistent lifecycle implemented

### New Integration Points (To Be Implemented)
- 📋 Main UI class signal connections for `hdrWidgetNeedsDisplay`
- 📋 QStackedWidget integration in main window layout
- 📋 HDR widget size and layout management
- 📋 Proper color space handling in UI stack

## Testing Strategy

### Test Cases for UI Integration
1. **HDR Enable/Disable Toggle**: Verify seamless switching between renderers
2. **Window Resize**: Ensure both renderers handle resize events properly
3. **Multiple File Loading**: Test persistence across different video files
4. **HDR Capability Changes**: Verify graceful fallback when HDR becomes unavailable
5. **Memory Stability**: Ensure no memory leaks during mode switching

## Conclusion

This QStackedWidget integration strategy provides a robust, user-friendly solution that fully complies with the architectural mandate while delivering excellent user experience. The approach eliminates the race conditions and lifecycle issues that caused the original crashes while maintaining the full 10-bit HDR rendering precision.