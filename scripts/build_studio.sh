#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_studio_linux/) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/build_studio.sh
#   (debe ejecutarse DENTRO de WSL o de un shell Linux/macOS nativo
#   con soporte de ventana grafica -- ava_studio es una app ImGui/GLFW)
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# Ava Studio build script (Linux / macOS) -- contraparte de build_studio.bat
#
# Configura y compila ava_studio, sus plugins (ai_agent, hello_world),
# ava_cli, avapack_gen y avapack_stub. Usa su propia carpeta de build
# (build_studio_linux/) para no tocar build_linux/, build_avaui_linux/
# ni build_avahost_linux/.
#
# A diferencia de Windows (que usa vcpkg + fast-path MSBuild sobre el
# .sln generado), en Linux/macOS no hay vcpkg ni .sln -- las deps
# (antlr4-runtime, libffi, java, libcurl-dev para ai_agent) se instalan
# por fuera via install.sh / el gestor de paquetes del sistema, y el
# build siempre pasa por 'cmake --build' (no hay atajo equivalente al
# MSBuild directo del .bat).
#
# Uso:
#   build_studio.sh                 build Release (Unix Makefiles, default)
#   build_studio.sh debug           build Debug en vez de Release
#   build_studio.sh clean           borra build_studio_linux/ y termina
#   build_studio.sh ninja           usa Ninja en vez de Unix Makefiles
#   build_studio.sh run             despues de compilar, lanza ava_studio
#
# Los flags se pueden combinar, ej.:  build_studio.sh clean debug
# =====================================================================

BUILD_DIR="build_studio_linux"
BUILD_TYPE="Release"
CLEAN=0
USE_NINJA=0
RUN_AFTER=0
OTHER_FLAG=0

show_help() {
    cat <<HELP_EOF
Usage: build_studio.sh [clean] [debug] [ninja] [run]
  clean   alone: delete ${BUILD_DIR}/ and exit, nothing else
          combined with debug/ninja/run: wipe ${BUILD_DIR}/ first, then build
  debug   build Debug instead of Release
  ninja   use the Ninja generator instead of Unix Makefiles
  run     launch ava_studio after a successful build
HELP_EOF
    exit 0
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        clean)  CLEAN=1 ;;
        debug)  BUILD_TYPE="Debug"; OTHER_FLAG=1 ;;
        ninja)  USE_NINJA=1; OTHER_FLAG=1 ;;
        run)    RUN_AFTER=1; OTHER_FLAG=1 ;;
        help|--help|-h) show_help ;;
        *) echo "[WARN] unknown flag: $1" ;;
    esac
    shift
done

# clean alone (no other flag): wipe build dir and exit
if [[ $CLEAN -eq 1 && $OTHER_FLAG -eq 0 ]]; then
    if [[ -d "$BUILD_DIR" ]]; then
        echo "Cleaning ${BUILD_DIR} ..."
        pkill -f ava_studio >/dev/null 2>&1 || true
        rm -rf "$BUILD_DIR"
        echo "Done -- ${BUILD_DIR}/ removed."
    else
        echo "Nothing to clean -- ${BUILD_DIR}/ doesn't exist."
    fi
    exit 0
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] cmake was not found on PATH."
    echo "        Install it (e.g. 'sudo apt-get install cmake') and re-run."
    exit 1
fi

if [[ $CLEAN -eq 1 ]]; then
    if [[ -d "$BUILD_DIR" ]]; then
        echo "Cleaning ${BUILD_DIR} ..."
        pkill -f ava_studio >/dev/null 2>&1 || true
        rm -rf "$BUILD_DIR"
    fi
fi

mkdir -p "$BUILD_DIR"

CMAKE_CONFIGURE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DAVA_BUILD_STUDIO=ON -DAVA_BUILD_PACK=ON -DAVA_ENABLE_LTO=OFF)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    echo "Using CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}"
else
    echo "[INFO] CMAKE_PREFIX_PATH is not set. If antlr4-runtime was"
    echo "       installed to a non-default prefix (e.g. ~/.local via"
    echo "       install.sh), export it first:"
    echo "         export CMAKE_PREFIX_PATH=\$HOME/.local"
fi

echo "[INFO] ava_studio still builds fine without libcurl, but the"
echo "       ai_agent plugin needs it (find_package(CURL REQUIRED)) --"
echo "       install it via the system package manager (e.g. 'sudo"
echo "       apt-get install libcurl4-openssl-dev' or 'brew install curl')"
echo "       if that plugin's CMake configure step fails."

echo
if [[ $USE_NINJA -eq 1 ]]; then
    if ! command -v ninja >/dev/null 2>&1; then
        echo "[ERROR] ninja not found on PATH but 'ninja' was requested."
        echo "        Install it (e.g. 'sudo apt-get install ninja-build') and re-run."
        exit 1
    fi
    echo "Configuring with Ninja (${BUILD_TYPE}) ..."
    if ! cmake -S . -B "$BUILD_DIR" -G "Ninja" "${CMAKE_CONFIGURE_ARGS[@]}"; then
        echo "[ERROR] CMake configure step failed. See output above."
        exit 1
    fi
else
    echo "Configuring with Unix Makefiles (${BUILD_TYPE}) ..."
    if ! cmake -S . -B "$BUILD_DIR" -G "Unix Makefiles" "${CMAKE_CONFIGURE_ARGS[@]}"; then
        echo "[ERROR] CMake configure step failed. See output above."
        exit 1
    fi
fi

echo
echo "Building (${BUILD_TYPE}) ..."
# Sin atajo MSBuild aca (no hay .sln en Unix Makefiles/Ninja) -- siempre
# pasamos por cmake --build, un target por vez, igual resultado final
# que el fast-path de build_studio.bat.
if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" \
    --target ava_studio --target ai_agent_plugin --target hello_world_plugin \
    --target ava_cli --target avapack_gen --target avapack_stub --parallel; then
    echo "[ERROR] Build failed. See output above."
    exit 1
fi

STUDIO_BIN="${BUILD_DIR}/runtime/avastudio/${BUILD_TYPE}/ava_studio"
if [[ ! -f "$STUDIO_BIN" ]]; then
    STUDIO_BIN="${BUILD_DIR}/runtime/avastudio/ava_studio"
fi
STUDIO_DIR="$(dirname "$STUDIO_BIN")"
PLUGINS_DIR="${STUDIO_DIR}/plugins"

AVALANG_LIB="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/libavalang.so"
if [[ ! -f "$AVALANG_LIB" ]]; then
    AVALANG_LIB="${BUILD_DIR}/runtime/avalang/libavalang.so"
fi
if [[ ! -f "$AVALANG_LIB" ]]; then
    AVALANG_LIB="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/libavalang.dylib"
fi
if [[ ! -f "$AVALANG_LIB" ]]; then
    AVALANG_LIB="${BUILD_DIR}/runtime/avalang/libavalang.dylib"
fi

AVAUI_LIB="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/libavalang_ui.so"
if [[ ! -f "$AVAUI_LIB" ]]; then
    AVAUI_LIB="${BUILD_DIR}/runtime/avalang/libavalang_ui.so"
fi
if [[ ! -f "$AVAUI_LIB" ]]; then
    AVAUI_LIB="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/libavalang_ui.dylib"
fi
if [[ ! -f "$AVAUI_LIB" ]]; then
    AVAUI_LIB="${BUILD_DIR}/runtime/avalang/libavalang_ui.dylib"
fi

# A diferencia de Windows (que copia los .dll junto a ava_studio.exe
# porque el loader de Windows busca ahi primero), en Linux/macOS el
# RPATH del build tree ya resuelve avalang/avalang_ui sin copiar nada
# (mismo razonamiento que build_cli.sh / build_avahost.sh).
if [[ -f "$AVALANG_LIB" ]]; then
    echo "[OK] avalang encontrado en $(dirname "$AVALANG_LIB")"
else
    echo "[WARN] avalang no encontrado en ${AVALANG_LIB} -- se quedo AVA_BUILD_SHARED en OFF?"
fi

LIBRARIES_DIR="libraries"
MODULES_DIR="${STUDIO_DIR}/modules"
if [[ -d "$LIBRARIES_DIR" ]]; then
    echo
    echo "Copying libraries/ into ${MODULES_DIR} ..."
    mkdir -p "$MODULES_DIR"
    cp -rf "${LIBRARIES_DIR}/." "$MODULES_DIR/"
fi

AVA_CLI_TOOLS_DIR="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}"
if [[ ! -f "${AVA_CLI_TOOLS_DIR}/ava_cli" ]]; then
    AVA_CLI_TOOLS_DIR="${BUILD_DIR}/runtime/avalang"
fi

echo
echo "====================================================================="
echo "Build succeeded."
echo "ava_studio:     ${STUDIO_BIN}"
echo "runtime libs:   ${AVALANG_LIB}"
echo "                ${AVAUI_LIB}"
echo "plugins:        ${PLUGINS_DIR}/libai_agent.so"
echo "                ${PLUGINS_DIR}/libhello_world.so"
echo "modules:        ${MODULES_DIR}"
echo "ava_cli / avapack_gen / avapack_stub:"
echo "                ${AVA_CLI_TOOLS_DIR}/ava_cli"
echo "====================================================================="

if [[ $RUN_AFTER -eq 1 ]]; then
    if [[ -f "$STUDIO_BIN" ]]; then
        echo
        echo "Launching ava_studio ..."
        "$STUDIO_BIN" &
    else
        echo "[WARN] Expected ava_studio at ${STUDIO_BIN} but it's not there."
    fi
fi

exit 0
