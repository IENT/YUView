## **PRD: HDR 渲染稳定性与智能启动重构**



### **1. 概要 (Executive Summary)**

本文档旨在解决 YUView 原生 10-bit HDR 渲染功能的严重稳定性问题。我们希望采用一种**要求重启的静态渲染管线初始化模型**。

请阅读现在已有的代码，应该能看到现在的代码`HDR_VideoWidget`和`HDRRenderingManager`已经使用OpenGL实现了一套完整的HDR渲染逻辑。但是这套逻辑似乎十分复杂，也不能满足我们的需求。

### **5. 用户故事与详细需求**

#### **用户故事 1: 在兼容设备上启用 HDR**
> “作为一名视频工程师，我希望在我的 HDR 显示器上启用 YUView 的 10-bit 渲染，并在重启后让它正常工作，以便我能准确地分析视频内容。”

*   **需求 5.1 (修改设置):**
    1.  当用户在设置面板中勾选 `Enable native 10-bit display` 时：
    2.  应用**不得**尝试更改当前正在运行的渲染模式。
    3.  用户的“启用 HDR”意图**必须立即**通过 `QSettings` 或类似机制保存到配置文件中。
    4.  复选框旁边**必须**立即出现一个标签，显示文字：**“(重启后生效)”**。

*   **需求 5.2 (成功启动):**
    1.  用户重启应用后，程序首先读取配置文件，确认用户的意图是“启用 HDR”。
    2.  程序接着执行硬件能力检测。检测结果为“支持 HDR”。
    3.  应用**必须**在启动时直接初始化 HDR 渲染管线。
    4.  主界面加载后，`Enable native 10-bit display` 复选框**必须**保持其“已勾选”的状态，且 `(重启后生效)` 的标签不应显示。

#### **用户故事 2: 禁用 HDR 功能**
> “作为一名用户，我希望能够随时关闭 HDR 模式并切换回标准的 SDR 模式，以便在普通显示器上查看或进行性能对比。”

*   **需求 5.3 (禁用设置):**
    1.  当用户取消勾选 `Enable native 10-bit display` 时：
    2.  应用**不得**尝试更改当前渲染模式。
    3.  用户的“禁用 HDR”意图**必须立即**保存到配置文件中。
    4.  复选框旁边**必须**立即出现 `(重启后生效)` 的标签。

*   **需求 5.4 (SDR 启动):**
    1.  用户重启应用后，程序读取配置，确认用户意图是“禁用 HDR”。
    2.  应用**必须**直接初始化标准 SDR 渲染管线，**无需**执行 HDR 能力检测。
    3.  主界面加载后，复选框**必须**保持其“未勾选”的状态。

#### **用户故事 3: 智能适应硬件环境（核心）**
> “作为一名用户，我把我在 HDR 电脑上的 YUView 文件夹（连同配置文件）拷贝到我的普通笔记本电脑上使用，我希望软件足够智能，能够自动识别新环境不支持 HDR，并安全地以普通模式启动，而不是报错或崩溃。”

*   **需求 5.5 (智能回退逻辑):**
    1.  应用启动时，读取到配置文件中的意图是“启用 HDR”。
    2.  程序接着执行硬件能力检测，但此次检测结果为“**不支持 HDR**”。
    3.  应用**必须**安全地回退，并初始化**标准 SDR 渲染管线**。
    4.  应用启动后，**必须**向用户显示一个一次性的、非阻塞的通知。通知的精确措辞为：“**HDR 模式启用失败：当前显示器或系统配置不支持。已自动以标准模式启动。**”
    5.  为了防止用户下次启动时再次遇到同样的问题，程序**必须自动地**：
        *   将 UI 上的 `Enable native 10-bit display` 复选框更新为**未勾选**状态。
        *   将这个新的“未勾选”状态**写回到配置文件中**，实现配置的“自我修复”。

### **6. 启动逻辑流程图**

```mermaid
graph TD
    A[应用启动] --> B{读取配置文件中<br>'Enable HDR' 的设置};
    B -- a. 设置为 false --> C[直接初始化 SDR 渲染管线];
    B -- b. 设置为 true --> D[执行 HDR 硬件能力检测];
    D -- 1. 检测成功 --> E[初始化 HDR 渲染管线];
    D -- 2. 检测失败 --> F[初始化 SDR 渲染管线<br>(安全回退)];
    F --> G[显示一次性通知: 'HDR 不支持, 已回退'];
    G --> H[更新UI: 取消勾选复选框<br>并写回配置文件];
    C --> Z[启动完成];
    E --> Z;
    H --> Z;
```

### **7. 技术实现说明 (供开发团队参考)**

*   **决策前移:** 渲染管线的决策点必须从运行时的 `HDRRenderingManager` 转移至应用的 `main()` 函数或主窗口构造函数的早期阶段。
*   **配置先行:** 使用 `QSettings` 在用户交互时立即持久化用户的意图。
*   **启动时验证:** 在 `main()` 中，`读取配置 -> 如果配置为 HDR -> 调用 HDRDetection -> 根据结果创建 HDR 或 SDR 的主渲染组件`。这个流程是同步且阻塞的，确保了在显示任何UI之前，渲染路径就已经确定。
*   **用户通知:** 实现一个非阻塞的通知机制。可以是在主窗口状态栏显示几秒钟，或者使用一个简单的 `QMessageBox::information` 对话框。
*   **配置同步:** 当发生需求 5.5 中的回退时，必须确保 `QSettings` 中的值被更新，以保证应用状态和用户配置的一致性。

### **8. 验收标准**

| ID | 场景 | 用户操作 | 预期结果 |
| :--- | :--- | :--- | :--- |
| **AC-1** | **SDR -> HDR (成功)** | 在支持 HDR 的系统上，勾选 HDR 选项并重启。 | 1. 应用以 HDR 模式启动。<br>2. 复选框保持勾选。<br>3. 无任何错误或黑屏。 |
| **AC-2** | **HDR -> SDR** | 在 HDR 模式下，取消勾选 HDR 选项并重启。 | 1. 应用以 SDR 模式启动。<br>2. 复选框保持未勾选。 |
| **AC-3** | **UI 即时反馈** | 在运行时，点击 HDR 复选框。 | 1. `(重启后生效)` 标签出现。<br>2. 当前渲染模式**不变**。 |
| **AC-4** | **HDR 失败回退** | 在**不支持** HDR 的系统上，尝试以“启用 HDR”的配置启动。 | 1. 应用以 **SDR 模式**稳定启动。<br>2. 显示一次性的“HDR 不支持”通知。<br>3. 复选框变为**未勾选**状态。<br>4. 配置文件被更新为“禁用 HDR”。 |
| **AC-5** | **可移植性测试 (关键)** | 将一个在 HDR 电脑上配置为“启用 HDR”的 YUView 文件夹，完整拷贝到一台不支持 HDR 的电脑上。 | 运行应用后，**必须**触发 **AC-4** 的所有预期结果。 |
| **AC-6** | **稳定性测试** | 在所有可能组合下（HDR/SDR/支持/不支持），反复重启、更改设置、关闭应用。 | 程序在任何情况下都不能崩溃、冻结或出现渲染错误。 |

### **9.功能现状**
1. 在支持 HDR 的系统上，勾选`Enable native 10bit display`并重启；
- 实际结果：仍然在走SDR渲染管线；
```
videoHandlerYUV: Saved Enable10BitDisplay setting to: true
videoHandlerYUV: Restart required for HDR mode change to take effect
videoHandlerYUV: Showing restart notice for HDR mode change to: true
10:31:52: The command "D:\SiruiWu_code\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\YUViewApp\YUView.exe" finished successfully.

10:32:11: Starting D:\SiruiWu_code\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\YUViewApp\YUView.exe...
YUView: User HDR preference from settings: false
YUView: User preference is SDR mode - using standard rendering

```
涉及文件：`YUView\YUViewApp\src\yuviewapp.cpp`
进入软件观察，`Enable native 10bit display`复选框仍然未被勾选中。
另外，请你简化HDR检测逻辑，保留`DXGI (DirectX Graphics Infrastructure)`的检测就足够了
