# AGENTS.md

Guidance for AI coding agents working on the YUView codebase.

## 构建项目

根目录两个脚本封装环境初始化、qmake 配置和并行编译：

### Windows（MSVC + Qt6 + jom）

```bat
build.bat
```

自动 `vcvars64.bat` → `build/` → `qmake ..` → `jom`。WSL 下用 `cmd.exe /c build.bat`。默认**不启用**测试；如需：
```bat
mkdir build && cd build
qmake .. CONFIG+=UNITTESTS
jom
```

### Linux / macOS

```bash
./build.sh           # 默认启用单元测试
./build.sh --asan    # ASan 构建，禁用测试，产物到 build-asan/
```

自动探测 `qmake6`/`qmake`，按 `nproc`/`sysctl` 并行；macOS 同此脚本。

### 启用单元测试

须加 `CONFIG+=UNITTESTS`，否则 `YUViewUnitTest` 和 googletest 不进 SUBDIRS。Linux/macOS 由 `build.sh` 默认加；Windows 需手动。

Windows 相关 target（见根 Makefile）：`sub-submodules-googletest-qmake`（gtest.lib）、`sub-YUViewUnitTest`（测试 exe）。`jom sub-YUViewUnitTest` 不自动构建 gtest，需先 `jom sub-submodules-googletest-qmake` 或全量 `jom`。

## 运行测试

```bash
./build/YUViewUnitTest/YUViewUnitTest                              # Linux
QT_QPA_PLATFORM=offscreen ./build/YUViewUnitTest/YUViewUnitTest    # CI/无显示
build\YUViewUnitTest\YUViewUnitTest.exe                            # Windows
build\YUViewUnitTest\YUViewUnitTest.exe --gtest_filter=StatisticsFileCSV.*  # 单套件
```

套件名用 `--gtest_list_tests` 查看。`StatisticsFileCSVTest.cpp` 与 `StatisticsFileVTMBMSTest.cpp` 都定义了 `TEST(StatisticsFileCSV, testCSVFileParsing)`，同名注册为同套件两个用例。

## 子模块

```bash
git clone --recurse-submodules <repo-url>
# 或
git submodule update --init --recursive
```

`submodules/googletest` 是测试依赖。SSH 克隆失败可临时用 HTTPS（用 `git -c url.<...>.insteadOf=<...> submodule update --init` 或直接 clone 后 checkout 到记录的 commit）。

## 项目结构

- **YUViewLib/** — 核心业务逻辑（静态库）：`parser/`（HEVC/AVC/VVC/AV1/MPEG2）、`decoder/`（FFmpeg/Dav1d/libde265/HM/VTM）、`video/`（RGB/YUV）、`playlistitem/`、`ui/`、`statistics/`
- **YUViewApp/** — Qt 应用入口
- **YUViewUnitTest/** — Google Test 单元测试
- **submodules/** — googletest 与 googletest-qmake
- **deployment/** — Windows NSIS 安装包脚本

## 架构模式

1. **解码器插件**：`decoderBase` 抽象接口，`QLibrary` 动态加载 dll；`decoderFFmpeg`/`Dav1d`/`Libde265`/`HM`/`VTM` 为实现
2. **播放列表项**：`playlistItem` 基类，子类对应不同媒体类型
3. **视频处理器**：`videoHandler` 及子类（`Difference`、`Resample`）
4. **解析器**：按编码器在 `parser/` 下组织
5. **统计绘制**：`stats::paintStatisticsData` 统一绘制，样式由 `StatisticsType::gridStyle`/`vectorStyle`（`LineDrawStyle`）控制

## 解码器库加载约定

- 运行时 `QLibrary` 动态加载，逻辑见 `decoderBase.cpp:80`（`loadDecoderLibrary`），候选名由 `getLibraryNames()` 返回按序尝试
- **internals 支持由符号解析（`QLibrary::resolve`）决定，不是文件名**：libde265 的 `["libde265-internals", "libde265"]` 仅为加载优先级，真正判据是能否 resolve 出 `de265_internals_*`（见 `decoderLibde265.cpp:166`）。`libde265-internals.dll` 导出 internals 接口提供编码结构统计（CTU/CU/PB/IntraDir/TU），官方 `libde265.dll` 只能解码。VTM/HM 同理靠 `internalsSupported` 标志

## 代码规范

- C++20，`.clang-format`（2 空格、Allman 风格），UTF8/LF
- 成员变量 CamelCase，**无前缀**（不要 `m_`/`i`/`p`）
- 参数：基本类型按值，复杂类型按 const 引用；const 正确性
- **核心代码优先 Qt-free**：业务逻辑用 `std` 而非 Qt 类型，Qt 仅用于 GUI
- 详细规范见 `HACKING.md`

### Include 顺序（`.cpp`）

1. `"foo.h"`（对应头文件，必须第一个，后空一行）
2. `<cfoo>` 标准库（C 头用 `<cmath>` 而非 `<math.h>`）
3. 系统头 → 4. 其它库头 → 5. Qt 头 → 6. YUView 本地头

`.h` 只 include 声明所必需的，按值持有必须 include，按指针/引用持有可前向声明。

## 提交约定

- 分支 `develop`；英文祈使句，首行简短，正文解释 why
- 示例：`Fix rawDataCacheValid data race and remove dead loadFrameForCaching code`
- **不要主动 commit**，除非用户明确要求

## 常见陷阱

- **改全局 git config**：临时 url 重写用 `git -c`，不要 `--global`
- **cmd.exe 下的 gtest_filter**：冒号 `:` 会被误解析，分多次运行或用单个 filter
- **`jom sub-YUViewUnitTest` 不构建 gtest**：需先 `sub-submodules-googletest-qmake` 或全量 `jom`
- **测试同名冲突**：两文件都定义 `TEST(StatisticsFileCSV, testCSVFileParsing)`，修改时两个都要同步
- **`StatisticsType` 默认值变更**：构造函数改默认值后 `setInitialState()` 捕获的 init 状态同步变化，相关单测断言可能需同步更新

## 依赖

- Qt6（qt6-base-dev / Qt 6.9.0 Windows 预编译包）
- FFmpeg（视频解码）
- libde265（HEVC 解码，可选；internals 变体提供统计）
- Google Test（git 子模块）
