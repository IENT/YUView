### **Subject: Action Plan for YUView 10-Bit Display & Feature Implementation**

**Preamble:**
This document outlines the implementation and debugging tasks for two distinct issues in YUView. The first is a rendering artifact in the 10-bit display path. The second concerns non-functional "Video Distortion Analysis" controls. Please follow the action plan below to resolve both.


### **Task1: Implement a Complete C++ Qt Solution for Native 10-bit HDR Video Rendering**

**My Role:** You are the lead developer of YUView, a C++ Qt-based video player.

**Your Role:** You are an expert C++ Qt/QML software architect specializing in high-performance, cross-platform graphics. Your task is to provide a complete, production-ready code solution to replace an existing, flawed rendering path in my application.

### **High-Level Objective:**

Provide a complete, drop-in C++ solution that enables my application to render 10-bit YUV video frames on a native 10-bit HDR display, while gracefully handling systems that do not support HDR. The final output must be free of color banding.

### **Current Flawed Implementation:**

My current code (Defined in `YUViewLib\src\video\yuv\videoHandlerYUV.cpp`) attempts to render 10-bit data by converting it to a `QImage::Format_RGBA64` and then drawing it with `QPainter::drawImage`. The Enable 10bit display switch is already defined in `YUViewLib\ui\videoHandlerYUV.ui`

```cpp
// THIS IS THE FLAWED CODE TO BE REPLACED if we want to retain 10 bit precision
if (enable10BitDisplay && yuvFormat.getBitsPerSample() == 10)
{
  // ... code converts 10-bit YUV to a QImage named 'outputImage' ...
  // The format is QImage::Format_RGBA64
  
  // Later, this is called, which causes banding:
  // painter->drawImage(rect, outputImage); 
}
```

I understand this is incorrect because `QPainter`'s pipeline is limited to 8-bit SDR and downsamples the high-precision data.

### **Requirements for the New Solution:**

I need you to provide a complete, self-contained solution in the form of new C++ classes that accomplish the following. Please provide the full `.h` and `.cpp` file contents.

**1. HDR Capability Detection:**
   *   The solution must first robustly detect if the primary display a widget is on truly supports HDR output.
   *   This check should be performed before attempting any HDR-specific rendering.
   *   **If HDR is NOT supported:** The code should trigger a signal or a callback that my application can connect to, in order to show the user a notification dialog (e.g., a `QMessageBox`) explaining that HDR mode is not active. The rendering should then automatically fall back to the existing 8-bit SDR path (i.e., do nothing and let the old `QPainter` path run).
   *   **If HDR IS supported:** Proceed with the native 10-bit HDR rendering pipeline.
   *   Provide a platform-agnostic way to do this if possible using Qt's APIs (e.g., checking `QScreen` properties), or provide platform-specific implementations for Windows and macOS if necessary.

**2. A New `HDR_VideoWidget` Class:**
   *   Create a new widget class, for example, `HDR_VideoWidget`, that inherits from `QOpenGLWidget` and the appropriate `QOpenGLFunctions` class.
   *   This widget will be responsible for the entire HDR rendering process.
   *   It should have a public slot or method, like `void updateFrame(const QImage &newFrame)`, that my application can call to pass in the new `QImage` (which is in `QImage::Format_RGBA64` format).

**3. Correct OpenGL Context Setup:**
   *   Provide the necessary `QSurfaceFormat` setup code that must be placed in `main.cpp`. This code must request a 10-bit-per-channel framebuffer to enable the hardware's 10-bit output mode.

**4. GPU Texture Management:**
   *   Inside `HDR_VideoWidget`, implement the logic to take the `QImage` passed to `updateFrame` and efficiently upload its pixel data to a `GL_RGBA16F` OpenGL texture. Handle both creating the texture for the first frame and updating it for subsequent frames.

**5. Complete GLSL Shaders for HDR10 Output:**
   *   Provide the full, production-quality GLSL source code for a vertex and fragment shader.
   *   The fragment shader is critical. It must contain an accurate implementation of the **PQ EOTF (ST.2084)** function to transform linear color values from the 16-bit texture into the non-linear signal required by an HDR10 display.

**6. The Rendering Loop (`paintGL`)**
   *   Implement the `paintGL` method to execute the rendering. This should bind the shaders and texture, and draw a full-screen quad to display the video frame.

### **Final Deliverable:**

The final output from you should be a set of C++ `.h` and `.cpp` files, and GLSL `.vert`/`.frag` files that I can directly add to my Qt project. The solution should be complete, well-commented, and encapsulate the entire HDR detection and rendering logic as requested. I will be responsible for integrating this `HDR_VideoWidget` into my application's UI and connecting its `updateFrame` slot.


### **Task2: Refactor "Video Distortion Analysis" Controls and Implement a "Revert" Feature**

**Your Role:** You are an expert C++ Qt developer with a strong focus on creating intuitive and responsive user interfaces.

### **High-Level Goal:**

I need you to refactor the "Video Distortion Analysis" section of our UI. This involves removing obsolete buttons and introducing a new "Revert" functionality that allows the user to instantly return to the starting frame of a distortion analysis playback loop.

### **Context: Current UI Layout**

Our application's UI is defined in a file named `videoHandlerYUV.ui`. Currently, this UI contains four buttons for distortion analysis: "First-level (30FPS)", "Second-level (1FPS)", "Third-level", and "Fourth-level". The playback logic for the first two buttons is already implemented.

### **The Problem: Obsolete UI and Missing Functionality**

The current UI has two major issues:
1.  The "Third-level" and "Fourth-level" distortion buttons are no longer needed and clutter the interface.
2.  When a user starts playback for "First-level" or "Second-level" analysis, there is no way to quickly return to the frame where the analysis began. This makes it difficult to compare the distorted video with the original starting point.

### **New Functional Requirements (Your Task):**

You will need to modify the UI file and the corresponding C++ handler class to implement the following changes.

**1. Modify the UI Layout:**

*   **Analyze the `videoHandlerYUV.ui` file** to identify the object names for all distortion analysis buttons.
*   **Remove the "Third-level" and "Fourth-level" distortion buttons** from the layout entirely.
*   **Add a new `QPushButton` next to the "First-level (30FPS)" button.**
    *   Set its display text to **"Revert"**.
    *   Set its `objectName` to something clear, like `revertButton_L1`.
*   **Add another new `QPushButton` next to the "Second-level (1FPS)" button.**
    *   Set its display text to **"Revert"**.
    *   Set its `objectName` to something clear, like `revertButton_L2`.

**2. Implement State-Aware Playback:**

*   When the user clicks the "First-level (30FPS)" or "Second-level (1FPS)" button:
    *   **Before starting playback, you must capture and store the current frame number.** This frame number is the "revert point". You will need a member variable in your handler class to store this state.

**3. Implement "Revert" Button Functionality:**

*   Create new C++ slots connected to the `clicked()` signals of the new "Revert" buttons (`revertButton_L1` and `revertButton_L2`).
*   When a "Revert" button is clicked:
    *   The video playback must **immediately pause**.
    *   The video must **seek back to the stored "revert point"** (the frame that was showing when the corresponding analysis button was initially clicked).

### **Implementation Plan:**

Please provide the C++ code modifications and UI change descriptions to achieve this. Your plan should be:

1.  **Update UI Definition:** Describe the changes needed in `videoHandlerYUV.ui` (removing two buttons and adding two new "Revert" buttons).
2.  **Enhance State Management:** Add a member variable to your C++ handler class (e.g., `qint64 revertFrameNumber;`) to store the frame number when playback begins.
3.  **Modify Existing Click Handlers:** In the existing slots for the "First-level" and "Second-level" buttons, add logic to get the current frame from the `PlaybackController` and store it in your new state variable before starting playback.
4.  **Create New "Revert" Click Handlers:**
    *   Implement the new slots (e.g., `on_revertButton_L1_clicked()`).
    *   Inside these slots, call the necessary methods on your `PlaybackController` to:
        *   First, pause the playback.
        *   Second, seek the video to the frame number stored in your `revertFrameNumber` state variable.