#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (CMakeLists.txt, third_party/, build_linux/) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/install.sh
#   (debe ejecutarse DENTRO de WSL o de un shell Linux nativo)
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# AvaLang install.sh (Linux / WSL)
#
# Contraparte Linux de install.bat (Windows). En vez de vcpkg (que
# clonaria ~300MB y haria un build from-source de antlr4-runtime dentro
# de su arbol), en Linux usamos:
#   1. apt-get  para las deps del sistema (cmake, ninja, build-essential,
#      libffi-dev, default-jdk) -- el gestor de paquetes nativo
#   2. Build-from-source  para antlr4-runtime 4.13.2 a un prefix local
#      (~/.local por defecto, o ~/antlr413-install para compat con
#      instalaciones manuales ya hechas) -- porque el paquete
#      libantlr4-runtime-dev de Ubuntu/Debian suele estar atrasado
#      (1.0.1 vs 4.13.2) y no compila contra el .g4 que usa este repo
#   3. Descarga del .jar  del generador ANTLR4 a third_party/ (solo
#      necesario en tiempo de CMake-configure para regenerar el parser
#      C++ desde AvaLang.g4; NO es dependencia de runtime)
#
# Por que build-from-source en vez de apt para antlr4-runtime:
#   - Ubuntu 22.04 trae libantlr4-runtime-dev 1.0.1 (fork obsoleto)
#   - Ubuntu 24.04 trae libantlr4-runtime-dev 4.9.3 (cercano pero no
#     suficiente -- el .g4 de este repo usa features de 4.13)
#   - El repo ya viene con un .g4 generado contra antlr4-4.13.2, y el
#     parser C++ regenerado en build_linux/generated/ tambien es 4.13.2
#   - Compilar antlr4-runtime 4.13.2 desde source tarda ~2 minutos y
#     da una lib estatica (.a) o dinamica (.so) que matchea exactamente
#     la version del .jar del generador -- sin riesgo de mismatch de
#     ABI entre el generador y el runtime
#
# Uso:
#   install.sh                  instala deps y compila antlr4-runtime
#   install.sh skipbuild        instala deps pero NO corra build_cli.sh
#   install.sh <prefix>         instala antlr4-runtime a <prefix> en
#                               vez de ~/.local (ej. ~/antlr413-install)
#   install.sh <prefix> skipbuild
#
# Ejemplo (instalar a ~/antlr413-install, replicando lo que ya tenias
# funcionando a mano):
#   bash scripts/install.sh ~/antlr413-install
# =====================================================================

SKIP_BUILD=0
ANTLR_PREFIX="${HOME}/.local"
for arg in "$@"; do
    case "$arg" in
        skipbuild) SKIP_BUILD=1 ;;
        help|--help|-h)
            cat <<EOF
Usage: install.sh [skipbuild] [<antlr4-install-prefix>]
  skipbuild   install deps only, don't run build_cli.sh afterwards
  <prefix>    install antlr4-runtime to <prefix> (default: ${HOME}/.local)
              Use ~/antlr413-install to match manual installs already on PATH.
EOF
            exit 0
            ;;
        *) ANTLR_PREFIX="$arg" ;;
    esac
done

ANTLR_VERSION="4.13.2"
ANTLR_SRC_NAME="antlr-${ANTLR_VERSION}"
ANTLR_SRC_TARBALL="${ANTLR_SRC_NAME}.tar.gz"
ANTLR_SRC_URL="https://github.com/antlr/antlr4/archive/refs/tags/${ANTLR_VERSION}.tar.gz"
ANTLR_JAR_NAME="antlr-${ANTLR_VERSION}-complete.jar"
ANTLR_JAR_URL="https://www.antlr.org/download/${ANTLR_JAR_NAME}"
THIRD_PARTY_DIR="third_party"

echo "====================================================================="
echo "AvaLang dependency installer (Linux / WSL)"
echo "  antlr4 install prefix : ${ANTLR_PREFIX}"
echo "  antlr4 version        : ${ANTLR_VERSION}"
echo "  antlr4 jar            : ${ANTLR_JAR_NAME}"
echo "====================================================================="
echo

# --- 1. Verificar git -------------------------------------------------
if ! command -v git >/dev/null 2>&1; then
    echo "[ERROR] git not found on PATH. Install it and re-run."
    exit 1
fi

# --- 2. apt-get: deps del sistema -------------------------------------
# Lista de paquetes que el build de avalang + antlr4-runtime necesita.
# Si ya estan instalados, apt-get install los saltea sin error.
APT_PKGS=(
    cmake
    ninja-build
    build-essential
    pkg-config
    libffi-dev
    default-jdk
    wget
    curl
)

if command -v apt-get >/dev/null 2>&1; then
    echo "Installing system packages via apt-get (may need sudo) ..."
    echo "  Packages: ${APT_PKGS[*]}"
    sudo apt-get update -y
    sudo apt-get install -y "${APT_PKGS[@]}"
    echo "[OK] system packages installed."
    echo
else
    echo "[WARN] apt-get not found (not Debian/Ubuntu?). Skipping system deps."
    echo "       Make sure cmake, ninja, gcc/g++, libffi-dev, java and wget"
    echo "       are installed via your distro's package manager."
    echo
fi

# --- 3. Verificar que las deps basicas si estan ----------------------
require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[ERROR] '$1' not found on PATH after apt-get step."
        echo "        Install it manually and re-run."
        exit 1
    fi
}
require_cmd cmake
require_cmd gcc
require_cmd g++

# java es opcional: solo se necesita en CMake-configure para regenerar
# el parser C++ desde el .g4. Si no esta, avalang cae al stub.
if ! command -v java >/dev/null 2>&1; then
    echo "[WARN] java not found on PATH."
    echo "       The ANTLR4 tool jar needs a JRE/JDK to run once at CMake"
    echo "       configure time (it only generates C++ parser source files;"
    echo "       it is NOT a runtime dependency of avalang)."
    echo "       Install a JDK (e.g. 'sudo apt-get install default-jdk')"
    echo "       and re-run this script. Continuing for now -- CMake will"
    echo "       fall back to the stub frontend if java or the jar cannot"
    echo "       be found."
    echo
else
    echo "[OK] java found at $(command -v java)"
    echo
fi

# --- 4. antlr4-runtime: build from source -----------------------------
# Si ya esta instalado en ANTLR_PREFIX (lib/libantlr4-runtime.a o .so
# + include/antlr4-runtime.h), se saltea el build.
ANTLR_LIB_STATIC="${ANTLR_PREFIX}/lib/libantlr4-runtime.a"
ANTLR_LIB_SHARED="${ANTLR_PREFIX}/lib/libantlr4-runtime.so"
ANTLR_HEADER="${ANTLR_PREFIX}/include/antlr4-runtime.h"

if [[ -f "$ANTLR_LIB_STATIC" || -f "$ANTLR_LIB_SHARED" ]] && [[ -f "$ANTLR_HEADER" ]]; then
    echo "[OK] antlr4-runtime already installed at ${ANTLR_PREFIX}"
    echo "     Header: ${ANTLR_HEADER}"
    if [[ -f "$ANTLR_LIB_STATIC" ]]; then
        echo "     Lib   : ${ANTLR_LIB_STATIC}"
    else
        echo "     Lib   : ${ANTLR_LIB_SHARED}"
    fi
else
    echo "Building antlr4-runtime ${ANTLR_VERSION} from source ..."
    echo "  Install prefix: ${ANTLR_PREFIX}"
    echo

    # Descargar el tarball del tag 4.13.2 de antlr4 en GitHub
    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "$TMPDIR"' EXIT

    echo "Downloading ${ANTLR_SRC_URL} ..."
    if ! curl -fsSL "$ANTLR_SRC_URL" -o "${TMPDIR}/${ANTLR_SRC_TARBALL}"; then
        echo "[ERROR] failed to download antlr4 ${ANTLR_VERSION} source."
        echo "        URL: ${ANTLR_SRC_URL}"
        exit 1
    fi

    echo "Extracting ..."
    tar -xzf "${TMPDIR}/${ANTLR_SRC_TARBALL}" -C "$TMPDIR"
    # El tarball se extrae como antlr4-<version>/ (no antlr-<version>/)
    SRC_DIR="${TMPDIR}/antlr4-${ANTLR_VERSION}"

    if [[ ! -d "$SRC_DIR" ]]; then
        echo "[ERROR] expected source dir ${SRC_DIR} not found after extract."
        exit 1
    fi

    # antlr4-runtime es el subdirectorio runtime/src/ dentro del arbol
    # principal de antlr4. Tiene su propio CMakeLists.txt en
    # runtime/Cpp/CMakeLists.txt -- no compilamos el arbol completo (que
    # incluye el generador Java), solo la lib C++.
    RUNTIME_DIR="${SRC_DIR}/runtime/Cpp"
    if [[ ! -d "$RUNTIME_DIR" ]]; then
        echo "[ERROR] runtime/Cpp dir not found in ${SRC_DIR}"
        echo "        (this means the tarball layout changed -- check the URL)"
        exit 1
    fi

    ANTLR_BUILD="${TMPDIR}/antlr4-build"
    echo "Configuring antlr4-runtime (cmake) ..."
    if ! cmake -S "$RUNTIME_DIR" -B "$ANTLR_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$ANTLR_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -G "Unix Makefiles"; then
        echo "[ERROR] antlr4-runtime cmake configure failed."
        exit 1
    fi

    echo "Building antlr4-runtime ..."
    if ! cmake --build "$ANTLR_BUILD" --parallel; then
        echo "[ERROR] antlr4-runtime build failed."
        exit 1
    fi

    echo "Installing to ${ANTLR_PREFIX} ..."
    # Si el prefix esta fuera de HOME (ej. /usr/local), cmake --install
    # necesita sudo. Como el default es ~/.local, casi siempre el
    # usuario tiene permiso y no hace falta.
    if ! cmake --install "$ANTLR_BUILD"; then
        echo "[ERROR] cmake --install failed. If ${ANTLR_PREFIX} is outside"
        echo "        your home dir, try running this script with sudo:"
        echo "          sudo bash scripts/install.sh ${ANTLR_PREFIX}"
        exit 1
    fi

    # Verificar que la instalacion dejo algo usable
    if [[ ! -f "${ANTLR_PREFIX}/lib/libantlr4-runtime.a" ]]; then
        echo "[ERROR] libantlr4-runtime.a not found at ${ANTLR_PREFIX}/lib after install."
        echo "        Check the build output above."
        exit 1
    fi
    if [[ ! -f "${ANTLR_PREFIX}/include/antlr4-runtime.h" ]]; then
        echo "[ERROR] antlr4-runtime.h not found at ${ANTLR_PREFIX}/include after install."
        echo "        Check the build output above."
        exit 1
    fi

    echo "[OK] antlr4-runtime ${ANTLR_VERSION} installed to ${ANTLR_PREFIX}"
    echo "     Header: ${ANTLR_PREFIX}/include/antlr4-runtime.h"
    echo "     Lib   : ${ANTLR_PREFIX}/lib/libantlr4-runtime.a"
    echo
fi

# --- 5. ANTLR4 tool jar (build-time only, regenera el parser C++) ----
mkdir -p "$THIRD_PARTY_DIR"
JAR_FOUND=0
shopt -s nullglob
for f in "${THIRD_PARTY_DIR}"/antlr-*-complete.jar; do
    JAR_FOUND=1
    break
done
shopt -u nullglob

if [[ $JAR_FOUND -eq 1 ]]; then
    echo "[OK] ANTLR4 tool jar already present in ${THIRD_PARTY_DIR}/"
else
    echo "Downloading ${ANTLR_JAR_NAME} into ${THIRD_PARTY_DIR}/ ..."
    if curl -fsSL "$ANTLR_JAR_URL" -o "${THIRD_PARTY_DIR}/${ANTLR_JAR_NAME}"; then
        echo "[OK] Saved to ${THIRD_PARTY_DIR}/${ANTLR_JAR_NAME}"
    else
        echo "[WARN] Could not download the ANTLR4 jar automatically."
        echo "       Download it by hand from https://www.antlr.org/download.html"
        echo "       and place the '...-complete.jar' file in ${THIRD_PARTY_DIR}/"
        echo "       (see ${THIRD_PARTY_DIR}/README.md). The build still works"
        echo "       with the stub frontend without it, just not the real one."
    fi
fi
echo

# --- 6. Export variables (this shell only; not persisted) ------------
# A diferencia del install.bat de Windows (que usa setx para persistir
# VCPKG_ROOT en el registro), en Linux no hay un equivalente "persistir
# en nuevas shells" limpio. Lo que SI podemos hacer es documentar las
# variables que el usuario quizas quiera exportar en su .bashrc si uso
# un prefix fuera de los paths del sistema:
#
#   export CMAKE_PREFIX_PATH="${ANTLR_PREFIX}:${CMAKE_PREFIX_PATH:-}"
#
# Pero cmake/FindAntlr4Jar.cmake YA busca en ${CMAKE_PREFIX_PATH} y
# tambien en paths comunes (/usr/local, /home/avalontm/antlr413-install).
# Si el prefix es ~/.local, find_path lo encuentra solo (esta en la
# lista de defaults de CMake). Si el prefix es custom, el usuario tiene
# que setear CMAKE_PREFIX_PATH a mano -- lo decimos aca abajo.
echo "====================================================================="
echo "Done."
echo "  antlr4-runtime : ${ANTLR_PREFIX}"
echo "  antlr4 jar     : ${THIRD_PARTY_DIR}/${ANTLR_JAR_NAME}"
echo "====================================================================="
echo
if [[ "$ANTLR_PREFIX" != "$HOME/.local" && "$ANTLR_PREFIX" != "$HOME/antlr413-install" ]]; then
    echo "[INFO] Since you installed antlr4-runtime to a custom prefix"
    echo "       (${ANTLR_PREFIX}), CMake may not find it by default."
    echo "       Before running build_cli.sh, export:"
    echo "         export CMAKE_PREFIX_PATH=\"${ANTLR_PREFIX}:\${CMAKE_PREFIX_PATH:-}\""
    echo
fi

if [[ $SKIP_BUILD -eq 1 ]]; then
    echo "Skipping build (skipbuild passed). Run build_cli.sh manually when ready."
    exit 0
fi

# --- 7. Correr build_cli.sh ------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ -f "${SCRIPT_DIR}/build_cli.sh" ]]; then
    echo
    echo "Running build_cli.sh ..."
    bash "${SCRIPT_DIR}/build_cli.sh"
    exit $?
else
    echo "[WARN] build_cli.sh not found at ${SCRIPT_DIR}/build_cli.sh -- skipping build."
    exit 0
fi
