#!/system/bin/sh
# =============================================================================
# Script de Validação de Ambiente de Build — AoM Native Port
# Execute em um Termux real ou ambiente Antigravity antes de rodar build_native.sh
# =============================================================================
set -eu

echo "============================================="
echo "  Validação de Ambiente de Build — AoM Native"
echo "============================================="
echo ""

ERROS=0
AVISOS=0

verificar_ferramenta() {
    local nome="$1"
    if command -v "$nome" >/dev/null 2>&1; then
        local versao
        versao=$("$nome" --version 2>/dev/null | head -n 1 || "$nome" -version 2>/dev/null | head -n 1 || echo "versão desconhecida")
        echo "  [OK]  $nome: $versao"
    else
        echo "  [FALTA] $nome: não encontrado no PATH"
        ERROS=$((ERROS + 1))
    fi
}

verificar_arquivo() {
    local caminho="$1"
    local descricao="$2"
    if [ -f "$caminho" ]; then
        local tamanho
        tamanho=$(ls -l "$caminho" 2>/dev/null | awk '{print $5}')
        echo "  [OK]  $descricao: $caminho ($tamanho bytes)"
    else
        echo "  [FALTA] $descricao: $caminho"
        ERROS=$((ERROS + 1))
    fi
}

verificar_diretorio() {
    local caminho="$1"
    local descricao="$2"
    if [ -d "$caminho" ]; then
        echo "  [OK]  $descricao: $caminho"
    else
        echo "  [FALTA] $descricao: $caminho"
        ERROS=$((ERROS + 1))
    fi
}

# Detectar PREFIX
PREFIX=""
if [ -d "/data/user/0/com.agy/files/usr/bin" ]; then
    PREFIX="/data/user/0/com.agy/files/usr"
elif [ -d "/data/data/com.termux/files/usr/bin" ]; then
    PREFIX="/data/data/com.termux/files/usr"
elif [ -d "/usr" ]; then
    PREFIX="/usr"
fi

if [ -n "$PREFIX" ]; then
    echo "PREFIX detectado: $PREFIX"
    export PATH="$PREFIX/bin:$PATH"
else
    echo "AVISO: Nenhum PREFIX de toolchain detectado (Termux/com.agy)"
    AVISOS=$((AVISOS + 1))
fi

echo ""
echo "--- Ferramentas de Build Nativo ---"
verificar_ferramenta cmake
verificar_ferramenta make
verificar_ferramenta clang
verificar_ferramenta clang++

echo ""
echo "--- Ferramentas de Empacotamento Android ---"
verificar_ferramenta javac
verificar_ferramenta d8
verificar_ferramenta aapt
verificar_ferramenta zipalign
verificar_ferramenta apksigner
verificar_ferramenta keytool
verificar_ferramenta patchelf

echo ""
echo "--- Ferramentas de Build Gradle (opcional para build direto) ---"
verificar_ferramenta gradle
verificar_ferramenta java

echo ""
echo "--- Arquivos do Projeto ---"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
verificar_arquivo "$PROJECT_DIR/CMakeLists.txt" "CMakeLists principal"
verificar_arquivo "$PROJECT_DIR/cmake/android-native.toolchain.cmake" "Toolchain CMake Android"
verificar_arquivo "$PROJECT_DIR/scripts/build_native.sh" "Script de build nativo"
verificar_arquivo "$PROJECT_DIR/scripts/package_apk.sh" "Script de empacotamento APK"
verificar_arquivo "$PROJECT_DIR/build.gradle" "build.gradle raiz"
verificar_arquivo "$PROJECT_DIR/settings.gradle" "settings.gradle"
verificar_arquivo "$PROJECT_DIR/gradle.properties" "gradle.properties"
verificar_arquivo "$PROJECT_DIR/android/app/build.gradle" "build.gradle do app"
verificar_arquivo "$PROJECT_DIR/android/app/src/main/AndroidManifest.xml" "AndroidManifest do app"
verificar_arquivo "$PROJECT_DIR/android/app/src/main/java/com/aom/nativeport/MyNativeActivity.java" "MyNativeActivity.java"

verificar_diretorio "$PROJECT_DIR/include" "Headers C++"
verificar_diretorio "$PROJECT_DIR/src" "Fontes C++"
verificar_diretorio "$PROJECT_DIR/third_party" "Dependências third_party"
verificar_diretorio "$PROJECT_DIR/assets" "Assets do jogo"

verificar_arquivo "$PROJECT_DIR/tools/android-sdk/android.jar" "android.jar (SDK)"
verificar_arquivo "$PROJECT_DIR/tools/lib/libc++_shared.so" "libc++_shared.so (NDK runtime)"

echo ""
echo "--- Variáveis de Ambiente ---"
if [ -n "${PREFIX:-}" ]; then
    echo "  PREFIX=$PREFIX"
else
    echo "  PREFIX: não definido"
fi
echo "  PATH=$PATH"
echo "  TMPDIR=${TMPDIR:-não definido}"

echo ""
echo "============================================="
if [ "$ERROS" -eq 0 ]; then
    echo "  AMBIENTE VALIDADO — Nenhum erro crítico"
    echo "  Pode prosseguir com: bash scripts/build_native.sh"
    echo "  E depois:            bash scripts/package_apk.sh"
else
    echo "  $ERROS ERRO(S) CRÍTICO(S) ENCONTRADO(S)"
    echo "  Instale as ferramentas faltantes antes de continuar"
    echo "  Em Termux: pkg install cmake make clang git unzip openjdk-17"
fi
if [ "$AVISOS" -gt 0 ]; then
    echo "  $AVISOS AVISO(S)"
fi
echo "============================================="

exit "$ERROS"