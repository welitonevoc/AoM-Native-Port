#!/system/bin/sh
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Detect PREFIX dynamically. A prefix is only usable if it actually contains a
# working clang++; stale inherited values (e.g. from an IDE shell) must not win.
prefix_is_usable() {
    [ -n "${1:-}" ] && [ -x "$1/bin/clang++" ]
}

if ! prefix_is_usable "${PREFIX:-}"; then
    PREFIX=""
    for candidate in \
        "/data/data/com.termux/files/usr" \
        "/data/user/0/com.agy/files/usr" \
        "/usr"
    do
        if prefix_is_usable "$candidate"; then
            PREFIX="$candidate"
            break
        fi
    done
fi

if ! prefix_is_usable "${PREFIX:-}"; then
    echo "ERROR: no usable toolchain prefix found (need \$PREFIX/bin/clang++)." >&2
    exit 127
fi
export PREFIX

# Prefer the selected native toolchain even when this script is launched from
# an IDE shell with a different PATH.
export PATH="$PREFIX/bin:$PATH"

# Detect valid POSIX shell for Make
if [ -n "${PREFIX:-}" ] && [ -x "$PREFIX/bin/sh" ]; then
    SHELL_BIN="$PREFIX/bin/sh"
elif [ -x "/system/bin/sh" ]; then
    SHELL_BIN="/system/bin/sh"
else
    SHELL_BIN="/bin/sh"
fi

if [ -n "${PREFIX:-}" ] && [ -d "$PREFIX/lib" ]; then
    export LD_LIBRARY_PATH="$PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

export TMPDIR="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
if [ ! -d "$TMPDIR" ] || [ ! -w "$TMPDIR" ]; then
    TMPDIR="$ROOT/build-m2/tmp"
fi
mkdir -p "$TMPDIR"
export TMPDIR

CMAKE_BIN="$(command -v cmake 2>/dev/null || true)"
if [ -z "$CMAKE_BIN" ] && [ -n "${PREFIX:-}" ] && [ -x "$PREFIX/bin/cmake" ]; then
    CMAKE_BIN="$PREFIX/bin/cmake"
fi
MAKE_BIN="$(command -v make 2>/dev/null || true)"
if [ -z "$MAKE_BIN" ] && [ -n "${PREFIX:-}" ] && [ -x "$PREFIX/bin/make" ]; then
    MAKE_BIN="$PREFIX/bin/make"
fi

[ -n "$CMAKE_BIN" ] && [ -x "$CMAKE_BIN" ] || {
    echo "ERROR: cmake not found. Install it or set PREFIX to the toolchain prefix." >&2
    exit 127
}
[ -n "$MAKE_BIN" ] && [ -x "$MAKE_BIN" ] || {
    echo "ERROR: make not found. Install it or set PREFIX to the toolchain prefix." >&2
    exit 127
}

# Drop caches that point at a toolchain prefix which no longer exists. This
# happens when the build tree is moved between environments/apps.
CACHE="$ROOT/build-m2/CMakeCache.txt"
if [ -f "$CACHE" ]; then
    CACHED_CXX="$(sed -n 's/^AOM_NATIVE_CLANGXX:FILEPATH=//p' "$CACHE" | head -n 1)"
    CACHED_MAKE="$(sed -n 's/^CMAKE_MAKE_PROGRAM:FILEPATH=//p' "$CACHE" | head -n 1)"
    if { [ -n "$CACHED_CXX" ] && [ ! -x "$CACHED_CXX" ]; } || \
       { [ -n "$CACHED_MAKE" ] && [ ! -x "$CACHED_MAKE" ]; }; then
        echo "Removing stale CMake cache (missing toolchain: ${CACHED_CXX:-$CACHED_MAKE})"
        rm -rf "$ROOT/build-m2/CMakeFiles" "$CACHE"
    fi
fi

"$CMAKE_BIN" -S "$ROOT" -B "$ROOT/build-m2" \
    -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/android-native.toolchain.cmake" \
    -DCMAKE_C_COMPILER="$PREFIX/bin/clang" \
    -DCMAKE_CXX_COMPILER="$PREFIX/bin/clang++" \
    -DCMAKE_MAKE_PROGRAM="$MAKE_BIN" \
    -DCMAKE_SHELL="$SHELL_BIN" \
    -DCMAKE_BUILD_TYPE=Release

"$MAKE_BIN" -C "$ROOT/build-m2" SHELL="$SHELL_BIN" -j"$(nproc 2>/dev/null || echo 4)"

# Record the exact source revision represented by this native library. This is
# consumed by package_apk.sh to prevent packaging a stale build artifact.
printf 'built_at=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$ROOT/build-m2/.aom-build-stamp"
