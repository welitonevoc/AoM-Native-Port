import subprocess, os

NDK = r"C:\Users\Jose Menezes\AppData\Local\Android\Sdk\ndk\28.2.13676358"
CLANG = os.path.join(NDK, "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin", "clang++.exe")
ROOT = r"E:\Port\AoM-Native-Port"
INC = ["-I" + os.path.join(ROOT, "include"),
       "-I" + os.path.join(ROOT, "third_party"),
       "-I" + os.path.join(ROOT, "third_party", "glm"),
       "-I" + os.path.join(ROOT, "third_party", "ndk-include")]
TARGET = ["--target=aarch64-none-linux-android24", "-std=c++20", "-O1"]
SYSROOT = os.path.join(NDK, "toolchains", "llvm", "prebuilt", "windows-x86_64", "sysroot")

def compile(name, srcs, extra_srcs=None):
    extra_srcs = extra_srcs or []
    all_srcs = [os.path.join(ROOT, s) for s in srcs] + [os.path.join(ROOT, s) for s in extra_srcs]
    out = os.path.join(ROOT, "build-win", "tests", name)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    args = [CLANG, "--target=aarch64-none-linux-android24", "-std=c++20", "-O1",
            f"--sysroot={SYSROOT}", "-fPIC"] + INC + all_srcs + ["-o", out, "-lc++", "-lm", "-ldl", "-lz", "-llog"]
    print("COMPILING", name)
    r = subprocess.run(args, capture_output=True, text=True)
    if r.stdout: print(r.stdout)
    if r.stderr: print(r.stderr)
    print("EXIT", r.returncode, "->", out)
    return r.returncode

# test_ddt
compile("test_ddt", ["tests/test_ddt.cpp"], ["src/assets/bar_archive.cpp", "src/assets/ddt_texture.cpp"])
# test_xmb
compile("test_xmb", ["tests/test_xmb.cpp"], ["src/assets/bar_archive.cpp", "src/assets/xmb_parser.cpp"])
# gameplay_test
compile("gameplay_test", ["src/tests/gameplay_test.cpp"], ["src/assets/xmb_parser.cpp"])
# ui_test
compile("ui_test", ["src/tests/ui_test.cpp"], ["src/ui/ui.cpp", "third_party/stb_image.c"])
print("done")
