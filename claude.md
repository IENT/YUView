### **总任务：修复原生10bit HDR输出功能，并完善多显示器支持**

### **1. 目标 (Goal)**

修复当前程序在启用原生10-bit HDR输出时遇到的激活流程、画面显示错误和功能限制问题。最终目标是实现稳定、正确且用户友好的原生10-bit HDR渲染功能，并能正确处理在HDR与SDR显示器间的切换。

### **2. 系统环境 (System Environment)**

*   **操作系统**: Windows 11
*   **显示设置**: 双显示器配置，其中一个为已在系统设置中启用HDR的主显示器，另一个为SDR显示器。
*   **测试文件**: 1024级灰阶图像，720p, YUV 4:2:0, 10-bit `420p10le` 格式。

### **3. 问题描述 (Problem Description)**

当前实现存在主要问题：激活流程异常，用户交互错误。

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

1.  打开YUView之后，必须在程序启动之后**立即**勾选`Enable native 10-bit display`，才有较小的几率显示出HDR图像，这时整个YUView的Windows界面都会被重新加载,然后可以正确的弹出HDR的界面；
2.  对于绝大多数的情况，勾选`Enable native 10-bit display`之后，根本就不会有任何反应，**滚动鼠标滚轮**后，整个图像会全部变成灰色（无论缩放为多大的放大倍率），取消勾选`Enable native 10-bit display`之后，图像恢复正常。
- 对于其他情况：
图像完全变成灰色。

### **5. 调试日志 (Debug Logs)**

1. 程序可以
为了进一步分析，以下是在可以成功显示HDR的前后，点击 `Enable native 10-bit display`前后捕获的调试日志。
- 对于可以成功加载HDR的情况
```
14:41:18: Debugging D:\SiruiWu_code\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Debug\YUViewApp\YUView.exe ...
=== splitViewWidget::checkCurrentDisplayHDRSupport() - Screen changed ===
Previous screen: "null"
Current screen: "CG319X"
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "CG319X" Depth: 32 bits
Performing DXGI HDR detection for display: "CG319X"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 455.523 nits
Display: "CG319X" HDR supported: true
HDR support changed - emitting signal: true
=== splitViewWidget::checkCurrentDisplayHDRSupport() - Screen changed ===
Previous screen: "null"
Current screen: "CG319X"
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "CG319X" Depth: 32 bits
Performing DXGI HDR detection for display: "CG319X"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 455.523 nits
Display: "CG319X" HDR supported: true
HDR support changed - emitting signal: true
=== splitViewWidget::checkCurrentDisplayHDRSupport() - Screen changed ===
Previous screen: "CG319X"
Current screen: "XWU-CBA"
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "XWU-CBA" Depth: 32 bits
Performing DXGI HDR detection for display: "XWU-CBA"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 455.523 nits
Display: "XWU-CBA" HDR supported: true
onecoreuap\internal\shell\inc\private\SharedStorageSources\dvthumbnail.cpp(2295)\SHELL32.dll!00007FF81741DBF8: (caller: 00007FF8172D497F) ReturnHr(1) tid(5a34) 80070057 ����������
onecore\vm\dv\storage\plan9\rdr\dll\util.cpp(99)\p9np.dll!00007FFFC639F0CC: (caller: 00007FFFC63993B0) LogHr(1) tid(1b38) C0000034     Msg:[�����V���꾴?���Pɢ?�L?�u����??����������?��î�b��?���N�ᠹ????߾?��?Ҳ�y?????߲??��?????߲??��??��??��?�珔??????��ƫ��???߲��?�ֵ͵�?����??߼?���H��??)] 
=== HDRRenderingManager: Constructor called ===
HDRRenderingManager: Initializing with parent: video::yuv::videoHandlerYUV(0x190f532e340)
HDRRenderingManager: HDR capabilities initialized to NOT SUPPORTED
HDRRenderingManager: Constructor completed successfully
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: false
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
dataChanged() called with an invalid index range:
    topleft: QModelIndex(-1,-1,0x0,QObject(0x0))
    bottomRight:QModelIndex(-1,-1,0x0,QObject(0x0))=== videoHandlerYUV::slot10BitDisplayChanged() called ===
videoHandlerYUV: 10-bit display checkbox state: true
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "CG319X" Depth: 32 bits
Performing DXGI HDR detection for display: "CG319X"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 455.523 nits
videoHandlerYUV: HDR supported - proceeding with activation
videoHandlerYUV: Saved Enable10BitDisplay setting to: true
videoHandlerYUV: Delegating HDR enable request to HDRRenderingManager
=== HDRRenderingManager::setHDRRenderingEnabled() called ===
HDRRenderingManager: HDR rendering enable request: true
HDRRenderingManager: Current HDR state: false
HDRRenderingManager: Changing HDR state from false to true
HDRRenderingManager: === ENABLING HDR RENDERING ===
HDRRenderingManager: Performing synchronous HDR detection for immediate feedback
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "CG319X" Depth: 32 bits
Performing DXGI HDR detection for display: "CG319X"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 455.523 nits
HDRRenderingManager: Immediate HDR detection - HDR supported
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
HDRRenderingManager: Found split view widget: splitViewWidget(0x190cd822b30, name="displaySplitView")
=== HDRRenderingManager::createHDRWidget() called ===
HDRRenderingManager: HDR widget creation requested with parent: splitViewWidget(0x190cd822b30, name="displaySplitView")
HDRRenderingManager: Current HDR rendering state: true
HDRRenderingManager: HDR widget exists: false
HDRRenderingManager: === CREATING NEW HDR WIDGET ===
HDRRenderingManager: Creating persistent HDR widget for push model architecture
HDRRenderingManager: Parent widget pointer: splitViewWidget(0x190cd822b30, name="displaySplitView")
HDRRenderingManager: HDR capabilities supported: true
HDR Surface Format: Starting with standard 8-bit format for stability
HDRRenderingManager: HDR_VideoWidget object created at: HDR_VideoWidget(0x190f516d170)
HDRRenderingManager: Connecting HDR widget signals...
HDRRenderingManager: HDR widget signals connected successfully
HDRRenderingManager: Setting HDR capabilities on widget...
HDRRenderingManager: HDR Mode: 1
HDRRenderingManager: Max Luminance: 455.523 nits
HDRRenderingManager: Display Name: "\\\\.\\DISPLAY2"
HDR capabilities set: Mode: "HDR10/BT.2020 PQ (10-bit)" Max Luminance: 455.523 nits Bits per channel: 10
Display max luminance set to: 455.523 nits
HDR texture format changed to: "RGB10_A2"
Upgrading surface format to HDR for mode: "HDR10/BT.2020 PQ (10-bit)"
Upgrading to BT2020 PQ 10-bit format
HDR surface format upgrade requested - will take effect on next context creation
HDRRenderingManager: Setting render mode based on capabilities...
HDRRenderingManager: Setting BT2020 PQ 10-bit mode
HDR render mode validation: HDR supported, mode "BT2020_PQ_10bit" is valid
HDR FPS monitoring started
YUView HDR: Render mode changed
HDR render mode changed to: "BT2020_PQ_10bit"
HDRRenderingManager: HDR widget created successfully with parent integration
HDRRenderingManager: Widget ready for display and frame updates
Initializing HDR Video Widget with OpenGL 3 . 3
HDR Surface Format validation:
  Red buffer size: 10
  Green buffer size: 10
  Blue buffer size: 10
  Alpha buffer size: 2
  Color space: QColorSpace()
  Is HDR format: false
HDR format requested but not available, will simulate HDR in shaders
HDR shaders compiled and linked successfully
HDR geometry initialized successfully
HDR texture object created successfully
HDR Video Widget initialized successfully
HDR Widget resized to: 1427 x 969
HDR_VideoWidget::paintGL: No frame to render (frame is null)
splitViewWidget: HDR overlay widget configured - geometry: QRect(0,0 1427x969)
splitViewWidget: HDR overlay widget visible: true
splitViewWidget: HDR overlay widget size: QSize(1427, 969)
splitViewWidget: HDR overlay visibility set to: true
HDRRenderingManager: HDR widget integrated with split view
=== videoHandlerYUV::onHDRRenderingStateChanged() called ===
videoHandlerYUV: HDR rendering state changed to: true
videoHandlerYUV: HDR widget pointer: HDR_VideoWidget(0x190f516d170)
videoHandlerYUV: HDR enabled - triggering immediate frame update
videoHandlerYUV: Conditions met for HDR rendering - forcing frame update
videoHandlerYUV: Pushing current frame to HDR widget
=== HDRRenderingManager::updateHDRFrame() called ===
HDRRenderingManager: Frame size: QSize(1280, 720)
HDRRenderingManager: Frame format: QImage::Format_ARGB32_Premultiplied
HDRRenderingManager: HDR widget exists: true
HDRRenderingManager: HDR rendering active: true
HDRRenderingManager: === PUSHING FRAME TO HDR WIDGET ===
HDRRenderingManager: Calling HDR_VideoWidget::updateFrame()...
HDRRenderingManager: Frame data successfully pushed to HDR widget
videoHandlerYUV: HDR state change handling completed
videoHandlerYUV: HDR enable request completed
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR_VideoWidget::paintGL: Video texture not created yet
HDR_VideoWidget: Using RGBA16F internal format for RGB10_A2 compatibility
HDR_VideoWidget: Using simplified RGBA8888 upload for RGB10_A2 texture
HDR_VideoWidget: Texture uploaded successfully - size: 1280 x 720 format: "RGB10_A2"
HDR_VideoWidget::updateFrame: Texture uploaded, triggering repaint
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR_VideoWidget::paintGL: Frame rendered successfully
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR Video Widget FPS: 1 frames/sec

```

对于图像显示为灰色的情况：

```
videoHandlerYUV: === USING HDR RENDERING PATH ===
=== HDRRenderingManager::updateHDRFrame() called ===
HDRRenderingManager: Frame size: QSize(1280, 720)
HDRRenderingManager: Frame format: QImage::Format_ARGB32_Premultiplied
HDRRenderingManager: HDR widget exists: true
HDRRenderingManager: HDR rendering active: true
HDRRenderingManager: === PUSHING FRAME TO HDR WIDGET ===
HDRRenderingManager: HDR widget is hidden, showing it now
HDRRenderingManager: Calling HDR_VideoWidget::updateFrame()...
HDRRenderingManager: Frame data successfully pushed to HDR widget
videoHandlerYUV: HDR frame update completed, skipping QPainter rendering
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
```


在1 operator() HDR_VideoWidget.cpp 101 0x7ff667cc040f打完断点后，
- 对于极少数可以正确的显示HDR的情况：
```
		__closure	@0x20c2633e780	struct {...}
		this	<无法访问>	HDR_VideoWidget
			[QOpenGLWidget]	@0x20c2623f0d0	QOpenGLWidget
			[QOpenGLFunctions_3_3_Core]	<无法访问>	QOpenGLFunctions_3_3_Core
				[0]		
			[34]		
			m_currentFrame	(1280x720)	QImage
				width	1280	int
				height	720	int
				nbytes	7372800	int
				format	27	int
				data	0x20c221c7040	void *
			m_displayMaxLuminance	455.523193	float
			m_displayMaxLuminanceLocation	2	int
			m_dragging	false	bool
			m_fpsTimer	<无法访问>	QTimer
				[0]		
			m_frameCount	0	int
			m_frameSize	(1280, 720)	QSize
			m_frameUpdated	true	bool
			m_hdrCapable	true	bool
			m_hdrExposure	0	float
			m_hdrExposureLocation	5	int
			m_hdrGamma	1	float
			m_hdrGammaLocation	4	int
			m_indexBuffer	@0x20c26345930	QOpenGLBuffer
				d_ptr	@0x20c26345970	QOpenGLBufferPrivate
			m_initialized	true	bool
			m_lastFpsUpdate	0	qint64
			m_lastMousePos	(0, 0)	QPoint
				xp	0	int
				yp	0	int
			m_projectionMatrix	@0x20c2623f214	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x20c2623f214	float[4][4]
			m_projectionMatrixLocation	0	int
			m_renderMode	HDR_VideoWidget::Mode_BT2020_PQ_10bit (1)	HDR_VideoWidget::RenderMode
			m_renderModeLocation	7	int
			m_shaderProgram	<无法访问>	QOpenGLShaderProgram
				[0]		
			m_sourceMaxLuminance	1000	float
			m_sourceMaxLuminanceLocation	3	int
			m_textureFormat	HDR_VideoWidget::Format_RGB10_A2 (2)	HDR_VideoWidget::TextureFormat
			m_textureLocation	6	int
			m_textureMatrix	@0x20c2623f1d0	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x20c2623f1d0	float[4][4]
			m_textureMatrixLocation	1	int
			m_vertexArrayObject	<无法访问>	QOpenGLVertexArrayObject
				[0]		
			m_vertexBuffer	@0x20c263452e0	QOpenGLBuffer
				d_ptr	@0x20c2633e720	QOpenGLBufferPrivate
			m_videoTexture	@0x20c26345a50	QOpenGLTexture
				d_ptr	@0x20c26345a90	QScopedPointer<QOpenGLTexturePrivate>
			staticMetaObject	@0x7ff6681668e0	QMetaObject

```

- 对于极大多数根本就不能显示HDR的情况：
```
		__closure	@0x152998cfef0	struct {...}
		this	<无法访问>	HDR_VideoWidget
			[QOpenGLWidget]	@0x152998e88c0	QOpenGLWidget
			[QOpenGLFunctions_3_3_Core]	<无法访问>	QOpenGLFunctions_3_3_Core
				[0]		
			[34]		
			m_currentFrame	(1280x720)	QImage
				width	1280	int
				height	720	int
				nbytes	7372800	int
				format	27	int
				data	0x152a0ce5040	void *
			m_displayMaxLuminance	455.523193	float
			m_displayMaxLuminanceLocation	-1	int
			m_dragging	false	bool
			m_fpsTimer	<无法访问>	QTimer
				[0]		
			m_frameCount	0	int
			m_frameSize	(1280, 720)	QSize
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
			m_projectionMatrix	@0x152998e8a04	QMatrix4x4
			m_projectionMatrixLocation	-1	int
			m_renderMode	HDR_VideoWidget::Mode_BT2020_PQ_10bit (1)	HDR_VideoWidget::RenderMode
			m_renderModeLocation	-1	int
			m_shaderProgram	0x0	QOpenGLShaderProgram*
			m_sourceMaxLuminance	1000	float
			m_sourceMaxLuminanceLocation	-1	int
			m_textureFormat	HDR_VideoWidget::Format_RGB10_A2 (2)	HDR_VideoWidget::TextureFormat
			m_textureLocation	-1	int
			m_textureMatrix	@0x152998e89c0	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x152998e89c0	float[4][4]
			m_textureMatrixLocation	-1	int
			m_vertexArrayObject	0x0	QOpenGLVertexArrayObject*
			m_vertexBuffer	0x0	QOpenGLBuffer*
			m_videoTexture	0x0	QOpenGLTexture*
			staticMetaObject	@0x7ff6681668e0	QMetaObject
```
