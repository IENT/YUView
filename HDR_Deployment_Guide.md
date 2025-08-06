# YUView HDR Implementation - Deployment Guide

## 編譯錯誤修復

### 問題描述
編譯時出現兩個主要錯誤：
1. `QOpenGLWidget: No such file or directory`
2. `expected identifier before ',' token` in `MoveAndZoomableView.h`

### 根本原因
1. **Qt 6.x模塊問題**: OpenGL相關的類需要額外的模塊依賴，特別是`QOpenGLWidget`需要`Qt::OpenGLWidgets`模塊
2. **Windows宏衝突**: Windows.h定義的`IN`和`OUT`宏與`ZoomMode`枚舉值衝突

## 修復步驟

### 1. 修改YUViewLib.pro文件

將現有的：
```pro
QT += core gui widgets opengl xml concurrent network
```

修改為：
```pro
QT += core gui widgets opengl openglwidgets xml concurrent network
```

**說明**: 添加`openglwidgets`模塊以支持`QOpenGLWidget`類。

### 2. 修復Windows宏衝突

**✅ 已修復**: 將`MoveAndZoomableView.h`中的`ZoomMode`枚舉值重新命名：
```cpp
enum class ZoomMode
{
  ZOOM_IN,    // 原來是 IN
  ZOOM_OUT,   // 原來是 OUT  
  TO_VALUE
};
```

**✅ 已修復**: 同時更新了`MoveAndZoomableView.cpp`中所有相關引用。

### 3. 驗證Qt模塊支持

確認您的Qt安裝包含以下模塊：
- `Qt::Core`
- `Qt::Gui` 
- `Qt::Widgets`
- `Qt::OpenGL`
- `Qt::OpenGLWidgets` ← **新增必需**
- `Qt::Xml`
- `Qt::Concurrent`
- `Qt::Network`

### 3. 檢查Qt版本兼容性

HDR實現需要：
- **最低Qt版本**: Qt 6.0+
- **推薦Qt版本**: Qt 6.2+ (更好的OpenGL支持)
- **OpenGL要求**: OpenGL 3.3 Core Profile或更高

### 4. Windows平台額外要求

確保系統具備：
- **Visual C++ Redistributable** (如使用MSVC編譯器)
- **Windows 10 版本1903+** (完整HDR支持)
- **支援HDR的顯示器和驅動程式**

## 完整的YUViewLib.pro配置

```pro
QT += core gui widgets opengl openglwidgets xml concurrent network

TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++17
CONFIG -= debug_and_release
CONFIG += object_parallel_to_source

SOURCES += $$files(src/*.cpp, true)
HEADERS += $$files(src/*.h, true)

FORMS += $$files(ui/*.ui, false)

INCLUDEPATH += src/

RESOURCES += \
    images/images.qrc \
    docs/docs.qrc \
    resources/shaders/shaders.qrc

contains(QT_ARCH, x86_32|i386) {
    warning("You are building for a 32 bit system. This is untested and not supported.")
}

SVNN = $$system("git describe --tags")
LASTHASH = $$system("git rev-parse HEAD")
isEmpty(LASTHASH) {
    LASTHASH = 0
}
isEmpty(SVNN) {
    SVNN = 0
}

win32 {
    DEFINES += NOMINMAX
    # Windows-specific libraries for HDR detection
    LIBS += -ldxgi -luser32
}

win32-msvc* {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=$${HASHSTRING}
}
win32-g++ | linux | macx {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=\"$${HASHSTRING}\"
}

VERSTR = '\\"$${SVNN}\\"'
DEFINES += YUVIEW_VERSION=$${VERSTR}
```

## 驗證步驟

### 1. 清理並重新編譯
```bash
# 清理舊的編譯文件
cd D:\YUView
rm -rf build/

# 重新配置項目
qmake

# 編譯
make -j4
```

### 2. 檢查HDR功能
編譯成功後，運行YUView並：
1. 開啟任何10-bit影片檔案
2. 勾選"Enable 10-bit Display"選項
3. 查看調試輸出中的HDR檢測信息

### 3. 預期的調試輸出
```
YUView: Detecting HDR capabilities...
HDR display detected: DISPLAY1
HDR mode: HDR10/BT.2020 PQ (10-bit)
Max luminance: 1000 nits
Bits per channel: 10
HDR surface format configured:
  Red buffer size: 10
  Green buffer size: 10
  Blue buffer size: 10
  Alpha buffer size: 2
  OpenGL version: 3.3
```

## 常見問題解決

### Q1: 仍然找不到QOpenGLWidget
**解決方案**: 
- 確認Qt安裝完整，重新安裝Qt並選擇所有OpenGL相關組件
- 檢查PATH環境變數包含正確的Qt bin目錄

### Q2: HDR檢測失敗
**解決方案**:
- 確認使用HDR相容顯示器
- 更新顯示器驅動程式到最新版本
- 在Windows顯示設定中啟用HDR

### Q3: 編譯成功但運行時OpenGL錯誤
**解決方案**:
- 更新顯卡驅動程式
- 檢查OpenGL版本：`glxinfo | grep "OpenGL version"` (Linux)
- 嘗試設定環境變數：`QT_OPENGL=desktop`

### Q4: mingw32-make錯誤
**解決方案**:
- 使用Qt Creator內建的編譯系統
- 或確保mingw32-make路徑正確設定
- 嘗試使用nmake (如果使用MSVC)

## 開發環境建議

### 推薦配置
- **IDE**: Qt Creator 最新版
- **編譯器**: MSVC 2019+ (Windows) 或 GCC 9+ (Linux)
- **Qt版本**: Qt 6.2.4 或更新
- **CMake**: 3.20+ (如果使用CMake)

### HDR測試環境
- **顯示器**: 支援HDR10的顯示器
- **連接**: HDMI 2.1 或 DisplayPort 1.4+
- **作業系統**: Windows 10 1903+ 或 Windows 11
- **顯卡**: 支援HDR的現代顯卡 (GTX 1060+, RTX系列, AMD RX 400+)

## 部署檢查清單

- [ ] 修改YUViewLib.pro添加openglwidgets模塊
- [ ] 清理並重新編譯項目
- [ ] 驗證HDR檢測功能正常
- [ ] 測試10-bit影片播放
- [ ] 確認HDR/SDR自動切換
- [ ] 驗證用戶通知功能
- [ ] 性能測試(60fps播放)

## 技術支援

如果遇到其他問題：

1. **檢查Qt文檔**: [Qt OpenGL](https://doc.qt.io/qt-6/qtopengl-index.html)
2. **查看調試輸出**: 運行時使用`QT_LOGGING_RULES="*.debug=true"`
3. **硬體兼容性**: 使用`dxdiag`(Windows)檢查OpenGL支援
4. **社群支援**: Qt論壇或Stack Overflow

---

*最後更新: 2024年12月 - YUView HDR實現完成*