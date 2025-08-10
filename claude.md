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
- 对于其他情况：
图像完全变成灰色，输出同1。

### **4. 调试日志 (Debug Logs)**

为了进一步分析，以下是在可以成功显示HDR的前后，点击 `Enable native 10-bit display`前后捕获的调试日志。

对于图像显示失败，只能呈现为灰色的情况：

```
22:39:33: Debugging D:\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Debug_software\YUViewApp\YUView.exe ...
=== splitViewWidget::checkCurrentDisplayHDRSupport() - Screen changed ===
Previous screen: "null"
Current screen: "\\\\.\\DISPLAY1"
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "\\\\.\\DISPLAY1" Depth: 32 bits
Performing DXGI HDR detection for display: "\\\\.\\DISPLAY1"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 1015.27 nits
Display: "\\\\.\\DISPLAY1" HDR supported: true
HDR support changed - emitting signal: true
=== splitViewWidget::checkCurrentDisplayHDRSupport() - Screen changed ===
Previous screen: "null"
Current screen: "\\\\.\\DISPLAY1"
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "\\\\.\\DISPLAY1" Depth: 32 bits
Performing DXGI HDR detection for display: "\\\\.\\DISPLAY1"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 1015.27 nits
Display: "\\\\.\\DISPLAY1" HDR supported: true
HDR support changed - emitting signal: true
=== splitViewWidget::checkCurrentDisplayHDRSupport() - Screen changed ===
Previous screen: "\\\\.\\DISPLAY1"
Current screen: "27G7K-PRO"
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "27G7K-PRO" Depth: 32 bits
Performing DXGI HDR detection for display: "27G7K-PRO"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 1015.27 nits
Display: "27G7K-PRO" HDR supported: true
QPainter::begin: Paint device returned engine == 0, type: 3
QPainter::setCompositionMode: Painter not active
QPainter::fillRect: Painter not active
QPainter::setCompositionMode: Painter not active
QPainter::setBrush: Painter not active
QPainter::setPen: Painter not active
QPainter::drawPath: Painter not active
QPainter::setPen: Painter not active
QPainter::setFont: Painter not active
QPainter::setFont: Painter not active
QPainter::setBrush: Painter not active
QPainter::setPen: Painter not active
QPainter::setPen: Painter not active
QPainter::setBrush: Painter not active
QPainter::setPen: Painter not active
QPainter::setPen: Painter not active
QPainter::setBrush: Painter not active
QPainter::setPen: Painter not active
QPainter::setPen: Painter not active
QPainter::end: Painter not active, aborted
onecore\vm\dv\storage\plan9\rdr\dll\util.cpp(99)\p9np.dll!00007FF82DFB32F0: (caller: 00007FF82DFAA6EA) LogHr(1) tid(3984) C0000034     Msg:[NtCreateFile(&device, SYNCHRONIZE, &attributes, &ioStatus, nullptr, FILE_ATTRIBUTE_NORMAL, (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0)] 
onecoreuap\internal\shell\inc\SrcPkg\FileExplorerSessionWatcher\inc\FileExplorerSessionWatcher.h(1762)\SHELL32.dll!00007FF8463DECA8: (caller: 00007FF8462EB092) ReturnHr(1) tid(3204) 80004001 Not implemented
shell\SrcPkg\FileExplorer\DefView\src\DefView.cpp(17832)\SHELL32.dll!00007FF8462EB0B1: (caller: 00007FF80DDD79BF) LogHr(1) tid(3204) 80004001 Not implemented
shell\explorerframe\navbar.cpp(82)\explorerframe.dll!00007FF80DEDAD05: (caller: 00007FF80DDBFE7E) LogHr(1) tid(3204) 80070057 The parameter is incorrect.
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "\\\\.\\DISPLAY1" Depth: 32 bits
Performing DXGI HDR detection for display: "\\\\.\\DISPLAY1"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 1015.27 nits
videoHandlerYUV: HDR supported - proceeding with activation
videoHandlerYUV: Saved Enable10BitDisplay setting to: true
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "\\\\.\\DISPLAY1" Depth: 32 bits
Performing DXGI HDR detection for display: "\\\\.\\DISPLAY1"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 1015.27 nits
=== HDRRenderingManager::onHDRDetectionComplete() called ===
HDRRenderingManager: HDR detection completed in main thread
HDRRenderingManager: HDR supported: true
HDRRenderingManager: HDR mode: 1
HDRRenderingManager: Max luminance: 1015.27 nits
HDRRenderingManager: Min luminance: 0 nits
HDRRenderingManager: Bits per channel: 10
HDRRenderingManager: Display name: "\\\\.\\DISPLAY2"
HDRRenderingManager: === HDR IS SUPPORTED ===
HDRRenderingManager: Storing HDR capabilities...
HDRRenderingManager: HDR capabilities stored and rendering enabled
HDRRenderingManager: HDR rendering enabled (integrated mode): "HDR10/BT.2020 PQ (10-bit)"
HDRRenderingManager: Looking for split view widget...
HDRRenderingManager: Found split view widget: splitViewWidget(0x344cfff518)
HDR Surface Format: Starting with standard 8-bit format for stability
HDR capabilities set: Mode: "HDR10/BT.2020 PQ (10-bit)" Max Luminance: 1015.27 nits Bits per channel: 10
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
videoHandlerYUV: Preserving 16-bit image format for true 10-bit HDR rendering
HDRRenderingManager: Widget not ready, attempting frame push anyway
HDR_VideoWidget::updateFrame: Received frame QSize(1280, 720) format: QImage::Format_RGBA64_Premultiplied initialized: false
HDR_VideoWidget::updateFrame: Frame stored, size: QSize(1280, 720) updated flag: true
HDR_VideoWidget::updateFrame: Widget not initialized, frame queued for later processing
HDR_VideoWidget::updateFrame: Cannot initialize yet - context: null size: 640 x 480
videoHandlerYUV: Preserving 16-bit image format for true 10-bit HDR rendering
HDRRenderingManager: Widget not ready, attempting frame push anyway
HDR_VideoWidget::updateFrame: Received frame QSize(1280, 720) format: QImage::Format_RGBA64_Premultiplied initialized: false
HDR_VideoWidget::updateFrame: Frame stored, size: QSize(1280, 720) updated flag: true
HDR_VideoWidget::updateFrame: Widget not initialized, frame queued for later processing
HDR_VideoWidget::updateFrame: Cannot initialize yet - context: null size: 640 x 480
videoHandlerYUV: Preserving 16-bit image format for true 10-bit HDR rendering
HDRRenderingManager: Widget not ready, attempting frame push anyway
HDR_VideoWidget::updateFrame: Received frame QSize(1280, 720) format: QImage::Format_RGBA64_Premultiplied initialized: false
HDR_VideoWidget::updateFrame: Frame stored, size: QSize(1280, 720) updated flag: true
HDR_VideoWidget::updateFrame: Widget not initialized, frame queued for later processing
HDR_VideoWidget::updateFrame: Cannot initialize yet - context: null size: 640 x 480
videoHandlerYUV: Preserving 16-bit image format for true 10-bit HDR rendering
HDRRenderingManager: Widget not ready, attempting frame push anyway
HDR_VideoWidget::updateFrame: Received frame QSize(1280, 720) format: QImage::Format_RGBA64_Premultiplied initialized: false
HDR_VideoWidget::updateFrame: Frame stored, size: QSize(1280, 720) updated flag: true
HDR_VideoWidget::updateFrame: Widget not initialized, frame queued for later processing
HDR_VideoWidget::updateFrame: Cannot initialize yet - context: null size: 640 x 480

```


在1 operator() HDR_VideoWidget.cpp 108行打断点时，断点不会被触发。

**堆栈回溯：**
```                                                                                                                                                                                                                                                                                                       qobjectdefs_impl.h    116 0x7ff72c499851 
3  QtPrivate::FunctorCallBase::call_internal<void, QtPrivate::FunctorCall<std::integer_sequence<long long unsigned int>, QtPrivate::List<>, void, HDR_VideoWidget::HDR_VideoWidget(QWidget *)::<lambda()>>::call(HDR_VideoWidget::HDR_VideoWidget(QWidget *)::<lambda()>&, void * *)::<lambda()>>(void * *, struct {...} &&) qobjectdefs_impl.h    65  0x7ff72c499a83 
4  QtPrivate::FunctorCall<std::integer_sequence<long long unsigned int>, QtPrivate::List<>, void, HDR_VideoWidget::HDR_VideoWidget(QWidget *)::<lambda()>>::call(struct {...} &, void * *)                                                                                                                                   qobjectdefs_impl.h    115 0x7ff72c499888 
5  QtPrivate::FunctorCallable<HDR_VideoWidget::HDR_VideoWidget(QWidget *)::<lambda()>>::call<QtPrivate::List<>, void>(struct {...} &, void *, void * *)                                                                                                                                                                      qobjectdefs_impl.h    337 0x7ff72c49929e 
6  QtPrivate::QCallableObject<HDR_VideoWidget::HDR_VideoWidget(QWidget *)::<lambda()>, QtPrivate::List<>, void>::impl(int, QtPrivate::QSlotObjectBase *, QObject *, void * *, bool *)                                                                                                                                        qobjectdefs_impl.h    547 0x7ff72c498fba 
20 YUViewApplication::YUViewApplication                                                                                                                                                                                                                                                                                      YUViewApplication.cpp 131 0x7ff72c3d2175
```
调试信息如下：
```
		Locals		
		__closure	@0x2b6d0da9f30	struct {...}
		currentTime	1754831289384	qint64
		this	<not accessible>	HDR_VideoWidget
			[QOpenGLWidget]	<not accessible>	QOpenGLWidget
				[0]		
			[QOpenGLFunctions_3_3_Core]	<not accessible>	QOpenGLFunctions_3_3_Core
				[0]		
			[34]		
			m_currentFrame	(invalid)	QImage
			m_displayMaxLuminance	1015.27252	float
			m_displayMaxLuminanceLocation	-1	int
			m_dragging	false	bool
			m_fpsTimer	<not accessible>	QTimer
				[0]		
			m_frameCount	0	int
			m_frameSize	(-1, -1)	QSize
				wd	-1	int
				ht	-1	int
			m_frameUpdated	false	bool
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
			m_projectionMatrix	@0x2b6d0ec2c74	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x2b6d0ec2c74	float[4][4]
					[0]	@0x2b6d0ec2c74	float[4]
							1	float
							0	float
							0	float
							0	float
					[1]	@0x2b6d0ec2c84	float[4]
							0	float
							1	float
							0	float
							0	float
					[2]	@0x2b6d0ec2c94	float[4]
					[3]	@0x2b6d0ec2ca4	float[4]
			m_projectionMatrixLocation	-1	int
			m_renderMode	HDR_VideoWidget::Mode_BT2020_PQ_10bit (1)	HDR_VideoWidget::RenderMode
			m_renderModeLocation	-1	int
			m_shaderProgram	0x0	QOpenGLShaderProgram*
			m_sourceMaxLuminance	1000	float
			m_sourceMaxLuminanceLocation	-1	int
			m_textureFormat	HDR_VideoWidget::Format_RGB10_A2 (2)	HDR_VideoWidget::TextureFormat
			m_textureLocation	-1	int
			m_textureMatrix	@0x2b6d0ec2c30	QMatrix4x4
				flagBits	QMatrix4x4::Identity (0x00000000)	QMatrix4x4::Flags
				m	@0x2b6d0ec2c30	float[4][4]
					[0]	@0x2b6d0ec2c30	float[4]
							1	float
							0	float
							0	float
							0	float
					[1]	@0x2b6d0ec2c40	float[4]
							0	float
							1	float
							0	float
							0	float
					[2]	@0x2b6d0ec2c50	float[4]
					[3]	@0x2b6d0ec2c60	float[4]
							0	float
							0	float
							0	float
							1	float
			m_textureMatrixLocation	-1	int
			m_vertexArrayObject	0x0	QOpenGLVertexArrayObject*
			m_vertexBuffer	0x0	QOpenGLBuffer*
			m_videoTexture	0x0	QOpenGLTexture*
			staticMetaObject	@0x7ff72c936720	QMetaObject
				[strings]	<at least 75 items>	
					[0]	"HDR_VideoWidget"	
					[1]	"widgetInitialized"	
					[2]	""	
					[3]	"hdrNotSupported"	
					[4]	"reason"	
					[5]	"renderModeChanged"	
					[6]	"RenderMode"	
					[7]	"mode"	
					[8]	"frameUpdated"	
					[9]	"openGLError"	
					[10]	"error"	
					[11]	"updateFrame"	
					[12]	"newFrame"	
					[13]	"clearFrame"	
					[14]	"setHDRExposureSlot"	
					[15]	"exposure"	
					[16]	"setHDRGammaSlot"	
					[17]	"gamma"	
					[18]	"<not available>"	
					[19]	"<not available>"	
					[20]	"<not available>"	
					[21]	"<not available>"	
					[22]	"<not available>"	
					[23]	"<not available>"	
					[24]	"<not available>"	
					[25]	"<not available>"	
					[26]	"<not available>"	
					[27]	"<not available>"	
					[28]	"<not available>"	
					[29]	"<not available>"	
					[30]	"<not available>"	
					[31]	"<not available>"	
					[32]	"<not available>"	
					[33]	"<not available>"	
					[34]	"<not available>"	
					[35]	"<not available>"	
					[36]	"<not available>"	
					[37]	"<not available>"	
					[38]	"<not available>"	
					[39]	"<not available>"	
					[40]	"<not available>"	
					[41]	"<not available>"	
					[42]	"<not available>"	
					[43]	""	
					[44]	""	
					[45]	""	
					[46]	""	
					[47]	""	
					[48]	""	
					[49]	"<not available>"	
					[50]	""	
					[51]	""	
					[52]	""	
					[53]	"<not available>"	
					[54]	""	
					[55]	"\014\000"	
					[56]	"\000"	
					[57]	"\000"	
					[58]	"\000\000"	
					[59]	"\000\000"	
					[60]	"\000"	
					[61]	"<not available>"	
					[62]	"<not available>"	
					[63]	""	
					[64]	"\000\013"	
					[65]	"<not available>"	
					[66]	"\000"	
					[67]	"\013\000"	
					[68]	"<not available>"	
					[69]	"\000"	
					[70]	"\000\005"	
					[71]	"<not available>"	
					[72]	""	
					[73]	"\000\000"	
					[74]	"<not available>"	
					[75]	"\000"	
				[raw]		
					revision	13	 
					classname	0	 
					classinfo	0	 
					methods	9 14	 
					properties	0 0	 
					enums/sets	0 0	 
					constructors	0 0	 
					flags	0	 
					signalCount	5	 
					method 0	1 0 68 2 6	 
					method 1	1 3 1 69 2	 
					method 2	6 2 5 1 72	 
					method 3	2 6 4 8 0	 
					method 4	75 2 6 6 9	 
					method 5	1 76 2 6 7	 
					method 6	11 1 79 2 10	 
					method 7	9 13 0 82 2	 
					method 8	10 11 14 1 83	 
				[properties]	<0 items>	
				[methods]	<9 items>	
				[superdata]	0x0	@QMetaObject *
				[members]		
					d	@0x7ff72c936720	QMetaObject::Data
	Inspector		
	Expressions		
	Return Value		
	Tooltip		

```
