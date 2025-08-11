### **总任务：修复原生10bit HDR输出功能，并完善多显示器支持**

### **1. 目标 (Goal)**

修复当前程序在启用原生10-bit HDR输出时遇到的画面显示错误。最终目标是实现稳定、正确且用户友好的原生10-bit HDR渲染功能，并能正确处理在HDR与SDR显示器间的切换。

### **2. 系统环境 (System Environment)**

*   **操作系统**: Windows 11
*   **显示设置**: 双显示器配置，其中一个为已在系统设置中启用HDR的主显示器，另一个为SDR显示器。
*   **测试文件**: 1024级灰阶图像，1080p, YUV 4:2:0, 10-bit `420p10le` 格式。

### **3. 问题描述 (Problem Description)**

当前实现存在主要问题：HDR显示异常，用户交互逻辑异常。

#### **3.1. 主要问题：HDR激活流程异常且功能不完整 (Main Issue: Abnormal HDR Activation and Incomplete Functionality)**

**复现步骤 (Steps to Reproduce):**

1.  在Debug模式下运行程序。
2.  加载指定的10bit yuv420p10le的YUV文件，包含一些白天的室外风景。
3.  点击 `Enable native 10-bit display` 复选框。


**预期行为 (Expected Behavior):**

*   在HDR显示器上，点击 `Enable native 10-bit display`,使得该`QCheckbox`被正确的勾选上之后，应立即切换到OpenGL渲染，并正确显示HDR灰阶图像。
*   启用HDR模式后，鼠标滚轮缩放视频倍率、视频播放列表控制等原有YUVuew的核心功能都应保持正常工作。
*   `QCheckbox`被取消勾选之后，应平滑地回退到原始的QPainter SDR渲染模式。

**实际行为 (Actual Behavior):**

- 测试用例是YUV420p10le, 720p的YUV图像，且使用Debug调试模式启动程序的时候，点击 `Enable native 10-bit display`,使得该`QCheckbox`被正确的勾选上：

*   **缺陷A：交互锁定与渲染循环中断 (Interaction Lock-up & Broken Render Loop)**
    *   激活HDR后，视频画面**完全静止**，成为一张静态图像。
    *   所有与视图的交互均失效，包括**鼠标滚轮缩放视频观看倍率**和**播放控件**（无法前进/后退帧）。
    *   取消勾选可成功退回SDR模式。但若要再次激活HDR，必须在点击 `Enable native 10-bit display之后，还要再**手动滚动鼠标滚轮**才能触发一次渲染，画面依然是静止的。
    *   **核心假设**: `HDR_VideoWidget` 覆盖层可能错误地“捕获”了所有鼠标和定时器事件，并未能将它们传递给父级视图或触发持续的重绘。

*   **缺陷B：颜色空间渲染错误 (Color Space Corruption)**
    *   渲染出的画面存在严重的**全局性偏绿**问题。
    *   在画面的高光区域，出现不自然的**粉红色或白色溢出**，表明色调映射（Tone Mapping）或数值削波（Clipping）存在问题。
    *   **核心假设**: 在着色器（Shader）中，从YUV转换后的RGB（很可能是Rec. 709色域）到目标HDR色域（如Rec. 2020）的颜色矩阵变换不正确，或PQ（Perceptual Quantizer）电光转换函数（EOTF）的实现有误。


### **4. 调试建议 (Debug Logs)**

### 针对缺陷A：交互锁定与渲染循环中断

**目标**：找出为什么渲染循环停止了，以及为什么鼠标事件没有被正确处理。

这通常是一个 **Qt事件处理和传递** 的问题。`HDR_VideoWidget` 作为一个覆盖在主视图之上的层，它的事件处理方式是关键。

#### 断点设置策略 (A)

| 文件 | 函数/位置 | 为什么要在此设置断点？ | 检查什么？ |
| :--- | :--- | :--- | :--- |
| **`HDR_VideoWidget.cpp`** | `wheelEvent(QWheelEvent* event)` | **事件入口点**：这是验证鼠标滚轮事件是否首先被HDR控件接收的第一站。 | 1.  **断点是否命中？** 当您滚动滚轮时，程序是否会停在这里？如果根本不命中，说明有其他控件（或事件过滤器）在此之前“偷”走了事件。<br>2. **代码路径**：程序是否进入了`else`分支？`event->modifiers() & Qt::ControlModifier` 的结果是什么？我们预期它会进入`else`去转发事件。<br>3. **`parentWidget()`**：`parentWidget()`的返回值是什么？它应该是`splitViewWidget`的有效指针，不能是`nullptr`。 |
| **`HDR_VideoWidget.cpp`** | `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent` | **（可选）** 与`wheelEvent`类似，检查其他鼠标事件的传递路径是否正确。 | 同上，检查事件是否被接收，以及是否被正确转发给`parentWidget()`。 |
| **`videoHandlerYUV.cpp`** | `drawFrame(...)` | **渲染循环核心**：这个函数负责准备并绘制每一帧。视频播放时，它应该被一个定时器（`QTimer`）反复调用。 | 1. **调用频率**：在激活HDR并开始播放后，这个断点**是否会持续命中**？如果只命中一次然后就停了，说明**渲染定时器被中断或失效了**。这就是“画面静止”的根本原因。<br>2. **调用时机**：在第二次激活HDR后，这个断点是否只有在您**手动滚动滚轮后才命中一次**？这会证实是`paintEvent`（由滚轮触发）在驱动唯一的渲染，而不是定时器。 |
| **`HDRRenderingManager.cpp`** | `updateHDRFrame(const QImage& frame)` | **数据推送路径**：`drawFrame`最终会调用这个函数把图像数据送给`HDR_VideoWidget`。 | 这个断点的行为应该和`videoHandlerYUV::drawFrame`一致。如果`drawFrame`不被调用，这里也绝不会被调用。 |

**小结与操作建议 (A)**：

1.  **首要任务**：在`HDR_VideoWidget::wheelEvent`设置断点。滚动滚轮，如果断点未命中，问题比想象的复杂。如果命中，检查它是否正确进入了`else`分支并尝试转发事件。
2.  **其次**：在`videoHandlerYUV::drawFrame`设置断点。在HDR模式下，观察它是否能被持续调用。如果不能，就说明驱动视频播放的`QTimer`循环因为某种原因失效了。`HDR_VideoWidget`的出现可能无意中停止了父视图的更新定时器。

---

### 针对缺陷B：颜色空间渲染错误

**目标**：找出着色器中导致偏绿和高光溢出的数学错误。

这100%是 **OpenGL着色器 (Shader) 内部的逻辑问题**。由于我们不能直接在GLSL代码中设置断点，我们的策略是：**在C++代码中检查送往GPU的数据是否正确，并通过修改着色器代码来输出中间结果以进行“可视化调试”。**

#### 断点与调试策略 (B)

| 文件 | 函数/位置 | 为什么要在此设置断点？ | 检查什么？ |
| :--- | :--- | :--- | :--- |
| **`HDR_VideoWidget.cpp`** | `uploadTextureData(const QImage& image)` | **GPU数据源头**：所有颜色问题的根源始于这里发送给GPU的纹理数据。 | 1. **`image.format()`**：传入的`QImage`格式是什么？根据您的成功日志，它应该是`QImage::Format_RGBA64`或`...Premultiplied`。这表示`videoHandlerYUV`已经成功将10-bit YUV数据转换为了**16-bit 线性RGB**。这是非常好的，说明YUV解码部分没问题。<br>2. **保存中间图像**：在`glTexImage2D`调用之前，加入一行代码 `image.save("C:/temp/debug_frame.png");`。然后打开这个PNG文件（需要用支持高位深的查看器如Photoshop/Krita查看），**它应该是完美的灰阶图像，不应该有任何偏色**。如果此时已经偏绿，那问题出在`videoHandlerYUV`的转换中。 |
| **`HDR_VideoWidget.cpp`** | `updateShaderUniforms()` | **着色器参数**：检查传递给着色器的控制参数是否正确。 | 1. **`m_renderMode`**: 它的值应该是`Mode_BT2020_PQ_10bit` (即 `1`)。<br>2. **`m_displayMaxLuminance`**: 它的值应该是从HDR检测中得到的正确亮度值（例如455.523）。 |
| **`HDR_VideoWidget.cpp`** | `initializeShaders()`内的 **Fragment Shader字符串** | **问题核心**：真正的错误在这里。我们需要通过修改它来调试。 | - |

#### 可视化调试着色器 (Shader)

这是解决颜色问题的最有效方法。您需要临时修改`fragmentShaderSource`字符串，然后重新编译运行，观察屏幕输出的变化。

1.  **调查全局偏绿问题：**
    *   **假设**：问题可能出在`from709to2020`颜色矩阵，或者在`if (maxDiff < 0.01)`的灰阶检测逻辑上。
    *   **实验1：测试灰阶检测**
        *   修改`main()`函数：
            ```glsl
            void main()
            {
                vec4 color = texture(videoTexture, TexCoord);
                float maxDiff = max(abs(color.r - color.g), abs(color.g - color.b));
                maxDiff = max(maxDiff, abs(color.r - color.b));

                if (maxDiff < 0.01) {
                    // 如果检测到是灰阶，输出纯红色
                    FragColor = vec4(1.0, 0.0, 0.0, 1.0);
                } else {
                    // 如果检测到是彩色，输出纯绿色
                    FragColor = vec4(0.0, 1.0, 0.0, 1.0);
                }
            }
            ```
        *   **预期结果**：您的1024级灰阶图像应该**完全变成纯红色**。如果它变成了**纯绿色**，说明`maxDiff < 0.01`这个条件对您的灰阶输入不成立，灰阶检测逻辑失败，导致灰阶图像被错误地当成彩色图像处理（应用了颜色矩阵，导致偏绿）。

2.  **调查高光溢出问题：**
    *   **假设**：问题出在`applyPQ`函数中的色调映射或PQ曲线计算。
    *   **实验2：绕过PQ变换，直接看色调映射结果**
        *   修改`main()`函数，找到处理`renderMode == 1`的部分：
            ```glsl
            // ...
            } else if (renderMode == 1) {
                // ...
                // 直接输出色调映射后的线性值，不经过PQ和Gamma
                vec3 toneMappedLinear = from709to2020 * color.rgb * (displayMaxLuminance / sourceMaxLuminance);
                FragColor = vec4(toneMappedLinear, color.a);
                // ...
            }
            // ...
            ```
        *   **预期结果**：您会看到一个比较暗的、但应该是平滑过渡的灰阶图像，高光部分不应该有突兀的粉红色或白色区域。如果此时高光正常了，说明问题就在`applyPQ`函数内部。如果仍然溢出，说明`displayMaxLuminance`或`sourceMaxLuminance`的值可能有问题。
