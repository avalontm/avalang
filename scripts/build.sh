#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_linux/, vcpkg/) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/build.sh
#   (debe ejecutarse DENTRO de WSL o de un shell Linux nativo)
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# AvaLang build script (Linux / WSL)
#
# Compila el proyecto completo: avalang (.so), avaui (.so) y ava_cli.
# A diferencia de build_cli.sh (que solo compila ava_cli), este script
# tambien compila avalang_ui.so (AVA_BUILD_UI=ON, que ya es el default).
# Usa su propia carpeta de build (build_linux/) para no tocar build_cli/
# u otras configuraciones.
#
# Uso:
#   build.sh                 build Release (Unix Makefiles, default)
#   build.sh debug           build Debug instead
#   build.sh clean           delete build_linux/ and exit -- nothing else,
#                            no configure/build (see below)
#   build.sh ninja           use Ninja instead of Unix Makefiles
#                            (requires 'ninja' on PATH)
#
# Flags can be combined, e.g.:  build.sh clean debug
# "clean" alone (no other flag) just wipes build_linux/ and stops there --
# handy when you only want to reclaim disk space or force a from-scratch
# CMake reconfigure later. Combine it with another flag (debug/ninja) to
# wipe build_linux/ first and then continue into a normal build.
#
# A diferencia de Windows (que usa vcpkg), en Linux las dependencias
# (antlr4-runtime, libffi, java) se instalan por fuera de este script
# via install.sh. Si no estan presentes, avalang compila igual pero el
# frontend cae al stub (ver README.md y install.sh).
# =====================================================================

BUILD_DIR="build_linux"
BUILD_TYPE="Release"
CLEAN=0
USE_NINJA=0
OTHER_FLAG=0

show_help() {
    cat <<EOF
Usage: build.sh [clean] [debug] [ninja]
  clean   alone: delete ${BUILD_DIR}/ and exit, nothing else
          combined with debug/ninja: wipe ${BUILD_DIR}/ first, then build
  debug   build Debug instead of Release
  ninja   use the Ninja generator instead of Unix Makefiles
EOF
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

# Check cmake
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

CMAKE_CONFIGURE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE")
# AVA_BUILD_UI ya es ON por defecto (ver CMakeLists.txt raiz);
# AVA_BUILD_CLI tambien. avahost/studio/pack se quedan en su default OFF.

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
# No pasamos --target: se compilan todos los targets habilitados
# (avalang, avalang_ui, ava_cli) -- a diferencia de build_cli.sh que
# pide solo ava_cli, aca queremos el set completo.
if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel; then
    echo "[ERROR] Build failed. See output above."
    exit 1
fi

echo
echo "====================================================================="
echo "Build succeeded."
echo "Binaries are in ${BUILD_DIR}/"
echo "====================================================================="

exit 0
