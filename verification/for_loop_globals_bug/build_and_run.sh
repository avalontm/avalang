#!/usr/bin/env bash
# Reconstruye y corre el harness de verificacion contra la version de
# compiler.cpp que este repo tenga en runtime/avalang/src/compiler/ en
# el momento de correr esto -- asi sirve tanto para la evidencia "antes
# del fix" (Fase 1) como para la confirmacion "despues del fix" (Fase 4),
# sin tocar el script.
#
# Requiere: los .o pre-compilados en runtime/avalang/*.o (Linux x86_64,
# ver NOTA_BUILD.md en la raiz del repo si no estan). No requiere
# ANTLR4/vcpkg -- ver README.md de esta carpeta.
set -euo pipefail
cd "$(dirname "$0")"
AVALANG_SRC="../../runtime/avalang/src"
AVALANG_ROOT="../../runtime/avalang"
OUT="${1:-/tmp/ava_verify}"
mkdir -p "$OUT"

echo "== 1/3: compilando compiler.cpp actual =="
g++ -std=c++20 -I "$AVALANG_SRC" -c "$AVALANG_SRC/compiler/compiler.cpp" -o "$OUT/compiler.o"

echo "== 2/3: compilando harness =="
SRCS="repro_recursion repro_case3_nested_call repro_case3_same_var_local repro_coroutine_nested repro_coroutine_simple repro_coroutine_local repro_coroutine_depth1"
for s in $SRCS; do
    g++ -std=c++20 -I . -I "$AVALANG_SRC" -I "$AVALANG_ROOT" -I "$AVALANG_ROOT/api/include" \
        -c "$s.cpp" -o "$OUT/$s.o"
done
g++ -std=c++20 -I "$AVALANG_SRC" -I "$AVALANG_ROOT" -I "$AVALANG_ROOT/api/include" \
    -c abi_shims.cpp -o "$OUT/abi_shims.o"

echo "== 3/3: linkeando contra los .o pre-compilados de runtime/avalang =="
OBJS=$(ls "$AVALANG_ROOT"/*.o | grep -v c_api.o | grep -v compiler.o | grep -v denter.o)

for s in $SRCS; do
    g++ -std=c++20 "$OUT/$s.o" "$OUT/abi_shims.o" "$OUT/compiler.o" $OBJS \
        -o "$OUT/$s" -lpthread
done

echo
echo "=== Casos 1 y 2 (seccion 6 del plan, tal cual -- no disparan el bug, ver README) ==="
"$OUT/repro_recursion"
echo
echo "=== Caso 3 (recursion DENTRO del for -- disparador real, lista) ==="
"$OUT/repro_case3_nested_call"
echo
echo "=== Caso 3 (mismo nombre de variable, for anidados) EN SCOPE LOCAL (4.5) ==="
"$OUT/repro_case3_same_var_local"
echo
echo "=== Coroutine standalone top-level (sin recursion, referencia) ==="
"$OUT/repro_coroutine_simple"
echo
echo "=== Coroutine local dentro de funcion (sin recursion) ==="
"$OUT/repro_coroutine_local"
echo
echo "=== Coroutine + recursion profundidad 1 (walk(1), sin 2da coroutine anidada) ==="
"$OUT/repro_coroutine_depth1"
echo
echo "=== Coroutine anidada (recursion DENTRO del for-coroutine, walk(2)) ==="
"$OUT/repro_coroutine_nested"
