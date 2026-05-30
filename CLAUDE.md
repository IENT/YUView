# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 构建项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目（启用单元测试）
qmake6 CONFIG+=UNITTESTS ..

# 编译
make -j$(nproc)
```

## 运行测试

```bash
# 运行单元测试（需要配置 UNITTESTS）
./build/YUViewUnitTest/YUViewUnitTest

# CI 环境使用 offscreen 模式
QT_QPA_PLATFORM=offscreen ./build/YUViewUnitTest/YUViewUnitTest
```

## 项目结构

- **YUViewLib/** - 核心业务逻辑库（静态库）
  - `src/parser/` - 比特流解析器（HEVC/AVC/VVC/AV1/MPEG2）
  - `src/decoder/` - 解码器抽象层和实现（FFmpeg/Dav1d/libde265/HM/VTM）
  - `src/video/` - 视频帧处理（RGB/YUV 格式转换）
  - `src/playlistitem/` - 播放列表项（Raw 文件/压缩视频/图像/统计叠加）
  - `src/ui/` - Qt UI 组件（主窗口、播放控制、设置对话框）
  - `src/statistics/` - 统计信息叠加系统
- **YUViewApp/** - Qt 应用程序入口点
- **YUViewUnitTest/** - Google Test 单元测试
- **submodules/** - googletest 和 googletest-qmake

## 架构模式

1. **解码器插件架构**：`decoderBase` 定义抽象接口，`decoderFFmpeg`、`decoderDav1d`、`decoderLibde265` 等实现具体解码器
2. **播放列表项层次**：`playlistItem` 基类，各类媒体（Raw YUV/压缩视频/图像序列/统计文件）作为子类
3. **视频处理器层次**：`videoHandler` 处理帧操作，子类包括 `videoHandlerDifference`、`videoHandlerResample`
4. **解析器层次**：按编码器在 `parser/` 下组织（HEVC/AVC/VVC/AV1 等）

## 代码规范

- C++20 标准
- 核心代码优先 Qt-free（增强可移植性）
- 格式化工具：`.clang-format`（2 空格缩进、Allman 大括号风格）
- 编码规范详见 `HACKING.md`：
  - UTF8 编码、LF 行尾
  - 成员变量使用 CamelCase，无前缀
  - 参数传递：按值传递基本类型，按引用传递复杂类型
  - const 正确性
  - 优先使用 `std` 而非 Qt 类型

## 依赖

- Qt6 (qt6-base-dev)
- libde265（HEVC 解码，可选）
- FFmpeg（视频解码）
- Google Test（通过 git 子模块管理）

## 子模块初始化

```bash
git clone --recurse-submodules <repo-url>
# 或初始化
git submodule update --init --recursive
```