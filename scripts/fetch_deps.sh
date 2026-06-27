#!/usr/bin/env bash
#
# Vendors the GlossHook inline-hooking library (header + prebuilt ARM64 static
# lib) into third_party/glosshook. bedrocklua links this statically, so the mod
# is self-contained and needs no preloader build.
#
#   third_party/glosshook/include/Gloss.h
#   third_party/glosshook/lib/arm64/libGlossHook.a
#
# Idempotent: re-running with both files present is a no-op. Override the source
# revision with GLOSSHOOK_REF (default: main).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/glosshook"
REF="${GLOSSHOOK_REF:-main}"

INC="$DEST/include/Gloss.h"
LIB="$DEST/lib/arm64/libGlossHook.a"

if [[ -f "$INC" && -f "$LIB" ]]; then
  echo "[fetch_deps] GlossHook already vendored at $DEST"
  exit 0
fi

mkdir -p "$DEST/include" "$DEST/lib/arm64"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "[fetch_deps] downloading XMDS/GlossHook@${REF}..."
curl -fsSL "https://github.com/XMDS/GlossHook/archive/refs/heads/${REF}.tar.gz" \
  -o "$TMP/glosshook.tar.gz"
tar -xzf "$TMP/glosshook.tar.gz" -C "$TMP"

SRC="$(find "$TMP" -maxdepth 1 -type d -name 'GlossHook-*' | head -n1)"
if [[ -z "$SRC" ]]; then
  echo "[fetch_deps] ERROR: could not locate extracted GlossHook source" >&2
  exit 1
fi

# Header (single self-contained header).
HDR="$(find "$SRC" -name 'Gloss.h' | head -n1)"
if [[ -z "$HDR" ]]; then
  echo "[fetch_deps] ERROR: Gloss.h not found in GlossHook source" >&2
  exit 1
fi
cp "$HDR" "$INC"

# Prebuilt ARM64 static lib (XMDS ships GlossHook/lib/ARM64/libGlossHook.a).
PREBUILT="$(find "$SRC" -path '*ARM64*' -name 'libGlossHook.a' | head -n1)"
if [[ -n "$PREBUILT" ]]; then
  cp "$PREBUILT" "$LIB"
  echo "[fetch_deps] vendored prebuilt $PREBUILT"
else
  # Fallback: build from source for arm64 with the NDK toolchain.
  echo "[fetch_deps] prebuilt libGlossHook.a not found; building from source..." >&2
  : "${ANDROID_NDK_HOME:=${ANDROID_NDK_ROOT:-${NDK_PATH:-}}}"
  if [[ -z "${ANDROID_NDK_HOME}" ]]; then
    echo "[fetch_deps] ERROR: ANDROID_NDK_HOME not set and no prebuilt lib available" >&2
    exit 1
  fi
  GLOSS_CMAKE="$(find "$SRC" -name CMakeLists.txt | head -n1)"
  if [[ -z "$GLOSS_CMAKE" ]]; then
    echo "[fetch_deps] ERROR: no prebuilt lib and no CMakeLists.txt to build GlossHook" >&2
    exit 1
  fi
  cmake -S "$(dirname "$GLOSS_CMAKE")" -B "$TMP/build" \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
  cmake --build "$TMP/build" -j"$(nproc)"
  BUILT="$(find "$TMP/build" -name 'libGlossHook.a' | head -n1)"
  if [[ -z "$BUILT" ]]; then
    echo "[fetch_deps] ERROR: GlossHook build produced no libGlossHook.a" >&2
    exit 1
  fi
  cp "$BUILT" "$LIB"
  echo "[fetch_deps] built $BUILT"
fi

echo "[fetch_deps] GlossHook vendored:"
echo "    $INC"
echo "    $LIB"
