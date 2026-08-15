#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_avahost_linux/) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/build_avahost.sh
#   (debe ejecutarse DENTRO de WSL o de un shell Linux nativo)
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# avahost build script (Linux / WSL) -- contraparte de build_avahost.bat
#
# Configura y compila con AVA_BUILD_AVAHOST=ON, asi que tambien arma
# avalang.so, avalang_ui.so y ava_cli (dependencias de link/uso). Usa
# su propia carpeta de build (build_avahost_linux/) para no tocar
# build_linux/ (la de build_cli.sh) ni build_studio/.
#
# Igual que build_cli.sh, las dependencias (antlr4-runtime, libffi,
# java, libglm) se instalan por fuera via install.sh -- si no estan
# presentes, avahost igual compila pero el frontend de avalang cae al
# stub (ver README.md).
#
# El primer configure con AVA_BUILD_AVAHOST=ON descarga nlohmann/json
# via FetchContent (CMake) -- necesita git y red esa unica vez, despues
# queda cacheado en build_avahost_linux/_deps/.
#
# Uso:
#   build_avahost.sh                 build Release (Unix Makefiles, default)
#   build_avahost.sh debug           build Debug en vez de Release
#   build_avahost.sh clean           borra build_avahost_linux/ y termina
#   build_avahost.sh ninja           usa Ninja en vez de Unix Makefiles
#   build_avahost.sh run             despues de compilar, corre "avahost run"
#
# Los flags se pueden combinar, ej.:  build_avahost.sh clean debug
# =====================================================================

BUILD_DIR="build_avahost_linux"
BUILD_TYPE="Release"
CLEAN=0
USE_NINJA=0
RUN_AFTER=0
OTHER_FLAG=0

show_help() {
    cat <<EOF
Usage: build_avahost.sh [clean] [debug] [ninja] [run]
  clean   alone: delete ${BUILD_DIR}/ and exit, nothing else
          combined with debug/ninja/run: wipe ${BUILD_DIR}/ first, then build
  debug   build Debug instead of Release
  ninja   use the Ninja generator instead of Unix Makefiles
  run     after a successful build, run "avahost run" from the repo root
EOF
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
    shift || true
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

CMAKE_CONFIGURE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DAVA_BUILD_AVAHOST=ON -DAVA_BUILD_CLI=ON)

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
if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target avahost --parallel; then
    echo "[ERROR] Build failed. See output above."
    exit 1
fi

AVAHOST_BIN="${BUILD_DIR}/runtime/avahost/${BUILD_TYPE}/avahost"
if [[ ! -f "$AVAHOST_BIN" ]]; then
    AVAHOST_BIN="${BUILD_DIR}/runtime/avahost/avahost"
fi
AVAHOST_DIR="$(dirname "$AVAHOST_BIN")"

# avahost linkea avalang/avalang_ui directamente (Stable C API). A
# diferencia de Windows (que copia los .dll al lado del .exe porque
# el loader de Windows busca ahi primero), en Linux alcanza con
# apuntar LD_LIBRARY_PATH a donde quedaron los .so -- no hace falta
# copiarlos. Se resuelve mas abajo, en el bloque "run".
AVALANG_SO="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/libavalang.so"
if [[ ! -f "$AVALANG_SO" ]]; then
    AVALANG_SO="${BUILD_DIR}/runtime/avalang/libavalang.so"
fi
# runtime/avaui/CMakeLists.txt redirects avalang_ui's own
# RUNTIME_OUTPUT_DIRECTORY to runtime/avalang/<Config>/ -- the SAME
# folder as libavalang.so, not runtime/avaui/<Config>/ -- so look for
# it there (matching AVALANG_SO above), same reasoning as build_avahost.bat.
AVAUI_SO="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/libavalang_ui.so"
if [[ ! -f "$AVAUI_SO" ]]; then
    AVAUI_SO="${BUILD_DIR}/runtime/avalang/libavalang_ui.so"
fi

echo
echo "====================================================================="
echo "Build succeeded."
echo "avahost: ${AVAHOST_BIN}"
echo "runtime libs: $(dirname "$AVALANG_SO")/libavalang.so"
echo "              $(dirname "$AVAUI_SO")/libavalang_ui.so"
echo
echo "To run it directly:"
echo "  export LD_LIBRARY_PATH=\"$(dirname "$AVALANG_SO"):$(dirname "$AVAUI_SO"):\$LD_LIBRARY_PATH\""
echo "  ${AVAHOST_BIN} run --project <your-project-dir>"
echo "====================================================================="

if [[ $RUN_AFTER -eq 1 ]]; then
    if [[ -f "$AVAHOST_BIN" ]]; then
        echo
        export LD_LIBRARY_PATH="$(dirname "$AVALANG_SO"):$(dirname "$AVAUI_SO"):${LD_LIBRARY_PATH:-}"
        echo "Running avahost run ..."
        "$AVAHOST_BIN" run
    else
        echo "[WARN] Expected avahost at ${AVAHOST_BIN} but it's not there."
    fi
fi

exit 0
