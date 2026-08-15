# Bugs de AvaLang — Tracker

Tabla de bugs encontrados corriendo scripts `.ava` en Linux (WSL Ubuntu).
Estado: `[ ]` pendiente, `[x]` corregido, `[~]` en progreso, `[-]` no aplicable / decisión de diseño.

## Resumen

| # | Estado | Componente | Descripción | Script que lo reproduce |
|---|--------|------------|-------------|-------------------------|
| 1 | [x] | docs/scripts | Script de prueba usaba `^` (potencia) — en avalang es `**` | `build_linux/tests/t_arith.ava:12` |
| 2 | [x] | compiler/VM | Iterar un dict con `for k in d` devuelve los valores en vez de las claves | `build_linux/tests/t_collections.ava` (bloque "iteracion dict") |
| 3 | [-] | modulos | `import mysql` falla sin `--modules` — no es bug, falta `modules/` al lado del exe | `libraries/mysql/mysql_example.ava` |
| 4 | [-] | FFI nativo | `libmysql` no carga en Linux (falta `.so` nativa) — dependencia externa, no bug del lenguaje | `libraries/mysql/mysql_example.ava` |
| 5 | [-] | UI/parser | `font`/`style`/`animation` directivas no parseadas — son del avastudio/web, no del lenguaje | `samples/web/testproj/app.ava` |
| 6 | [x] | compiler | `CompileMultiAssign` declarado y llamado pero sin implementación → linker error | (preexistente, bloqueaba build) |
| 7 | [x] | compiler | `for` loop en top-level no iteraba — `CompileForDynamic` roto | `build_linux/tests/t_forloop.ava` |
| 8 | [x] | ast_builder | Multi-assign `x, y = 5, 6` aceptado silenciosamente, solo asigna el primero | vm_test.ava sección 7 |
| 9 | [x] | compiler | Multi-assign `a = b, b = a` (swap) asignaba secuencialmente → swap incorrecto | vm_test.ava sección 7 |
| 10 | [x] | ast_builder/cli/frontend | Errores semánticos sin `file:line:col` ni caret — ahora con posición y formato | `build_linux/tests/t_err_ma.ava` |
| 11 | [x] | compiler | `self` no funcionaba dentro de métodos — solo `this` estaba registrado | `build_linux/tests/t_full.ava` sección 1 |
| 12 | [x] | VM | `raise "msg"` no guardaba el valor en `__exception__` → `catch (e)` recibía nil | `build_linux/tests/t_full.ava` sección 3 |
| 13 | [x] | VM | `OpGetIndex` no soportaba strings → `for ch in "abc"` daba nilnilnil | `build_linux/tests/t_full.ava` sección 6 |
| 14 | [x] | compiler | `continue` en `for` saltaba al inicio del loop sin incrementar → bucle infinito | `build_linux/tests/t_full.ava` sección 7 |
| 15 | [x] | compiler | `for v in g` (g es una variable coroutine) no detecta el tipo en compile-time → cae a `CompileForDynamic` | `build_linux/tests/t_corofor.ava` |
| 16 | [x] | compiler | Nested `for` loops en top-level colisionan en temporales globales (`__for_list`, etc.) → loop externo itera solo una vez | `build_linux/tests/t_nested.ava` sections 3-15, `t_nestedfor2.ava` |
| 17 | [x] | compiler | `CompileForIterator` siempre caía a `CompileForDynamic` sin distinguir dict vs list → `for k in {..}` roto en top-level | `build_linux/tests/t_nested.ava` section 9 |
| 18 | [x] | compiler | `CompileForDict` no existía como función separada → dict iteration usaba `CompileForList` con GETINDEX numérico (devolvía valores, no claves) | `build_linux/tests/t_nested.ava` section 9 |
| 19 | [x] | VM | `EQK`/`NEK` con `nil == 0` y `bool == 0` devolvían resultado invertido o incorrecto | `build_linux/tests/t_nested.ava` (comparaciones en sections 10-12) |
| 20 | [x] | VM | `EQ`/`NE` no soportaban comparación profunda de listas y dicts (siempre false) | `build_linux/tests/t_nested.ava` (comparaciones de estructuras) |
| 21 | [x] | VM | `raise` no hacía `pop` del handler de la pila de excepciones → catch anidado colgaba | `build_linux/tests/t_nested.ava` section 14 |
| 22 | [x] | VM | `catch` sin excepción pendiente no saltaba el cuerpo del catch → ejecutaba el body igual | `build_linux/tests/t_nested.ava` section 14 |
| 23 | [x] | compiler | Short-circuit `and`/`or` evaluaba el JMP antes del `TEST` → resultado incorrecto | `build_linux/tests/t_nested.ava` sections 10-12 |
| 24 | [x] | compiler | `self` no se registraba como local en métodos (solo `this`) → `self.x = v` caía a global | `build_linux/tests/t_full.ava` (regresión bug 11) |
| 25 | [x] | compiler | Asignación a upvalue no emitía `SETUPVAL` → closures modificados desde scope externo no persistían | `build_linux/tests/t_advanced.ava` (closures) |
| 26 | [x] | builtins | `list.pop(x)` interpretaba `x` como índice, no como valor a remover → comportamiento incorrecto | manual |
| 27 | [x] | builtins/API | `input(prompt)` no existía como builtin → scripts no podían leer de stdin | manual |
| 28 | [x] | VM | `vm_call_op.cpp`: `frame` referenciaba elemento de `frames_` que se invalidaba tras `push_back` → use-after-free en llamadas nativas | `build_linux/tests/t_advanced.ava` (coroutines anidadas) |
| 29 | [x] | VM | `coroutine.cpp`: `is_coroutine_suspended_` no se reseteaba antes de reanudar → resume() fallaba en segunda llamada | `build_linux/tests/t_coro.ava` |
| 30 | [x] | platform/linux | `LinConsole::ReadLine` era stub (retornaba false) → `input()` fallaba en Linux | manual |
| 31 | [x] | platform/linux | `LinConsole` sin colores ANSI → `SetForegroundColor` era no-op | manual |
| 32 | [x] | compiler | `ast_builder.cpp`: `visitAssignStatement` no lanzaba error para `x, y = 5, 6` (solo tomaba primer target/expr) | `build_linux/tests/t_err_ma.ava` (regresión bug 8) |
| 33 | [x] | frontend | `frontend_antlr.cpp`: errores semánticos con prefijo `"Compilation error: "` duplicado en mensaje | `build_linux/tests/t_err_ma.ava` (regresión bug 10) |
| 34 | [x] | compiler | `CompileMultiAssign` no estaba conectado al dispatcher `CompileStmt` → multi-assign nunca se compilaba | (regresión bug 6) |
| 35 | [x] | VM | `vm_arith.cpp`: concatenación de string con número usaba `NumberToString` directo → no respetaba formato de valor (bool, nil, list) | `build_linux/tests/t_advanced.ava` |
| 36 | [x] | vm_extern | `dlopen` en Linux no probaba sufijos versionados (`libfoo.so.6`) → librerías comunes no cargaban | manual |
| 37 | [x] | compiler | `aug-assign` (`+=`, etc.) a `self.x` no detectaba `self` como this → caía a global | `build_linux/tests/t_full.ava` (regresión bug 11) |
| 38 | [x] | ast_builder | `//=` (integer divide assign) no se reconocía como operador → error de sintaxis | manual |

## Detalle

### Bug 2 — Iteración de dict devuelve valores en vez de claves
- **Síntoma**: `for k in d` recorre los valores (`Ada`, `37`, `AvaLang`), no las claves (`name`, `age`, `lang`).
- **Causa raíz**: `CompileForDynamic` (compiler.cpp) solo distingue `coroutine` vs `list`. Un dict cae al branch `CompileForList`, que itera con `GETINDEX` numérico; `OpGetIndex` (vm_containers.cpp:38-46) sobre un dict con índice numérico devuelve `entries[pos].second` (el valor), no `.first` (la clave).
- **Decisión**: iterar un dict directamente devuelve las **claves** (estilo Python/Lua, consistente con `d[key]`). Para valores usar `d.values()`, para pares `d.items()`.
- **Fix aplicado**: Modificado `OpGetIndex` (vm_containers.cpp) para que dict con índice numérico devuelva `.first` (la clave). Eliminado `CompileForDynamic` (roto) de la ruta del `for`, reemplazado por `CompileForList` directo.
- **Archivos**: `runtime/avalang/src/vm/vm_containers.cpp`, `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 6 — `CompileMultiAssign` no implementado (preexistente)
- **Síntoma**: Linker error `undefined reference to ava::Compiler::CompileMultiAssign`.
- **Causa**: Método declarado en `compiler.h:90`, llamado en `compiler.cpp:751`, pero sin implementación.
- **Fix**: Implementado en compiler.cpp antes de `CompileClass`: evalúa todos los valores a temporales globales primero, luego asigna cada target desde su temporal.
- **Estado**: ✅ corregido y verificado.

### Bug 7 — `for` loop en top-level no itera
- **Síntoma**: `for x in [10,20,30]` en top-level no ejecuta el body (imprime "before for" y "after for" pero no "x = 10").
- **Causa**: `CompileForDynamic` (que hacía type-check en runtime) estaba roto — la lógica de jumps no funcionaba.
- **Fix**: Eliminado `CompileForDynamic` de la ruta del `for`. `CompileForIterator` ahora llama `CompileForList` directamente para listas, `CompileForCoroutine` para coroutines.
- **Estado**: ✅ corregido y verificado.

### Bug 8 — Multi-assign `x, y = 5, 6` aceptado silenciosamente
- **Síntoma**: `x, y = 5, 6` no da error; solo asigna `x = 5` e ignora `y` (queda nil).
- **Causa**: `visitAssignStatement` (ast_builder.cpp) solo toma `target(0)` y `expr(0)`, ignorando targets/exprs adicionales. La gramática define `multiAssignStatement: assignStatement (',' assignStatement)+` donde cada `assignStatement` es `targetList '=' exprList` — la sintaxis correcta es `x = 5, y = 6` (cada asignación con su propio `=`), no `x, y = 5, 6`.
- **Fix**: Añadida validación en `visitAssignStatement` y `visitMultiAssignStatement`: si hay más de 1 target o más de 1 expr en un solo `assignStatement`, lanza `AvaError` con línea y columna.
- **Estado**: ✅ corregido y verificado.

### Bug 9 — Multi-assign swap asignaba secuencialmente
- **Síntoma**: `a = b, b = a` (swap) daba `a=2, b=2` en vez de `a=2, b=1` — porque asignaba secuencialmente (`a = b` primero, luego `b = a` ya modificado).
- **Causa**: `CompileMultiAssign` compilaba cada par (target, value) como un `AssignStmt` individual secuencialmente.
- **Fix**: `CompileMultiAssign` ahora evalúa **todos** los valores a temporales globales (`__ma_tmp_0`, `__ma_tmp_1`, ...) primero, luego asigna cada target desde su temporal — garantizando semántica de evaluación simultánea.
- **Estado**: ✅ corregido y verificado.

### Bug 10 — Errores semánticos sin `file:line:col` ni caret

#### Contexto: cómo funcionan los errores en AvaLang

Hay **dos tipos** de errores de compilación:

1. **Errores de sintaxis** (lexer/parser ANTLR): capturados por `AvaLangErrorListener` en `frontend_antlr.cpp`. El listener recolecta `line`, `column` (1-based) y `message`. Luego `formatError()` construye un string con el formato:
   ```
   error at <file>:<line>:<col>: <message>
       <line> | <source_line>
             ^~~~
   ```
   Este string se pasa como `e.what()` al `CompileError`, con `e.line` y `e.column` seteados.

2. **Errores semánticos** (AST builder / compiler): lanzados con `throw AvaError(mensaje, line, column)`. El frontend los captura en `frontend_antlr.cpp:160` y los re-lanza como `CompileError(mensaje, e.line, e.column, source)`.

El `CompileError` llega a `ava_compile()` (c_api.cpp:281), que guarda `e.line`/`e.column` en `raw_vm->last_error_line/column` y `e.what()` en `out_error`.

El CLI (`ava_cli main.cpp`) lee `out_error` y, si el mensaje **no** empieza con `"error at "` (es decir, no viene pre-formateado por `formatError`), usa `ava_last_error_line()`, `ava_last_error_column()` y `ava_last_error_source()` para formatear el error con `file:line:col` y el caret.

#### Cómo implementar un nuevo error semántico con posición

Usar `AvaError` con `line` y `column` extraídas del contexto ANTLR:

```cpp
#include "../common/ava_error.h"

// En visitAssignStatement (ast_builder.cpp):
if (targets.size() > 1 || exprs.size() > 1) {
    throw AvaError(
        "mensaje en español del error",
        static_cast<int>(ctx->getStart()->getLine()),           // línea 1-based
        static_cast<int>(ctx->getStart()->getCharPositionInLine()) + 1  // columna 1-based
    );
}
```

- `ctx` es el contexto ANTLR (`*Context*`), que tiene `getStart()` → token ANTLR con `getLine()` y `getCharPositionInLine()` (0-based, por eso `+ 1`).
- El mensaje debe estar en **español** (consistente con el resto del lenguaje).
- **No** añadir `"Compilation error: "` como prefijo — el frontend y CLI lo manejan automáticamente.
- El error se propaga: `AvaError` → `CompileError` (frontend) → `out_error` + `last_error_line/col` (c_api) → formato con caret (CLI).

#### Formato de salida esperado

```
error at <archivo>:<línea>:<columna>: <mensaje en español>
    <línea> | <código fuente de esa línea>
             ^
```

#### Archivos involucrados

| Archivo | Rol |
|---------|-----|
| `runtime/avalang/src/common/ava_error.h` | Estructura `AvaError { message, line, column, source }` |
| `runtime/avalang/src/frontend/frontend.h` | `CompileError` hereda de `AvaError` |
| `runtime/avalang/src/frontend/frontend_antlr.cpp` | Captura `AvaError`, la relanza como `CompileError` con `line/col/source`. `formatError()` formatea errores de sintaxis. |
| `runtime/avalang/api/src/c_api.cpp` | `ava_compile()` guarda `e.line/col` en `raw_vm->last_error_*` y `e.what()` en `out_error` |
| `runtime/avacli/src/main.cpp` | Lee `out_error`; si no empieza con `"error at "`, lo formatea con `file:line:col` + caret usando `ava_last_error_*` |
| `runtime/avalang/src/ast/ast_builder.cpp` | Lanzar `AvaError` con `ctx->getStart()->getLine()` y `getCharPositionInLine()` |
| `runtime/avalang/src/compiler/compiler.cpp` | Lanzar `AvaError` con `line` (de `stmt->line` o similar) |

#### Ejemplo de error corregido

```
$ ava_cli test.ava
error at test.ava:3:1: asignacion multiple requiere 'x = v, y = w' (cada asignacion con su propio '='); la forma 'x, y = v, w' no esta soportada
    3 | x, y = 5, 6
      ^
```

### Bug 11 — `self` no funcionaba dentro de métodos
- **Síntoma**: `__init__(name, sound)` con `self.name = name` → `dog.name` es nil.
- **Causa**: `CompileClass` solo registraba `this` como local (registro 0), no `self`. Y los chequeos `name->name == "this"` y `n->name != "this"` en el compilador no incluían `self`.
- **Fix**: 
  1. `CompileClass` ahora también registra `self` como local (mismo registro que `this`).
  2. Reemplazo global en compiler.cpp: `name->name == "this"` → `(name->name == "this" || name->name == "self")` y `n->name != "this"` → `(n->name != "this" && n->name != "self")`.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 12 — `raise` no guardaba el valor en `__exception__`
- **Síntoma**: `raise "mi error"` → `catch (e)` recibía `e = nil`.
- **Causa**: `OpRaise` (vm_exceptions.cpp) solo saltaba al catch_pc pero no llamaba `RaiseException()` para guardar el valor. `OpCatch` verificaba `HasException()` pero no guardaba el valor en `__exception__` ni lo limpiaba.
- **Fix**:
  1. `OpRaise`: ahora llama `vm.RaiseException(frame.registers[in.a])` antes de saltar, y hace `pop` del handler.
  2. `OpCatch`: si hay excepción, la guarda en global `__exception__` y la limpia (`GetAndClearException`). Si NO hay excepción, salta (se salta el cuerpo del catch).
- **Archivos**: `runtime/avalang/src/vm/vm_exceptions.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 13 — `OpGetIndex` no soportaba strings
- **Síntoma**: `for ch in "abc"` iteraba pero `ch` era nil (nilnilnil).
- **Causa**: `OpGetIndex` (vm_containers.cpp) solo manejaba `List` y `Dict`, no `String`. Un string con índice numérico caía al `else` final que asignaba nil.
- **Fix**: Añadido caso `ValueType::String` en `OpGetIndex`: con índice numérico, devuelve un string de 1 carácter (`str->data[pos]`).
- **Archivos**: `runtime/avalang/src/vm/vm_containers.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 14 — `continue` en `for` causaba bucle infinito
- **Síntoma**: `continue` dentro de un `for` loop en top-level causaba bucle infinito.
- **Causa**: `PatchContinueJump` saltaba a `loop_start` (antes del cuerpo), que está ANTES del incremento del índice. Entonces el índice nunca se incrementaba.
- **Fix**: Añadido `continue_target` después del cuerpo (antes del incremento) en `CompileForList` y `CompileForDict` (ambas ramas global y local). `PatchContinueJump` ahora salta a `continue_target` (al incremento), no a `loop_start`.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 15 — `for v in g` no detecta coroutine en variable
- **Síntoma**: `g = coroutine(gen); for v in g` no itera correctamente (cae a `CompileForDynamic`, que estaba roto).
- **Causa**: `DetectIteratorKind` solo detecta coroutines cuando el iterable es directamente `coroutine(...)` (un `CallExpr` con callee `coroutine`), no cuando es una variable (`NameExpr`) que contiene una coroutine en runtime. Para variables, devuelve `List` y el `for` cae a `CompileForDynamic`, que estaba roto (re-evaluaba el iterable múltiples veces, gestión incorrecta de saltos).
- **Fix**: `CompileForDynamic` reescrito completamente para evaluar el iterable **una sola vez** en un global `__for_iter`, computar `is_coro = (type(__for_iter) == "coroutine")` en un registro estable, y usar branches en runtime con `TEST`:
  1. **Init branch** (`TEST is_coro, 0, 1` → c=1: falsy→skip JMP→list init; truthy→exec JMP→coro init): list init crea `__for_len`/`__for_idx`; coro init carga `resume` builtin en `__for_resume`.
  2. **Loop start** (`TEST is_coro, 0` → c=0: truthy→skip JMP→coro loop; falsy→exec JMP→list loop): coro hace `resume(iter)`→`__for_val`, check `!= nil`, extrae `val[0]`; list hace `GETINDEX`.
  3. **Shared body**: `CompileChunk(stmt->body)`.
  4. **Post-body** (`TEST is_coro, 0`): coro JMP al loop start; list incrementa `idx`, check `idx < len`, JMP al loop start.
  5. **Coro exit**: cuando `resume` devuelve nil, JMP al loop exit (jumps recolectados y parchados al final).
  - Breaks parchados al loop exit; continues parchados a `continue_target` (coro loop start).
  - **Crítico**: `Emit(OpCode::TEST, reg, 1)` pone `1` en `b`, no en `c` — deja `c=0` (truthy→skip). Para `c=1` (falsy→skip) se debe usar `Emit(OpCode::TEST, reg, 0, 1)`.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp` (`CompileForDynamic`, líneas 1190-1376).
- **Tests**: `t_corofor.ava` (variable coroutine), `t_corofor2.ava` (literal `coroutine(gen)` — regresión), `t_dynlist.ava` (variable lista vía dynamic path — nuevo).
- **Estado**: ✅ corregido y verificado.

### Bug 16 — Nested `for` loops colisionan en temporales globales
- **Síntoma**: `for outer in range(3) { for inner in range(3) { ... } }` en top-level solo iterna `outer` una vez (el inner loop sobrescribe los temporales del outer).
- **Causa**: `CompileForList`, `CompileForCoroutine`, `CompileForDict` y `CompileForDynamic` usaban nombres globales hardcoded (`__for_list`, `__for_len`, `__for_idx`, etc.) cuando compilaban en top-level (`is_top_level_ == true`). Loops anidados compartían los mismos nombres → el loop interno sobrescribía los del externo.
- **Fix**: Añadido contador `uint32_t for_depth_` al `Compiler`. `CompileForIterator` captura `my_depth = for_depth_++` antes de dispatchar, pasa `depth` a las sub-funciones, y decrementa al terminar. Cada sub-función usa `"__for_list_" + std::to_string(depth)`, etc., garantizando nombres únicos por nivel de anidamiento.
- **Archivos**: `runtime/avalang/src/compiler/compiler.h` (miembro `for_depth_`), `runtime/avalang/src/compiler/compiler.cpp` (`CompileForIterator`, `CompileForList`, `CompileForCoroutine`, `CompileForDict`, `CompileForDynamic` — todas con parámetro `uint32_t depth`).
- **Tests**: `t_nestedfor2.ava` (minimal repro), `t_nested.ava` (15 sections — todas pasan).
- **Estado**: ✅ corregido y verificado.

### Bug 17 — `CompileForIterator` no distinguía dict vs list en top-level
- **Síntoma**: `for k in {..}` en top-level no iteraba correctamente (caía a `CompileForDynamic` que no manejaba dicts).
- **Causa**: `CompileForIterator` solo tenía dos cases: `List` → `CompileForDynamic` y `Coroutine` → `CompileForCoroutine`. Los dicts caían al path de listas, que usaba `GETINDEX` numérico.
- **Fix**: `CompileForIterator` ahora usa `dynamic_cast` para detectar `DictExpr` → `CompileForDict`, `ListExpr`/`StringExpr`/`CallExpr` → `CompileForList`, y solo cae a `CompileForDynamic` para `NameExpr` (variables). Añadido `IteratorKind::Dict` al enum.
- **Archivos**: `runtime/avalang/src/compiler/compiler.h`, `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 18 — `CompileForDict` no existía
- **Síntoma**: Iteración de dict en top-level devolvía valores en vez de claves (caía a `CompileForList` con GETINDEX numérico).
- **Causa**: No existía una función `CompileForDict` dedicada. La iteración de dict usaba el path de listas, donde `GETINDEX` numérico sobre un dict devolvía `entries[pos].second` (el valor).
- **Fix**: Creada `CompileForDict(stmt, depth)` con paths separados para top-level (globals `__for_dict_N`, `__for_keys_N`, `__for_len_N`, `__for_idx_N`) y local (registros). Llama a `dict.keys()`, itera las claves con `GETINDEX` sobre la lista de keys, y asigna cada clave a la variable del loop.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 19 — `EQK`/`NEK` con nil y bool vs número 0
- **Síntoma**: `nil == 0` devolvía false (debería ser true en AvaLang), `true == 0` devolvía true (debería ser false).
- **Causa**: `OpEqK`/`OpNeK` (vm_compare.cpp) no tenían casos especiales para `Bool` vs `Number` ni `Nil` vs `Number`. Caían al branch de string o al fallback.
- **Fix**: Añadidos casos en `OpEqK`: `Rb.type == Bool && Kc.type == Number && Kc.n == 0` → `!Rb.b`; `Rb.type == Nil && Kc.type == Number && Kc.n == 0` → `true`. Invertidos en `OpNeK`.
- **Archivos**: `runtime/avalang/src/vm/vm_compare.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 20 — `EQ`/`NE` no comparaba listas ni dicts
- **Síntoma**: `[1,2] == [1,2]` devolvía false. `{} == {}` devolvía false.
- **Causa**: `OpEq`/`OpNe` (vm_compare.cpp) solo comparaban números, strings y bools. Listas y dicts caían al fallback (siempre false en EQ, siempre true en NE).
- **Fix**: Añadidos casos `List` vs `List` y `Dict` vs `Dict` que llaman a `ValueEquals()` — comparación profunda recursiva implementada en `vm_helpers.cpp`.
- **Archivos**: `runtime/avalang/src/vm/vm_compare.cpp`, `runtime/avalang/src/vm/vm_helpers.cpp` (nueva función `ValueEquals`), `runtime/avalang/src/vm/vm_helpers.h`.
- **Estado**: ✅ corregido y verificado.

### Bug 21 — `raise` no hacía pop del handler de excepciones
- **Síntoma**: `try`/`catch` anidado (try dentro de try) colgaba o ejecutaba el catch equivocado.
- **Causa**: `OpRaise` (vm_exceptions.cpp) saltaba al `catch_pc` pero no removía el handler de `exception_handlers_` → la pila de handlers crecía indefinidamente.
- **Fix**: `OpRaise` ahora hace `exception_handlers_.pop_back()` después de copiar el handler (antes de saltar al catch).
- **Archivos**: `runtime/avalang/src/vm/vm_exceptions.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 22 — `catch` sin excepción ejecutaba el body igual
- **Síntoma**: `try { ... } catch (e) { print(e) }` ejecutaba el catch incluso cuando no había excepción.
- **Causa**: `OpCatch` solo verificaba `HasException()` para decidir si guardar el valor, pero no saltaba el cuerpo del catch si no había excepción.
- **Fix**: `OpCatch` ahora: si hay excepción → la guarda en `__exception__` y la limpia (`GetAndClearException`); si NO hay excepción → salta (JMP) pasado el cuerpo del catch.
- **Archivos**: `runtime/avalang/src/vm/vm_exceptions.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 23 — Short-circuit `and`/`or` evaluaba JMP antes de TEST
- **Síntoma**: `x and y` devolvía resultado incorrecto cuando `x` era falsy.
- **Causa**: En `CompileExpr` para `BinOp::And`, el `jmp_falsy` se calculaba ANTES de emitir `NEK` y `TEST`. El offset del JMP apuntaba al lugar equivocado.
- **Fix**: Movido el cálculo de `jmp_falsy`/`jmp_truthy` a DESPUÉS de emitir `TEST`, antes de `JMP`, para que el offset sea correcto.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 24 — `self` no registrado como local (regresión bug 11)
- **Síntoma**: `self.x = v` dentro de un método caía a asignación global en vez de atributo de instancia.
- **Causa**: Bug 11 se corrigió en `CompileClass` (registrar `self` como local), pero los chequeos `n->name != "this"` en `CompileStmt`, `CompileExprToReg`, y aug-assign no se actualizaron para incluir `self`.
- **Fix**: Reemplazo global de `n->name != "this"` → `(n->name != "this" && n->name != "self")` y `name->name == "this"` → `(name->name == "this" || name->name == "self")` en todos los paths de asignación y aug-assign.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 25 — Asignación a upvalue no emitía `SETUPVAL`
- **Síntoma**: Closures modificados desde scope externo no persistaban el cambio.
- **Causa**: `CompileStmt` y `CompileExprToReg` para `AssignStmt` con `NameExpr` target: si no era local ni top-level, caía a `SETGLOBAL`. No verificaba si era un upvalue.
- **Fix**: Añadido `FindUpvalue(name)` que busca en `proto_->upvalue_descs` con `from_parent_local`. Si encuentra el upvalue, emite `SETUPVAL` en vez de `SETGLOBAL`.
- **Archivos**: `runtime/avalang/src/compiler/compiler.h` (nueva función `FindUpvalue`), `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 26 — `list.pop(x)` interpretaba x como índice
- **Síntoma**: `list.pop(5)` removía el elemento en índice 5, no el valor 5.
- **Causa**: `builtin_lists.cpp` `pop` usaba `args[1].as.n` como índice posicional directo.
- **Fix**: `pop` ahora busca el valor `args[1]` en la lista (comparando `type` y `as.n`) y remueve la primera ocurrencia. Retorna la lista modificada.
- **Archivos**: `runtime/avalang/src/builtins/builtin_lists.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 27 — `input(prompt)` no existía como builtin
- **Síntoma**: Scripts `.ava` no podían leer entrada del usuario.
- **Causa**: No había un builtin `input` registrado.
- **Fix**: Creada `builtin_input` en `builtin_natives.cpp` que llama a `VM::ReadLine(prompt)`. Registrada en `builtin_init.cpp`. `VM::ReadLine` usa `input_sink_` (callback opcional) o `IConsole::ReadLine` del PAL. Añadida `ava_vm_set_input_callback` al C API para que hosts GUI (Ava Studio) puedan enrutar input a su propia UI.
- **Archivos**: `runtime/avalang/src/builtins/builtin_natives.cpp`, `runtime/avalang/src/builtins/builtin_init.cpp`, `runtime/avalang/src/vm/vm.h`, `runtime/avalang/src/vm/vm_core.cpp`, `runtime/avalang/api/include/avalang.h`, `runtime/avalang/api/src/c_api.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 28 — Use-after-free en llamadas nativas (`vm_call_op.cpp`)
- **Síntoma**: Llamadas nativas (C FFI) con coroutines anidadas podían crashear aleatoriamente.
- **Causa**: `frame` era una referencia a `vm.frames_.data()[idx]`. Tras `push_back` (que puede reasignar el buffer de `frames_`), `frame` quedaba colgante. El código usaba `frame.registers[save_a]` después del push_back.
- **Fix**: Guardar `frame_idx_native = &frame - vm.frames_.data()` antes del push_back. Usar `vm.frames_[frame_idx_native].registers[save_a]` después.
- **Archivos**: `runtime/avalang/src/vm/vm_call_op.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 29 — `is_coroutine_suspended_` no se reseteaba al reanudar
- **Síntoma**: `resume()` fallaba en la segunda llamada a una coroutine.
- **Causa**: `coroutine.cpp` no reseteaba `vm.is_coroutine_suspended_` antes de reanudar → el flag quedaba true de la suspensión anterior.
- **Fix**: `vm.is_coroutine_suspended_ = false` al inicio de la reanudación.
- **Archivos**: `runtime/avalang/src/vm/coroutine.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 30 — `LinConsole::ReadLine` era stub
- **Síntoma**: `input()` fallaba en Linux (retornaba string vacío).
- **Causa**: `LinConsole::ReadLine` retornaba `false` (stub no implementado).
- **Fix**: Implementado con `std::fgets` de `stdin`, removiendo `\n` y `\r` finales. Añadido `fflush` en `Write`/`WriteLine`/`WriteError`.
- **Archivos**: `runtime/avalang/platform/linux/LinConsole.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 31 — `LinConsole` sin colores ANSI
- **Síntoma**: `SetForegroundColor` era no-op en Linux.
- **Causa**: `LinConsole` no tenía implementación de colores.
- **Fix**: Añadido `ColorToAnsi()` que mapea `ConsoleColor` a códigos ANSI (`\033[30m`..`\033[37m`, `\033[0m` para default). `SetForegroundColor` y `ResetColor` ahora emiten los códigos.
- **Archivos**: `runtime/avalang/platform/linux/LinConsole.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 32 — `visitAssignStatement` no validaba multi-assign (regresión bug 8)
- **Síntoma**: `x, y = 5, 6` se aceptaba silenciosamente (solo asignaba `x`).
- **Causa**: El fix del bug 8 se perdió o no se aplicó completamente.
- **Fix**: `visitAssignStatement` ahora extrae todos los targets y exprs. Si `targets.size() > 1 || exprs.size() > 1`, lanza `AvaError` con `ctx->getStart()->getLine()` y `getCharPositionInLine()`.
- **Archivos**: `runtime/avalang/src/ast/ast_builder.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 33 — Prefijo `"Compilation error: "` duplicado (regresión bug 10)
- **Síntoma**: Errores semánticos mostraban `"Compilation error: error at file:line:col: msg"` (doble prefijo).
- **Causa**: `frontend_antlr.cpp` añadía `"Compilation error: "` al mensaje antes de lanzar `CompileError`, y el CLI también lo formateaba.
- **Fix**: Removido el prefijo `"Compilation error: "` de `frontend_antlr.cpp` — el CLI ya lo maneja con `file:line:col`.
- **Archivos**: `runtime/avalang/src/frontend/frontend_antlr.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 34 — `CompileMultiAssign` no conectado al dispatcher (regresión bug 6)
- **Síntoma**: Multi-assign `a = b, b = a` (swap) no funcionaba.
- **Causa**: `CompileMultiAssign` existía pero `CompileStmt` no tenía el `dynamic_cast<MultiAssignStmt*>` para dispatcharla.
- **Fix**: Añadido `if (auto* ma = dynamic_cast<MultiAssignStmt*>(stmt.get())) { CompileMultiAssign(ma); return; }` en `CompileStmt`. Añadido `MultiAssignStmt` a `ast.h`.
- **Archivos**: `runtime/avalang/src/ast/ast.h`, `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 35 — Concatenación string + número no respetaba formato de valor
- **Síntoma**: `"x" + true` producía `"x1"` en vez de `"xtrue"`. `"x" + nil` producía `"x0"`.
- **Causa**: `vm_arith.cpp` usaba `NumberToString(Rb.n)` para el operando no-string, que solo funciona para números.
- **Fix**: Reemplazado con `ValueToString(Rb)` / `ValueToString(Rc)` — nueva función en `vm_helpers.cpp` que convierte nil→"nil", bool→"true"/"false", number→NumberToString, string→data, list→"[...]", dict→"{...}".
- **Archivos**: `runtime/avalang/src/vm/vm_arith.cpp`, `runtime/avalang/src/vm/vm_helpers.cpp`, `runtime/avalang/src/vm/vm_helpers.h`.
- **Estado**: ✅ corregido y verificado.

### Bug 36 — `dlopen` en Linux no probaba sufijos versionados
- **Síntoma**: `import mysql` fallaba en Linux aunque `libmysqlclient` estaba instalado.
- **Causa**: `dlopen("libmysqlclient.so")` falla en muchas distros porque `.so` es un linker script (texto), no un ELF. El ELF real tiene versión (`libmysqlclient.so.6`).
- **Fix**: `vm_extern.cpp` ahora prueba sufijos versionados comunes (`.so.6`, `.so.5`, `.so.4`, etc.) como último recurso.
- **Archivos**: `runtime/avalang/src/vm/vm_extern.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 37 — Aug-assign a `self.x` no detectaba self (regresión bug 11)
- **Síntoma**: `self.x += 1` dentro de un método caía a global.
- **Causa**: Los paths de aug-assign en `CompileStmt` y `CompileExprToReg` no incluían `self` en el chequeo `n->name != "this"`.
- **Fix**: Mismo patrón que bug 24: `(n->name != "this" && n->name != "self")` en todos los paths de aug-assign.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 38 — `//=` no se reconocía como operador
- **Síntoma**: `x //= 2` daba error de sintaxis.
- **Causa**: `ast_builder.cpp` `BinOpFromText` no tenía caso para `//=` → `BinOp::IDiv`.
- **Fix**: Añadido `if (op == "//=") return BinOp::IDiv;` en `BinOpFromText`.
- **Archivos**: `runtime/avalang/src/ast/ast_builder.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 39 — `yield` era statement, no expresión (no se podía capturar el valor de `resume()`)
- **Síntoma**: `x = yield v` no era válido (yield solo existía como `yieldStatement` suelto); tampoco se podía anidar en una expresión (`f(yield v)`, `1 + (yield v)`).
- **Causa**: La gramática tenía `yieldStatement: 'yield' exprList?` como alternativa de `smallStatement`, separada de `primary`. El AST tenía `YieldStmt : StmtNode` en vez de un nodo de expresión.
- **Fix**: Movido `yield` de `yieldStatement` a una alternativa de `primary` (`'yield' exprList? # yieldAtom`) en `AvaLang.g4`, y regenerado el parser ANTLR. `YieldStmt` → `YieldExpr : ExprNode` en `ast.h`. `ast_builder.cpp` ahora tiene `visitYieldAtom` en vez de `visitYieldStatement`, despachado desde `exprFromAny`. El compilador movió la lógica de `CompileStmt`/`CompileYield(YieldStmt*)` a un caso dentro de `CompileExpr`.
- **Archivos**: `runtime/avalang/grammar/AvaLang.g4`, `runtime/avalang/src/ast/ast.h`, `runtime/avalang/src/ast/ast_builder.h`, `runtime/avalang/src/ast/ast_builder.cpp`, `runtime/avalang/src/compiler/compiler.h`, `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 40 — Registro base de `yield` hardcodeado a 0 (no componía como sub-expresión)
- **Síntoma**: Al implementar `yield` como expresión, empacar los valores yielded usaba `Emit(OpCode::YIELD, 0, count)` con el registro 0 fijo — chocaba con lo que ya viviera ahí (ej. `this`/`self` dentro de un método) al usar `yield` como sub-expresión.
- **Fix**: El registro base ahora se asigna dinámicamente con `AllocReg()`, mismo patrón que usa `CallExpr` para empacar argumentos.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 41 — `resume(co, valor)` siempre escribía en `frames_[0]` al reanudar
- **Síntoma**: `x = yield v` nunca capturaba el valor pasado a `resume()` — quedaba como el valor yielded original, no el de resume.
- **Causa**: La rama `ValueType::Coroutine` de `VM::Call()` en `vm_call.cpp` escribía los argumentos de `resume()` siempre en `frames_[0]`, sin importar en qué frame (posiblemente anidado) ocurrió el `yield`.
- **Fix**: Al reanudar una coroutine ya suspendida, se recupera la instrucción `YIELD` que la puso a dormir (leyendo `pc - 1` del frame en el tope de la pila de la coroutine) y se escribe el valor de `resume()` exactamente en el registro que esa instrucción expone como resultado (empacado en lista si son más de un valor, igual que hace `yield`).
- **Archivos**: `runtime/avalang/src/vm/vm_call.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 42 — Lambdas (`func() ... end`) sin `return` explícito siempre devolvían `nil`
- **Síntoma**: `f = func() 1 + 2 end; f()` devolvía `nil` en vez de `3`. Las funciones con nombre (`func f() ... end`) sí devolvían el valor de la última expresión suelta; las lambdas no.
- **Causa**: En el compile de `LambdaExpr`, se emitía `sub.Emit(OpCode::RETURN);` sin operandos `a`/`b`, lo que siempre caía en la rama `b=0` → `Value::Nil()`, ignorando `sub.result_reg_` (que `CompileChunk` sí deja seteado cuando el último statement del cuerpo es una expresión suelta).
- **Fix**: Igual convención que `CompileFunctionDecl` para funciones con nombre: `uint8_t ret_a = sub.result_reg_ > 0 ? sub.result_reg_ : 0; uint8_t ret_b = sub.result_reg_ > 0 ? 1 : 0; sub.Emit(OpCode::RETURN, ret_a, ret_b);`. Las short lambdas (`(x) => expr`) no sufrían el bug porque ya envuelven su cuerpo en un `ReturnStmt` explícito — ambos caminos comparten la misma `LambdaExpr` compilada, así que el fix cubre los dos casos sin tocar el short-lambda path.
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 43 — `list_length` registrado apuntando a `builtin_str_length` en vez de `builtin_list_length`
- **Síntoma**: `[1,2,3].length()` devolvía `nil` en vez de `3`, aunque la función global `len(lst)` sí funcionaba bien.
- **Causa**: `builtin_registry.cpp` registraba el método `"list_length"` apuntando a `builtin_str_length` (que solo maneja `AVA_STRING` y devuelve `nil` para cualquier otro tipo) — copy-paste error. La función correcta `builtin_list_length` (que sí maneja `AVA_LIST` con `ava_list_length`) ya existía en `builtin_lists.cpp` pero nunca se usaba.
- **Fix**: Cambiado el registro a `raw_vm->RegisterBuiltinMethod("list_length", builtin_list_length, nullptr);`.
- **Archivos**: `runtime/avalang/src/builtins/builtin_registry.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 44 — Indexado simple con índice negativo (`lst[-1]`) fallaba, a diferencia del slicing negativo
- **Síntoma**: `lst[-1]` lanzaba `runtime error: list index: index must not be negative, got -1`, mientras que `lst[-3:]` (slicing) sí funcionaba con índices negativos. Mismo problema en `s[-1]` (strings), `d[-1]` (dict por posición), y en asignación `lst[-1] = v`.
- **Causa**: `OpGetIndex`/`OpSetIndex` en `vm_containers.cpp` llamaban a `ValidateIntegerIndex` directo sobre el índice crudo, sin normalizar negativos. `builtin_slice` (usado para `x[a:b:c]`) sí hacía `start += len` para negativos, pero esa normalización nunca se aplicó al indexado de un solo elemento.
- **Fix**: Añadida `NormalizeIndex(double n, size_t len)` en `vm_containers.cpp`, que aplica `n += len` cuando `n < 0`, aplicada antes de `ValidateIntegerIndex` en los tres tipos (`List`, `Dict` por posición, `String`) tanto en lectura (`OpGetIndex`) como en escritura (`OpSetIndex`).
- **Archivos**: `runtime/avalang/src/vm/vm_containers.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 45 — `%` (módulo) usaba `fmod` truncado, inconsistente con `//` (floor division)
- **Síntoma**: `(-7 // 3) * 3 + (-7 % 3)` daba `-10` en vez de `-7` — la identidad matemática que debe cumplir división entera + módulo estaba rota. `-7 % 3` daba `-1` (estilo C) en vez de `2` (floor-mod, consistente con `-7 // 3 == -3`).
- **Causa**: `OpIdiv` en `vm_arith.cpp` usa `std::floor(a/b)` (floor division, estilo Python), pero `OpMod` usaba `std::fmod(a,b)` (resto truncado, estilo C, signo sigue al dividendo) — dos convenciones de redondeo distintas que no son consistentes entre sí.
- **Fix**: `OpMod` ahora ajusta el resultado de `fmod` sumando el divisor cuando el signo del resultado no coincide con el signo del divisor, recuperando el floor-mod correcto y restaurando la identidad `(a // b) * b + (a % b) == a`.
- **Archivos**: `runtime/avalang/src/vm/vm_arith.cpp`.
- **Estado**: ✅ corregido y verificado.

### Bug 46 — Slicing con step negativo y bounds por defecto (`x[::-1]`) devolvía vacío
- **Síntoma**: `s[::-1]` (reversa completa de string) y `lst[::-1]` (reversa completa de lista) devolvían `""`/`[]` en vez de la secuencia invertida. También fallaba con solo `start` explícito y step negativo (ej. `s[9::-1]`).
- **Causa**: `builtin_slice` en `builtin_natives.cpp` usaba defaults fijos `start=0, end=len` sin importar el signo de `step`. Con `step=-1` y esos defaults, el loop quedaba `for (i=0; i>len; i-=1)` — la condición es falsa desde la primera iteración (`0 > len` nunca es cierto), así que el resultado siempre era vacío.
- **Fix**: Reescrito `builtin_slice` para calcular los bounds por defecto según el signo de `step` (estilo Python): con step positivo, `start=0, end=len` (como antes); con step negativo, `start=len-1, end=-1` (sentinel que representa "hasta antes del índice 0 inclusive", que **no** se normaliza sumando `len` como sí se hace con un valor negativo explícito del usuario — de lo contrario el sentinel se convertiría en `len-1` y reproduciría el mismo bug). Aplicado igual para listas y strings.
- **Archivos**: `runtime/avalang/src/builtins/builtin_natives.cpp`.
- **Estado**: ✅ corregido y verificado.

> **Nota de sesión (re-aplicación tras reset de filesystem):** al retomar este repo desde un zip, los bugs #43, #44 y #45 aparecían documentados como corregidos aquí, pero el fix ya NO estaba en el código fuente (`list_length` volvía a apuntar a `builtin_str_length`, `OpGetIndex`/`OpSetIndex` volvían a usar `ValidateIntegerIndex` sin `NormalizeIndex`, y `OpMod` volvía a usar `fmod` truncado sin el ajuste floor-mod) — la doc sobrevivió al reset pero el código no. Se re-aplicaron los tres fixes exactamente como se describen arriba y se re-verificaron con scripts .ava reales (index negativo en list/dict/string tanto en lectura como en asignación, `-7 % 3 == 2`, identidad `(a//b)*b + a%b == a`, `[1,2,3].length()`). El bug de call-clobbering (registro del callee sobrescrito por el valor de retorno) y el de lambdas sin `return` explícito sí sobrevivieron al reset y siguen andando bien.

### Bug 47 — Funciones anidadas *con nombre* (`func inc() ... end` dentro de otra función) no capturaban upvalues del scope padre
- **Síntoma**: Un closure hecho con función anónima (`inc = func() ... end` o `(x) => ...`) capturaba correctamente variables del scope padre entre llamadas sucesivas, pero la misma lógica escrita como función anidada *con nombre* (`func inc() ... end`) no — cada llamada operaba sobre una variable nueva en vez de compartir la del padre. Repro: un `make_counter()` que devuelve `inc` daba `1, 1, 1` en vez de `1, 2, 3`.
- **Causa**: `Compiler::CompileFunc` (usado para cualquier `FuncDef`, sea top-level o anidado) nunca poblaba `sub.parent_locals_` ni `sub.proto_->upvalue_descs` con las locals del compiler padre — a diferencia del caso `LambdaExpr` en `CompileExpr`, que sí lo hace. Sin esa info, cualquier nombre no encontrado como local/param dentro de la función anidada no resolvía como upvalue (`FindUpvalue` siempre devolvía -1), y `AssignStmt` caía en la rama de "auto-declarar variable local nueva" en vez de usar `SETUPVAL`.
- **Fix**: Se agregó a `CompileFunc` el mismo loop que ya usa `LambdaExpr` (itera `locals_` del compiler padre, excluyendo `this`, y llena `parent_locals_`/`upvalue_descs` de `sub`), y se ajustó `sub.proto_->num_registers` para reservar espacio también para esos upvalues (mismo cálculo `std::max(max_reg_+1, next_reg_)` que ya usa `LambdaExpr`).
- **Archivos**: `runtime/avalang/src/compiler/compiler.cpp`.
- **Estado**: ✅ corregido y verificado.

## Tests creados

| Script | Cobertura | Estado |
|--------|-----------|--------|
| `build_linux/tests/t_arith.ava` | Aritmética, potencia, división entera | ✅ |
| `build_linux/tests/t_collections.ava` | Listas, dicts, range, slice, iteración | ✅ |
| `build_linux/tests/t_forloop.ava` | For loop básico en top-level | ✅ |
| `build_linux/tests/t_forrange.ava` | For con range() | ✅ |
| `build_linux/tests/t_dictiter.ava` | Iteración de dict (bug 2) | ✅ |
| `build_linux/tests/t_continue.ava` | Continue en for loop (bug 14) | ✅ |
| `build_linux/tests/t_err_ma.ava` | Error de multi-assign con posición (bug 8/10) | ✅ |
| `build_linux/tests/t_err_syntax.ava` | Error de sintaxis con posición | ✅ |
| `build_linux/tests/t_full.ava` | Clases, f-strings, excepciones, lambdas, closures, strings, for-string, continue, break | ✅ |
| `build_linux/tests/t_advanced.ava` | Coroutines, nested func, recursion, while, aug-assign, slices, list/dict methods | ✅ |
| `build_linux/tests/t_coro.ava` | Coroutine con resume manual | ✅ |
| `build_linux/tests/t_corofor.ava` | `for v in g` con variable coroutine (bug 15) | ✅ |
| `build_linux/tests/t_corofor2.ava` | `for v in coroutine(gen)` literal — regresión bug 15 | ✅ |
| `build_linux/tests/t_dynlist.ava` | `for v in lst` con variable lista vía dynamic path (bug 15) | ✅ |
| `build_linux/tests/t_nested.ava` | Nested control flow: while+if, for+while, for-in-for, break/continue, coro-in-while, try-in-for-in-while, 15 sections (bugs 16-25) | ✅ |
| `build_linux/tests/t_nestedfor2.ava` | Minimal nested for repro (bug 16) | ✅ |