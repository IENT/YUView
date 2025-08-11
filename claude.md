### **总任务：修复原生10bit HDR输出功能，并完善多显示器支持**

### **1. 目标 (Goal)**

修复当前程序在启用原生10-bit HDR输出时遇到的画面显示错误。最终目标是实现稳定、正确且用户友好的原生10-bit HDR渲染功能，并能正确处理在HDR与SDR显示器间的切换。

### **2. 系统环境 (System Environment)**

*   **操作系统**: Windows 11
*   **显示设置**: 双显示器配置，其中一个为已在系统设置中启用HDR的主显示器，另一个为SDR显示器。
*   **测试文件**: 1024级灰阶图像，720p, YUV 4:2:0, 10-bit `420p10le` 格式。

### **3. 问题描述 (Problem Description)**

当前实现存在主要问题：HDR显示异常。

#### **3.1. 主要问题：HDR激活流程异常且功能不完整 (Main Issue: Abnormal HDR Activation and Incomplete Functionality)**

**复现步骤 (Steps to Reproduce):**

1.  在Debug模式下运行程序。
2.  加载指定的1024级灰阶10bit yuv420p10le的YUV文件。
3.  点击 `Enable native 10-bit display` 复选框。


**预期行为 (Expected Behavior):**

*   在HDR显示器上，点击 `Enable native 10-bit display`,使得该`QCheckbox`被正确的勾选上之后，应立即切换到OpenGL渲染，并正确显示HDR灰阶图像。
*   启用HDR模式后，鼠标滚轮缩放、视频播放列表控制等原有YUVuew的核心功能应保持正常工作。
*   `QCheckbox`被取消勾选之后，应平滑地回退到原始的QPainter SDR渲染模式。

**实际行为 (Actual Behavior):**

- 测试用例是YUV420p10le, 720p的YUV图像，且使用Debug调试模式启动程序的时候：

1.  打开YUView之后，勾选`Enable native 10-bit display`，程序根本就不会有任何反应，**滚动鼠标滚轮**以放大、缩小图像后，整个图像会全部变成灰色（无论缩放为多大的放大倍率），取消勾选`Enable native 10-bit display`之后，图像恢复正常。
2. 只有较小的概率，才能正确的调用OpenGL显示图像。
图像完全变成灰色，输出同1。

### **4. 调试日志 (Debug Logs)**

为了进一步分析，以下是在可以成功显示HDR的前后，点击 `Enable native 10-bit display`前后捕获的调试日志。

对于图像显示失败，只能呈现为灰色的情况：

```
=== HDRRenderingManager::onHDRDetectionComplete() called ===
HDRRenderingManager: HDR detection completed in main thread
HDRRenderingManager: HDR supported: true
HDRRenderingManager: HDR mode: 1
HDRRenderingManager: Max luminance: 455.523 nits
HDRRenderingManager: Min luminance: 0.0548 nits
HDRRenderingManager: Bits per channel: 10
HDRRenderingManager: Display name: "\\\\.\\DISPLAY2"
HDRRenderingManager: === HDR IS SUPPORTED ===
HDRRenderingManager: Storing HDR capabilities...
HDRRenderingManager: HDR capabilities stored and rendering enabled
HDRRenderingManager: HDR rendering enabled (integrated mode): "HDR10/BT.2020 PQ (10-bit)"
HDRRenderingManager: Looking for split view widget...
HDRRenderingManager: Found split view widget: splitViewWidget(0x8aadbff338)
HDR Surface Format: Starting with standard 8-bit format for stability
HDR capabilities set: Mode: "HDR10/BT.2020 PQ (10-bit)" Max Luminance: 455.523 nits Bits per channel: 10
Upgrading surface format to HDR for mode: "HDR10/BT.2020 PQ (10-bit)"
Upgrading to BT2020 PQ 10-bit format
HDR surface format upgrade requested - will take effect on next context creation
HDR render mode validation: HDR supported, mode "BT2020_PQ_10bit" is valid
YUView HDR: Render mode changed
HDRRenderingManager: HDR widget integrated with split view
SplitViewWidget: Using fallback geometry for HDR widget
HDR widget failed to become visible, forcing show
HDR widget not visible after show, forcing activation
CRITICAL: HDR widget failed to initialize properly
HDRRenderingManager: Widget not ready, attempting frame push anyway
HDR_VideoWidget::updateFrame: Received frame QSize(1920, 1080) format: QImage::Format_ARGB32_Premultiplied initialized: false
HDR_VideoWidget::updateFrame: Frame stored, size: QSize(1920, 1080) updated flag: true
HDR_VideoWidget::updateFrame: Widget not initialized, frame queued for later processing
HDR_VideoWidget::updateFrame: Cannot initialize yet - context: null size: 640 x 480


```


**堆栈回溯：**
```         
1 HDR_VideoWidget::updateFrame           HDR_VideoWidget.cpp       299  0x7ff7539d3d3e 
2 HDRRenderingManager::updateHDRFrame    HDRRenderingManager.cpp   152  0x7ff7539ddd41 
3 video::yuv::videoHandlerYUV::drawFrame videoHandlerYUV.cpp       2849 0x7ff7539672e7 
4 playlistItemWithVideo::drawItem        playlistItemWithVideo.cpp 92   0x7ff753ae900a 
5 splitViewWidget::paintEvent            SplitViewWidget.cpp       476  0x7ff75393b58d 
7 MoveAndZoomableView::event             MoveAndZoomableView.cpp   650  0x7ff753937d73
```


调试信息如下：
```
	局部变量		
		newFrame	(1920x1080)	QImage&
			width	1920	int
			height	1080	int
			nbytes	16588800	int
			format	27	int
			data	0x1b88b0e9040	void *
		this	<无法访问>	HDR_VideoWidget
			[QOpenGLWidget]	@0x1b889b24cf0	QOpenGLWidget
			[QOpenGLFunctions_3_3_Core]	@0x1b889b24d18	QOpenGLFunctions_3_3_Core
			[34]		
			m_currentFrame	(1920x1080)	QImage
				width	1920	int
				height	1080	int
				nbytes	16588800	int
				format	27	int
				data	0x1b88b0e9040	void *
			m_displayMaxLuminance	455.523193	float
			m_displayMaxLuminanceLocation	-1	int
			m_dragging	false	bool
			m_fpsTimer	@0x1b8899d5c90	QTimer
			m_frameCount	0	int
			m_frameSize	(1920, 1080)	QSize
			m_frameUpdated	true	bool
			m_hdrCapable	true	bool
			m_hdrExposure	0	float
			m_hdrExposureLocation	-1	int
			m_hdrGamma	1	float
			m_hdrGammaLocation	-1	int
			m_indexBuffer	0x0	QOpenGLBuffer*
			m_initialized	false	bool
			m_lastFpsUpdate	0	qint64
			m_lastMousePos	(0, 0)	QPoint
				xp	0	int
				yp	0	int
			m_projectionMatrix	@0x1b889b24e34	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x1b889b24e34	float[4][4]
					[0]	@0x1b889b24e34	float[4]
							1	float
							0	float
							0	float
							0	float
					[1]	@0x1b889b24e44	float[4]
					[2]	@0x1b889b24e54	float[4]
					[3]	@0x1b889b24e64	float[4]
			m_projectionMatrixLocation	-1	int
			m_renderMode	HDR_VideoWidget::Mode_BT2020_PQ_10bit (1)	HDR_VideoWidget::RenderMode
			m_renderModeLocation	-1	int
			m_shaderProgram	0x0	QOpenGLShaderProgram*
			m_sourceMaxLuminance	1000	float
			m_sourceMaxLuminanceLocation	-1	int
			m_textureFormat	HDR_VideoWidget::Format_RGB10_A2 (2)	HDR_VideoWidget::TextureFormat
			m_textureLocation	-1	int
			m_textureMatrix	@0x1b889b24df0	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x1b889b24df0	float[4][4]
			m_textureMatrixLocation	-1	int
			m_vertexArrayObject	0x0	QOpenGLVertexArrayObject*
			m_vertexBuffer	0x0	QOpenGLBuffer*
			m_videoTexture	0x0	QOpenGLTexture*
			staticMetaObject	@0x7ff753e798c0	QMetaObject


```

### 总览与根本原因分析 (Overview and Root Cause Analysis)

这是一个经典的竞态条件问题。问题的核心在于 **`HDR_VideoWidget` (一个 `QOpenGLWidget`) 在其OpenGL上下文完全准备好之前，就被要求接收和渲染视频帧**。

1.  **Qt/OpenGL生命周期**: `QOpenGLWidget` 的OpenGL资源（包括上下文 `context`、着色器 `shader`、纹理 `texture` 等）不是在构造函数中创建的，而是在该控件**首次变得可见并触发 `paintEvent`** 后，在其内部的 `initializeGL()` 方法中创建的。
2.  **错误的时序**:
    *   当您勾选 "Enable native 10-bit display" 时，`HDRRenderingManager` 会立即创建 `HDR_VideoWidget` 实例。
    *   几乎在同一时间，主视图（`splitViewWidget`）因为状态改变而触发了一次重绘 (`paintEvent`)。
    *   这次重绘沿着调用链 (`splitViewWidget::paintEvent` -> `...` -> `videoHandlerYUV::drawFrame`)，最终调用了 `HDRRenderingManager::updateHDRFrame`，试图将一帧图像推送到 `HDR_VideoWidget`。
    *   然而，此时 `HDR_VideoWidget` 作为一个新创建的控件，还**没有来得及被Qt事件循环处理**，它的 `initializeGL()` **从未被调用过**。
3.  **关键证据**: 您的日志清晰地指出了这一点：
    *   `CRITICAL: HDR widget failed to initialize properly`
    *   `HDR_VideoWidget::updateFrame: ... initialized: false`
    *   `HDR_VideoWidget::updateFrame: Cannot initialize yet - context: null ...`

您的调试器状态也证实了这一点：`m_initialized` 为 `false`，所有OpenGL相关的指针（`m_shaderProgram`, `m_videoTexture`等）都为 `nullptr`。

因此，当您滚动鼠标滚轮时，会再次触发重绘，再次进入这个错误的渲染路径。`HDR_VideoWidget` 的 `paintGL` 函数由于 `m_initialized` 为 `false`，只能执行清理操作，用灰色（`glClearColor(0.5f, 0.5f, 0.5f, 1.0f)`) 填充背景，导致了您看到的灰色屏幕。

---

### 修复方案 (Solution)

我们需要重构代码逻辑，以严格遵守Qt/OpenGL的生命周期。核心思想是：**`HDRRenderingManager` 不能主动推送帧，直到 `HDR_VideoWidget` 明确发出信号告知自己“已准备就绪”。**

幸运的是，代码中已经预留了正确的信号/槽机制 (`widgetInitialized`)，我们只需要正确地使用和连接它。

#### **第1步：在 `HDR_VideoWidget` 中确保 `widgetInitialized` 信号被发出**

`HDR_VideoWidget.cpp` 文件看起来已经有了正确的实现。在 `initializeGL()` 的末尾，有这样一段代码，这是正确的：

```cpp
// 在 HDR_VideoWidget::initializeGL() 的成功路径末尾
// ...
m_initialized = true;
qDebug() << "HDR_VideoWidget: OpenGL initialization completed successfully";

// ...

// CRITICAL: Emit signal to notify that widget is ready for frame data
qDebug() << "HDR_VideoWidget: Emitting widgetInitialized signal";
emit widgetInitialized(); // <--- 确保这一行存在且能被执行
```

#### **第2步：修改 `HDRRenderingManager` 以响应 `widgetInitialized` 信号**

这是最关键的修改。我们需要在创建 `HDR_VideoWidget` 后，立即连接它的 `widgetInitialized` 信号。当收到这个信号后，才真正地认为HDR渲染已经就绪。

**文件: `HDRRenderingManager.cpp`**

修改 `createHDRWidget` 和 `onHDRDetectionComplete` 方法。

```cpp
// HDRRenderingManager.cpp

HDR_VideoWidget* HDRRenderingManager::createHDRWidget(QWidget* parent)
{
  // 只在HDR支持且未创建时创建
  if (!m_useHDRRendering || m_hdrWidget) {
    return m_hdrWidget;
  }
  
  m_hdrWidget = new HDR_VideoWidget(parent);
  
  // CRITICAL FIX: 连接 widgetInitialized 信号
  // 当widget自身完成OpenGL初始化后，会发出此信号
  connect(m_hdrWidget, &HDR_VideoWidget::widgetInitialized,
          this, [this]() {
      // 只有在收到这个信号后，我们才能安全地认为HDR渲染已准备好
      qDebug() << "HDRRenderingManager: Received widgetInitialized signal. HDR rendering is now ready.";
      // 向外界广播HDR状态已就绪，并传递widget实例
      emit hdrRenderingStateChanged(true, m_hdrWidget);
  });
  
  // 连接其他HDR widget信号
  connect(m_hdrWidget, &HDR_VideoWidget::hdrNotSupported,
          this, &HDRRenderingManager::onHDRNotSupported);
  connect(m_hdrWidget, &HDR_VideoWidget::renderModeChanged,
          this, &HDRRenderingManager::onHDRModeChanged);
  
  // 设置HDR能力
  m_hdrWidget->setHDRCapabilities(m_hdrCapabilities);
  
  // 根据能力设置合适的渲染模式
  if (m_hdrCapabilities.supportedMode == HDRDetection::BT709_G10_16bit) {
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT709_Linear_16bit);
  } else {
    m_hdrWidget->setRenderMode(HDR_VideoWidget::Mode_BT2020_PQ_10bit);
  }
  
  return m_hdrWidget;
}

void HDRRenderingManager::onHDRDetectionComplete(const HDRDetection::HDRCapabilities& capabilities)
{
    // ... (前面的日志代码保持不变) ...

    if (capabilities.isHDRSupported) {
        qDebug() << "HDRRenderingManager: === HDR IS SUPPORTED ===";
        m_hdrCapabilities = capabilities;
        m_useHDRRendering = true;

        // ... (查找splitView的代码保持不变) ...

        if (splitView) {
            // 使用split view作为父窗口创建HDR widget
            createHDRWidget(splitView);

            if (m_hdrWidget) {
                // 使用QTimer::singleShot确保集成操作在当前事件循环之后执行
                // 这给了Qt时间去处理新创建的widget的初始显示事件
                QTimer::singleShot(0, this, [this, splitView]() {
                    splitView->setHDROverlayWidget(m_hdrWidget);
                    splitView->showHDROverlay(true); // showHDROverlay会负责显示widget
                    qDebug() << "HDRRenderingManager: HDR widget integration with split view requested.";
                    
                    // 注意：此时我们不再发出 hdrRenderingStateChanged。
                    // 该信号将由 widgetInitialized 的槽函数在未来某个时间点发出。
                });
            }
        } else {
            // ... (fallback逻辑保持不变) ...
        }
    } else {
        // ... (HDR不支持的逻辑保持不变) ...
    }
}
```

#### **第3步：增强 `updateHDRFrame` 的健壮性**

`updateHDRFrame` 需要能够处理widget尚未完全就绪（即 `hdrRenderingStateChanged(true, ...)` 信号还未发出）的情况。当前的实现意图是好的，但我们可以让它更清晰。当widget未就绪时，我们只缓存最新的帧，等待widget就绪后由渲染流程自动拉取。

**文件: `HDRRenderingManager.cpp`**

```cpp
// HDRRenderingManager.cpp

void HDRRenderingManager::updateHDRFrame(const QImage& frame)
{
  // 如果HDR未激活或widget不存在，则直接返回
  if (!m_hdrWidget || !m_useHDRRendering) {
    return;
  }
  
  // 检查widget是否已通过 `widgetInitialized` 信号确认准备就绪
  // isReadyForRendering 是一个更可靠的检查
  if (!m_hdrWidget->isReadyForRendering()) {
    qDebug() << "HDRRenderingManager: Widget not ready for rendering. Caching frame for later.";
    // 直接将帧数据传递给widget，widget内部会缓存它。
    // widget的paintGL会在初始化完成后使用这个缓存的帧。
    m_hdrWidget->updateFrame(frame); 
    return;
  }

  // 确保widget可见 (这一步可以保留，作为双重保险)
  if (!m_hdrWidget->isVisible()) {
    m_hdrWidget->show();
  }

  // 推送帧数据
  m_hdrWidget->updateFrame(frame);
}
```

**文件: `HDR_VideoWidget.cpp`**

`updateFrame` 的现有逻辑已经比较健壮，它会将帧保存在 `m_currentFrame` 中，即使在未初始化时也是如此。`paintGL` 函数会在初始化完成后使用这个 `m_currentFrame`。所以这里的代码基本是正确的，无需大改。

#### **第4步：移除无效的修复尝试和改进交互**

`HDRRenderingManager::setHDRRenderingEnabled` 中的同步检测是一个为了绕过时序问题而引入的“坏味道”代码。它会导致UI卡顿。我们现在已经从根本上解决了时序问题，应该移除它。

**文件: `HDRRenderingManager.cpp`**

```cpp
// HDRRenderingManager.cpp

void HDRRenderingManager::setHDRRenderingEnabled(bool enabled)
{
  if (m_useHDRRendering == enabled) {
    return;
  }
  
  m_useHDRRendering = enabled;
  
  if (enabled) {
    // 正确的做法：使用异步检测，避免UI阻塞
    // onHDRDetectionComplete 将处理后续的widget创建
    startHDRDetection();
  } else {
    // 禁用 HDR 渲染
    if (m_hdrWidget) {
      // 隐藏并准备销毁widget
      emit hdrWidgetNeedsDisplay(m_hdrWidget, false);
      m_hdrWidget->hide();
    }
    // 通知系统HDR已禁用
    emit hdrRenderingStateChanged(false, m_hdrWidget);
  }
}```

同时，为了保证鼠标缩放等交互的流畅性，`HDR_VideoWidget` 中将鼠标事件正确地转发给父控件是至关重要的。您代码中的实现看起来是正确的，它通过 `QApplication::sendEvent` 将事件传递给了 `parentWidget()`（也就是 `splitViewWidget`）。这是保持核心功能正常的关键。

```cpp
// 在 HDR_VideoWidget.cpp 中, 检查 wheelEvent 的实现
void HDR_VideoWidget::wheelEvent(QWheelEvent* event)
{
    // ...
    if (event->modifiers() & Qt::ControlModifier) {
        // 处理自己的事件 (例如调整曝光)
    } else {
        // 转发给父控件处理 (例如缩放)
        if (parentWidget()) {
            // ... 转发逻辑 ...
            QApplication::sendEvent(parentWidget(), &parentEvent);
            event->accept();
        } else {
            QOpenGLWidget::wheelEvent(event);
        }
    }
}
```
这个逻辑是正确的，它保证了在未按下`Ctrl`键时，滚轮事件会被用于视图缩放。

### 总结

通过以上修改，整个HDR激活流程将变为：

1.  用户勾选 `Enable native 10-bit display`。
2.  `setHDRRenderingEnabled(true)` 被调用，启动**异步**的 `HDRDetectionWorker`。
3.  `HDRDetectionWorker` 完成检测，在主线程触发 `onHDRDetectionComplete`。
4.  `onHDRDetectionComplete` 创建 `HDR_VideoWidget`，**连接 `widgetInitialized` 信号**，并请求 `splitView` 显示它。
5.  Qt事件循环处理显示请求，`HDR_VideoWidget` 变为可见，其 `paintEvent` 被触发。
6.  `HDR_VideoWidget` 的 `initializeGL()` 被调用，成功创建OpenGL资源，`m_initialized`设为`true`，最后 **发出 `widgetInitialized` 信号**。
7.  `HDRRenderingManager` 的槽函数接收到 `widgetInitialized` 信号，它现在知道widget已万事俱备，于是发出 `hdrRenderingStateChanged(true, m_hdrWidget)` 信号。
8.  主渲染逻辑（`videoHandlerYUV`）收到此信号，开始将后续的视频帧通过 `updateHDRFrame` 推送到 `HDR_VideoWidget`。
9.  `HDR_VideoWidget` 在其 `paintGL` 中安全地使用这些帧进行渲染。

这个流程消除了竞态条件，保证了所有操作都在正确的时机发生，从而修复灰色屏幕的问题，并能正确地显示10-bit HDR内容。
