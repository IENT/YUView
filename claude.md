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


### **Task2: Enhancing UI/UX for "Video Distortion Analysis" Controls in a Qt Application**

**Your Role:** You are an expert C++ Qt developer with a strong focus on creating intuitive and responsive user interfaces.

### **High-Level Goal:**

I need you to enhance the User Experience (UX) of two existing buttons in Your application's UI. The core playback functionality for these buttons is already working correctly. This task is purely about improving their visual feedback and interaction logic.

### **Context: Current Working Functionality**

I have a UI file (`videoHandlerYUV.ui`) with several "Video Distortion Analysis" buttons. Two of them, "First-level (30FPS)" and "Second-level (1FPS)", already function as follows:
*   When clicked, they correctly start video playback at their specified speed (30 FPS or 1 FPS).

### **The Problem: The UI is Not Responsive**

The current user experience is poor because the buttons provide no visual feedback after being clicked. The user cannot tell which mode is active, and there is no intuitive way to stop the playback started by these buttons.

### **New Functional Requirements (Your Task):**

I need you to implement the following UI/UX enhancements. You will likely need to modify the C++ slot connected to these buttons' `clicked()` signals.

**1. Implement a Visual "Active State" for the Buttons:**

*   **On the FIRST click** of either the "First-level (30FPS)" or "Second-level (1FPS)" button:
    *   Start video playback at the specified speed (this part already works).
    *   **Change the appearance of the clicked button to indicate it is "active".** Specifically:
        *   Set the button's background color to a light green (e.g., `#90EE90`). You can achieve this using a stylesheet.
        *   Make the button's font bold.
    *   If another button is already in this "active" state, it should first be reset to its normal state before the newly clicked button becomes active. Only one button can be active at a time.

**2. Implement Loop/Repeat Playback:**

*   When playback is initiated by either of these two buttons, it must **automatically loop continuously**.
*   Please find and enable the relevant setting in Your existing `PlaybackController` class. I believe there is a property or method related to a `repeatModeButton` that can be used to enable this.

**3. Implement Toggle-to-Pause Functionality:**

*   **On a SECOND click** on a button that is *already in the "active" state*:
    *   The playback should **immediately pause**.
    *   **Reset the button's appearance** back to its default (normal background color, normal font weight).

### **Implementation Plan:**

Please provide the C++ code modifications to achieve this. Your plan should be:

1.  **Introduce State Management:** Add member variables to the C++ handler class to keep track of which button (if any) is currently active.
2.  **Modify the Click Handler Slot:** Refactor the single slot connected to both buttons' `clicked()` signals.
3.  **Implement State Logic:** Inside the slot, use `sender()` to identify which button was clicked.
    *   If the clicked button is **not** the currently active one:
        *   Reset any other active button to its default style.
        *   Set the clicked button's style to "active" (green background, bold font).
        *   Enable loop mode in the `PlaybackController`.
        *   Start playback at the correct FPS.
        *   Update the state variable to mark this button as active.
    *   If the clicked button **is** the currently active one:
        *   Pause playback.
        *   Reset the button's style to default.
        *   Update the state variable to indicate no button is active.
4.  **Provide Stylesheet Code:** Provide the simple Qt Stylesheet strings needed to set and unset the button's appearance.

By providing the updated C++ slot implementation and any necessary state variables, you will solve this UX problem completely.
