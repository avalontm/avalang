#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_avaui_linux/) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/build_avaui.sh
#   (debe ejecutarse DENTRO de WSL o de un shell Linux/macOS nativo)
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# AvaUI build script (Linux / macOS) -- contraparte de build_avaui.bat
#
# Configura y compila el proyecto completo con AVA_BUILD_UI=ON, asi que
# tambien arma avalang.so/.dylib y ava_cli (dependencias de link/uso).
# Usa su propia carpeta de build (build_avaui_linux/) para no tocar
# build_linux/, build_studio_linux/ ni build_avahost_linux/.
#
# No hay flag "run": avalang_ui.so/.dylib es una libreria, no un
# ejecutable -- no hay nada que lanzar todavia (mismo comentario que
# build_avaui.bat).
#
# ui/include/avalang/ui/scene/ISceneNode.h (Fase 7) necesita glm. A
# diferencia de Windows (que usa vcpkg), en Linux/macOS glm se instala
# como dependencia del sistema (ej. 'sudo apt-get install libglm-dev' o
# 'brew install glm') -- install.sh no lo instala, este script tampoco.
# Sin el, CMake no encuentra glm y las fuentes de Scene Graph no compilan.
#
# Uso:
#   build_avaui.sh                 build Release (Unix Makefiles, default)
#   build_avaui.sh debug           build Debug en vez de Release
#   build_avaui.sh clean           borra build_avaui_linux/ y termina
#   build_avaui.sh ninja           usa Ninja en vez de Unix Makefiles
#
# Los flags se pueden combinar, ej.:  build_avaui.sh clean debug
# =====================================================================

BUILD_DIR="build_avaui_linux"
BUILD_TYPE="Release"
CLEAN=0
USE_NINJA=0
OTHER_FLAG=0

show_help() {
    cat <<HELP_EOF
Usage: build_avaui.sh [clean] [debug] [ninja]
  clean   alone: delete ${BUILD_DIR}/ and exit, nothing else
          combined with debug/ninja: wipe ${BUILD_DIR}/ first, then build
  debug   build Debug instead of Release
  ninja   use the Ninja generator instead of Unix Makefiles
HELP_EOF
    exit 0
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        clean)  CLEAN=1 ;;
        debug)  BUILD_TYPE="Debug"; OTHER_FLAG=1 ;;
        ninja)  USE_NINJA=1; OTHER_FLAG=1 ;;
        help|--help|-h) show_help ;;
        *) echo "[WARN] unknown flag: $1" ;;
    esac
    shift
done

# clean alone (no other flag): wipe build dir and exit
if [[ $CLEAN -eq 1 && $OTHER_FLAG -eq 0 ]]; then
    if [[ -d "$BUILD_DIR" ]]; then
        echo "Cleaning ${BUILD_DIR} ..."
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
        rm -rf "$BUILD_DIR"
    fi
fi

mkdir -p "$BUILD_DIR"

CMAKE_CONFIGURE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DAVA_BUILD_UI=ON)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    echo "Using CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}"
else
    echo "[INFO] CMAKE_PREFIX_PATH is not set. If antlr4-runtime was"
    echo "       installed to a non-default prefix (e.g. ~/.local via"
    echo "       install.sh), export it first:"
    echo "         export CMAKE_PREFIX_PATH=\$HOME/.local"
fi

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
if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target avalang_ui --parallel; then
    echo "[ERROR] Build failed. See output above."
    exit 1
fi

# runtime/avaui/CMakeLists.txt redirige el RUNTIME_OUTPUT_DIRECTORY de
# avalang_ui a runtime/avalang/<Config>/ -- la MISMA carpeta que
# libavalang.so/.dylib, no runtime/avaui/<Config>/ (ver el mismo
# comentario en build_avahost.sh).
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

echo
echo "====================================================================="
echo "Build succeeded."
echo "avalang_ui: ${AVAUI_LIB}"
echo "====================================================================="

exit 0
