#!/system/bin/sh
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

export PREFIX="/data/data/com.termux/files/usr"
export PATH="$PREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export TMPDIR="/data/data/com.termux/files/usr/tmp"

APK_BUILD_DIR="$PROJECT_DIR/build-apk"
ANDROID_JAR="$PROJECT_DIR/tools/android-sdk/android.jar"
SO_SRC="$PROJECT_DIR/build-m2/libaom_native.so"
LIBCXX_SRC="$PROJECT_DIR/tools/lib/libc++_shared.so"
RES_DIR="$PROJECT_DIR/android/app/src/main/res"
ASSETS_DIR="$PROJECT_DIR/assets"
OUTPUT_APK="$PROJECT_DIR/aom-native-v1.0.11111453.apk"

# Versão fixa seguindo a sequência do APK de referência no sdcard
VERSION_CODE=1794419605
VERSION_NAME="1.0.11111453"

echo "=== Packaging AoM Native APK v$VERSION_NAME (code $VERSION_CODE) ==="

for tool in aapt aapt2 javac d8 keytool apksigner patchelf readelf; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: $tool not found in PATH" >&2; exit 127; }
done
[ -f "$ANDROID_JAR" ] || { echo "ERROR: android.jar not found: $ANDROID_JAR" >&2; exit 1; }
[ -f "$LIBCXX_SRC" ] || { echo "ERROR: libc++_shared.so not found: $LIBCXX_SRC" >&2; exit 1; }
[ -f "$SO_SRC" ] || { echo "ERROR: native library not found: $SO_SRC" >&2; exit 1; }

rm -rf "$APK_BUILD_DIR"
mkdir -p "$APK_BUILD_DIR"/lib/arm64-v8a
cd "$APK_BUILD_DIR"

echo "Building APK with versionCode: $VERSION_CODE, versionName: $VERSION_NAME"

# 1. AndroidManifest com a versão fixa
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

# 2. Compilar Java sources em classes.dex
echo "Compiling Java sources with javac and d8..."
mkdir -p "$APK_BUILD_DIR/classes"
javac -cp "$ANDROID_JAR" -source 8 -target 8 -d "$APK_BUILD_DIR/classes" \
  "$PROJECT_DIR/android/app/src/main/java/com/aom/nativeport/MyNativeActivity.java"
d8 --min-api 24 --output "$APK_BUILD_DIR" $(find "$APK_BUILD_DIR/classes" -name "*.class")

# 3. Keystore
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

# 4. Compilar manifest, resources e assets (sem compressão para ttf/png/ddt/mp3/wav)
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

# 5. Adicionar classes.dex e libs nativas
aapt add app.unaligned.apk classes.dex
cp "$SO_SRC" lib/arm64-v8a/
cp "$LIBCXX_SRC" lib/arm64-v8a/
patchelf --remove-rpath lib/arm64-v8a/libaom_native.so 2>/dev/null || true
echo "Native library dependencies:"
readelf -d lib/arm64-v8a/libaom_native.so | grep NEEDED || true
aapt add app.unaligned.apk lib/arm64-v8a/libaom_native.so lib/arm64-v8a/libc++_shared.so

# 6. zipalign pular - não disponível nesta arquitetura ARM64 Termux.
#    Não é obrigatório para instalação, apenas otimização.
cp app.unaligned.apk app.aligned.apk
echo "AVISO: zipalign pulado (não disponível nesta arquitetura). O APK ainda assim instala."

# 7. Apksigner v1+v2+v3
apksigner sign \
  --ks "$PROJECT_DIR/debug.keystore" \
  --ks-pass pass:android \
  --ks-key-alias androiddebugkey \
  --key-pass pass:android \
  --v1-signing-enabled true \
  --v2-signing-enabled true \
  --v3-signing-enabled true \
  --out "$OUTPUT_APK" \
  app.aligned.apk

echo "=== Verifying APK ==="
apksigner verify -v "$OUTPUT_APK"

echo "=== APK gerado com sucesso ==="
echo "Arquivo: $OUTPUT_APK"
echo "Tamanho: $(ls -lh "$OUTPUT_APK" | awk '{print $5}')"
echo "Versão: versionCode=$VERSION_CODE, versionName=$VERSION_NAME"
