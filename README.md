# AoM Native Port

An independent C++20/Android prototype exploring a native RTS engine inspired
by Age of Mythology. This is an unofficial project and is not affiliated with
or endorsed by Microsoft or World's Edge.

## Repository contents

This repository contains the project's source code, build scripts, and
documentation. It does **not** contain the game's proprietary assets, extracted
game data, APKs, signing keys, build outputs, or the separately maintained
`tools/rea` repository.

To run the project with game data, provide your own legally obtained copy of
the game and keep its files local; do not commit or redistribute those files.
The Android packaging scripts expect the required assets and Android SDK files
to be present locally.

## Build and importar dados no Android

O projeto já contém a base nativa C++20, a entrada `NativeActivity`, GLES3,
parsers BAR/DDT/BRG/XMB, áudio, vídeo e o módulo Gradle Android. Ele não contém
nem redistribui os arquivos proprietários do jogo.

Para usar dados de uma cópia legalmente obtida, mantenha-os fora do repositório
e disponibilize uma destas estruturas no dispositivo:

- `/storage/emulated/0/Download/AoM/`
- `/storage/emulated/0/Documents/AoM/`
- diretório específico do aplicativo (`externalDataPath/AoM`)

O diretório deve conter pelo menos `textures/textures.bar`, `textures.bar` ou
`data/data.bar`. A biblioteca também procura os arquivos empacotados em
`assets/`, que são apenas os assets livres/prototípicos já presentes neste
projeto.

### Android Studio/Gradle

Configure um SDK com API 34 e um NDK instalado, então execute:

```sh
./gradlew :app:assembleDebug
```

O APK será produzido em `android/app/build/outputs/apk/debug/`. O módulo usa
somente `arm64-v8a` e Android API 24 ou superior.

### Fluxo nativo no dispositivo

Em Termux, com CMake, Clang e as ferramentas Android disponíveis:

```sh
sh scripts/build_native.sh
sh scripts/package_apk.sh
```

O primeiro comando gera `build-m2/libaom_native.so`; o segundo gera
`aom-native-debug.apk`. O empacotamento exige `android.jar`, `libc++_shared.so`,
`aapt`, `d8`, `zipalign`, `apksigner` e um keystore local. Esses componentes
não são incluídos no repositório.

O projeto é um protótipo jogável/MVP, não uma conversão completa ou equivalente
à versão original. Campanhas, multiplayer, editor, várias facções e parte dos
formatos de animação ainda não estão implementados. Consulte `docs/MVP.md` e
`PROGRESSO.md` antes de considerar uma build pronta para uso.

## Status and technical notes

See [PROGRESSO.md](PROGRESSO.md) and the analysis documents under `docs/`.
The current project is a prototype, not a complete or feature-equivalent port
of the original game.

## License

No license is granted by this repository. All rights remain with their
respective owners; third-party components retain their own licenses.
