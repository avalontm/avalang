#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos movemos a
# la raiz del repo (un nivel arriba) para que las rutas relativas
# sigan funcionando sin importar desde donde se invoque este .sh.
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# Fase 9 (runtime/avapack/README.md) -- contraparte Linux/macOS de
# build_pack_tools.bat. Compila UNA VEZ las herramientas que
# `ava_cli build` (target desktop) necesita para empacar proyectos SIN
# el repo/CMake al lado: avapack_gen + avapack_stub.
#
# A diferencia de build_cli.sh/build_studio.sh (que compilan binarios
# para USAR), esto es un paso de "preparar distribucion" -- se corre
# una sola vez (o cuando cambia avapack/avalang), y el resultado
# (avapack_gen, avapack_stub, libavalang.so, libavalang_ui.so) se copia
# a mano junto a ava_cli -- ahi es donde build_command.cpp los busca
# (GetSelfExecutableDir(), ver runtime/avacli/src/build_command.cpp).
# Sin este paso, `ava_cli build` sigue funcionando igual que antes
# (recompila avapack desde fuente en build_pack/ via CMake) -- este
# script habilita el camino rapido nuevo, no reemplaza al viejo.
#
# Uso:
#   build_pack_tools.sh                build Release, copia a dist_pack_tools/
#   build_pack_tools.sh clean          borra build_pack_tools/ y dist_pack_tools/
#   build_pack_tools.sh ninja          usa Ninja en vez de Unix Makefiles
# =====================================================================

BUILD_DIR="build_pack_tools_linux"
DIST_DIR="dist_pack_tools"
BUILD_TYPE="Release"
USE_NINJA=0

if [[ "${1:-}" == "clean" ]]; then
    [[ -d "$BUILD_DIR" ]] && rm -rf "$BUILD_DIR"
    [[ -d "$DIST_DIR" ]] && rm -rf "$DIST_DIR"
    echo "Done -- ${BUILD_DIR}/ y ${DIST_DIR}/ removidos."
    exit 0
fi
if [[ "${1:-}" == "ninja" ]]; then
    USE_NINJA=1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] cmake no esta en el PATH."
    exit 1
fi

mkdir -p "$BUILD_DIR"

CMAKE_CONFIGURE_ARGS=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DAVA_BUILD_PACK=ON -DAVA_BUILD_CLI=OFF -DAVA_BUILD_STUDIO=OFF -DAVA_BUILD_AVAHOST=OFF)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    echo "Usando CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}"
else
    echo "[INFO] CMAKE_PREFIX_PATH no esta definida -- corre install.sh"
    echo "       primero si avalang necesita el frontend ANTLR real."
fi

echo
if [[ $USE_NINJA -eq 1 ]]; then
    if ! command -v ninja >/dev/null 2>&1; then
        echo "[ERROR] ninja no esta en el PATH pero se pidio 'ninja'."
        exit 1
    fi
    echo "Configurando con Ninja (${BUILD_TYPE}) ..."
    if ! cmake -S . -B "$BUILD_DIR" -G "Ninja" "${CMAKE_CONFIGURE_ARGS[@]}"; then
        echo "[ERROR] cmake (configure) fallo. Ver arriba."
        exit 1
    fi
else
    echo "Configurando con Unix Makefiles (${BUILD_TYPE}) ..."
    if ! cmake -S . -B "$BUILD_DIR" -G "Unix Makefiles" "${CMAKE_CONFIGURE_ARGS[@]}"; then
        echo "[ERROR] cmake (configure) fallo. Ver arriba."
        exit 1
    fi
fi

echo
echo "Compilando avapack_gen + avapack_stub (${BUILD_TYPE}) ..."
if ! cmake --build "$BUILD_DIR" --target avapack_gen --target avapack_stub --config "$BUILD_TYPE" --parallel; then
    echo "[ERROR] el build fallo. Ver arriba."
    exit 1
fi

OUT_DIR="${BUILD_DIR}/runtime/avalang"
if [[ $USE_NINJA -eq 0 ]]; then
    OUT_DIR="${BUILD_DIR}/runtime/avalang/${BUILD_TYPE}"
fi
if [[ ! -d "$OUT_DIR" ]]; then
    OUT_DIR="${BUILD_DIR}/runtime/avalang"
fi

mkdir -p "$DIST_DIR"
cp -f "${OUT_DIR}/avapack_gen" "${DIST_DIR}/" 2>/dev/null || true
cp -f "${OUT_DIR}/avapack_stub" "${DIST_DIR}/" 2>/dev/null || true
cp -f "${OUT_DIR}/libavalang.so" "${DIST_DIR}/" 2>/dev/null || true
cp -f "${OUT_DIR}/libavalang_ui.so" "${DIST_DIR}/" 2>/dev/null || true
cp -f "${OUT_DIR}/libavalang.dylib" "${DIST_DIR}/" 2>/dev/null || true
cp -f "${OUT_DIR}/libavalang_ui.dylib" "${DIST_DIR}/" 2>/dev/null || true

echo
echo "Listo. Copia el contenido de ${DIST_DIR}/ junto a ava_cli para que"
echo "\`ava_cli build --target desktop\` use el camino sin-repo (Fase 9) en vez"
echo "de recompilar avapack desde fuente via CMake en cada build."
exit 0
