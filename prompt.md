# YUView HDR 功能修复请求

## 简介 (Introduction)

本提示描述了 YUView 视频播放器在启用 HDR 渲染模式时遇到的两个关键可用性问题。目标是修改 C++/Qt 源代码以修复这些错误，恢复应用的核心交互功能并确保正确的视频画面显示。

## 上下文 (Context)

-   **应用**: YUView, 一款高级 YUV 播放器。
-   **功能**: 高动态范围 (HDR) 视频渲染。
-   **实现方式**: 当启用 HDR 时，系统会创建一个 `HDR_VideoWidget`（基于 `QOpenGLWidget`）。这个 Widget 会作为一个**覆盖层 (Overlay)** 叠加在主视频显示区域 (`splitViewWidget`) 之上，专门用于处理 HDR 内容的 OpenGL 渲染。
-   **核心问题**: 这个覆盖层架构导致了事件处理和画面渲染两方面的问题。

## 涉及文件 (Files Provided)

-   `HDR_VideoWidget.cpp` (主要问题所在文件)
-   `HDRRenderingManager.cpp`
-   `HDRDetectionWorker.cpp`
-   `videoHandler.cpp`
-   `FrameHandler.cpp`

## 附带日志 (Provided Logs)

日志显示 HDR 检测和 `HDR_VideoWidget` 的创建流程基本成功，但渲染画面无法交互，且初始化过程可能存在问题。关键日志片段：

```log
HDRRenderingManager: Found split view widget: splitViewWidget(...)
HDRRenderingManager: HDR widget integrated with split view synchronously.
...
HDR_VideoWidget::updateFrame: Received frame QSize(3840, 2160) format: QImage::Format_RGBA64_Premultiplied initialized: true
...
HDR Video Widget FPS: 0 frames/sec
```

这表明 HDR 流程已启动，帧数据已送达，但画面是静止的（FPS: 0），并且交互失效。

---

## 描述问题 (Problem Description)

### 问题 1: 鼠标事件被劫持，导致交互失效

-   **症状**: 启用 HDR 模式后，用户无法通过**鼠标滚轮缩放**视频，也无法通过**鼠标拖动平移**视频画面。所有依赖于父窗口 (`splitViewWidget`) 的鼠标交互全部失效。
-   **根本原因**: `HDR_VideoWidget` 作为一个顶层覆盖窗口，捕获了所有的鼠标事件（滚轮、点击、移动）。虽然代码尝试将事件手动转发给父窗口，但这种机制是不可靠的，并且在当前实现中已失效。
-   **影响**: 这使得 HDR 模式在功能上几乎不可用，因为它破坏了最基本的导航和查看操作。

### 问题 2: 视频画面长宽比错误且静态显示

-   **症状**:
    1.  渲染出的视频画面被**拉伸以填满**整个 `HDR_VideoWidget` 区域，没有保持视频源本身的长宽比。
    2.  当用户调整窗口大小时，视频画面不会相应地重新缩放或居中，表现为**静态**。
-   **根本原因**: `HDR_VideoWidget` 中的 OpenGL **投影矩阵 (Projection Matrix)** 没有被正确计算和更新。很可能存在一个竞态条件：投影矩阵在获取到视频帧的实际尺寸 (`m_frameSize`) 之前就被设置了，或者在窗口大小调整后 (`resizeGL` 事件) 没有被重新计算。
-   **影响**: 视频画面显示失真，严重影响观看体验。

---

## 建议的改进 (Suggested Improvements)

请专注于修改 `HDR_VideoWidget.cpp` 文件来解决以上两个问题。

### 针对问题 1 的改进方案：优化事件处理

-   **核心思想**: 放弃手动、易出错的事件转发。采用更健壮、更符合 Qt 设计哲学的**事件忽略 (Event Ignoring)** 机制，让 Qt 的事件系统自动处理事件的向上传播。
-   **具体步骤**:
    1.  修改 `wheelEvent`, `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent` 这四个鼠标事件处理器。
    2.  在每个事件处理器内部，增加判断逻辑：
        -   如果事件是用于**控制 HDR 参数**的（例如，按住 `Ctrl` 键滚动滚轮调整曝光度，或按住 `Shift` 键拖动调整 Gamma），则**接受**该事件 (`event->accept()`) 并执行相应操作。
        -   对于所有**其他情况**，立即调用 `event->ignore()`。这将通知 Qt：“此控件不处理该事件，请将其传递给父控件”。
-   **预期结果**: `splitViewWidget` 将能够接收到所有未经 `HDR_VideoWidget` 特别处理的鼠标事件，从而恢复正常的缩放和平移功能。

### 针对问题 2 的改进方案：正确管理投影矩阵

-   **核心思想**: 确保投影矩阵总能根据最新的窗口尺寸和视频帧尺寸进行更新，以保持正确的长宽比。
-   **具体步骤**:
    1.  在 `HDR_VideoWidget` 类中创建一个新的私有辅助函数，例如 `void updateProjectionMatrix()`。
    2.  此函数负责核心计算逻辑：
        -   获取当前控件的宽度和高度。
        -   获取当前视频帧的宽度和高度 (`m_frameSize`)。
        -   比较两者长宽比，计算出能够将视频帧**无拉伸地**放入控件中央所需的缩放和偏移量（即实现 "letterboxing" 或 "pillarboxing" 效果）。
        -   使用 `m_projectionMatrix.ortho(...)` 设置正确的正交投影。
        -   设置完成后，立即将更新后的矩阵传递给着色器 (`m_shaderProgram->setUniformValue(...)`)。
    3.  在以下两个关键位置调用这个新的 `updateProjectionMatrix()` 函数：
        -   在 `resizeGL(int width, int height)` 函数的末尾，以响应窗口大小的变化。
        -   在 `updateFrame(const QImage& newFrame)` 函数中，**在 `m_frameSize` 被更新之后**，以响应新视频帧的加载（这解决了竞态条件）。
-   **预期结果**: 视频画面将始终保持正确的长宽比，并且在窗口大小调整时能够动态、正确地重新缩放。

**最终请求**: 请根据上述建议，修改 `HDR_VideoWidget.cpp` 文件，以解决描述的两个核心问题。
