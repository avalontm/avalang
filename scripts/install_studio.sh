#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_studio_linux/) sigan
# funcionando sin importar desde donde se invoque este .sh.
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# Ava Studio install_studio.sh (Linux / macOS) -- contraparte de
# install_studio.bat
#
# Instala/chequea todo lo necesario para configurar y compilar
# ava_studio (el shell del IDE basado en ImGui), y despues corre
# build_studio.sh.
#
# Que chequea/hace:
#   1. git      -- lo necesita CMake FetchContent para bajar GLFW +
#                  Dear ImGui (rama docking) la primera vez que se
#                  configura con AVA_BUILD_STUDIO=ON.
#   2. cmake    -- necesario para configurar/compilar.
#   3. A diferencia de Windows (que usa vcpkg via install.bat), en
#      Linux/macOS libcurl-dev (necesaria para el plugin ai_agent,
#      find_package(CURL REQUIRED) en su CMakeLists.txt) se instala
#      via el gestor de paquetes del sistema, no via este script --
#      solo se chequea que este presente y se avisa si falta.
#   4. Corre build_studio.sh (salvo que se pase "skipbuild").
#
# Uso:
#   install_studio.sh              chequea deps, despues compila
#   install_studio.sh skipbuild    solo chequea deps
# =====================================================================

SKIP_BUILD=0
for arg in "$@"; do
    case "$arg" in
        skipbuild) SKIP_BUILD=1 ;;
    esac
done

echo "====================================================================="
echo "Ava Studio dependency check"
echo "====================================================================="
echo

# --- 1. git --------------------------------------------------------------
if ! command -v git >/dev/null 2>&1; then
    echo "[ERROR] git not found on PATH."
    echo "        CMake needs it to fetch GLFW and Dear ImGui the first time"
    echo "        you configure with AVA_BUILD_STUDIO=ON. Install it (e.g."
    echo "        'sudo apt-get install git') and re-run."
    exit 1
fi
echo "[OK] git found"

# --- 2. cmake --------------------------------------------------------------
if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] cmake not found on PATH."
    echo "        Install it (e.g. 'sudo apt-get install cmake') and re-run."
    exit 1
fi
echo "[OK] cmake found"

# --- 3. libcurl para el plugin ai_agent -----------------------------------
echo
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libcurl 2>/dev/null; then
    echo "[OK] libcurl found via pkg-config -- ai_agent plugin should configure cleanly."
elif [[ -f /usr/include/curl/curl.h ]] || [[ -f /usr/local/include/curl/curl.h ]]; then
    echo "[OK] curl.h found -- ai_agent plugin should configure cleanly."
else
    echo "[INFO] libcurl development headers were not found. ava_studio will"
    echo "       still build and run, but the ai_agent plugin's CMake"
    echo "       configure step (find_package(CURL REQUIRED)) will fail."
    echo "       Install it via your system package manager, e.g.:"
    echo "         sudo apt-get install libcurl4-openssl-dev   (Debian/Ubuntu)"
    echo "         brew install curl                           (macOS)"
    echo "       then re-run this script."
fi

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    echo
    echo "[OK] CMAKE_PREFIX_PATH is set (${CMAKE_PREFIX_PATH}) -- looks like"
    echo "     install.sh already ran. Ava Studio will get the real AvaLang parser."
else
    echo
    echo "[INFO] CMAKE_PREFIX_PATH is not set. Ava Studio will still build and"
    echo "       run, but Run (F5) inside it will hit the stub-frontend error"
    echo "       instead of actually parsing your script (see README.md) if"
    echo "       antlr4-runtime isn't on a default CMake search path."
    echo "       Run install.sh once first if you haven't, then come back and"
    echo "       run this script."
fi

echo
echo "====================================================================="
echo "Dependency check done."
echo "====================================================================="

if [[ $SKIP_BUILD -eq 1 ]]; then
    echo
    echo "Skipping build (skipbuild passed). Run build_studio.sh manually when ready."
    exit 0
fi

echo
echo "Running build_studio.sh ..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
bash "${SCRIPT_DIR}/build_studio.sh"
exit $?
