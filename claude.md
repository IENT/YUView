# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

YUView is a cross-platform Qt-based YUV player and video analysis tool with advanced analytics capabilities. The project supports multiple video formats, decoders, and includes specialized features for HDR video rendering and bitstream analysis.

## Build System

**Build Commands:**
```bash
# Standard build
qmake YUView.pro
make

# Windows with MinGW
"C:/Qt/6.9.1/mingw_64/bin/qmake.exe" -o Makefile YUView.pro -spec win32-g++
"C:/Qt/Tools/mingw1310_64/bin/mingw32-make"

# Build with unit tests
qmake "CONFIG+=UNITTESTS" YUView.pro
make

# Clean build
make clean
```

**Project Structure:**
- `YUView.pro` - Main project file (subdirs template)
- `YUViewLib/` - Core library (static lib with all functionality)
- `YUViewApp/` - Application entry point (depends on YUViewLib)
- `YUViewUnitTest/` - Unit tests using GoogleTest

## Core Architecture

**Video Processing Pipeline:**
- `FrameHandler` - Base class for all frame processing
- `videoHandler` - Abstract base for video-specific handling
- `videoHandlerYUV` - YUV format processing
- `videoHandlerRGB` - RGB format processing
- `videoHandlerDifference` - Frame comparison functionality

**HDR Rendering System (Important for Current Work):**
- `HDRRenderingManager` - Manages HDR detection and widget lifecycle
- `HDR_VideoWidget` - OpenGL widget for native 10-bit HDR rendering
- `HDRDetection`/`HDRDetectionWorker` - Asynchronous HDR capability detection
- Key issue: Widget lifecycle management during enable/disable cycles

**Parser Architecture:**
- `Parser` - Base parser interface
- `ParserAnnexB` - Base for Annex-B format parsers
- Format-specific parsers: `ParserAnnexBHEVC`, `ParserAnnexBAVC`, `ParserAnnexBVVC`
- Each parser includes detailed NAL unit parsing and SEI message handling

**Decoder Integration:**
- `decoderBase` - Abstract decoder interface
- Multiple decoder backends: FFmpeg, libde265, HM, VTM, dav1d
- Dynamic library loading for optional decoders

## Development Guidelines

**Code Style:**
- Follow `.clang-format` configuration in root directory
- 2-space indentation, no tabs
- CamelCase for variables, PascalCase for classes/files
- No prefixes for class members (no `m_`, `p_`, etc.)
- Include order: own header, std, system, other libs, Qt, YUView headers

**Qt Integration:**
- Minimize Qt dependencies outside GUI code
- Parsers/decoders should compile "Qt free"
- Use std equivalents when possible for portability

**Platform Support:**
- Windows (MSVC, MinGW), Linux, macOS
- C++17 required
- 32-bit builds unsupported

## Testing

**Unit Tests:**
```bash
# Build and run tests
qmake "CONFIG+=UNITTESTS" YUView.pro
make
./YUViewUnitTest/YUViewUnitTest
```

**Test Structure:**
- GoogleTest framework
- Tests organized by component (common/, video/, statistics/, etc.)
- Test utilities in `YUViewUnitTest/common/`

## Critical Current Issue (HDR Rendering)

**Problem:** HDR widget lifecycle management bug causing rendering failures on second enable
**Root Cause:** Widget reuse without proper reinitialization
**Files Involved:**
- `HDRRenderingManager.cpp` - Primary fix location
- `HDR_VideoWidget.cpp` - Widget initialization handling
- `videoHandlerYUV.cpp` - Frame pushing logic

**Solution Pattern:** Replace widget hiding with complete destruction in `cleanupHDRResources()`

## Key Dependencies

- Qt 6.x (core, gui, widgets, opengl, xml, concurrent, network)
- Optional: FFmpeg, libde265, dav1d (runtime loaded)
- Windows: DXGI, user32, ole32 (for HDR detection)
- Testing: GoogleTest

## Resource Management

**Important Patterns:**
- OpenGL context management in video widgets
- Thread-safe video processing with Qt concurrent
- Memory management for large video frames
- Decoder library lifecycle (dynamic loading/unloading)

## File Organization

**Video Processing:** `YUViewLib/src/video/`
**Parsers:** `YUViewLib/src/parser/[FORMAT]/`
**UI Components:** `YUViewLib/src/ui/`
**Common Utilities:** `YUViewLib/src/common/`
**Platform Integration:** Format-specific handling in respective directories