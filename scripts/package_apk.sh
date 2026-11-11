#!/system/bin/sh
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# Keep packaging in sync with the same on-device toolchain used by the native
# build. An IDE may expose a different PATH than the Android terminal.
if [ -z "${PREFIX:-}" ] && [ -d "/data/user/0/com.agy/files/usr/bin" ]; then
  export PREFIX="/data/user/0/com.agy/files/usr"
fi
if [ -n "${PREFIX:-}" ] && [ -d "$PREFIX/bin" ]; then
  export PATH="$PREFIX/bin:$PATH"
fi
APK_BUILD_DIR="$PROJECT_DIR/build-apk"
ANDROID_JAR="${ANDROID_JAR:-$PROJECT_DIR/tools/android-sdk/android.jar}"
SO_SRC="${AOM_NATIVE_LIBRARY:-$PROJECT_DIR/build-m2/libaom_native.so}"
LIBCXX_SRC="${LIBCXX_SRC:-$PROJECT_DIR/tools/lib/libc++_shared.so}"
RES_DIR="$PROJECT_DIR/android/app/src/main/res"
ASSETS_DIR="$PROJECT_DIR/assets"
OUTPUT_APK="$PROJECT_DIR/aom-native-debug.apk"

echo "=== Packaging AoM Native APK ==="

for tool in aapt zipalign apksigner keytool; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: $tool not found in PATH" >&2; exit 127; }
done
[ -f "$ANDROID_JAR" ] || { echo "ERROR: android.jar not found: $ANDROID_JAR" >&2; exit 1; }
[ -f "$LIBCXX_SRC" ] || { echo "ERROR: libc++_shared.so not found: $LIBCXX_SRC" >&2; exit 1; }

if [ ! -f "$SO_SRC" ]; then
    echo "Compiling native library first..."
    bash "$PROJECT_DIR/scripts/build_native.sh"
fi

# Never package a stale native library. The old script only checked whether the
# .so existed, which allowed an APK from a previous source revision to be
# generated successfully.
if find "$PROJECT_DIR/src" "$PROJECT_DIR/include" "$PROJECT_DIR/third_party" "$PROJECT_DIR/CMakeLists.txt" \
       "$PROJECT_DIR/cmake" "$PROJECT_DIR/scripts/build_native.sh" \
       -type f -newer "$SO_SRC" -print -quit 2>/dev/null | grep -q .; then
    if [ -n "${AOM_NATIVE_LIBRARY:-}" ]; then
        echo "ERROR: native sources are newer than $SO_SRC; rebuild the selected CMake target before packaging." >&2
        exit 1
    fi
    echo "Native sources are newer than $SO_SRC; rebuilding..."
    bash "$PROJECT_DIR/scripts/build_native.sh"
fi

[ -f "$SO_SRC" ] || { echo "ERROR: native library was not generated: $SO_SRC" >&2; exit 1; }

rm -rf "$APK_BUILD_DIR"
mkdir -p "$APK_BUILD_DIR"/lib/arm64-v8a
cd "$APK_BUILD_DIR"

VERSION_CODE="$(date +%s)"
VERSION_NAME="1.0.$(date +%m%d%H%M)"
echo "Building APK with versionCode: $VERSION_CODE, versionName: $VERSION_NAME"

# 1. AndroidManifest with app icon and storage permissions
cat << MANIFEST > AndroidManifest.xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.aom.nativeport"
    android:versionCode="${VERSION_CODE}"
    android:versionName="${VERSION_NAME}">

    <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="34" />

    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
    <uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
    <uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />

    <application
        android:label="AoM Native"
        android:icon="@mipmap/ic_launcher"
        android:hasCode="true"
        android:extractNativeLibs="true"
        android:requestLegacyExternalStorage="true"
        android:theme="@android:style/Theme.NoTitleBar.Fullscreen">

        <activity
            android:name="com.aom.nativeport.MyNativeActivity"
            android:exported="true"
            android:screenOrientation="landscape"
            android:configChanges="orientation|keyboardHidden|screenSize">

            <meta-data
                android:name="android.app.lib_name"
                android:value="aom_native" />

            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
MANIFEST

# 2. Compile Java sources into classes.dex
echo "Compiling Java sources with javac and d8..."
mkdir -p "$APK_BUILD_DIR/classes"
javac -cp "$ANDROID_JAR" -source 8 -target 8 -d "$APK_BUILD_DIR/classes" \
  "$PROJECT_DIR/android/app/src/main/java/com/aom/nativeport/MyNativeActivity.java"
d8 --min-api 24 --output "$APK_BUILD_DIR" $(find "$APK_BUILD_DIR/classes" -name "*.class")

# 3. Keystore (if not exists)
if [ ! -f "$PROJECT_DIR/debug.keystore" ]; then
    echo "Creating debug keystore..."
    keytool -genkeypair -v \
      -keystore "$PROJECT_DIR/debug.keystore" \
      -storepass android \
      -alias androiddebugkey \
      -keypass android \
      -keyalg RSA \
      -keysize 2048 \
      -validity 10000 \
      -dname "CN=Android Debug,O=Android,C=US"
fi

# 3.5 Ensure original menu and UI assets are extracted
if [ ! -f "$ASSETS_DIR/main_menu.png" ] || [ ! -f "$ASSETS_DIR/button_main.png" ] || [ ! -f "$ASSETS_DIR/loading.png" ]; then
    echo "Extracting original AoM menu and UI assets..."
    python3 "$PROJECT_DIR/scripts/extract_menu_assets.py" || true
fi

# 3.6 Generate app launcher icons for all Android densities from assets/icon.png
echo "Generating launcher icons for all densities..."
bash "$PROJECT_DIR/scripts/generate_app_icons.sh"

# 4. Compile manifest, resources and full assets
# -0 "" ensures font.ttf and textures are stored uncompressed for instant zero-copy loading
aapt package -F app.unaligned.apk \
  -M AndroidManifest.xml \
  -S "$RES_DIR" \
  -A "$ASSETS_DIR" \
  -0 ttf \
  -0 png \
  -0 ddt \
  -0 mp3 \
  -0 wav \
  -I "$ANDROID_JAR" -f

# 5. Add classes.dex and native shared libraries
aapt add app.unaligned.apk classes.dex
cp "$SO_SRC" lib/arm64-v8a/
cp "$LIBCXX_SRC" lib/arm64-v8a/
if command -v patchelf >/dev/null 2>&1; then
    patchelf --remove-rpath lib/arm64-v8a/libaom_native.so 2>/dev/null || true
fi
echo "Native library dependencies:"
readelf -d lib/arm64-v8a/libaom_native.so | grep NEEDED || true
aapt add app.unaligned.apk lib/arm64-v8a/libaom_native.so lib/arm64-v8a/libc++_shared.so

# 6. Zipalign APK entries (page-aligned at 4 bytes)
zipalign -f -p 4 app.unaligned.apk app.aligned.apk

# 6. Apksigner (v2 & v3 signature scheme)
apksigner sign \
  --ks "$PROJECT_DIR/debug.keystore" \
  --ks-pass pass:android \
  --ks-key-alias androiddebugkey \
  --key-pass pass:android \
  --out "$OUTPUT_APK" \
  app.aligned.apk

echo "=== Verifying APK ==="
apksigner verify -v "$OUTPUT_APK"
APK_SIZE="$(ls -lh "$OUTPUT_APK" | awk '{print $5}')"
echo "=== APK gerado com sucesso ==="
echo "Arquivo: $OUTPUT_APK"
echo "Tamanho: $APK_SIZE"
echo "Versao:  versionCode=$VERSION_CODE, versionName=$VERSION_NAME"
