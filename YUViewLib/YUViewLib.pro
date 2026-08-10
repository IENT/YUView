# Qt module configuration with version compatibility
QT += core gui widgets opengl xml concurrent network

# openglwidgets module only exists in Qt6, not in Qt5
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += openglwidgets
    # gui-private required for QRhi (Qt Rendering Hardware Interface) HDR support
    QT += gui-private
    # shadertools module for runtime shader compilation (optional, for RHI HDR support)
    # If not available, pre-compiled .qsb files are needed
    qtHaveModule(shadertools): QT += shadertools
}

TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++20
CONFIG -= debug_and_release
CONFIG += object_parallel_to_source

# -----------------------------------------------------------------------------
# Portable baseline ISA + hot-path dynamic dispatch
# -----------------------------------------------------------------------------
# Do NOT pass global -arch:AVX2 / -mavx2 here: that would make the whole library
# illegal on older CPUs. Hot kernels (e.g. HDRYUVRepack_avx2.cpp) are compiled
# separately with AVX2 flags below; CpuFeatures (CPUID + XGETBV) picks AVX2 /
# SSE2 / scalar once at first use.
# -----------------------------------------------------------------------------
win32-msvc*|win32-clang-msvc {
    # MSVC / clang-cl: WPO + LTCG; precise FP for reproducible analysis math
    QMAKE_CFLAGS_RELEASE   += -GL -fp:precise -Ob3 /Gw
    QMAKE_CXXFLAGS_RELEASE += -GL -fp:precise -Ob3 /Gw
    QMAKE_LFLAGS_RELEASE   += /LTCG /OPT:REF /OPT:ICF

    message("YUViewLib: portable Release flags = MSVC/LTCG/fp:precise (baseline ISA)")
}
else:win32-g++|win32-clang-g++|unix {
    # GCC / Clang / MinGW / Apple Clang: -O3 + LTO; no fast-math; no global -mavx*
    QMAKE_CFLAGS_RELEASE   -= -O2
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CFLAGS_RELEASE   += -O3 -flto -funroll-loops -fomit-frame-pointer \
                              -ffunction-sections -fdata-sections
    QMAKE_CXXFLAGS_RELEASE += -O3 -flto -funroll-loops -fomit-frame-pointer \
                              -ffunction-sections -fdata-sections
    QMAKE_LFLAGS_RELEASE   += -flto

    contains(QT_ARCH, arm64)|contains(QT_ARCH, arm) {
        QMAKE_CFLAGS_RELEASE   += -ftree-vectorize
        QMAKE_CXXFLAGS_RELEASE += -ftree-vectorize
    }

    linux|win32-g++|win32-clang-g++ {
        QMAKE_LFLAGS_RELEASE += -Wl,--gc-sections -Wl,--as-needed
    }
    macx {
        QMAKE_LFLAGS_RELEASE += -Wl,-dead_strip
    }

    message("YUViewLib: portable Release flags = GCC/Clang -O3/LTO (baseline ISA)")
}

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libpng
}

SOURCES += $$files(src/*.cpp, true)
HEADERS += $$files(src/*.h, true)

# Real QRhi HDR window needs Qt 6.4+. On older kits (Ubuntu 22.04 apt Qt 6.2)
# keep the stub from HDR_WindowStub.h and skip compiling the RHI translation units.
!versionAtLeast(QT_VERSION, 6.4.0) {
    SOURCES -= \
        src/video/hdr/HDR_RhiVideoWindow.cpp \
        src/video/hdr/HDR_RhiVideoWindow_Overlays.cpp
    HEADERS -= src/video/hdr/HDR_RhiVideoWindow.h
    message("YUViewLib: Qt < 6.4 — HDR RHI window stubbed (no QRhi build)")
}

# AVX2 kernel TU must not be compiled with the portable baseline flags (MSVC
# rejects _mm256_* without /arch:AVX2). Strip it from SOURCES and rebuild via
# QMAKE_EXTRA_COMPILERS with an ISA-specific command line on x86 only.
SOURCES ~= s/.*HDRYUVRepack_avx2\\.cpp//g
SOURCES -= src/video/hdr/HDRYUVRepack_avx2.cpp
SOURCES -= $$PWD/src/video/hdr/HDRYUVRepack_avx2.cpp

contains(QT_ARCH, x86_64)|contains(QT_ARCH, x86_32)|contains(QT_ARCH, i386) {
    DEFINES += YUVIEW_HAS_AVX2_KERNELS=1
    HDR_AVX2_SOURCES = $$PWD/src/video/hdr/HDRYUVRepack_avx2.cpp

    hdr_avx2_compiler.name = hdr_avx2 ${QMAKE_FILE_IN}
    hdr_avx2_compiler.input = HDR_AVX2_SOURCES
    hdr_avx2_compiler.dependency_type = TYPE_C
    hdr_avx2_compiler.variable_out = OBJECTS
    hdr_avx2_compiler.output = src/video/hdr/${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
    win32-msvc*|win32-clang-msvc {
        hdr_avx2_compiler.commands = \
            $${QMAKE_CXX} -c $(CXXFLAGS) $(INCPATH) -arch:AVX2 \
            -Fo${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
    } else {
        hdr_avx2_compiler.commands = \
            $${QMAKE_CXX} -c $(CXXFLAGS) $(INCPATH) -mavx2 -mfma \
            -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
    }
    QMAKE_EXTRA_COMPILERS += hdr_avx2_compiler
    message("YUViewLib: AVX2 hot-path kernels enabled (runtime CPUID dispatch)")
} else {
    DEFINES += YUVIEW_HAS_AVX2_KERNELS=0
}

# HDR files are automatically included by the recursive $$files() above

FORMS += $$files(ui/*.ui, false)

INCLUDEPATH += src/

RESOURCES += \
    images/images.qrc \
    docs/docs.qrc \
    resources/shaders/shaders.qrc

# Bake GLSL -> .qsb during the build when qsb is available.
# Many kits ship without Qt Shader Tools; in that case keep using pre-baked
# *.qsb under resources/shaders/ (CI still runs compile_shaders.sh/.bat).
greaterThan(QT_MAJOR_VERSION, 5) {
    QSB_BIN =
    !isEmpty($$(QSB)):exists($$(QSB)) {
        QSB_BIN = $$shell_path($$(QSB))
    }

    win32 {
        isEmpty(QSB_BIN):exists($$[QT_HOST_BINS]/qsb.exe) {
            QSB_BIN = $$shell_path($$[QT_HOST_BINS]/qsb.exe)
        }
        isEmpty(QSB_BIN):exists($$[QT_INSTALL_BINS]/qsb.exe) {
            QSB_BIN = $$shell_path($$[QT_INSTALL_BINS]/qsb.exe)
        }
        # Qt Creator bundles qsb even when the selected kit has no shadertools module.
        isEmpty(QSB_BIN):exists($$[QT_HOST_PREFIX]/../../Tools/QtCreator/bin/qsb.exe) {
            QSB_BIN = $$shell_path($$[QT_HOST_PREFIX]/../../Tools/QtCreator/bin/qsb.exe)
        }
        isEmpty(QSB_BIN):exists(C:/Qt/Tools/QtCreator/bin/qsb.exe) {
            QSB_BIN = $$shell_path(C:/Qt/Tools/QtCreator/bin/qsb.exe)
        }
    } else {
        isEmpty(QSB_BIN):exists($$[QT_HOST_BINS]/qsb) {
            QSB_BIN = $$shell_path($$[QT_HOST_BINS]/qsb)
        }
        isEmpty(QSB_BIN):exists($$[QT_INSTALL_BINS]/qsb) {
            QSB_BIN = $$shell_path($$[QT_INSTALL_BINS]/qsb)
        }
    }

    HDR_SHADER_SOURCES = \
        $$files($$PWD/resources/shaders/*.vert, false) \
        $$files($$PWD/resources/shaders/*.frag, false)

    !isEmpty(QSB_BIN) {
        shader_compiler.name = qsb ${QMAKE_FILE_IN}
        shader_compiler.input = HDR_SHADER_SOURCES
        shader_compiler.output = ${QMAKE_FILE_IN}.qsb
        shader_compiler.commands = $$QSB_BIN --glsl \"440,310 es\" --hlsl 50 --msl 12 -o \"${QMAKE_FILE_OUT}\" \"${QMAKE_FILE_IN}\"
        shader_compiler.CONFIG += no_link target_predeps
        QMAKE_EXTRA_COMPILERS += shader_compiler
        message("YUViewLib: baking HDR shaders with $$QSB_BIN")
    } else {
        missing_qsb =
        for(src, HDR_SHADER_SOURCES) {
            !exists($${src}.qsb): missing_qsb += $${src}.qsb
        }
        isEmpty(missing_qsb) {
            message("YUViewLib: qsb not found; using pre-baked *.qsb under resources/shaders/")
        } else {
            error("YUViewLib: qsb not found and missing $$missing_qsb. Install Qt Shader Tools, set QSB=.../qsb(.exe), or run resources/shaders/compile_shaders.")
        }
    }
}

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
    LIBS += -ldxgi -luser32 -lole32
    # Note: WinRT support (-lwindowsapp) temporarily disabled for MinGW compatibility
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
