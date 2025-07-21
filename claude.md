### **Subject: Action Plan for YUView 10-Bit Display & Feature Implementation**

**Preamble:**
This document outlines the implementation and debugging tasks for two distinct issues in YUView. The first is a rendering artifact in the 10-bit display path. The second concerns non-functional "Video Distortion Analysis" controls. Please follow the action plan below to resolve both.

***

### **Task 1: Resolve 10-Bit Display Color Banding Artifacts**

**Background:**
The "enable 10bit native display" feature, when tested on a 10-bit capable HDR monitor with a 10-bit linear grayscale gradient (luma 0-1023) YUV420 or YUV444, still produces significant color banding. The transition is not smooth, indicating a loss of precision in the rendering pipeline.

**Key Observation:**
However, the banding pattern is different from the 8-bit path, confirming the new YUV-to-RGB conversion is active, but still not able to retain the 10bit precision. The issue likely lies further down the pipeline.

**Core Problem:**
We suspect that the rendering process, specifically involving `QPainter` and `QImage`, is down-sampling the 16-bit image data before it reaches the display, thus nullifying the 10-bit precision.

---

#### **Implementation & Debugging Plan:**

Your task is to investigate and fix the entire rendering pipeline to ensure true 10-bit output. Follow these steps methodically.

1.  **Verify `QImage` Data Integrity:**
    * **Action:** Before any rendering call, directly inspect the `QImage` object holding the gradient data.
    * **Goal:** Programmatically read the pixel values from the `QImage` (`QImage::Format_RGBA64_Premultiplied`) and confirm they represent a full 16-bit range that correctly maps the original 10-bit (0-1023) YUV data. Log these values to ensure no precision was lost during the YUV-to-`QImage` conversion.

2.  **Analyze and Configure the Qt Rendering Backend:**
    * **Action:** Investigate which rendering backend Qt is currently using (e.g., OpenGL, DirectX, Vulkan).
    * **Goal:** Ensure the backend is configured for high-bit-depth rendering. If using Qt 6, prioritize migrating this rendering path to **QRhi (Rendering Hardware Interface)**, as it provides the most direct control for HDR output. If on Qt 5, ensure the OpenGL context is created with the necessary attributes for 10-bit+ framebuffers.

3.  **Implement Correct Color & Data Mapping:**
    * **Action:** Review the shader code or `QPainter` operations responsible for drawing the image.
    * **Goal:** The 16-bit integer data in the `QImage` must be correctly mapped to the format required by the HDR display pipeline (e.g., a normalized float for a shader). Ensure there is no implicit or explicit truncation from 16-bit integers to 8-bit integers anywhere in this process. Verify that the color space transformations (if any) are precision-preserving.

4.  **Validate System-Level HDR Configuration:**
    * **Action:** Double-check the host machine's settings.
    * **Goal:** Confirm that the OS (Windows/macOS) is in **HDR mode** and the graphics driver (NVIDIA/AMD/Intel) is configured to output a **10-bit color depth**. An issue at this level will override any application-level correctness.

***

### **Task 2: Implement and Fix "Video Distortion Analysis" QT Controls**

**Background:**
The four newly added "Video Distortion Analysis" controls are not functioning as specified. The "First-level distortion" button, for instance, fails to set the correct playback speed and magnification.

---

#### **Functional Requirements:**

You must implement the logic for these controls to match the following specifications exactly.

1.  **First-level distortion:**
    * **On-Click Action:** Start video playback from the current frame.
    * **Playback Speed:** 30 FPS.
    * **Magnification:** **Set view to 1x zoom.** (This was identified as a missing step).

2.  **Second-level distortion:**
    * **On-Click Action:** Start video playback from the current frame.
    * **Playback Speed:** 1 FPS.
    * **Magnification:** **Set view to 1x zoom.** (This was identified as a missing step).


3.  **Third-level distortion:**
    * **On-Click Action:** At the current frame, initiate a repetitive 1 FPS visual comparison against the "ORI" file at 1x magnification.
    * **Activation Pre-conditions:**
        * An "ORI" YUV file (filename contains "ORI") must exist.
        * The "ORI" file size must exactly match the current file's size.
        * The user's mouse cursor must **not** be hovering over the "ORI" file's UI element.
    * **Exception:** If the current file *is* the "ORI" file, display a user notification and abort the action.

4.  **Fourth-level distortion:**
    * **Functionality:** Identical to the third-level distortion, but set view to 2x zoom.

#### **Implementation & Debugging Plan:**

1.  **Verify Signal/Slot Connections:**
    * **Action:** Ensure each button's `clicked()` signal is correctly connected to its corresponding handler slot in the C++ code.

2.  **Implement "First-level" and "Second-level" Controls:**
    * **Action:** Debug the handler for "First-level".
    * **Fixes:**
        * Implement the logic to set the player's playback rate to 30 FPS.
        * **Add the missing logic to set the viewport's zoom level to 1x.**
        * Ensure the player state is correctly updated to "playing."
    * **Action:** Debug the handler for "Second-level" to ensure the playback rate is set to 1 FPS.

3.  **Implement "Third-level" and "Fourth-level" Controls:**
    * **Action:** Implement and debug the handler shared by these two controls.
    * **Fixes:**
        * **Pre-condition Logic:** Write robust code to search for the "ORI" file, compare file sizes, and check the mouse cursor's position. This logic must be flawless.
        * **Exception Handling:** Implement the check to see if the current file is the "ORI" file and show a `QMessageBox` or similar notification if it is.
        * **Core Functionality:** Use a `QTimer` set to a 1000ms interval (1 FPS) to trigger the comparison logic. In the timer's slot, implement the frame-swapping or side-by-side rendering logic against the "ORI" file, ensuring the view is at 1x zoom.
