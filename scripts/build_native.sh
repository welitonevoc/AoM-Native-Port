#!/system/bin/sh
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Detect PREFIX dynamically
if [ -z "${PREFIX:-}" ]; then
    if [ -d "/data/user/0/com.agy/files/usr/bin" ]; then
        PREFIX="/data/user/0/com.agy/files/usr"
    elif [ -d "/data/data/com.termux/files/usr/bin" ]; then
        PREFIX="/data/data/com.termux/files/usr"
    elif [ -d "/usr" ]; then
        PREFIX="/usr"
    fi
fi

# Prefer the selected native toolchain even when this script is launched from
# an IDE shell with a different PATH.
if [ -n "${PREFIX:-}" ] && [ -d "$PREFIX/bin" ]; then
    export PATH="$PREFIX/bin:$PATH"
fi

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

export TMPDIR="${TMPDIR:-/data/local/tmp}"
[ ! -d "$TMPDIR" ] && TMPDIR="$ROOT/build-m2/tmp"
mkdir -p "$TMPDIR"

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

"$CMAKE_BIN" -S "$ROOT" -B "$ROOT/build-m2" \
    -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/android-native.toolchain.cmake" \
    -DCMAKE_SHELL="$SHELL_BIN" \
    -DCMAKE_BUILD_TYPE=Release

"$MAKE_BIN" -C "$ROOT/build-m2" SHELL="$SHELL_BIN" -j"$(nproc 2>/dev/null || echo 4)"

# Record the exact source revision represented by this native library. This is
# consumed by package_apk.sh to prevent packaging a stale build artifact.
printf 'built_at=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$ROOT/build-m2/.aom-build-stamp"
