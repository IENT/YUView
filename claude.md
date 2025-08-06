### 任务
修改现有的错误代码，根据日志和GDB调试信息，解决程序在点击“Enable native 10 bit HDR display”后产生的未定义行为，并继续实现屏幕上支持原生10bit HDR的输出，而在1024平滑过渡的测试灰阶图像上不会发生断层。
### 设备
双屏幕，其中一台在windows11中已开启HDR显示的功能，另一台不支持HDR。
### 调试日志
我们目前已经在程序的每个HDR的渲染步骤中打了调试日志，目前该程序在点击“Enable native 10 bit HDR display”后，画布自动退出，不再能显示任何画面：
其日志输出是：
```
22:18:27: Starting D:\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Debug_software\YUViewApp\YUView.exe...
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
=== HDRRenderingManager: Constructor called ===
HDRRenderingManager: Initializing with parent: video::yuv::videoHandlerYUV(0x14768825270)
HDRRenderingManager: HDR capabilities initialized to NOT SUPPORTED
HDRRenderingManager: Constructor completed successfully
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
=== videoHandlerYUV::drawFrame() HDR Check ===
videoHandlerYUV: HDR rendering active: false
videoHandlerYUV: 10-bit display enabled: true
videoHandlerYUV: Pixel format bits per sample: 10
videoHandlerYUV: Using standard QPainter rendering (HDR conditions not met)
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
HDRRenderingManager: HDRDetectionWorker created at: HDRDetectionWorker(0x1476874eb70)
HDRRenderingManager: Connecting HDR detection signals...
HDRRenderingManager: HDR detection signals connected successfully
HDRRenderingManager: Checking if HDR detection is already in progress...
HDRRenderingManager: Starting HDR detection in background thread...
Starting simplified HDR detection (non-blocking)...
HDRRenderingManager: HDR detection started successfully in background thread
HDR detection worker started (simplified, non-blocking)
YUView HDR Detection: Starting display capability analysis...
YUView HDR Detection: Analyzing screen: "\\\\.\\DISPLAY1" Depth: 32 bits
Performing DXGI HDR detection for display: "\\\\.\\DISPLAY1"
YUView HDR Detection: ? HDR display detected! Mode: "HDR10/BT.2020 PQ (10-bit)" Max luminance: 1015.27 nits
HDR detection completed. Supported: true
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
HDRRenderingManager: Found split view widget: splitViewWidget(0x375fbfee58)
=== HDRRenderingManager::createHDRWidget() called ===
HDRRenderingManager: HDR widget creation requested with parent: splitViewWidget(0x375fbfee58)
HDRRenderingManager: Current HDR rendering state: true
HDRRenderingManager: HDR widget exists: false
HDRRenderingManager: === CREATING NEW HDR WIDGET ===
HDRRenderingManager: Creating persistent HDR widget for push model architecture
HDRRenderingManager: Parent widget pointer: splitViewWidget(0x375fbfee58)
HDRRenderingManager: HDR capabilities supported: true
HDR Surface Format: Starting with standard 8-bit format for stability
HDRRenderingManager: HDR_VideoWidget object created at: HDR_VideoWidget(0x14768a1d1b0)
HDRRenderingManager: Connecting HDR widget signals...
HDRRenderingManager: HDR widget signals connected successfully
HDRRenderingManager: Setting HDR capabilities on widget...
HDRRenderingManager: HDR Mode: 1
HDRRenderingManager: Max Luminance: 1015.27 nits
HDRRenderingManager: Display Name: "\\\\.\\DISPLAY2"
HDR capabilities set: Mode: "HDR10/BT.2020 PQ (10-bit)" Max Luminance: 1015.27 nits Bits per channel: 10
Display max luminance set to: 1015.27 nits
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
splitViewWidget: HDR overlay widget set and shown
splitViewWidget: HDR overlay visibility set to: true
HDRRenderingManager: HDR widget integrated with split view
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
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
HDR Video Widget FPS: 0 frames/sec
```
堆栈日志是：

```asm

1 ntdll!DbgBreakPoint            0x7ff8469e5b91 
2 ntdll!DbgUiRemoteBreakin       0x7ff8469f7bce 
3 KERNEL32!BaseThreadInitThunk   0x7ff844dbe8d7 
4 ntdll!RtlUserThreadStart       0x7ff8468fc34c 
5 ??

0x7ff8469f7b80                  48 83 ec 28                 sub    $0x28,%rsp
0x7ff8469f7b84  <+    4>        65 48 8b 04 25 60 00 00 00  mov    %gs:0x60,%rax
0x7ff8469f7b8d  <+   13>        80 78 02 00                 cmpb   $0x0,0x2(%rax)
0x7ff8469f7b91  <+   17>        75 0a                       jne    0x7ff8469f7b9d <ntdll!DbgUiRemoteBreakin+29>
0x7ff8469f7b93  <+   19>        f6 04 25 d4 02 fe 7f 02     testb  $0x2,0x7ffe02d4
0x7ff8469f7b9b  <+   27>        74 33                       je     0x7ff8469f7bd0 <ntdll!DbgUiRemoteBreakin+80>
0x7ff8469f7b9d  <+   29>        65 48 8b 04 25 30 00 00 00  mov    %gs:0x30,%rax
0x7ff8469f7ba6  <+   38>        f6 80 ee 17 00 00 20        testb  $0x20,0x17ee(%rax)
0x7ff8469f7bad  <+   45>        75 21                       jne    0x7ff8469f7bd0 <ntdll!DbgUiRemoteBreakin+80>
0x7ff8469f7baf  <+   47>        83 3d ee bd 09 00 00        cmpl   $0x0,0x9bdee(%rip)        # 0x7ff846a939a4
0x7ff8469f7bb6  <+   54>        74 11                       je     0x7ff8469f7bc9 <ntdll!DbgUiRemoteBreakin+73>
0x7ff8469f7bb8  <+   56>        48 8b 05 69 07 0b 00        mov    0xb0769(%rip),%rax        # 0x7ff846aa8328
0x7ff8469f7bbf  <+   63>        48 85 c0                    test   %rax,%rax
0x7ff8469f7bc2  <+   66>        74 05                       je     0x7ff8469f7bc9 <ntdll!DbgUiRemoteBreakin+73>
0x7ff8469f7bc4  <+   68>        ff d0                       call   *%rax
0x7ff8469f7bc6  <+   70>        0f 1f 00                    nopl   (%rax)
0x7ff8469f7bc9  <+   73>        e8 c2 df fe ff              call   0x7ff8469e5b90 <ntdll!DbgBreakPoint>
0x7ff8469f7bce  <+   78>        eb 00                       jmp    0x7ff8469f7bd0 <ntdll!DbgUiRemoteBreakin+80>
0x7ff8469f7bd0  <+   80>        33 c9                       xor    %ecx,%ecx
0x7ff8469f7bd2  <+   82>        e8 b9 47 f0 ff              call   0x7ff8468fc390 <ntdll!RtlExitUserThread>
0x7ff8469f7bd7  <+   87>        90                          nop
0x7ff8469f7bd8  <+   88>        cc                          int3
0x7ff8469f7bd9  <+   89>        cc                          int3
0x7ff8469f7bda  <+   90>        cc                          int3
0x7ff8469f7bdb  <+   91>        cc                          int3
0x7ff8469f7bdc  <+   92>        cc                          int3
0x7ff8469f7bdd  <+   93>        cc                          int3
0x7ff8469f7bde  <+   94>        cc                          int3
0x7ff8469f7bdf  <+   95>        cc                          int3



```

