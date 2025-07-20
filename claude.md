### **Request: Add Four New QT Controls for Video Distortion Analysis**

We need to add four new QT controls to the "Raw file properties" section, located beneath the existing "enableNative10BitDisplay" option. These controls will provide different levels of video distortion analysis.

**1. First-level distortion:**
*   **Functionality:** When clicked, this control will initiate playback of the video from the current frame at a rate of 30 frames per second (FPS).

**2. Second-level distortion:**
*   **Functionality:** When this control is clicked, the video will start playing from the current frame at a rate of 1 frame per second (FPS).

**3. Third-level distortion:**
*   **Functionality:** This control, upon being clicked, will initiate a repetitive 1-frame-per-second (FPS) comparison at the current frame's position. The comparison will be at a 1X magnification and will be against a YUV file that contains "ORI" in its filename.
*   **Pre-conditions for activation:**
    *   A YUV file with "ORI" in its name must exist.
    *   The "ORI" YUV file must have the exact same file size as the currently loaded file.
    *   The user's mouse cursor must not be positioned over the "ORI" YUV file.
*   **Exception:** If the currently loaded file is the "ORI" file, a notification will be displayed to the user, and no further action will be taken.

**4. Fourth-level distortion:**
*   **Functionality:** Similar to the third-level distortion, clicking this control will start a repetitive 1-frame-per-second (FPS) comparison at the current frame's position. This comparison will also be at a 1X magnification and against a YUV file with "ORI" in its filename.
*   **Pre-conditions for activation:**
    *   A YUV file with "ORI" in its name must exist.
    *   The "ORI" YUV file must have the exact same file size as the currently loaded file.
    *   The user's mouse cursor must not be positioned over the "ORI" YUV file.
*   **Exception:** If the currently loaded file is the "ORI" file, a notification will be displayed to the user, and no further action will be taken.

***
