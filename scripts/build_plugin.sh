#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (build_studio_linux/, etc.) sigan funcionando sin importar
# desde donde se invoque este .sh.
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# Ava Studio plugin build script (Linux / macOS) -- contraparte de
# build_plugin.bat
#
# Compila UN SOLO plugin (ai_agent o hello_world) contra el
# build_studio_linux/ que ya existe, sin tocar ava_studio ni el resto
# del proyecto -- para iterar rapido en un plugin sin esperar a que
# recompile el IDE entero cada vez. NO configura CMake: si
# build_studio_linux/ no existe todavia, corre build_studio.sh primero
# (una sola vez alcanza, despues este script reusa esa configuracion).
#
# Uso:
#   build_plugin.sh                    build ai_agent (default), Release
#   build_plugin.sh ai_agent           lo mismo, explicito
#   build_plugin.sh hello_world        build hello_world en cambio
#   build_plugin.sh ai_agent debug     build ai_agent en Debug
#
# El nombre de plugin y "debug" pueden ir en cualquier orden.
# =====================================================================

BUILD_DIR="build_studio_linux"
BUILD_TYPE="Release"
PLUGIN_NAME="ai_agent"

show_help() {
    cat <<HELP_EOF
Usage: build_plugin.sh [ai_agent|hello_world] [debug]
  ai_agent      build the ai_agent plugin (default)
  hello_world   build the hello_world plugin instead
  debug         build Debug instead of Release

Requires ${BUILD_DIR}/ to already be configured -- runs
build_studio.sh once automatically if it's missing.
HELP_EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        debug)        BUILD_TYPE="Debug" ;;
        ai_agent)     PLUGIN_NAME="ai_agent" ;;
        hello_world)  PLUGIN_NAME="hello_world" ;;
        help|--help|-h) show_help ;;
        *)
            echo "[ERROR] Unknown argument: $1"
            show_help
            ;;
    esac
    shift
done

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "[INFO] ${BUILD_DIR}/ doesn't exist yet -- needs to be configured once"
    echo "       before this script can build just the plugin. Running"
    echo "       build_studio.sh first (this one full build is unavoidable"
    echo "       the first time; after this, build_plugin.sh reuses it) ..."
    echo
    BOOTSTRAP_ARG=""
    if [[ "$BUILD_TYPE" == "Debug" ]]; then
        BOOTSTRAP_ARG="debug"
    fi
    if ! bash "$(dirname "$0")/build_studio.sh" $BOOTSTRAP_ARG; then
        echo "[ERROR] build_studio.sh failed. See output above."
        exit 1
    fi
    echo
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] cmake was not found on PATH."
    echo "        Install it (e.g. 'sudo apt-get install cmake') and re-run."
    exit 1
fi

PLUGIN_TARGET="${PLUGIN_NAME}_plugin"

echo
echo "Building ${PLUGIN_TARGET} (${BUILD_TYPE}) against existing ${BUILD_DIR}/ ..."
if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target "$PLUGIN_TARGET" --parallel; then
    echo "[ERROR] Build failed. See output above."
    echo "        If this is a stale-cache error (target not found, etc.),"
    echo "        run build_studio.sh once to reconfigure, then retry."
    exit 1
fi

# runtime/avastudio/CMakeLists.txt pone ava_studio en
# build_studio_linux/runtime/avastudio/<Config>/ (single-config
# generators como Unix Makefiles/Ninja lo dejan directo en
# runtime/avastudio/, sin subcarpeta de Config) -- el plugin tiene
# RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:ava_studio>/plugins",
# osea justo al lado.
STUDIO_DIR="${BUILD_DIR}/runtime/avastudio/${BUILD_TYPE}"
if [[ ! -f "${STUDIO_DIR}/ava_studio" ]]; then
    STUDIO_DIR="${BUILD_DIR}/runtime/avastudio"
fi
PLUGIN_LIB="${STUDIO_DIR}/plugins/lib${PLUGIN_NAME}.so"
if [[ ! -f "$PLUGIN_LIB" ]]; then
    PLUGIN_LIB="${STUDIO_DIR}/plugins/lib${PLUGIN_NAME}.dylib"
fi

echo
echo "====================================================================="
echo "Build succeeded."
echo "${PLUGIN_NAME}: ${PLUGIN_LIB}"
echo "====================================================================="
echo
echo "Si ava_studio ya esta abierto, cerralo y volvelo a abrir para que"
echo "cargue la libreria nueva -- PluginHost::LoadAll solo escanea plugins/"
echo "al arrancar, no hay hot-reload."

exit 0
