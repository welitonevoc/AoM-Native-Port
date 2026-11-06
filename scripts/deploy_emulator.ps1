# scripts/deploy_emulator.ps1
# Instala o APK no dispositivo/emulador connected via ADB e envia os dados
# originais do Age of Mythology para a pasta privada do app.
#
# Por que este script existe:
#   1. `adb install -r` apaga /sdcard/Android/data/com.aom.nativeport/files/,
#      entao os dados do jogo precisam ser reenviados a cada instalacao.
#   2. `adb push` cria diretorios com modo 0770 e dono `shell`; como o app
#      roda em outro UID, ele nao conseguia atravessar essas pastas e os
#      archives abriam vazios ("Loaded archives: ... valid=0"). O `chmod`
#      final normaliza as permissoes.
param(
    [string]$Apk = "",
    [string]$GameData = "E:\Age of Mythology\Age of Mythology",
    [string]$Package = "com.aom.nativeport",
    [string]$Activity = ".MyNativeActivity",
    [switch]$SkipPush,
    [switch]$Launch
)

$ErrorActionPreference = "Stop"

$ProjectDir = (Resolve-Path "$PSScriptRoot\..").Path
if (-not $Apk) { $Apk = "$ProjectDir\aom-native-debug.apk" }

$AndroidHome = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$Adb = "$AndroidHome\platform-tools\adb.exe"

if (-not (Test-Path $Adb)) { throw "adb nao encontrado em '$Adb'" }
if (-not (Test-Path $Apk))  { throw "APK nao encontrado em '$Apk' (rode scripts/package_apk.ps1 antes)" }

Write-Host "=== AoM Native Port - Deploy no dispositivo ADB ===" -ForegroundColor Cyan

$devices = & $Adb devices | Select-Object -Skip 1 | Where-Object { $_ -match "device$" }
if (-not $devices) {
    throw "Nenhum dispositivo/emulador pronto. Inicie o emulador e aguarde o boot."
}
Write-Host "Dispositivo: $($devices -join ', ')" -ForegroundColor DarkGray

Write-Host "--- Instalando APK ---" -ForegroundColor Cyan
& $Adb install -r $Apk
if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar o APK" }

if (-not $SkipPush) {
    if (-not (Test-Path $GameData)) {
        throw "Pasta de dados do jogo nao encontrada em '$GameData'"
    }
    $dest = "/sdcard/Android/data/$Package/files/AoM"
    Write-Host "--- Enviando dados do jogo para $dest ---" -ForegroundColor Cyan
    & $Adb shell mkdir -p $dest

    $folders = @("data", "textures", "models", "scenario", "sound", "anim", "god powers")
    foreach ($folder in $folders) {
        $source = Join-Path $GameData $folder
        if (-not (Test-Path $source)) {
            Write-Host "  (pasta opcional ausente: $folder)" -ForegroundColor DarkYellow
            continue
        }
        & $Adb push $source "$dest/" | Out-Null
        Write-Host "  $folder enviado" -ForegroundColor DarkGray
    }

    Write-Host "--- Normalizando permissoes (o app roda em outro UID) ---" -ForegroundColor Cyan
    & $Adb shell "chmod -R a+rX $dest"
    & $Adb shell "ls -ld $dest"
}

if ($Launch) {
    Write-Host "--- Iniciando o app ---" -ForegroundColor Cyan
    & $Adb shell am force-stop $Package
    Start-Sleep -Seconds 2
    & $Adb shell monkey -p $Package -c android.intent.category.LAUNCHER 1 | Out-Null
}

Write-Host "=== Deploy concluido ===" -ForegroundColor Green
Write-Host "Para ver os logs: adb logcat -s AoM-Engine AoM-Mission AoM-Native AoM-GLES" -ForegroundColor DarkGray
