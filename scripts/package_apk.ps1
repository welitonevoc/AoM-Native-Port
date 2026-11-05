# scripts/package_apk.ps1
# Automates APK packaging, signing, and installation for AoM Native Port on Windows PowerShell.
param(
    [switch]$Install,
    [switch]$Launch
)

$ErrorActionPreference = "Stop"

$ProjectDir = (Resolve-Path "$PSScriptRoot\..").Path
$ApkBuildDir = "$ProjectDir\build-apk"
$ResDir = "$ProjectDir\android\app\src\main\res"
$AssetsDir = "$ProjectDir\assets"
$OutputApk = "$ProjectDir\aom-native-debug.apk"
$Keystore = "$ProjectDir\debug.keystore"

# Resolve Android SDK paths
$AndroidHome = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$BuildToolsDir = Get-ChildItem "$AndroidHome\build-tools" | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$PlatformDir = Get-ChildItem "$AndroidHome\platforms" | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$AndroidJar = "$PlatformDir\android.jar"
$NdkDir = if ($env:ANDROID_NDK_HOME) { $env:ANDROID_NDK_HOME } else { Get-ChildItem "$AndroidHome\ndk" | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName }

$Aapt = "$BuildToolsDir\aapt.exe"
$Zipalign = "$BuildToolsDir\zipalign.exe"
$Apksigner = "$BuildToolsDir\apksigner.bat"
$D8 = "$BuildToolsDir\d8.bat"
$D8Jar = "$BuildToolsDir\lib\d8.jar"
$LlvmStrip = "$NdkDir\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-strip.exe"

# Java / Keytool
$JavaBin = if ($env:JAVA_HOME) { "$env:JAVA_HOME\bin" } else { "C:\Program Files\Android\Android Studio\jbr\bin" }
$JavaExe = "$JavaBin\java.exe"
$Javac = "$JavaBin\javac.exe"
$Keytool = "$JavaBin\keytool.exe"

# Adb
$Adb = "$AndroidHome\platform-tools\adb.exe"

Write-Host "=== AoM Native Port - Packaging APK ===" -ForegroundColor Cyan
Write-Host "Build Tools: $BuildToolsDir"
Write-Host "Android Jar: $AndroidJar"
Write-Host "Java Home:   $JavaBin"

# Verify tool existence
foreach ($tool in @($Aapt, $Zipalign, $Apksigner, $D8, $Javac, $Keytool)) {
    if (-not (Test-Path $tool)) {
        throw "Ferramenta obrigatoria nao encontrada: $tool"
    }
}

# Clean and recreate build-apk
if (Test-Path $ApkBuildDir) {
    Remove-Item -Recurse -Force $ApkBuildDir
}
New-Item -ItemType Directory -Path "$ApkBuildDir\classes" | Out-Null
New-Item -ItemType Directory -Path "$ApkBuildDir\lib\arm64-v8a" | Out-Null
New-Item -ItemType Directory -Path "$ApkBuildDir\lib\x86_64" | Out-Null

# 1. Native shared libraries
$Arm64So = if (Test-Path "$ProjectDir\build-arm64\libaom_native.so") { "$ProjectDir\build-arm64\libaom_native.so" } else { "$ProjectDir\build-m2\libaom_native.so" }
$EmuSo = "$ProjectDir\build-emu\libaom_native.so"

$Arm64Libcxx = "$NdkDir\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\libc++_shared.so"
$X86_64Libcxx = "$NdkDir\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\x86_64-linux-android\libc++_shared.so"

if (Test-Path $Arm64So) {
    Copy-Item $Arm64So "$ApkBuildDir\lib\arm64-v8a\libaom_native.so"
    if (Test-Path $LlvmStrip) {
        & $LlvmStrip --strip-all "$ApkBuildDir\lib\arm64-v8a\libaom_native.so"
    }
    if (Test-Path $Arm64Libcxx) {
        Copy-Item $Arm64Libcxx "$ApkBuildDir\lib\arm64-v8a\libc++_shared.so"
        if (Test-Path $LlvmStrip) {
            & $LlvmStrip --strip-all "$ApkBuildDir\lib\arm64-v8a\libc++_shared.so"
        }
    }
    Write-Host "Added ABI: arm64-v8a (stripped)" -ForegroundColor Green
}

if (Test-Path $EmuSo) {
    Copy-Item $EmuSo "$ApkBuildDir\lib\x86_64\libaom_native.so"
    if (Test-Path $LlvmStrip) {
        & $LlvmStrip --strip-all "$ApkBuildDir\lib\x86_64\libaom_native.so"
    }
    if (Test-Path $X86_64Libcxx) {
        Copy-Item $X86_64Libcxx "$ApkBuildDir\lib\x86_64\libc++_shared.so"
        if (Test-Path $LlvmStrip) {
            & $LlvmStrip --strip-all "$ApkBuildDir\lib\x86_64\libc++_shared.so"
        }
    }
    Write-Host "Added ABI: x86_64 (stripped)" -ForegroundColor Green
}

# 2. Versioning and AndroidManifest.xml
$VersionCode = [int][double]::Parse((Get-Date -UFormat %s))
$VersionName = "1.0." + (Get-Date -Format "MMddHHmm")
Write-Host "VersionCode: $VersionCode, VersionName: $VersionName"

$ManifestContent = @"
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.aom.nativeport"
    android:versionCode="$VersionCode"
    android:versionName="$VersionName">

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
"@
Set-Content -Path "$ApkBuildDir\AndroidManifest.xml" -Value $ManifestContent -Encoding UTF8

# 3. Compile Java sources into classes.dex with R8 optimization
Write-Host "Compiling Java sources..." -ForegroundColor Yellow
$JavaSource = "$ProjectDir\android\app\src\main\java\com\aom\nativeport\MyNativeActivity.java"
& $Javac -cp $AndroidJar -source 8 -target 8 -d "$ApkBuildDir\classes" $JavaSource

$ClassFiles = Get-ChildItem "$ApkBuildDir\classes" -Recurse -Filter "*.class" | ForEach-Object { $_.FullName }
$ProguardRules = "$ProjectDir\android\app\proguard-rules.pro"

if ((Test-Path $D8Jar) -and (Test-Path $JavaExe) -and (Test-Path $ProguardRules)) {
    Write-Host "Running R8 optimizer & minifier..." -ForegroundColor Yellow
    & $JavaExe -cp $D8Jar com.android.tools.r8.R8 `
        --release `
        --min-api 24 `
        --lib "$AndroidJar" `
        --pg-conf "$ProguardRules" `
        --output "$ApkBuildDir" `
        $ClassFiles
} else {
    Write-Host "Falling back to standard d8..." -ForegroundColor Yellow
    & $D8 --min-api 24 --output "$ApkBuildDir" $ClassFiles
}

# 4. Generate Keystore if missing
if (-not (Test-Path $Keystore)) {
    Write-Host "Generating debug.keystore..." -ForegroundColor Yellow
    & $Keytool -genkeypair -v -keystore $Keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
}

# 5. Package resources and assets with AAPT
Write-Host "Packaging APK with aapt..." -ForegroundColor Yellow
Push-Location $ApkBuildDir
try {
    & $Aapt package -F app.unaligned.apk `
        -M AndroidManifest.xml `
        -S "$ResDir" `
        -A "$AssetsDir" `
        -0 ttf `
        -0 png `
        -0 ddt `
        -0 mp3 `
        -0 wav `
        -I "$AndroidJar" -f

    # Add classes.dex
    & $Aapt add app.unaligned.apk classes.dex

    # Add native libs
    if (Test-Path "lib\arm64-v8a\libaom_native.so") {
        & $Aapt add app.unaligned.apk lib/arm64-v8a/libaom_native.so
        if (Test-Path "lib\arm64-v8a\libc++_shared.so") {
            & $Aapt add app.unaligned.apk lib/arm64-v8a/libc++_shared.so
        }
    }
    if (Test-Path "lib\x86_64\libaom_native.so") {
        & $Aapt add app.unaligned.apk lib/x86_64/libaom_native.so
        if (Test-Path "lib\x86_64\libc++_shared.so") {
            & $Aapt add app.unaligned.apk lib/x86_64/libc++_shared.so
        }
    }

    # 6. Zipalign (4-byte alignment)
    Write-Host "Aligning APK..." -ForegroundColor Yellow
    & $Zipalign -f -p 4 app.unaligned.apk app.aligned.apk

    # 7. Apksigner (v2 & v3 signature scheme)
    Write-Host "Signing APK with apksigner..." -ForegroundColor Yellow
    & $Apksigner sign `
        --ks "$Keystore" `
        --ks-pass pass:android `
        --ks-key-alias androiddebugkey `
        --key-pass pass:android `
        --out "$OutputApk" `
        app.aligned.apk

    Write-Host "Verifying signature..." -ForegroundColor Yellow
    & $Apksigner verify -v "$OutputApk"

    $ApkSize = (Get-Item "$OutputApk").Length / 1MB
    Write-Host "=== APK gerado com sucesso! ===" -ForegroundColor Green
    Write-Host "Arquivo: $OutputApk"
    Write-Host ("Tamanho: {0:N2} MB" -f $ApkSize)
    Write-Host "Versao:  versionCode=$VersionCode, versionName=$VersionName"
}
finally {
    Pop-Location
}

# 8. Install and validate if requested
if ($Install) {
    Write-Host "Installing APK on connected Android device / emulator..." -ForegroundColor Cyan
    & $Adb install -r "$OutputApk"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Installation SUCCESS!" -ForegroundColor Green
        if ($Launch) {
            Write-Host "Launching AoM Native on device..." -ForegroundColor Cyan
            & $Adb shell am start -n com.aom.nativeport/com.aom.nativeport.MyNativeActivity
        }
    } else {
        Write-Error "Failed to install APK via adb."
    }
}
