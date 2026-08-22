#!/usr/bin/env bash
# Este script vive en scripts/ (ver AVALAND_STRUCT.md); nos
# movemos a la raiz del repo (un nivel arriba) para que las rutas
# relativas (build_linux/, samples/test/main.ava, etc.) sigan
# funcionando sin importar desde donde se invoque este .sh.
#
# Para correrlo:  bash scripts/measure_release.sh
set -euo pipefail
cd "$(dirname "$0")/.."

# =====================================================================
# Fase 8 -- Optimizacion: items 6-9 (medir tamano de .so, memoria de
# VM, startup, rendimiento). Ver avalang_runtime_stl_barekernel_plan.md.
#
# Requiere un build Release ya hecho (ver build.sh). No compila nada
# aqui -- solo busca los binarios ya generados y los mide.
#
# Uso:
#   measure_release.sh                 usa ./build_linux (default de build.sh)
#   measure_release.sh <carpeta>       usa esa carpeta de build en su lugar
#
# Memoria pico: usa /usr/bin/time -v si esta disponible (paquete
# "time" -- no confundir con el builtin de bash del mismo nombre, que
# no reporta memoria). Si no esta instalado, esa parte se omite con un
# aviso en vez de fallar todo el script.
# =====================================================================

BUILD_DIR="${1:-build_linux}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "[ERROR] No existe '$BUILD_DIR'. Corre build.sh primero (build Release)." >&2
    exit 1
fi

echo "Buscando binarios en $BUILD_DIR/ ..."
AVA_CLI="$(find "$BUILD_DIR" -type f -name "ava_cli" | head -n1 || true)"
AVALANG_SO="$(find "$BUILD_DIR" -type f -name "libavalang.so" | head -n1 || true)"
AVALANG_UI_SO="$(find "$BUILD_DIR" -type f -name "libavalang_ui.so" | head -n1 || true)"

if [ -z "$AVA_CLI" ]; then
    echo "[ERROR] No se encontro ava_cli bajo $BUILD_DIR/. Compila con build.sh primero." >&2
    exit 1
fi

echo
echo "====================================================================="
echo "6. Tamano de binarios"
echo "====================================================================="
if [ -n "$AVALANG_SO" ]; then
    printf "libavalang.so     %s bytes  (%s)\n" "$(stat --format=%s "$AVALANG_SO")" "$AVALANG_SO"
else
    echo "libavalang.so     no encontrado (build estatico? ver AVA_BUILD_SHARED)"
fi
if [ -n "$AVALANG_UI_SO" ]; then
    printf "libavalang_ui.so  %s bytes  (%s)\n" "$(stat --format=%s "$AVALANG_UI_SO")" "$AVALANG_UI_SO"
else
    echo "libavalang_ui.so  no encontrado (build estatico o AVA_BUILD_UI=OFF)"
fi
printf "ava_cli           %s bytes  (%s)\n" "$(stat --format=%s "$AVA_CLI")" "$AVA_CLI"

echo
echo "====================================================================="
echo "7/8. Memoria de VM y startup (samples/test/main.ava, 5 corridas)"
echo "====================================================================="
STARTUP_SCRIPT="samples/test/main.ava"
if [ ! -f "$STARTUP_SCRIPT" ]; then
    echo "[WARN] $STARTUP_SCRIPT no existe, se omite esta seccion."
else
    HAVE_GNU_TIME=0
    if command -v /usr/bin/time >/dev/null 2>&1; then
        HAVE_GNU_TIME=1
    fi

    total_ms=0
    times_list=""
    peak_kb=0
    for i in 1 2 3 4 5; do
        start_ns=$(date +%s%N)
        if [ "$HAVE_GNU_TIME" -eq 1 ]; then
            run_out="$(/usr/bin/time -v "$AVA_CLI" "$STARTUP_SCRIPT" 2>&1 >/dev/null || true)"
            run_peak="$(echo "$run_out" | grep "Maximum resident set size" | awk '{print $NF}' || true)"
            if [ -n "$run_peak" ] && [ "$run_peak" -gt "$peak_kb" ] 2>/dev/null; then
                peak_kb=$run_peak
            fi
        else
            "$AVA_CLI" "$STARTUP_SCRIPT" >/dev/null 2>&1 || true
        fi
        end_ns=$(date +%s%N)
        run_ms=$(( (end_ns - start_ns) / 1000000 ))
        total_ms=$(( total_ms + run_ms ))
        times_list="$times_list ${run_ms}ms"
    done
    avg_ms=$(( total_ms / 5 ))
    echo "startup promedio: ${avg_ms} ms (${times_list# })"
    if [ "$HAVE_GNU_TIME" -eq 1 ] && [ "$peak_kb" -gt 0 ]; then
        echo "memoria pico (maximum resident set size): ${peak_kb} KB"
    else
        echo "memoria pico: /usr/bin/time -v no disponible (instala el paquete 'time') -- se omite"
    fi
fi

echo
echo "====================================================================="
echo "9. Rendimiento (scripts/benchmark.ava -- fib(27) recursivo)"
echo "====================================================================="
BENCH_SCRIPT="scripts/benchmark.ava"
if [ ! -f "$BENCH_SCRIPT" ]; then
    echo "[WARN] $BENCH_SCRIPT no existe, se omite esta seccion."
else
    start_ns=$(date +%s%N)
    "$AVA_CLI" "$BENCH_SCRIPT" >/dev/null 2>&1 || true
    end_ns=$(date +%s%N)
    bench_ms=$(( (end_ns - start_ns) / 1000000 ))
    echo "fib(27): ${bench_ms} ms"
fi

echo
echo "====================================================================="
echo "Listo. Estos numeros son una linea base local, no un benchmark"
echo "formal -- comparalos build a build (antes/despues de un cambio de"
echo "optimizacion) en la misma maquina, no contra numeros de otra PC."
echo "====================================================================="
