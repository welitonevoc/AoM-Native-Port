# P0 — Build Recovery: Plano de Ação Detalhado

**Objetivo:** Obter um build executável do `libaom_native.so` e gerar um APK assinado instalável no dispositivo Android, validando que o código atual compila e inicia sem ANR/crash.

**Bloqueador atual:** O ambiente Windows PowerShell (VS Code sandbox) não possui `cmake`, `clang`, `clang++`, `java`, `gradle`, `adb`, `aapt`, `zipalign`, `apksigner`, `keytool` no PATH. Não é possível compilar ou empacotar aqui.

**Ambiente necessário para execução:** Termux real no dispositivo Android ou ambiente Antigravity com Android SDK/NDK completo.

---

## Fase P0.1 — Disponibilizar Toolchain de Build

**Responsável:** Build & CI Engineer + Android/Systems Engineer  
**Agentes:** technical-director, lead-programmer, engine-programmer

### Passos no Termux real:

```bash
# 1. Atualizar repositórios
pkg update -y
pkg upgrade -y

# 2. Instalar ferramentas de build
pkg install -y cmake make clang git unzip openjdk-17

# 3. Verificar instalações
cmake --version
clang++ --version
java -version
```

### Passos no ambiente Antigravity (se disponível):

```bash
# Verificar se há NDK e SDK
ls /data/user/0/com.agy/files/android-ndk-r27b/
ls /data/user/0/com.agy/files/tools/android-sdk/

# Se houver, adicionar ao PATH
export ANDROID_NDK=/data/user/0/com.agy/files/android-ndk-r27b
export ANDROID_SDK=/data/user/0/com.agy/files/tools/android-sdk
export PATH=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-aarch64/bin:$PATH
export PATH=$ANDROID_SDK/cmdline-tools/latest/bin:$PATH
export PATH=$ANDROID_SDK/platform-tools:$PATH
export PATH=$ANDROID_SDK/build-tools/34.0.0:$PATH
```

### Critério de Aceitação P0.1:
- `cmake --version` retorna versão válida (≥ 3.22)
- `clang++ --version` retorna versão válida
- `java -version` retorna OpenJDK 17+
- `gradle --version` retorna Gradle válido (se instalado via sdkman ou manual)

---

## Fase P0.2 — Regenerar Build Nativo

**Responsável:** Build & CI Engineer + Engine Programmer  
**Agentes:** engine-programmer, lead-programmer

### Comando no dispositivo (Termux ou Antigravity):

```bash
export PREFIX=/data/user/0/com.agy/files/usr  # ou /data/data/com.termux/files/usr
export PATH="$PREFIX/bin:$PATH"
export TMPDIR="$PREFIX/tmp"
mkdir -p "$TMPDIR"

cd /storage/emulated/0/Documents/AoM-Native-Port

# Verificar ferramentas
command -v cmake && command -v make && command -v clang++

# Executar build nativo
bash scripts/build_native.sh
```

### O que o script `build_native.sh` faz:

1. Detecta PREFIX (Termux ou com.agy)
2. Configura PATH e LD_LIBRARY_PATH
3. Detecta shell POSIX válido para Make
4. Executa CMake com Unix Makefiles + toolchain android-native
5. Compila com make -j$(nproc)
6. Gera `build-m2/libaom_native.so` e `.aom-build-stamp`

### Arquivos compilados (14 alvos de teste + biblioteca):

| Alvo | Tipo |
|------|------|
| `libaom_native.so` | Biblioteca compartilhada nativa (ARM64) |
| `gameplay_test` | Teste de gameplay |
| `ui_test` | Teste de UI |
| `test_camera_track` | Teste de câmeras cinemáticas |
| `test_ai_system` | Teste de IA |
| `test_anim_video` | Teste de animação e vídeo |
| `test_data_docs` | Teste de dados e documentação |
| `test_fonts_gfx` | Teste de fontes e config gráfica |
| `test_godpowers_scenarios` | Teste de poderes divinos e cenários |
| `test_localization_lcids` | Teste de localização LCID |
| `test_terrain_water` | Teste de terreno e água |
| `test_particles_anim` | Teste de partículas e animação |
| `test_campaign_mission` | Teste de campanha e missão |
| `test_audio_3d` | Teste de áudio 3D |
| `test_network` | Teste de rede |

### Critério de Aceitação P0.2:
- `build-m2/libaom_native.so` é gerado sem erros (ARM64 ELF)
- Todos os 14 alvos de teste compilam sem warnings críticos
- `.aom-build-stamp` é criado com timestamp

---

## Fase P0.3 — Executar Suíte de Testes

**Responsável:** QA & Diagnostics Engineer + QA Tester  
**Agentes:** qa-lead, qa-tester

### Comando:

```bash
cd /storage/emulated/0/Documents/AoM-Native-Port/build-m2

# Executar cada teste
./gameplay_test
./ui_test
./test_camera_track
./test_ai_system
./test_anim_video
./test_data_docs
./test_fonts_gfx
./test_godpowers_scenarios
./test_localization_lcids
./test_terrain_water
./test_particles_anim
./test_campaign_mission
./test_audio_3d
./test_network
```

### Critério de Aceitação P0.3:
- Todos os 14 testes retornam "PASS" (100% de aprovação)
- Nenhuma falha de segmentação ou erro de execução

---

## Fase P0.4 — Gerar e Assinar APK

**Responsável:** Build & CI Engineer + Android/Systems Engineer  
**Agentes:** devops-engineer, release-manager

### Pré-requisitos no dispositivo:

```bash
# Verificar ferramentas de empacotamento
command -v aapt && command -v zipalign && command -v apksigner && command -v keytool && command -v javac && command -v d8
```

### Arquivos necessários:

| Arquivo | Caminho esperado | Origem |
|---------|-----------------|--------|
| `android.jar` | `tools/android-sdk/android.jar` | Android SDK platforms |
| `libc++_shared.so` | `tools/lib/libc++_shared.so` | NDK ou Termux |
| `debug.keystore` | Gerado automaticamente | keytool |

### Comando:

```bash
cd /storage/emulated/0/Documents/AoM-Native-Port

# Verificar ferramentas
command -v aapt zipalign apksigner keytool javac d8

# Empacotar e assinar APK
bash scripts/package_apk.sh
```

### O que o script `package_apk.sh` faz:

1. Verifica ferramentas (aapt, zipalign, apksigner, keytool)
2. Verifica android.jar e libc++_shared.so
3. Recompila biblioteca nativa se fontes estiverem mais recentes que o .so
4. Gera AndroidManifest.xml com versionCode dinâmico (timestamp)
5. Compila Java sources → classes.dex (javac + d8)
6. Cria debug.keystore se não existir (keytool)
7. Empacota manifest, recursos e assets com aapt (armazenamento não-comprimido para ttf, png, ddt, mp3, wav)
8. Adiciona classes.dex e bibliotecas nativas (arm64-v8a)
9. Remove rpath do .so com patchelf (se disponível)
10. Zipalign (4 bytes)
11. Assina com APK Signature Scheme v2 + v3 (apksigner)
12. Verifica assinatura (apksigner verify -v)

### Critério de Aceitação P0.4:
- `aom-native-debug.apk` é gerado com sucesso
- `apksigner verify -v aom-native-debug.apk` retorna OK (v2 + v3)
- versionCode é baseado em timestamp Unix (força atualização no Android)
- APK contém `lib/arm64-v8a/libaom_native.so` e `libc++_shared.so`

---

## Fase P0.5 — Instalar e Validar no Dispositivo

**Responsável:** QA & Diagnostics Engineer + Android/Systems Engineer  
**Agentes:** qa-tester, engine-programmer

### Comando no dispositivo Android:

```bash
# Instalar APK
adb install -r aom-native-debug.apk

# Ou instalar diretamente via terminal do dispositivo
pm install -r /storage/emulated/0/Documents/AoM-Native-Port/aom-native-debug.apk

# Capturar Logcat durante inicialização
adb logcat | grep -i "AoM\|AoM-Native\|ANR\|crash\|Fatal"
```

### Validações de runtime:

| Validação | Método | Critério de Sucesso |
|-----------|--------|---------------------|
| App inicia sem ANR | Logcat + observação visual | Nenhuma mensagem "Application Not Responding" em 5s |
| Renderização 60 FPS | Logcat + observação visual | FPS estável em 60 |
| Som não trava | Logcat | Nenhum erro OpenSL ES |
| Toque responsivo | Teste manual | Resposta < 100ms |
| Memória estável | Logcat + dumpsys | Sem OOM em 5 min |

### Critério de Aceitação P0.5:
- App instala e inicia no dispositivo Android
- Nenhum ANR no launch (watchdog de 5s não é acionado)
- Renderização a 60 FPS sem travamentos
- Logcat sem erros críticos (Fatal, SIGSEGV)

---

## Gate Check P0→P1

Antes de avançar para P1 (Gameplay Core), executar `/gate-check`:

- [ ] Build válido: `libaom_native.so` compila sem erros
- [ ] APK instalável: `aom-native-debug.apk` assinado v2+v3
- [ ] Sem ANR: App inicia sem ANR no dispositivo
- [ ] Testes passando: Todos os 14 testes retornam PASS
- [ ] Renderização: 60 FPS estáveis no dispositivo

---

## Integração com as 4 Skills

### @claude-code-game-studio
- Técnico-diretor: Valida arquitetura do build e orçamento técnico
- Lead Programmer: Revisa código do CMakeLists e scripts de build
- Engine Programmer: Valida integração do native_activity.cpp com o engine
- QA & Diagnostics: Valida testes e Logcat
- Build & CI: Gerencia toolchain e empacotamento

### @reverse-engineer-anything
- Inspecionar `libaom_native.so` para verificar dependências dinâmicas (readelf -d)
- Verificar se o .so contém `ANativeActivity_onCreate` exportado
- Confirmar que não há dependências ausentes (libz.so.1 vs libz.so)
- Validar assinatura APK v2+v3

### @r8-analyzer
- AGP 8.5.2 < 9.3 → Path B obrigatório
- R8 não está habilitado (`minifyEnabled false`)
- Para análise completa: habilitar minify em release, instalar Gradle + R8 9.4.34+
- Executar Path B: temporariamente atualizar R8 version, gerar relatório, reverter

### @offtec-portai
- Analisar portabilidade do código C++ para Android GLES3
- Validar dependências de desktop vs mobile
- Automatizar empacotamento de APK se Gradle não estiver disponível
- Verificar compatibilidade de touchscreen/GLES3

---

## Próxima Ação Imediata

**No dispositivo Android (Termux real ou Antigravity):**

```bash
# 1. Instalar toolchain
pkg update -y && pkg install -y cmake make clang git unzip openjdk-17

# 2. Configurar ambiente
export PREFIX=/data/user/0/com.agy/files/usr
export PATH="$PREFIX/bin:$PATH"
export TMPDIR="$PREFIX/tmp"
mkdir -p "$TMPDIR"

# 3. Navegar para o projeto
cd /storage/emulated/0/Documents/AoM-Native-Port

# 4. Build nativo
bash scripts/build_native.sh

# 5. Empacotar APK
bash scripts/package_apk.sh

# 6. Instalar e testar
pm install -r aom-native-debug.apk
```

Este plano P0 deve ser executado integralmente antes de qualquer avanço para P1 (Gameplay Core).