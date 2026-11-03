# android-native.toolchain.cmake
# Custom toolchain for on-device (ARM64) compilation on Android.
# Uses the native aarch64 clang from pkg — no cross-compilation needed.

# Do not set ANDROID/CMAKE_SYSTEM_NAME to Android: that makes CMake look for
# a cross-compilation NDK layout. The native clang already targets Android.

# Native clang compiler (aarch64, installed via pkg install clang)
find_program(AOM_NATIVE_CLANG clang REQUIRED)
find_program(AOM_NATIVE_CLANGXX clang++ REQUIRED)
set(CMAKE_C_COMPILER "${AOM_NATIVE_CLANG}")
set(CMAKE_CXX_COMPILER "${AOM_NATIVE_CLANGXX}")

# Do NOT set CMAKE_SYSTEM_NAME to Android — that triggers CMake's
# built-in NDK detection which fails without a proper NDK.
# We are compiling natively, so the host IS the target.

# Do NOT set CMAKE_SYSROOT — the native clang already knows its sysroot.
# Do NOT set target triple — native clang already targets aarch64-android.

# Search paths for Android system libraries (EGL, GLESv3, log, etc.)
list(APPEND CMAKE_FIND_ROOT_PATH
    /system
    ENV PREFIX
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
