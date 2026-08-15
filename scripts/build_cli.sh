#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_linux/, vcpkg/) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/build_cli.sh
#   (debe ejecutarse DENTRO de WSL o de un shell Linux nativo)
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# ava_cli build script (Linux / WSL)
#
# Configura y pide como target explicito SOLO ava_cli -- ni avahost, ni
# ava_studio, ni avalang_ui.so se compilan (todos OFF/no-target por
# defecto). avalang no se pide como --target aparte: es dependencia de
# link de ava_cli, asi que CMake la compila sola si hace falta (primera
# vez, o fuentes desactualizadas) y no la toca si ya esta al dia en
# build_linux/ -- no se fuerza una recompilacion de avalang en cada
# corrida. Si avalang_ui.so ya existe en build_linux/ por otra razon,
# se copia junto a ava_cli (ver copy step mas abajo), pero este script
# no la requiere ni la fuerza a compilar. Usa su propia carpeta de
# build (build_linux/) para no tocar build/, build_avahost/ ni
# build_studio/.
#
# Uso:
#   build_cli.sh                 build Release (Unix Makefiles, default)
#   build_cli.sh debug           build Debug en vez de Release
#   build_cli.sh clean           borra build_linux/ y termina
#   build_cli.sh ninja           usa Ninja en vez de Unix Makefiles
#   build_cli.sh run <script>    despues de compilar, corre ava_cli <script>
#
# Los flags se pueden combinar, ej.:  build_cli.sh clean debug
#
# A diferencia de Windows (que usa vcpkg), en Linux las dependencias
# (antlr4-runtime, libffi, java) se instalan por fuera de este script
# via install.sh (apt + build-from-source para antlr4-runtime). Si no
# estan presentes, ava_cli igual compila pero el frontend cae al stub
# (ver README.md y install.sh).
# =====================================================================

BUILD_DIR="build_linux"
BUILD_TYPE="Release"
CLEAN=0
USE_NINJA=0
RUN_AFTER=0
RUN_SCRIPT=""
OTHER_FLAG=0

show_help() {
    cat <<EOF
Usage: build_cli.sh [clean] [debug] [ninja] [run <script.ava>]
  clean   alone: delete ${BUILD_DIR}/ and exit, nothing else
          combined with debug/ninja/run: wipe ${BUILD_DIR}/ first, then build
  debug   build Debug instead of Release
  ninja   use the Ninja generator instead of Unix Makefiles
  run <script.ava>   after a successful build, run ava_cli against that script
EOF
    exit 0
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        clean)  CLEAN=1 ;;
        debug)  BUILD_TYPE="Debug"; OTHER_FLAG=1 ;;
        ninja)  USE_NINJA=1; OTHER_FLAG=1 ;;
        run)
            RUN_AFTER=1
            OTHER_FLAG=1
            shift
            if [[ $# -gt 0 && "$1" != -* ]]; then
                RUN_SCRIPT="$1"
            fi
            ;;
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

# AVA_BUILD_CLI ya es ON por defecto (ver CMakeLists.txt raiz); lo
# pasamos explicito igual por claridad. avahost/studio/ui se quedan
# en su default OFF (UI es ON por defecto pero ava_cli no lo linkea).
CMAKE_CONFIGURE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DAVA_BUILD_CLI=ON)

# En Linux no usamos vcpkg: las deps se instalan via install.sh
# (apt-get para libffi/java, build-from-source para antlr4-runtime).
# CMake las encuentra solo si estan en los paths del sistema
# (cmake/FindAntlr4Jar.cmake y cmake/FindLibFFI.cmake).

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
# Solo pedimos el target ava_cli. avalang NO se pasa como --target
# aparte: ava_cli lo linkea (target_link_libraries PRIVATE avalang en
# runtime/avacli/CMakeLists.txt), asi que CMake ya lo arma en el grafo
# de dependencias y lo compila solo -- primera vez que no existe, o si
# sus fuentes cambiaron. Si libavalang.so ya esta al dia en build_linux/,
# no se vuelve a compilar; make/ninja lo detectan solos sin que lo
# forcemos con un --target separado. avalang_ui tampoco se agrega como
# target -- ava_cli no lo linkea ni lo necesita para correr.
if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target ava_cli --parallel; then
    echo "[ERROR] Build failed. See output above."
    exit 1
fi

# Unix Makefiles (single-config) no anade el subdir del Config: ava_cli
# cae directo en build_linux/runtime/avalang/ava_cli. Ninja igual.
# Si algun dia se usa un generador multi-config, cubrimos ese caso.
AVA_CLI_BIN="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}/ava_cli"
if [[ ! -f "$AVA_CLI_BIN" ]]; then
    AVA_CLI_BIN="${BUILD_DIR}/runtime/avalang/ava_cli"
fi

AVA_CLI_DIR="$(dirname "$AVA_CLI_BIN")"

# ava_cli no linkea avalang_ui y esta build no lo compila a proposito
# (ver arriba) -- esto solo copia la .so si ya quedo compilada ahi por
# otra razon (ej. alguien corrio build.sh apuntando a esta misma carpeta,
# o dejo AVA_BUILD_UI/otro target prendido antes). Si no esta, no pasa
# nada, sin warning -- no es un requisito de este script.
AVAUI_SO="${BUILD_DIR}/runtime/avaui/${BUILD_TYPE}/libavalang_ui.so"
if [[ ! -f "$AVAUI_SO" ]]; then
    AVAUI_SO="${BUILD_DIR}/runtime/avaui/libavalang_ui.so"
fi
if [[ -f "$AVAUI_SO" ]]; then
    echo "Copying libavalang_ui.so next to ava_cli ..."
    cp -f "$AVAUI_SO" "$AVA_CLI_DIR/"
fi

# Copia las librerias empaquetadas en libraries/ (mysql, etc.) a
# modules/<nombre>/ junto a ava_cli, el mismo lugar que
# ModulesDirNextToExecutable() (main.cpp) le pasa a SetStdlibPath.
# AvaStudio resuelve import mysql porque tiene SU PROPIA carpeta
# modules/ junto a AvaStudio.exe -- build_linux/ tiene la suya aparte,
# asi que hay que copiar aca tambien.
if [[ -d "libraries" ]]; then
    echo "Copying libraries/ into modules/ next to ava_cli ..."
    mkdir -p "${AVA_CLI_DIR}/modules"
    cp -rf libraries/* "${AVA_CLI_DIR}/modules/"
fi

echo
echo "====================================================================="
echo "Build succeeded."
echo "ava_cli: ${AVA_CLI_BIN}"
echo "====================================================================="

if [[ $RUN_AFTER -eq 1 ]]; then
    if [[ -f "$AVA_CLI_BIN" ]]; then
        echo
        # ava_cli busca libavalang.so via rpath o LD_LIBRARY_PATH. Si no
        # esta en el path de busqueda del SO, lo agregamos aca para que
        # el "run" del script funcione sin que el usuario tenga que
        # setear nada a mano.
        if [[ -f "${AVA_CLI_DIR}/libavalang.so" ]]; then
            export LD_LIBRARY_PATH="${AVA_CLI_DIR}:${LD_LIBRARY_PATH:-}"
        fi
        if [[ -n "$RUN_SCRIPT" ]]; then
            echo "Running ava_cli ${RUN_SCRIPT} ..."
            "$AVA_CLI_BIN" "$RUN_SCRIPT"
        else
            echo "No script given after 'run' -- showing usage:"
            "$AVA_CLI_BIN"
        fi
    else
        echo "[WARN] Expected ava_cli at ${AVA_CLI_BIN} but it's not there."
    fi
fi

exit 0
