import os
import shutil
import subprocess


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build-apk")
SDK_ROOT = os.environ.get(
    "ANDROID_SDK_ROOT",
    os.environ.get(
        "ANDROID_HOME",
        os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk"),
    ),
)
BUILD_TOOLS_VERSION = os.environ.get("ANDROID_BUILD_TOOLS_VERSION", "36.1.0")
BUILD_TOOLS = os.environ.get(
    "ANDROID_BUILD_TOOLS",
    os.path.join(SDK_ROOT, "build-tools", BUILD_TOOLS_VERSION),
)
ANDROID_JAR = os.environ.get(
    "ANDROID_JAR", os.path.join(ROOT, "tools", "android-sdk", "android.jar")
)
ANDROID_MANIFEST = os.path.join(
    ROOT, "android", "app", "src", "main", "AndroidManifest.xml"
)
JAVA_SOURCE = os.path.join(
    ROOT, "android", "app", "src", "main", "java", "com", "aom", "nativeport",
    "MyNativeActivity.java",
)
RES_DIR = os.path.join(ROOT, "android", "app", "src", "main", "res")
ASSETS_DIR = os.path.join(ROOT, "assets")
SO_SRC = os.environ.get(
    "AOM_NATIVE_LIB", os.path.join(ROOT, "build-m2", "libaom_native.so")
)
LIBCXX_SRC = os.environ.get(
    "LIBCXX_SRC", os.path.join(ROOT, "tools", "lib", "libc++_shared.so")
)
VIDEO_DIR = os.environ.get("AOM_VIDEO_DIR", os.path.join(ROOT, "build", "cinematics"))
OUTPUT_APK = os.path.join(ROOT, "aom-native-debug.apk")
KEYSTORE = os.path.join(ROOT, "debug.keystore")
VIDEO_NAMES = (
    "aom-logo.mp4",
    "introcg.mp4",
    "xintrocg.mp4",
    "outtakes.mp4",
    "credits.mp4",
    "xcredits.mp4",
)


def command_for(tool, *args):
    command = [tool, *args]
    if os.name == "nt" and tool.lower().endswith((".bat", ".cmd")):
        return [os.environ.get("COMSPEC", "cmd.exe"), "/c", *command]
    return command


def run(args, cwd=None):
    print("RUN:", " ".join(args))
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)
    if result.returncode != 0:
        raise SystemExit(f"FAILED (exit {result.returncode}): {' '.join(args)}")
    return result


def resolve_tool(environment_name, sdk_name, executable_name):
    override = os.environ.get(environment_name)
    if override:
        return override
    sdk_candidate = os.path.join(BUILD_TOOLS, sdk_name)
    if os.path.isfile(sdk_candidate):
        return sdk_candidate
    return shutil.which(executable_name) or sdk_candidate


def main():
    aapt = resolve_tool("AAPT", "aapt.exe", "aapt")
    zipalign = resolve_tool("ZIPALIGN", "zipalign.exe", "zipalign")
    apksigner = resolve_tool("APKSIGNER", "apksigner.bat", "apksigner")
    d8 = resolve_tool("D8", "d8.bat", "d8")
    javac = os.environ.get("JAVAC") or shutil.which("javac")
    keytool = os.environ.get("KEYTOOL") or shutil.which("keytool")

    required = {
        "aapt": aapt,
        "zipalign": zipalign,
        "apksigner": apksigner,
        "d8": d8,
        "javac": javac,
        "keytool": keytool,
        "android.jar": ANDROID_JAR,
        "manifest": ANDROID_MANIFEST,
        "Java activity": JAVA_SOURCE,
        "native library": SO_SRC,
        "libc++_shared.so": LIBCXX_SRC,
    }
    for label, path in required.items():
        if not path or (label not in ("aapt", "zipalign", "apksigner", "d8", "javac", "keytool") and not os.path.isfile(path)):
            raise SystemExit(f"Required packaging input missing: {label}: {path}")
        if label in ("aapt", "zipalign", "apksigner", "d8", "javac", "keytool") and not (
            os.path.isfile(path) or shutil.which(path)
        ):
            raise SystemExit(f"Required Android build tool missing: {label}: {path}")

    os.makedirs(os.path.join(BUILD, "lib", "arm64-v8a"), exist_ok=True)
    classes_dir = os.path.join(BUILD, "java-classes")
    dex_dir = os.path.join(BUILD, "java-dex")
    for generated_dir in (classes_dir, dex_dir):
        if os.path.isdir(generated_dir):
            shutil.rmtree(generated_dir)
        os.makedirs(generated_dir)

    run(command_for(
        javac,
        "-source", "8",
        "-target", "8",
        "-classpath", ANDROID_JAR,
        "-d", classes_dir,
        JAVA_SOURCE,
    ))
    run(command_for(
        d8,
        "--lib", ANDROID_JAR,
        "--min-api", "24",
        "--output", dex_dir,
        os.path.join(classes_dir, "com", "aom", "nativeport", "MyNativeActivity.class"),
    ))
    classes_dex = os.path.join(dex_dir, "classes.dex")
    if not os.path.isfile(classes_dex):
        raise SystemExit(f"D8 did not produce classes.dex: {classes_dex}")

    os.chdir(BUILD)
    run(command_for(
        aapt,
        "package", "-F", "app.unaligned.apk",
        "-M", ANDROID_MANIFEST,
        "-S", RES_DIR,
        "-A", ASSETS_DIR,
        "-I", ANDROID_JAR,
        "-f",
    ))

    shutil.copy(SO_SRC, os.path.join(BUILD, "lib", "arm64-v8a", "libaom_native.so"))
    shutil.copy(LIBCXX_SRC, os.path.join(BUILD, "lib", "arm64-v8a", "libc++_shared.so"))
    shutil.copy(classes_dex, os.path.join(BUILD, "classes.dex"))

    optional_assets_dir = os.path.join(BUILD, "assets", "avi")
    os.makedirs(optional_assets_dir, exist_ok=True)
    video_entries = []
    for video_name in VIDEO_NAMES:
        source = os.path.join(VIDEO_DIR, video_name)
        staged = os.path.join(optional_assets_dir, video_name)
        if os.path.isfile(source) and os.path.getsize(source) > 0:
            shutil.copyfile(source, staged)
            video_entries.append(f"assets/avi/{video_name}")
        else:
            if os.path.isfile(staged):
                os.remove(staged)
            print(f"Optional converted video not found; splash fallback remains: {source}")

    entries = [
        "classes.dex",
        "lib/arm64-v8a/libaom_native.so",
        "lib/arm64-v8a/libc++_shared.so",
        *video_entries,
    ]
    run(command_for(aapt, "add", "app.unaligned.apk", *entries), cwd=BUILD)

    if not os.path.isfile(KEYSTORE):
        java_home = os.environ.get("JAVA_HOME")
        java_bin = os.path.join(java_home, "bin", "keytool") if java_home else keytool
        run(command_for(
            java_bin,
            "-genkeypair", "-v",
            "-keystore", KEYSTORE,
            "-storepass", "android",
            "-alias", "androiddebugkey",
            "-keypass", "android",
            "-keyalg", "RSA",
            "-keysize", "2048",
            "-validity", "10000",
            "-dname", "CN=Android Debug,O=Android,C=US",
        ))

    run(command_for(
        zipalign,
        "-f", "-p", "4", "app.unaligned.apk", "app.aligned.apk",
    ))
    run(command_for(
        apksigner,
        "sign",
        "--ks", KEYSTORE,
        "--ks-pass", "pass:android",
        "--ks-key-alias", "androiddebugkey",
        "--key-pass", "pass:android",
        "--out", OUTPUT_APK,
        "app.aligned.apk",
    ))
    run(command_for(apksigner, "verify", "-v", OUTPUT_APK))
    print("APK gerado:", OUTPUT_APK)


if __name__ == "__main__":
    main()
