### 现状与任务
目前的程序离成功实现原生10bit HDR输出的目标很近了，但是输出仍未达预期。
我按照如下步骤操作才能获得HDR显示支持：
- 在Debug模式下运行，加载1024灰阶，420p10le格式的YUV；
- 点击`Enable native 10-bit display`之后，整个软件的窗口会被重新加载，渲染画面变为纯黑色；
- 再次点击`Enable native 10-bit display`，关闭此选项后，YUV回到了原来的QPainter SDR画面；
- 再次点击`Enable native 10-bit display`，再次勾选此选项，并滚动鼠标滚轮更改放大率后，才能显示完整灰阶的HDR的画面；
显示完HDR画面后，画面也完全不能通过鼠标滚轮进行放大、缩小，也不能在playlist当中正常的播放视频。

另外，这个画面的颜色明显偏绿，而我的输入是灰阶图像，图像的显示也明显的不正常。

请你仔细阅读我的问题单描述、调式日志和边界条件，修复代码以完成我的需求
### 设备、边界条件
双屏幕，其中一台在windows11中已开启HDR显示的功能，另一台不支持HDR。
当您的YUView软件被拖动到HDR屏幕的时候，您应该在点击`Enable native 10-bit display`之后，正确的调用OpenGL实现HDR的输出；
如果软件被拖动到SDR屏幕中，您应该在点击`Enable native 10-bit display`之后，明确提示当前屏幕不支持HDR，并自动关闭`Enable native 10-bit display`可操作性。
边界条件可能有：
- 软件在SDR单屏幕上运行；
- 软件在HDR单屏幕上运行；
- 软件在SDR单屏幕上被启动，然后被拖动到HDR屏幕上；
- 软件在HDR单屏幕上被启动，然后被拖动到SDR屏幕上；

### 调试日志
我们目前已经在程序的每个HDR的渲染步骤中打了调试日志：
在缩放图像，点击`Enable native 10-bit display`前后，其日志输出是：
```
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::slot10BitDisplayChanged() called ===
videoHandlerYUV: 10-bit display checkbox state: true
videoHandlerYUV: Saved Enable10BitDisplay setting to: true
videoHandlerYUV: Delegating HDR enable request to HDRRenderingManager
=== HDRRenderingManager::setHDRRenderingEnabled() called ===
HDRRenderingManager: HDR rendering enable request: true
HDRRenderingManager: Current HDR state: false
HDRRenderingManager: Changing HDR state from false to true
HDRRenderingManager: === ENABLING HDR RENDERING ===
HDRRenderingManager: 10-bit display requested, starting HDR detection...
HDRRenderingManager: Deferring HDR detection to next event loop iteration
videoHandlerYUV: HDR enable request completed
HDRRenderingManager: QTimer callback - starting HDR detection now
=== HDRRenderingManager::startHDRDetection() called ===
HDRRenderingManager: Starting background HDR detection...
HDRRenderingManager: HDR detection worker exists: false
HDRRenderingManager: Creating new HDRDetectionWorker...
HDRRenderingManager: HDRDetectionWorker created at: HDRDetectionWorker(0x1e106814790)
HDRRenderingManager: Connecting HDR detection signals...
HDRRenderingManager: HDR detection signals connected successfully
HDRRenderingManager: Checking if HDR detection is already in progress...
HDRRenderingManager: Starting HDR detection in background thread...
Starting simplified HDR detection (non-blocking)...
HDRRenderingManager: HDR detection started successfully in background thread
HDR detection worker started (simplified, non-blocking)
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "CG319X" Depth: 32 bits
Performing DXGI HDR detection for display: "CG319X"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 455.523 nits
HDR detection completed. Supported: true
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
HDRRenderingManager: Found split view widget: splitViewWidget(0x1e1066f23c0, name="displaySplitView")
=== HDRRenderingManager::createHDRWidget() called ===
HDRRenderingManager: HDR widget creation requested with parent: splitViewWidget(0x1e1066f23c0, name="displaySplitView")
HDRRenderingManager: Current HDR rendering state: true
HDRRenderingManager: HDR widget exists: false
HDRRenderingManager: === CREATING NEW HDR WIDGET ===
HDRRenderingManager: Creating persistent HDR widget for push model architecture
HDRRenderingManager: Parent widget pointer: splitViewWidget(0x1e1066f23c0, name="displaySplitView")
HDRRenderingManager: HDR capabilities supported: true
HDR Surface Format: Starting with standard 8-bit format for stability
HDRRenderingManager: HDR_VideoWidget object created at: HDR_VideoWidget(0x1e12e17de80)
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
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR_VideoWidget::paintGL: No frame to render (frame is null)
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR Video Widget FPS: 0 frames/sec
=== videoHandlerYUV::slot10BitDisplayChanged() called ===
videoHandlerYUV: 10-bit display checkbox state: false
videoHandlerYUV: Saved Enable10BitDisplay setting to: false
videoHandlerYUV: Delegating HDR enable request to HDRRenderingManager
=== HDRRenderingManager::setHDRRenderingEnabled() called ===
HDRRenderingManager: HDR rendering enable request: false
HDRRenderingManager: Current HDR state: true
HDRRenderingManager: Changing HDR state from true to false
HDRRenderingManager: === DISABLING HDR RENDERING ===
HDRRenderingManager: HDR widget exists: true
HDRRenderingManager: Hiding HDR widget and emitting display signal
HDRRenderingManager: HDR widget hidden successfully
HDRRenderingManager: Emitting HDR rendering state changed signal (disabled)
HDRRenderingManager: HDR rendering disabled successfully
videoHandlerYUV: HDR enable request completed
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: false
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::slot10BitDisplayChanged() called ===
videoHandlerYUV: 10-bit display checkbox state: true
videoHandlerYUV: Saved Enable10BitDisplay setting to: true
videoHandlerYUV: Delegating HDR enable request to HDRRenderingManager
=== HDRRenderingManager::setHDRRenderingEnabled() called ===
HDRRenderingManager: HDR rendering enable request: true
HDRRenderingManager: Current HDR state: false
HDRRenderingManager: Changing HDR state from false to true
HDRRenderingManager: === ENABLING HDR RENDERING ===
HDRRenderingManager: HDR already supported and enabled
HDRRenderingManager: HDR Mode: 1
HDRRenderingManager: Max Luminance: 455.523 nits
videoHandlerYUV: HDR enable request completed
HDR Video Widget FPS: 0 frames/sec
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: true
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: *** IMPLEMENTING HDR PUSH MODEL ARCHITECTURE ***
videoHandlerYUV: HDR widget retrieved: true
videoHandlerYUV: Getting current frame as QImage for HDR rendering...
videoHandlerYUV: Current frame image size: QSize(1280, 720)
videoHandlerYUV: Current frame image is null: false
videoHandlerYUV: === USING HDR RENDERING PATH ===
=== HDRRenderingManager::updateHDRFrame() called ===
HDRRenderingManager: Frame size: QSize(1280, 720)
HDRRenderingManager: Frame format: QImage::Format_RGBA64_Premultiplied
HDRRenderingManager: HDR widget exists: true
HDRRenderingManager: HDR rendering active: true
HDRRenderingManager: === PUSHING FRAME TO HDR WIDGET ===
HDRRenderingManager: HDR widget is hidden, showing it now
HDRRenderingManager: Calling HDR_VideoWidget::updateFrame()...
HDRRenderingManager: Frame data successfully pushed to HDR widget
videoHandlerYUV: HDR frame update completed, skipping QPainter rendering
HDR_VideoWidget: Using RGB10_A2 texture format for 10-bit HDR
HDR_VideoWidget: Texture uploaded successfully - size: 1280 x 720 format: "RGB10_A2"
HDR_VideoWidget::updateFrame: Texture uploaded, triggering repaint
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR_VideoWidget::paintGL: Frame rendered successfully
SplitViewWidget: Skipping QPainter rendering - HDR overlay active
HDR Video Widget FPS: 1 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
.......
```
堆栈日志是：

```asm
1 ntdll!DbgBreakPoint            0x7ff818613641 

0x7ff818613640                  cc                       int3
0x7ff818613641  <+    1>        c3                       ret ; 我们位于此处
0x7ff818613642  <+    2>        cc                       int3
0x7ff818613643  <+    3>        cc                       int3
0x7ff818613644  <+    4>        cc                       int3
0x7ff818613645  <+    5>        cc                       int3
2 ntdll!DbgUiRemoteBreakin       0x7ff818646f6e 
0x7ff818646f5f  <+   63>        48 85 c0                    test   %rax,%rax
0x7ff818646f62  <+   66>        74 05                       je     0x7ff818646f69 <ntdll!DbgUiRemoteBreakin+73>
0x7ff818646f64  <+   68>        ff d0                       call   *%rax
0x7ff818646f66  <+   70>        0f 1f 00                    nopl   (%rax)
0x7ff818646f69  <+   73>        e8 d2 c6 fc ff              call   0x7ff818613640 <ntdll!DbgBreakPoint>
0x7ff818646f6e  <+   78>        eb 00                       jmp    0x7ff818646f70 <ntdll!DbgUiRemoteBreakin+80> ;位于此处
0x7ff818646f70  <+   80>        33 c9                       xor    %ecx,%ecx
0x7ff818646f72  <+   82>        e8 09 3b f8 ff              call   0x7ff8185caa80 <ntdll!RtlExitUserThread>
0x7ff818646f77  <+   87>        90                          nop
0x7ff818646f78  <+   88>        71 90                       jno    0x7ff818646f0a <ntdll!DbgUiIssueRemoteBreakin+90>
0x7ff818646f7a  <+   90>        5b                          pop    %rbx
0x7ff818646f7b  <+   91>        12 e7                       adc    %bh,%ah
0x7ff818646f7d  <+   93>        9e                          sahf
0x7ff818646f7e  <+   94>        70 ce                       jo     0x7ff818646f4e <ntdll!DbgUiRemoteBreakin+46>
3 KERNEL32!BaseThreadInitThunk   0x7ff8170a257d 
4 ntdll!RtlUserThreadStart       0x7ff8185caa48 
```

