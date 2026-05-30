# FFmpeg 版本支持矩阵

## 加载器支持的版本组合

来自 `FFmpegVersionHandler.cpp:104-110`。按优先级从新到旧尝试加载。

| avutil | avcodec | avformat | swresample | FFmpeg 分支 |
|--------|---------|----------|------------|-------------|
| 59 | 61 | 61 | 5 | FFmpeg 7.x |
| 58 | 60 | 60 | 4 | FFmpeg 6.x |
| 57 | 59 | 59 | 4 | FFmpeg 5.x |
| 56 | 58 | 58 | 3 | FFmpeg 4.x |
| 55 | 57 | 57 | 2 | FFmpeg 3.x |
| 54 | 56 | 56 | 1 | FFmpeg 2.x |

## 各 wrapper 实际覆盖范围

| 文件 | 检查库 | 覆盖版本 | 对应 FFmpeg 分支 | 未知版本行为 |
|------|--------|---------|-----------------|-------------|
| AVMotionVectorWrapper | avutil | 54, 55-59 | 2.x ~ 7.x | 构造 throw / count return 0 |
| AVFrameSideDataWrapper | avutil | 54-59 | 2.x ~ 7.x | throw |
| AVFrameWrapper | avutil | 54-59 | 2.x ~ 7.x | throw（getMetadata 断言 >=57） |
| AVPixFmtDescriptorWrapper | avutil | 54-59 | 2.x ~ 7.x | 静默跳过（无 throw） |
| AVCodecContextWrapper | avcodec | 56-61 | 2.x ~ 7.x | throw |
| AVCodecWrapper | avcodec | 56-61 | 2.x ~ 7.x | throw |
| AVPacketWrapper | avcodec | 56-61 | 2.x ~ 7.x | throw |
| AVFormatContextWrapper | avformat | 56-61 | 2.x ~ 7.x | throw |
| AVStreamWrapper | avformat | 56-61 | 2.x ~ 7.x | throw |
| AVCodecParametersWrapper | avformat | 56-61 | 2.x ~ 7.x | throw（56 设 nullptr） |
| AVInputFormatWrapper | avformat | 56-61 | 2.x ~ 7.x | 静默跳过（无 throw） |

## 关键风险点

1. **AVPixFmtDescriptorWrapper.cpp** 和 **AVInputFormatWrapper.cpp** 是仅有的两个对未知版本静默失败的文件，可能导致后续空指针访问。

---

# 解码器与外部库

## 解码器架构

### 基类层次

```
decoderBase (抽象基类)
    │
    └── decoderFFmpeg          ← 直接继承，使用 FFmpeg 多格式解码
    │
    └── decoderBaseSingleLib   ← 通过 QLibrary 加载外部库
            │
            ├── decoderLibde265  (libde265)
            ├── decoderHM         (HM 参考软件)
            ├── decoderVTM        (VTM 参考软件)
            ├── decoderVVDec      (VVDec)
            └── decoderDav1d      (dav1d)
```

### 各解码器支持情况

| 解码器 | 底层库 | 支持格式 | 预编译库提供 |
|--------|--------|----------|-------------|
| `decoderFFmpeg` | FFmpeg | HEVC/AVC/VVC/AV1/MPEG-2/VP9 等 | **所有平台** |
| `decoderLibde265` | libde265 | HEVC | **Windows/macOS/Linux** (CI 下载) |
| `decoderDav1d` | dav1d | AV1 | 仅源码构建 |
| `decoderHM` | HM 参考软件 | HEVC | 仅源码构建 |
| `decoderVTM` | VTM 参考软件 | VVC | 仅源码构建 |
| `decoderVVDec` | VVDec | VVC | 仅源码构建 |

---

# CI 预编译库支持

来自 `.github/workflows/Build.yml` 和 `flatpak.yml`。

## 按平台分发

| 平台 | Qt | libde265 | openSSL | Flatpak |
|------|-----|----------|---------|---------|
| **Ubuntu 22.04/24.04 (x64)** | apt / 预编译 | 预编译 .so | - | - |
| **Ubuntu 22.04/24.04 (ARM)** | apt | - | - | - |
| **macOS 15 (Apple Silicon/Intel)** | Homebrew / 预编译 | 预编译 .dylib | - | - |
| **Windows 2022** | 预编译 | 预编译 .dll | 预编译 | - |
| **Linux Flatpak** | - | - | - | ✅ |

## CI Jobs

| Job | 平台 | 构建目标 |
|-----|------|---------|
| `build-unix-native` | ubuntu-22.04/24.04 (x64 + ARM) | 本地测试 |
| `build-mac-native` | macos-15 (Arm64 + Intel) | 本地测试 |
| `build-linux-mac` | ubuntu-22.04 / macos-15 | .AppImage / .app.zip |
| `build-windows` | windows-2022 | .zip / .msi |
| `flatpak-builder` | ubuntu-latest (GNOME 50) | .flatpak |

---

# Packet Analysis 解析架构

Packet Analysis 的数据**不是**由 FFmpeg 直接提供，而是由 YUView 自己的解析器完成。FFmpeg 仅作为容器格式的解复用器（demuxer）。

## 解析器层次

```
Parser (基类)
    │
    ├── ParserAnnexBAVC   ──► 纯 YUView 解析 AVC/H.264 语法
    ├── ParserAnnexBHEVC  ──► 纯 YUView 解析 HEVC/H.265 语法
    ├── ParserAnnexBVVC   ──► 纯 YUView 解析 VVC/H.266 语法
    ├── ParserAV1OBU      ──► 纯 YUView 解析 AV1 OBU 语法
    ├── ParserSubtitle    ──► 纯 YUView 解析字幕
    │
    └── ParserAVFormat    ──► FFmpeg 解复用 + YUView 深解析
```

## ParserAVFormat 的工作流程

`ParserAVFormat`（`YUViewLib/src/parser/AVFormat/ParserAVFormat.cpp`）用于容器格式：

1. **FFmpeg 解复用**：使用 `FileSourceFFmpegFile` 调用 `libavformat` 提取原始 packet
2. **YUView 深解析**：根据视频格式选择对应解析器
   - AVC/HEVC/MPEG2 → `ParserAnnexB*` 解析 NAL unit 内部语法
   - AV1 → `ParserAV1OBU` 解析 OBU
   - 字幕 → YUView 自己的字幕解析器

## 输入格式与解析器映射

| InputFormat | 解析器 | 数据来源 |
|-------------|--------|---------|
| `AnnexBHEVC` | `ParserAnnexBHEVC` | YUView 纯解析 |
| `AnnexBVVC` | `ParserAnnexBVVC` | YUView 纯解析 |
| `AnnexBAVC` | `ParserAnnexBAVC` | YUView 纯解析 |
| `AnnexBMPEG2` | `ParserAnnexBMPEG2` | YUView 纯解析 |
| `Libav` | `ParserAVFormat` | FFmpeg demux + YUView 深解析 |
| `AV1` | `ParserAV1OBU` | YUView 纯解析 |

## 关键文件

- UI: `YUViewLib/src/ui/widgets/BitstreamAnalysisWidget.cpp`
- 解析器基类: `YUViewLib/src/parser/Parser.h`
- FFmpeg 容器解析: `YUViewLib/src/parser/AVFormat/ParserAVFormat.cpp`
- AVC 解析: `YUViewLib/src/parser/AVC/`
- HEVC 解析: `YUViewLib/src/parser/HEVC/`
- VVC 解析: `YUViewLib/src/parser/VVC/`
- AV1 解析: `YUViewLib/src/parser/AV1/`