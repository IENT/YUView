#!/usr/bin/env bash
# Compile GLSL shaders to QSB format for QRhi.
# Usage:
#   ./compile_shaders.sh
#   QSB=/path/to/qsb ./compile_shaders.sh
#
# Requires: qsb from Qt 6 (shadertools). Prefer QSB env, then PATH, then common Qt paths.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

resolve_qsb() {
  if [[ -n "${QSB:-}" && -x "${QSB}" ]]; then
    echo "${QSB}"
    return 0
  fi
  if command -v qsb >/dev/null 2>&1; then
    command -v qsb
    return 0
  fi
  if command -v qsb6 >/dev/null 2>&1; then
    command -v qsb6
    return 0
  fi

  local candidate
  for candidate in \
    /usr/lib/qt6/bin/qsb \
    /usr/lib/x86_64-linux-gnu/qt6/bin/qsb \
    /opt/homebrew/opt/qt/bin/qsb \
    /usr/local/opt/qt/bin/qsb
  do
    if [[ -x "${candidate}" ]]; then
      echo "${candidate}"
      return 0
    fi
  done

  return 1
}

if ! QSB_BIN="$(resolve_qsb)"; then
  echo "Error: qsb tool not found."
  echo "Install Qt Shader Tools, or set QSB=/path/to/qsb, or add Qt bin to PATH."
  exit 1
fi

echo "Using qsb: ${QSB_BIN}"
echo "Compiling HDR RHI shaders in ${SCRIPT_DIR}..."

compile_one() {
  local src="$1"
  local out="${src}.qsb"
  "${QSB_BIN}" --glsl "440,310 es" --hlsl 50 --msl 12 -o "${out}" "${src}"
  echo "  ${src} -> ${out}"
}

compile_one hdr_rhi_vertex.vert
compile_one hdr_rhi_fragment.frag
compile_one hdr_rhi_yuv_vertex.vert
compile_one hdr_rhi_yuv_fragment.frag

echo "Done! All shaders compiled successfully."
