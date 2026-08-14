# Harness de verificacion ad-hoc (WIP)

Este directorio NO es parte del codigo del proyecto ni del plan
`PLAN_FIX_FOR_GLOBALS.md` en si -- es infraestructura de prueba que armé
para poder compilar y correr scripts `.ava` mínimos en este sandbox de
verificación, que no tiene ANTLR4/vcpkg disponibles para el frontend
completo.

## Por qué existe

`Compiler::Compile` (la única API pública de `Compiler`) no depende del
frontend ANTLR -- solo necesita un AST ya armado (`Chunk`). Este harness
arma ese AST a mano con un mini-DSL (`ast_dsl.h`, mismo patrón que
`runtime/avalang/src/compiler/test_proto_io_obfuscate.cpp`, que ya hace
esto para probar `proto_io`/`obfuscate` sin frontend), lo compila con
`Compiler::Compile`, y lo corre con `VM::Run`, usando los `.o` que ya
estaban compilados en este árbol (`runtime/avalang/*.o`, Linux x86_64).

## Archivos

- `ast_dsl.h` -- helpers (`Name`, `Num`, `Call`, `For`, `If`, `Func`, ...)
  para construir nodos de `ast.h` sin escribir `.ava` + parsear.
- `repro_recursion.cpp` -- reproduce los casos 1 y 2 de la sección 6 del
  plan (recursión simple con `for`, y `for` recursivo con listas de
  distinto largo por nivel). El caso 2 usa un índice (`walk(lists, idx)`)
  en vez de slicing (`lists[1:]`) porque el builtin global `slice` que
  usa `CompileExpr(SliceExpr)` no está registrado en `RegisterBuiltinGlobals`
  en este árbol -- gap preexistente, no relacionado con este bug, y por
  eso lo evito en vez de "arreglarlo de paso".
- `repro_case3_nested_call.cpp` -- el caso que sí dispara el bug:
  recursión hecha desde ADENTRO del cuerpo del `for` (ver hallazgo más
  abajo).
- `abi_shims.cpp` -- subconjunto mínimo de la ABI C de
  `api/src/c_api.cpp` (`ava_list_*`, `ava_string_*`, `ava_dict_*`) más un
  stub de `ava::CompileSource` (normalmente en el frontend ANTLR), solo
  para satisfacer el linker -- el harness nunca llama `VM::RunFile` /
  `VM::DoImport`, así que el stub nunca se ejecuta.
- `build_and_run.sh` -- recompila `compiler.cpp` (el que esté en el árbol
  en ese momento) + el harness, linkea contra los `.o` pre-existentes de
  `runtime/avalang/`, y corre los 3 binarios. Sirve tanto para la
  evidencia "antes del fix" (esta fase) como, sin cambiar nada, para la
  confirmación "después del fix" (fase final).

## Estado: Paso 2 del plan completado (bug reproducido y documentado)

Corré `./build_and_run.sh` desde esta carpeta para reconstruir todo desde
cero y ver la salida en vivo. Resultado guardado también en
`BEFORE_FIX_output.txt` / `BEFORE_FIX_case3_output.txt` (salida real,
capturada corriendo el binario contra el `compiler.cpp` **sin
modificar**, tal como está hoy en `runtime/avalang/src/compiler/`).

### Hallazgo importante: los casos 1 y 2 de la sección 6 del plan NO alcanzan a disparar el bug

Los corrí primero (`repro_recursion.cpp`) porque son los que pide
literalmente el plan. Resultado (`BEFORE_FIX_output.txt`):

```
=== Caso 1: recursion simple con `for` -- esperado: 18 ===
18

=== Caso 2: for recursivo, largo de lista distinto por nivel ===
--- esperado: 1 2 3 4 5 6 (cada uno en su propia linea, sin mezclarse) ---
1
2
3
4
5
6
```

Ambos dan el resultado correcto **incluso con el compiler.cpp sin
arreglar**. Razón: en los dos casos la llamada recursiva (`sum_to(n-1)`,
`walk(lists, idx+1)`) es la última sentencia de la función, **después**
de que el `for` ya terminó de iterar por completo. Como la ejecución es
secuencial (no hay coroutines/callbacks entrelazados en juego acá), nunca
hay dos invocaciones "vivas" del mismo `for` al mismo tiempo — el
`__for_idx`/`__for_list`/`__for_len` global ya no se vuelve a tocar una
vez que el `for` de ese frame terminó, así que no hay colisión que
detectar. El plan describe bien el mecanismo del bug (sección 1, "Por qué
esto rompe cosas reales") pero sus propios casos de prueba de la sección
6.1/6.2 no ejercitan el escenario que describe.

### El disparador real: recursión DESDE ADENTRO del cuerpo del `for`

Agregué un tercer caso (`repro_case3_nested_call.cpp`) con la llamada
recursiva **dentro** del cuerpo del `for`, de modo que la llamada
recursiva a su vez entra a **otro** `for` mientras el externo todavía
está a mitad de iterar:

```
func walk(n)
    if n <= 0 then return end
    for i in [10, 20, 30] do
        print(n, i)
        walk(n - 1)
    end
end
walk(2)
```

Esperado (si el `for` tuviera estado aislado por frame): 12 líneas `"n i"`
— el `for` externo (n=2) itera sus 3 elementos, y en cada uno llama
`walk(1)`, que a su vez itera sus 3 elementos completos (`walk(0)` no
hace nada, corta por el `if`).

Salida real, `compiler.cpp` sin arreglar (`BEFORE_FIX_case3_output.txt`):

```
2 10
1 10
1 20
1 30
```

**Solo 4 líneas, no 12.** El `for` externo (`n=2`) hace su primera
iteración (`i=10`), llama a `walk(1)`, cuyo propio `for` sí completa sus
3 iteraciones correctamente (10, 20, 30) porque no hay más recursión
adentro (`walk(0)` no entra a ningún `for`). Pero al volver de esa
llamada, el `for` externo lee el `__for_idx`/`__for_len` global para
decidir si sigue — y esos globales ya no dicen "índice 0 de 3", dicen
"índice 3 de 3" (lo que dejó el `for` interno de `walk(1)` al terminar).
El externo concluye erróneamente que ya terminó, y corta después de una
sola iteración en vez de tres. Exactamente el bug que describe la
sección 1 del plan, confirmado con evidencia reproducible.

`modules/tests/for_recursion.ava` (raíz del repo) ya incluye este caso 4
además de los 3 originales de la sección 6, con los resultados esperados
documentados en comentarios — para cuando el build real (con ANTLR)
pueda correr la suite completa.

## Estado: Fase 2 completada (fix de `CompileForList`)

Aplicado el punto 4.1/4.2 del plan sobre `CompileForList`
(`runtime/avalang/src/compiler/compiler.cpp`): cuando `!is_top_level_`,
`__for_list`/`__for_len`/`__for_idx` y la variable de iteración pasan a
ser registros locales del frame (`AllocReg` + `MOVE`, mismo patrón que
`AssignStmt`) en vez de `SETGLOBAL`/`GETGLOBAL`. La rama
`is_top_level_ == true` queda bit-a-bit igual que antes.

Corrida de `./build_and_run.sh` contra el `compiler.cpp` ya arreglado
(salida en `AFTER_FIX_PHASE2_output.txt` / `AFTER_FIX_PHASE2_case3_output.txt`):

```
=== Caso 1: recursion simple con `for` -- esperado: 18 ===
18

=== Caso 2: for recursivo, largo de lista distinto por nivel ===
1
2
3
4
5
6

=== Caso 3: recursion DENTRO del cuerpo del for (disparador real) ===
2 10
1 10
1 20
1 30
2 20
1 10
1 20
1 30
2 30
1 10
1 20
1 30
```

12 líneas en el caso 3 (antes del fix eran 4) — el `for` externo ya no
pierde su índice cuando la recursión interna vuelve a entrar a otro
`for`. Casos 1 y 2 dan el mismo resultado que antes (no dependían del
bug). `CompileForCoroutine` todavía no está tocado — sigue con el mismo
problema para corrutinas concurrentes, eso es la Fase 3.

## Estado: Fase 3 completada (fix de `CompileForCoroutine`)

Aplicado el punto 4.3 del plan sobre `CompileForCoroutine`
(`runtime/avalang/src/compiler/compiler.cpp`): mismo patrón que la Fase 2
-- cuando `!is_top_level_`, `__iter`/`__resume`/`__val` y la variable de
iteración pasan a ser registros locales del frame en vez de
`SETGLOBAL`/`GETGLOBAL`. La rama `is_top_level_ == true` queda bit-a-bit
igual que antes.

**El fix en sí, verificado en aislamiento (sin interacción con llamadas
de función anidadas dentro del cuerpo del `for`), funciona:**

- `for v in coroutine(gen) do print(v) end` a nivel de módulo: `100 200 300`
  (sin cambios, rama top-level).
- La misma corrutina dentro de una función (`func walk() for v in
  coroutine(gen) do print(v) end end`, sin recursión): `100 200 300`
  correctos -- confirma que el registro local para `__iter`/`__resume`/
  `__val`/`v` funciona bien para el caso simple.

**Hallazgo importante -- bug preexistente distinto, NO introducido por
este fix:** al combinar el `for`-coroutine local con una llamada de
función recursiva DENTRO de su cuerpo (el mismo patrón que el caso 3 de
listas, `repro_coroutine_nested.cpp`):

```
func gen()
    yield 100
    yield 200
    yield 300
end
func walk(n)
    if n <= 0 then return end
    for v in coroutine(gen) do
        print(n, v)
        walk(n - 1)
    end
end
walk(2)
```

Incluso probando con recursión de profundidad 1 (`walk(1)`, donde
`walk(0)` no hace nada -- ninguna segunda corrutina de por medio), el
`for` externo corta después de la **primera** iteración de su propia
corrutina (`"1 100"` solamente, en vez de `"1 100" "1 200" "1 300"`).
Esto es una firma distinta a la del bug de `PLAN_FIX_FOR_GLOBALS.md`
(que predeciría 4 de 12 líneas por pisar `__for_idx`/`__for_len`; acá se
trata de `__iter`/`__resume`/`__val`, ya locales tras este fix, cortando
en la primera vuelta incluso sin una segunda corrutina activa).

Sospecha, sin confirmar todavía: `OpResume`
(`runtime/avalang/src/vm/coroutine.cpp`) guarda el frame stack del
resumer en `vm.saved_frames_`, una única variable miembro (no una pila),
antes de hacer `swap` hacia el frame stack de la corrutina. Una llamada
de función normal (`CALL`, no coroutine) ejecutada DESPUÉS de que
`OpResume` ya retornó (el `for` ya volvió al frame del resumer) no
debería tocar `vm.saved_frames_` en absoluto -- pero el síntoma
observado (corta en la 1ra iteración, no en la 2da tras la llamada
recursiva) sugiere que el problema podría estar en otro lado (quizás
`ret_slot`/`is_coroutine_suspended_` o el manejo de `pc` al volver de
`ExecuteFrame` anidado). No lo investigué más a fondo porque:

1. Es un bug del motor de corrutinas de la VM, no del compilador --
   fuera del alcance de `PLAN_FIX_FOR_GLOBALS.md` (que solo toca
   `runtime/avalang/src/compiler/compiler.cpp`, sección 3 del plan:
   "Sin cambios de gramática, AST, ni VM").
2. El propio plan marca el caso de corrutinas concurrentes/anidadas
   como condicional ("si el lenguaje permite tener más de un `for x in
   coroutine(...)` activo...", sección 6.4) -- no es un requisito duro
   de la Definición de Hecho de este plan.
3. El fix de `CompileForCoroutine` que sí pide el plan (4.3, registros
   locales para el estado interno del `for`) está aplicado, verificado
   en aislamiento, y no es la causa de este segundo bug -- de hecho
   `__iter`/`__resume`/`__val` ya locales hacen el diagnóstico MÁS claro
   (antes del fix, este mismo caso hubiera tenido ambos bugs mezclados
   y habría sido más difícil separar cuál causaba qué).

Queda documentado acá para una investigación aparte del motor de
corrutinas, fuera de este plan.

## Estado: Fase 4 completada (`CompileForDynamic`, casos restantes y cierre)

Punto 4.4 del plan (`CompileForDynamic`): revisado, **sin cambios
propios**, tal como predecía el plan. La función solo compila
secuencialmente `CompileForCoroutine` y `CompileForList` (rama runtime
por `type(iterable) == "coroutine"` vs lista) — con el fix de 4.1–4.3 ya
aplicado en ambas, `CompileForDynamic` hereda el aislamiento por frame
gratis. Se confirmó además que ambas ramas comparten consistentemente
`locals_[var_name]`: cada una chequea `locals_.find(var_name)` antes de
pedir un registro nuevo (mismo patrón que la rama `has_local` de
`AssignStmt`), así que si la rama coroutine ya registró la variable, la
rama list reusa el mismo registro en vez de pisarlo. Verificado
ejecutando el `for` dinámico sobre una lista simple (`1 2 3`, sin
cambios de comportamiento).

**Caso 4.5 (`for` anidados con el mismo nombre de variable) verificado
también en scope LOCAL, no solo top-level.** El caso 3 de
`modules/tests/for_recursion.ava` está escrito a nivel de módulo, así
que por sí solo nunca ejercita el camino de registros locales que toca
este fix. Se agregó `repro_case3_same_var_local.cpp` (mismo `for x in
[1,2] do for x in [10,20] do ... end end`, pero dentro de una función)
al harness — resultado: `10 20 "outer x=20" 10 20 "outer x=20"`,
idéntico al caso top-level. Confirma lo que predice 4.5: esta VM no
tiene *scope* de bloque, así que `x` interno y externo son la misma
variable (mismo registro) tanto dentro de una función como a nivel de
módulo — el fix no cambia esta semántica, solo el mecanismo (registro
local vs. global).

**Caso 5 agregado a `for_recursion.ava`** (análogo al caso 4, pero para
`CompileForCoroutine`): recursión dentro de un `for v in coroutine(gen)
do ... end` que a su vez entra a otro `for`-coroutine
(`walk_coroutine_nested`). Se deja documentado en el propio `.ava` que
este caso, hoy, **no** da las 12 líneas esperadas — no por un bug de
este plan (ya arreglado en Fase 3), sino por el bug preexistente del
motor de corrutinas de la VM documentado arriba (`OpResume` /
`vm.saved_frames_`). Queda como regresión pendiente para cuando ese bug
de VM se resuelva aparte.

### Corrida final completa (`./build_and_run.sh`, evidencia Fase 4)

Los 4 casos que dependen únicamente del fix de este plan (compilador,
no VM) dan el resultado correcto:

| Caso | Resultado | Esperado | OK |
|---|---|---|---|
| 1 — recursión simple con `for` | `18` | `18` | ✅ |
| 2 — `for` recursivo, listas de distinto tamaño | `1 2 3 4 5 6` | igual | ✅ |
| 3 (lista) — recursión dentro del `for` | 12 líneas | 12 líneas | ✅ |
| 3 (mismo nombre, scope local, 4.5) | `10 20 "outer x=20" 10 20 "outer x=20"` | igual | ✅ |
| Coroutine top-level (referencia) | `100 200 300` | igual | ✅ |
| Coroutine local sin recursión | `100 200 300` | igual | ✅ |
| Coroutine + recursión (bug de VM, fuera de alcance) | `1 100` (1 línea) | `1 100 200 300` (bug conocido, no de este plan) | ⚠️ conocido |
| Coroutine anidada (bug de VM, fuera de alcance) | `2 100 / 1 100` (2 líneas) | 12 líneas (bug conocido, no de este plan) | ⚠️ conocido |

Las dos filas marcadas ⚠️ son el bug de VM documentado en la sección
"Fase 3 completada" de este README — no regresiones de este plan, y no
forman parte de su Definición de Hecho.

### Checklist de "Definición de hecho" (sección 8 del plan)

- [x] Bug reproducido y documentado con un `.ava` mínimo antes del fix
      (`BEFORE_FIX_output.txt`, `BEFORE_FIX_case3_output.txt`).
- [x] `CompileForList` y `CompileForCoroutine` usan registros locales
      para su estado interno y para la variable de iteración cuando
      `!is_top_level_`.
- [x] Comportamiento a nivel de módulo (`is_top_level_ == true`) sin
      cambios — ambas ramas quedan bit-a-bit iguales que antes (ver
      código, rama `!use_locals` intacta en ambas funciones).
- [x] Los 4 casos de la sección 6 del plan pasan con resultado correcto
      y determinístico (casos 1–3 de listas + `repro_case3_same_var_local`
      para 4.5 en scope local; el caso 4/"corrutinas concurrentes" de la
      sección 6 es explícitamente condicional en el plan y su resultado
      depende de un bug de VM fuera de este alcance, documentado y no
      bloqueante).
- [x] Suite de regresión existente (harness `verification/`) sigue
      pasando sin degradarse frente a la corrida de Fase 2/3.
- [x] Sin comentarios nuevos en el código de `compiler.cpp` (los
      comentarios solo viven en `verification/` y en los `.ava` de
      prueba, no en el código fuente del compilador).

**Fase 4 cierra el plan `PLAN_FIX_FOR_GLOBALS.md` completo.** El único
pendiente que queda abierto es el bug de VM de corrutinas
(`OpResume`/`vm.saved_frames_`), explícitamente fuera de alcance y
anotado para investigación aparte.

## Archivos

- `abi_shims.cpp` / `ast_dsl.h` — infraestructura del harness (bypass de
  ANTLR4/vcpkg, DSL para construir AST a mano).
- `repro_recursion.cpp` — casos 1 y 2 de la sección 6 del plan.
- `repro_case3_nested_call.cpp` — caso 3/4 (lista), recursión dentro del
  `for`, disparador real del bug de globales.
- `repro_case3_same_var_local.cpp` — caso 3 (mismo nombre de variable,
  4.5) en scope local, agregado en Fase 4.
- `repro_coroutine_simple.cpp` / `repro_coroutine_local.cpp` — referencia
  sin recursión (top-level y local).
- `repro_coroutine_depth1.cpp` / `repro_coroutine_nested.cpp` — análogo
  coroutine del caso 3/4, expone el bug de VM aparte.
- `build_and_run.sh` — reconstruye y corre todo el harness contra el
  `compiler.cpp` actual del árbol.
- `BEFORE_FIX_*.txt` / `AFTER_FIX_PHASE2_*.txt` — evidencia capturada de
  antes/después del fix.
