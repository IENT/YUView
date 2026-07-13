#!/usr/bin/env bash
set -euo pipefail

ASAN=false
BUILD_DIR="build"
if [ "${1:-}" = "--asan" ]; then
    ASAN=true
    BUILD_DIR="build-asan"
fi

QMAKE=$(command -v qmake6 2>/dev/null || command -v qmake 2>/dev/null || { echo "qmake not found"; exit 1; })

if [[ "$(uname)" == "Darwin" ]]; then
    JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
else
    JOBS=$(nproc 2>/dev/null || echo 4)
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$ASAN" = true ]; then
    rm -f Makefile
    "$QMAKE" CONFIG-=UNITTESTS \
        QMAKE_CXXFLAGS+="-fsanitize=address -fno-omit-frame-pointer" \
        QMAKE_LFLAGS+="-fsanitize=address" \
        ..
else
    if [ ! -f Makefile ]; then
        "$QMAKE" CONFIG+=UNITTESTS ..
    fi
fi

make -j"$JOBS"
