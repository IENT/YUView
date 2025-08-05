我来帮您深入分析这个程序闪退的问题。从错误日志来看，主要有几个关键的错误信息需要关注。让我先搜索相关代码来理解问题的根源。
Search files...

Ran tool

Read file: D:\SiruiWu_code\YUView\claude.md

Read file: claude.md

Ran tool

Read file: YUViewLib/src/video/yuv/videoHandlerYUV.cpp

Ran tool

Read file: YUViewLib/src/video/yuv/videoHandlerYUV.cpp

Ran tool

Ran tool

Read file: YUViewLib/src/video/HDR_VideoWidget.cpp

Ran tool

Read file: YUViewLib/src/video/yuv/videoHandlerYUV.cpp

Ran tool

Read file: HDR_UI_Integration_Strategy.md

Ran tool

Ran tool

Ran tool

Read file: debug

基于您提供的日志和我对代码的深入分析，我发现了程序闪退的根本原因。让我详细解释为什么程序会在点击"Enable native 10bit display"后创建空白窗口并立即崩溃：

## 问题根源分析

### 1. **HDR Widget 未正确集成到主窗口**
最关键的问题是 HDR widget 创建后没有被正确集成到主UI中：

```cpp
// videoHandlerYUV.cpp 中发出了信号
emit hdrWidgetNeedsDisplay(m_hdrWidget, true);
```

但是**主窗口没有相应的槽函数来接收这个信号**！这导致：
- HDR widget 虽然创建时传入了 parent，但没有被添加到主窗口的 widget 层次结构中
- Widget 实际上创建了一个**独立的离屏窗口**（这就是您看到的空白白色窗口）
- 由于没有正确的父窗口管理，widget 处于不稳定状态

### 2. **QPaintDevice 竞态条件**
错误信息 `QPaintDevice: Cannot destroy paint device that is being painted` 表明：
- HDR widget 在绘制过程中被销毁或状态改变
- 虽然代码中已经移除了 `deleteLater()`，但 widget 的生命周期管理仍有问题
- OpenGL 上下文在错误的时机被访问或释放

### 3. **无效的模型索引**
在 `createHDRWidget` 函数中（第3374-3376行）：
```cpp
this->currentImageIndex = -1;
this->setCacheInvalid();
emit signalHandlerChanged(true, RECACHE_CLEAR);
```
这导致大量的 `dataChanged() called with an invalid index range` 警告，表明数据模型处于不一致状态。

### 4. **缺失的 UI 集成架构**
根据 `HDR_UI_Integration_Strategy.md` 的设计，应该使用 `QStackedWidget` 来切换标准渲染和 HDR 渲染，但实际代码中：
- 主窗口没有实现 `onHDRWidgetNeedsDisplay` 槽函数
- 没有 `QStackedWidget` 来管理视频显示 widgets
- HDR widget 变成了一个孤立的窗口

### 5. **Push 模型实现不完整**
虽然代码尝试实现 push 模型架构：
```cpp
// 推送帧数据到 HDR widget
m_hdrWidget->updateFrame(currentFrameImage);
```
但由于 widget 没有被正确显示和管理，推送的数据无法被渲染（FPS 显示为 0）。

## 解决方案

要修复这个问题，需要：

1. **在主窗口中实现信号接收**：
   - 添加 `onHDRWidgetNeedsDisplay` 槽函数
   - 正确连接 `videoHandlerYUV` 的信号

2. **使用 QStackedWidget 管理视频显示**：
   - 创建一个 stacked widget 包含标准渲染 widget 和 HDR widget
   - 根据信号切换当前显示的 widget

3. **修复数据模型更新**：
   - 避免将 `currentImageIndex` 设置为 -1
   - 确保数据模型状态一致

4. **确保 HDR widget 生命周期稳定**：
   - Widget 只创建一次，通过 show/hide 管理
   - 确保 OpenGL 上下文在正确的线程和时机访问

这些问题综合导致了程序的不稳定和崩溃。HDR widget 作为一个独立窗口出现，没有正确的父窗口管理和 OpenGL 上下文同步，最终导致绘制设备错误和程序崩溃。
