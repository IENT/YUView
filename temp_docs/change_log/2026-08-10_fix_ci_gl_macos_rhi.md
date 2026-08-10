# Change Log

## 2026-08-10 — Fix Linux GL headers + macOS RHI backend selection

### Linux CI (`build-unix-native`)
`FrameHandler` pulls `QOpenGLContext` → needs `GL/gl.h`. Install
`libgl1-mesa-dev` / `libglu1-mesa-dev` in the apt step (already present in
the deploy job, missing from native).

### macOS RHI
`#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)` matched macOS (UNIX) and
tried `QRhiVulkanInitParams`, which Apple Qt builds do not provide.
Order is now: Win D3D → macOS/iOS Metal → Linux Vulkan (`QT_CONFIG(vulkan)`)
→ OpenGL fallback. Private-header includes follow the same platform split.
