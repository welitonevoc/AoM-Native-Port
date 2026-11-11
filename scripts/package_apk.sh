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

# Função: verifica se uma ferramenta está disponível E executável
# Detecta placeholders quebrados (ex: zipalign com "404: Not Found")
# e binários ELF reais (ex: aapt, aapt2) que podem não aceitar --help.
tool_usable() {
    local tool="$1"
    local path
    path="$(command -v "$tool" 2>/dev/null)" || return 1

    # Se o arquivo não existe ou não é executável, não serve
    [ -x "$path" ] || return 1

    # Detecta binário ELF real (magia: \x7fELF) -> sempre usável
    local magic
    magic="$(head -c 4 "$path" 2>/dev/null)"
    if [ "$magic" = $'\x7fELF' ]; then
        return 0
    fi

    # Para scripts, testa execução real (--help ou sem argumentos)
    "$tool" --help >/dev/null 2>&1 || "$tool" >/dev/null 2>&1 || return 1
    return 0
}

# Verifica ferramenta obrigatória: aapt
if ! tool_usable aapt; then
    echo "ERROR: aapt not found or not usable in PATH" >&2
    exit 127
fi
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

# 3. Keystore (if not exists and keytool available)
if tool_usable keytool && [ ! -f "$PROJECT_DIR/debug.keystore" ]; then
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
elif [ ! -f "$PROJECT_DIR/debug.keystore" ]; then
    echo "AVISO: keytool não encontrado ou inutilizável. O APK será gerado sem assinatura (apenas para teste interno)."
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

# Localiza o uber-apk-signer.jar (alternativa Java para zipalign + apksigner)
find_uber_signer() {
    local candidates="
        $PROJECT_DIR/uber-apk-signer.jar
        $PROJECT_DIR/tools/uber-apk-signer.jar
        $HOME/uber-apk-signer.jar
        /data/data/com.termux/files/home/uber-apk-signer.jar
    "
    for c in $candidates; do
        if [ -f "$c" ]; then
            echo "$c"
            return 0
        fi
    done
    return 1
}

UBER_SIGNER="$(find_uber_signer || true)"

# 6. Zipalign APK entries (page-aligned at 4 bytes) — opcional
if tool_usable zipalign; then
    zipalign -f -p 4 app.unaligned.apk app.aligned.apk
    APK_TO_SIGN="app.aligned.apk"
elif [ -n "$UBER_SIGNER" ]; then
    echo "zipalign não disponível; uber-apk-signer fará zipalign + assinatura em um passo."
    APK_TO_SIGN="app.unaligned.apk"
else
    echo "AVISO: zipalign não encontrado ou inutilizável. O APK será gerado sem alinhamento (funciona normalmente)."
    APK_TO_SIGN="app.unaligned.apk"
fi

# 7. Assinatura do APK — apksigner, uber-apk-signer ou sem assinatura
if tool_usable apksigner && [ -f "$PROJECT_DIR/debug.keystore" ]; then
    apksigner sign \
      --ks "$PROJECT_DIR/debug.keystore" \
      --ks-pass pass:android \
      --ks-key-alias androiddebugkey \
      --key-pass pass:android \
      --out "$OUTPUT_APK" \
      "$APK_TO_SIGN"
    echo "=== Verifying APK ==="
    apksigner verify -v "$OUTPUT_APK"
elif [ -n "$UBER_SIGNER" ] && [ -f "$PROJECT_DIR/debug.keystore" ]; then
    echo "Usando uber-apk-signer.jar para zipalign + assinatura..."
    java -jar "$UBER_SIGNER" \
      --apk "$APK_TO_SIGN" \
      --ks "$PROJECT_DIR/debug.keystore" \
      --ks-pass android \
      --ks-key-alias androiddebugkey \
      --key-pass android \
      --out "$OUTPUT_APK"
    echo "=== APK assinado com uber-apk-signer ==="
elif [ -f "$APK_TO_SIGN" ]; then
    echo "AVISO: apksigner/uber-apk-signer não disponível. Copiando APK não assinado para $OUTPUT_APK"
    cp "$APK_TO_SIGN" "$OUTPUT_APK"
else
    echo "ERROR: não foi possível gerar o APK final" >&2
    exit 1
fi
APK_SIZE="$(ls -lh "$OUTPUT_APK" | awk '{print $5}')"
echo "=== APK gerado com sucesso ==="
echo "Arquivo: $OUTPUT_APK"
echo "Tamanho: $APK_SIZE"
echo "Versao:  versionCode=$VERSION_CODE, versionName=$VERSION_NAME"

# 8. Copiar o APK para o sdcard (armazenamento compartilhado do Android)
SDCARD_DIR=""
for cand in "/sdcard" "/storage/emulated/0"; do
    if [ -d "$cand" ] && [ -w "$cand" ]; then
        SDCARD_DIR="$cand"
        break
    fi
done

if [ -n "$SDCARD_DIR" ]; then
    APK_NAME="aom-native-v${VERSION_NAME}.apk"
    cp "$OUTPUT_APK" "$SDCARD_DIR/$APK_NAME"
    echo ""
    echo "=== APK copiado para o sdcard ==="
    echo "Arquivo: $SDCARD_DIR/$APK_NAME"
    echo "Tamanho: $(ls -lh "$SDCARD_DIR/$APK_NAME" | awk '{print $5}')"
else
    echo ""
    echo "AVISO: sdcard não encontrado ou não gravável. O APK permanece apenas em:"
    echo "  $OUTPUT_APK"
fi
