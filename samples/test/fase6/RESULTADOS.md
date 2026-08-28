# Fase 6 -- resultados reales contra ava_cli compilado

Verificado corriendo el binario real (antlr4-runtime 4.13.2 compilado desde
fuente + avalang + ava_cli, sin AvaStudio/avapack), no solo `g++ -fsyntax-only`.

| archivo               | esperado                        | resultado real                                                      | exit |
|-----------------------|----------------------------------|----------------------------------------------------------------------|------|
| a_syntax_error.ava     | falla en compilación (sintaxis) | `extraneous input '\n    ' expecting {')', NAME}` / `missing ')'`   | 1    |
| b_call_typo.ava        | falla en compilación (Fase 2)   | `function 'saludar_typo' is not defined`                             | 1    |
| c_builtin_call.ava     | compila y corre limpio          | imprime `3`, `[1, 2, 3]`, `42`                                       | 0    |
| d_method_typo.ava      | falla en compilación (Fase 3)   | `method 'saludar_typo' is not defined on class 'Persona'`            | 1    |
| e_import_call.ava      | compila y corre limpio (Fase 4) | imprime las 2 líneas, incluida la que usa `Console` traído por `import system` | 0 |

Los tres casos que deben fallar (a, b, d) no imprimen NADA por stdout --
confirma que revientan en el paso de compilación, antes de que `ava_run()`
ejecute una sola línea (tal como pide la Fase 6 del plan).

## Cómo reproducir
```
cd runtime/avalang && cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DAVA_BUILD_UI=OFF -DAVA_ENABLE_LTO=OFF -DAVA_BUILD_CLI=ON
cmake --build build --target ava_cli -j2
# requiere antlr4-runtime 4.13.2 compilado con -fPIC (el de apt es 4.10,
# incompatible -- ver PLAN_VALIDACION_ESTATICA.md / historial del chat)
LD_LIBRARY_PATH=build/runtime/avalang build/runtime/avalang/ava_cli samples/test/fase6/<archivo>.ava
```
