#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Build Script for Minecraft PE 0.6.1 (Surface RT / Windows RT - ARM32 D3D11)
# ==============================================================================

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/src"
DATA="$ROOT/data"
GLPORT="$ROOT/d3d11_port"
LIBSARM="$ROOT/libs/arm32"
BUILD_DIR="$ROOT/build"
OBJ_DIR="$BUILD_DIR/obj-arm32"
DIST_DIR="$ROOT/dist"
LIST="$ROOT/compile_list.txt"

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
BUILD_TYPE="release"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --debug)
            BUILD_TYPE="debug"
            ;;
        --release|--quiet)
            BUILD_TYPE="release"
            ;;
        -j*)
            JOBS="${arg#-j}"
            ;;
        *)
            echo "Usage: $0 [--debug|--release] [-jN]"
            exit 1
            ;;
    esac
done

# Find LLVM-MinGW Toolchain
find_toolchain() {
    if [[ -n "${LLVM_MINGW_ROOT:-}" && -x "$LLVM_MINGW_ROOT/bin/armv7-w64-mingw32-clang++" ]]; then
        echo "$LLVM_MINGW_ROOT"
        return 0
    fi

    if command -v armv7-w64-mingw32-clang++ >/dev/null 2>&1; then
        dirname "$(dirname "$(command -v armv7-w64-mingw32-clang++)")"
        return 0
    fi

    local candidate_dirs=(
        "/opt/llvm-mingw"
        "/usr/local/llvm-mingw"
        "$HOME/llvm-mingw"
        "$ROOT/toolchain"
    )

    for dir in "${candidate_dirs[@]}"; do
        if [[ -x "$dir/bin/armv7-w64-mingw32-clang++" ]]; then
            echo "$dir"
            return 0
        fi
    done

    return 1
}

TOOLCHAIN="$(find_toolchain || true)"
if [[ -z "$TOOLCHAIN" || ! -x "$TOOLCHAIN/bin/armv7-w64-mingw32-clang++" ]]; then
    echo "ERROR: llvm-mingw toolchain with armv7-w64-mingw32-clang++ not found." >&2
    echo "Please set LLVM_MINGW_ROOT environment variable or add it to PATH." >&2
    exit 1
fi

CXX="$TOOLCHAIN/bin/armv7-w64-mingw32-clang++"
echo "==> Using toolchain: $TOOLCHAIN"
echo "==> Target: ARMv7 (Windows RT / Surface RT)"
echo "==> Configuration: $BUILD_TYPE (Jobs: $JOBS)"

# Verify required files
for req in "$SRC" "$DATA" "$GLPORT/d3d11_gl.cpp" "$LIBSARM/libpng.a" "$LIBSARM/libzlib.a" "$LIST"; do
    if [[ ! -e "$req" ]]; then
        echo "ERROR: Missing required source file or directory: $req" >&2
        exit 1
    fi
done

mkdir -p "$OBJ_DIR" "$DIST_DIR"

if [[ "$BUILD_TYPE" == "release" ]]; then
    SHIM_OBJ="$OBJ_DIR/d3d11_gl_quiet.o"
    OUT_EXE="$BUILD_DIR/MinecraftWinARM32_D3D11.exe"
    QUIET_FLAGS=(-DMCPE_D3D11_NO_LOG)
else
    SHIM_OBJ="$OBJ_DIR/d3d11_gl_debug.o"
    OUT_EXE="$BUILD_DIR/MinecraftWinARM32_D3D11_debug.exe"
    QUIET_FLAGS=()
fi

COMMON_FLAGS=(-w -Wno-c++11-narrowing -O2 -std=gnu++11 -DWIN32 -DNO_EGL -DGLEW_STATIC -Dd3d11_port)
INCLUDE_FLAGS=(-I "$GLPORT/include" -I "$LIBSARM/include" -I "$SRC")

echo "==> [1/3] Compiling D3D11 shim..."
"$CXX" -c "${COMMON_FLAGS[@]}" "${QUIET_FLAGS[@]}" "${INCLUDE_FLAGS[@]}" "$GLPORT/d3d11_gl.cpp" -o "$SHIM_OBJ"

echo "==> [2/3] Compiling $(wc -l < "$LIST") game sources..."
emit_compile_jobs() {
    local src_file obj_file
    while IFS=$'\t' read -r src_file obj_file || [[ -n "$src_file" ]]; do
        src_file="${src_file%$'\r'}"
        obj_file="${obj_file%$'\r'}"
        [[ -n "$src_file" ]] || continue
        printf '%s\0%s\0' "$ROOT/$src_file" "$obj_file"
    done < "$LIST"
}

export CXX SRC GLPORT LIBSARM OBJ_DIR
emit_compile_jobs | xargs -0 -r -n 2 -P "$JOBS" bash -c '
    set -euo pipefail
    "$CXX" -c -w -Wno-c++11-narrowing -O2 -std=gnu++11 \
        -DWIN32 -DNO_EGL -DGLEW_STATIC -Dd3d11_port \
        -I "$GLPORT/include" -I "$LIBSARM/include" -I "$SRC" \
        "$1" -o "$OBJ_DIR/$2"
' _

echo "==> [3/3] Linking executable..."
OBJECTS=()
while IFS=$'\t' read -r src_file obj_file || [[ -n "${src_file:-}" ]]; do
    obj_file="${obj_file%$'\r'}"
    [[ -n "${obj_file:-}" ]] && OBJECTS+=("$OBJ_DIR/$obj_file")
done < "$LIST"

"$CXX" -mwindows -static -s -o "$OUT_EXE" "${OBJECTS[@]}" "$SHIM_OBJ" \
    -L "$LIBSARM" -lpng -lzlib -ld3d11 -ldxgi -luser32 -lgdi32 \
    -lws2_32 -lshell32 -ladvapi32 -lwinmm -lole32 -loleaut32 \
    -luuid -limm32 -lcomdlg32

# Prepare dist bundle
echo "==> Preparing distribution package in $DIST_DIR..."
cp -f "$OUT_EXE" "$DIST_DIR/"
if [[ ! -d "$DIST_DIR/data" ]]; then
    cp -a "$DATA" "$DIST_DIR/"
fi

echo "==> Build successful!"
echo "    Executable: $OUT_EXE"
echo "    Package:    $DIST_DIR"
