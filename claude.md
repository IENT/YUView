
***

### **Subject: Architectural Mandate & Action Plan for YUView's Native 10-Bit HDR Rendering Path**

**Preamble:**
This document outlines a step-by-step development plan to resolve a critical stability issue in YUView's 10-bit display path. We will proceed iteratively to ensure correctness at each stage. The final goal is a robust, production-ready solution for native 10-bit HDR video rendering.

**My Role:** I am the lead developer of YUView.

**Your Role:** You are an expert C++/Qt software architect specializing in high-performance, cross-platform graphics. Your task is to provide a complete, production-ready code solution based on the principles outlined below.

**The debug output of current code**
```
Creating HDR widget in main thread for proper OpenGL context
Creating HDR widget with proper parent for UI integration
QPaintDevice: Cannot destroy paint device that is being painted
HDR Surface Format: BT2100Pq color space configured
HDR capabilities set: Mode: "HDR10/BT.2020 PQ (10-bit)" Max Luminance: 455.523 nits Bits per channel: 10
Display max luminance set to: 455.523 nits
HDR texture format changed to: "RGB10_A2"
HDR render mode validation: HDR supported, mode "BT2020_PQ_10bit" is valid
HDR FPS monitoring started
HDR render mode changed to: "HDR10/BT.2020 PQ (10-bit)"
HDR render mode changed to: "BT2020_PQ_10bit"
HDR widget created with parent integration, ready for display
dataChanged() called with an invalid index range:
    topleft: QModelIndex(-1,-1,0x0,QObject(0x0))
    bottomRight:QModelIndex(-1,-1,0x0,QObject(0x0))
HDR widget created with size 256x256
HDR widget available but skipping HDR rendering during QPainter operations to prevent crashes
Using standard rendering to maintain stability
14:29:31: The command "D:\SiruiWu_code\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\YUViewApp\YUView.exe" terminated abnormally.

14:44:53: Starting D:\SiruiWu_code\YUView\build\Desktop_Qt_6_9_1_MinGW_64_bit-Release\YUViewApp\YUView.exe...
```
---

The HDR exemplar code is located in `krita\libs\ui\opengl`

### **1. Root Cause Analysis of the Current Crash**

The provided debug output reveals a crash stemming from two fundamental architectural flaws. The solution MUST resolve both root causes.

**Cause 1: Rendering Control Conflict & Race Conditions**
The primary issue is a conflict between two competing rendering control flows(Defined in `YUViewLib\src\video\yuv\videoHandlerYUV.cpp` of `videoHandlerYUV::drawFrame`):
*   **A) Qt's Event-Driven Path:** The standard, safe rendering path where the Qt event loop calls `paintEvent`, which in turn triggers `paintGL()` within a managed OpenGL context.
*   **B) The Manual, Imperative Path:** The `videoHandlerYUV::getHDRRenderedImage()` function attempts to force an immediate, out-of-band render by manually calling `m_hdrWidget->makeCurrent()`, `renderOffscreen()`, and `grabHDRFramebuffer()`.

This conflict creates a race condition for the OpenGL context, leading to undefined behavior and crashes when Qt's `paintEvent` and the manual render calls collide.

**Cause 2: Widget Lifecycle and State Management Instability**
The log messages `QPaintDevice: Cannot destroy paint device that is being painted` and `dataChanged() called with an invalid index range` point directly to this flaw.
*   **The `QPaintDevice` Error:** The application calls `m_hdrWidget->deleteLater()` (e.g., in `slot10BitDisplayChanged`) while a `paintEvent` for that same widget is still pending or active in the event queue. This is a fatal operation.
*   **The `dataChanged` Error:** The rapid creation/destruction of the widget, combined with calls to clear data models (`currentImageIndex = -1`), creates moments of state inconsistency, where the UI is asked to update from an invalid model state.

---

### **2. The Unbreakable Architectural Mandate (The "How")**

The new implementation must strictly adhere to the following architectural principles to guarantee stability and correctness. **There will be no exceptions.**

*   **Principle #1: A Single, Unified Rendering Path.** All OpenGL rendering MUST occur exclusively within the `HDR_VideoWidget::paintGL()` method. All external, manual rendering calls (like `makeCurrent`, `renderOffscreen`) are strictly forbidden. The system will be purely event-driven.

*   **Principle #2: A State-Driven, "Push" Model.** The flow of control will be inverted.
    *   **FROM (Flawed):** The `videoHandlerYUV` "pulls" a rendered image from the widget when it needs one.
    *   **TO (Correct):** The `videoHandlerYUV` "pushes" new frame data (e.g., a `QImage`) *to* the `HDR_VideoWidget`. It then simply calls the widget's `update()` slot to schedule a repaint at the next opportune moment in the event loop.

*   **Principle #3: A Stable, Persistent Widget Lifecycle.** The `HDR_VideoWidget` will be created **once** and will persist. It will be managed via `show()` and `hide()` instead of being created and destroyed repeatedly. This completely eliminates the `QPaintDevice` race condition.

---

### **3. Required Deliverables (The "What")**

Please provide the complete, production-quality C++ code for the following components, implementing the principles above.

**Deliverable 1: The Refactored `HDR_VideoWidget` Class (View)**
Our first task is to define a clean, compliant interface for our rendering widget.

*   **Your Task:** Provide the complete code for **`HDR_VideoWidget.h` only**.
*   **Requirements for the Header:**
    1.  It must inherit from `QOpenGLWidget` and the necessary `QOpenGLFunctions`.
    2.  It must expose a single public slot for data input: `void updateFrame(const QImage& newFrame);`. This is the sole entry point for the Controller.
    3.  It **must not** contain any public methods related to manual rendering, such as `renderOffscreen`, `grabHDRFramebuffer`, or `makeCurrent`.
    4.  The standard `initializeGL()`, `paintGL()`, and `resizeGL()` methods should be declared as `protected` overrides.

**Deliverable 2: The Modified `videoHandlerYUV` Logic (Controller)**
*   **Your Task:** Provide the complete code for **`HDR_VideoWidget.cpp`**.
*   **Requirements for the Implementation:**
    1.  The constructor must correctly configure the `QSurfaceFormat` for 10-bit color and the `QColorSpace::Bt2100Pq`.
    2.  `initializeGL()` will set up all OpenGL resources (shaders, VBO/VAO, textures).
    3.  `updateFrame()` will receive the new `QImage`, store it safely (considering thread safety if necessary), and then call `this->update()` to schedule a repaint. It **must not** perform any direct OpenGL calls.
    4.  `paintGL()` will be the **only** place where OpenGL rendering commands (`glUseProgram`, `glBindTexture`, `glDraw...`, etc.) are executed. It will render the most recently received frame.

**Deliverable 3: Refactor the Controller (`videoHandlerYUV`) & UI Integration**

With a fully functional View component, the final step is to adapt the Controller and UI to use it correctly.

*   **Your Task:** Provide the modified code snippets for the `videoHandlerYUV` class and a clear description of the UI integration strategy.
*   **Requirements for the Controller Logic:**
    1.  Modify `slot10BitDisplayChanged()` to manage a persistent `m_hdrWidget` instance using `show()` and `hide()`. All `deleteLater()` calls related to toggling the view must be removed.
    2.  The core video processing loop must be updated to call `m_hdrWidget->updateFrame(theNewFrameAsQImage)` instead of its old rendering logic.
    3.  The function `getHDRRenderedImage()` **must be completely removed.**
*   **Requirements for the UI Integration Strategy:**
    Propose and explain the use of a `QStackedWidget` in the main UI to seamlessly switch between the standard 8-bit rendering widget and our new `HDR_VideoWidget` **without creating new windows** and **retain 10bit HDR precision**.


### **4. Acceptance Criteria**

The final solution will be considered successful if and only if:
1.  The application runs without crashing when enabling and disabling 10-bit display mode.
2.  All `QPaintDevice` and `dataChanged` errors are eliminated from the debug output.
3.  The implementation strictly follows the single, unified rendering path via `paintGL`.
4.  The `HDR_VideoWidget` instance is persistent and managed only by `show()` and `hide()`.
5.  10-bit YUV video content is displayed on a capable HDR monitor without visible color banding.
6.  The application falls back gracefully to standard 8-bit rendering on non-HDR systems without instability.
