# Fase 0 — Auditoría: AvaLang Runtime independiente de STL

Referencia: `avalang_runtime_stl_barekernel_plan.md` (plan maestro), sección 16.

## 1. Punto de partida: esto no arranca de cero

Antes de este plan ya existe `docs/kernel/PLAN_BAREKERNEL_STDCOMPAT.md` y su
implementación, `platform/barekernel/stdcompat/` (namespace `avastd`). Esa
capa resuelve una parte real de lo que pide este plan nuevo, pero con un
enfoque distinto:

- `avastd::vector/string/shared_ptr/unordered_map/function/...` son **alias
  1:1 de la STL real** en hosted (`AVA_HAVE_STD_LIBRARY=1`, Windows/Linux/
  macOS) y **reimplementaciones freestanding propias** en BareKernel
  (`AVA_HAVE_STD_LIBRARY=0`), elegido en compilación vía `CKM_CAP_LIBSTDCPP`.
- No son los tipos de valor del lenguaje (`AvaValue`, `AvaString`, `AvaArray`,
  `AvaMap`, `AvaObject` de la sección 4 del plan nuevo). Son un *reemplazo
  de bajo nivel de la STL*, no la capa semántica del runtime del lenguaje.
- En modo freestanding, `avastd::vector`/`avastd::string` reservan memoria
  con `operator new`/`operator delete` crudos, respaldados por
  `CorLib::malloc/free` del kernel — **no pasan por ninguna capa `AvaMemory`
  propia**. Esto es exactamente el hueco que la sección 5 del plan nuevo
  (`ava_alloc`/`ava_realloc`/`ava_free`) pide cerrar.

Conclusión: `avastd` ya cubre gran parte del trabajo mecánico de Fase 0-2
del plan viejo (`PLAN_BAREKERNEL_STDCOMPAT.md`) y sirve de base para la
Fase 1 y 2 de este plan nuevo, pero no las reemplaza. Se adopta como
cimiento en vez de descartarla.

## 2. Estado real de la migración (verificado en el zip actual)

| Área | Archivos | std:: hoy | Estado |
|---|---|---|---|
| `src/vm/*.h` (8 headers: vm.h, value.h, vm_helpers.h, proto.h, closure.h, module.h, coroutine.h, task.h) | 8 | 0 (ya en `avastd::`) | **Migrado** |
| `src/vm/*.cpp` (20 archivos, `CORE_SOURCES`) | 20 | 0 fuera de `#if AVA_HAVE_STD_LIBRARY` (`vm_extern.cpp` usa `std::filesystem` para resolver DLLs nativas, correctamente guardado tras esa macro -- no corre en build freestanding) | **Migrado** |
| `src/builtins/*.cpp` | 9 | 0 | **Migrado** (Fase 3) |
| `src/ui/builtins.cpp` | 1 | 0 | **Migrado** (Fase 3) |
| `src/compiler/proto_io.cpp` + `.h` | 2 | 0 | **Migrado** (Fase 3) |
| `src/compiler/obfuscate.cpp` + `.h` | 2 | 0 | **Migrado** (Fase 3) |
| `platform/memory/MemoryFileSystem.cpp` + `.h` | 2 | 0 | **Migrado** (ya estaba listo desde antes de Fase 3, verificado) |
| `api/src/c_api.cpp` + `main.cpp` | 2 | 82 | **Pendiente** (es la frontera ABI, sección 12 del plan; próximo objetivo) |
| `src/ast/*`, `src/compiler/compiler.cpp`, `src/frontend/frontend_antlr.cpp` | — | 1128 | **Fuera de alcance freestanding** (ver §3) |

## 3. Qué debe ser freestanding y qué no

`CMakeLists.txt` de `runtime/avalang` compila `CORE_SOURCES` (VM completa +
builtins + proto_io + obfuscate + ui/builtins + PAL) para **todos** los
targets, incluido BareKernel. `FRONTEND_SOURCES`
(`ast_builder.cpp`, `compiler.cpp`, `frontend_antlr.cpp`, parser generado
por ANTLR4) también se agrega incondicionalmente a `CORE_SOURCES`, pero en
la práctica el toolchain freestanding del kernel no tiene `antlr4-runtime`
disponible, así que el build cae a `frontend_stub.cpp` y esos tres archivos
**no entran en el binario BareKernel hoy**. Es un límite implícito (por
ausencia de dependencia), no explícito en el CMake — vale la pena
endurecerlo más adelante con un `if(NOT AVA_TARGET_BAREKERNEL)` explícito
para que no se rompa si algún día alguien cross-compila ANTLR.

Clasificación (sección 9 del plan, "Runtime-critical / VM / Compiler /
Tooling / Tests / Host-specific"):

- **Runtime-critical (debe ser freestanding, prioridad de migración):**
  `src/vm/*` (ya hecho), `src/builtins/*`, `src/ui/builtins.cpp`,
  `src/compiler/proto_io.*`, `src/compiler/obfuscate.*`,
  `platform/memory/MemoryFileSystem.*`, `api/src/c_api.cpp` (frontera ABI).
- **Compiler/Tooling (puede seguir usando STL, sección 10):**
  `src/ast/*`, `src/compiler/compiler.cpp`, `src/frontend/frontend_antlr.cpp`,
  `src/frontend/denter.cpp`, el parser generado por ANTLR4, `avacli`,
  `avastudio`. No corren en BareKernel hoy.
- **Host-specific (ya correcto, un archivo por plataforma):**
  `platform/windows/*`, `platform/linux/*`, `platform/macos/*`,
  `platform/barekernel/*`.
- **Tests:** `src/compiler/test_proto_io_obfuscate.cpp` — sin restricción.

## 4. Símbolos std:: más usados (todo el árbol, referencia)

```
471 std::string        286 std::shared_ptr     121 std::any
339 std::vector         125 std::move           116 std::make_shared
 45 std::runtime_error   45 std::any_cast        25 std::pair
 23 std::exception        22 std::to_string      16 std::unordered_map
 13 std::cout             12 std::uint            11 std::round
 11 std::mutex            10 std::function         9 std::ostream/istream
  8 std::unique_ptr        8 std::ostringstream    7 std::lock_guard
```

`avastd` ya cubre: `string`, `vector`, `shared_ptr`, `unordered_map`,
`function`, `mutex`, `runtime_error` (vía `AvaError`), utilidades de
`ava_algorithm.h`/`ava_math.h` (`min`/`max`/`abs`/`round`/`pow`/`swap`).

No cubre todavía (huecos frente a lo que usan builtins/proto_io/c_api):
`std::any`/`std::any_cast` (121+45 usos — builtin_natives.cpp para el
FFI/extern), `std::ostringstream`/`std::ostream`/`std::istream`
(proto_io.cpp, c_api.cpp para serialización), `std::unique_ptr`,
`std::filesystem`.

## 5. Gap arquitectónico frente al plan nuevo

1. **AvaMemory no existe todavía.** `avastd` freestanding llama a
   `operator new`/`delete` crudo → `CorLib::malloc/free`. El plan pide una
   capa `ava_alloc/ava_realloc/ava_free` explícita, ruteada por Ava
   Platform API, para tener tracking de memoria propio y una base sobre la
   que después construir `AvaGC`.
2. **No hay tipos de valor del lenguaje separados de los tipos de la STL.**
   `AvaString`/`AvaArray`/`AvaMap` de la sección 4 son conceptualmente
   distintos de `avastd::string`/`avastd::vector`/`avastd::unordered_map`:
   estos últimos son sustitutos de bajo nivel, no portadores de semántica
   de GC/objeto del lenguaje.
3. **`std::any` en el FFI (`builtin_natives.cpp`) no tiene equivalente en
   `avastd` hoy.** Va a hacer falta antes de poder migrar ese archivo.

## 6. Orden recomendado para lo que sigue

Dado que gran parte de la "Fase 1 (AvaMemory)" y "Fase 2 (Tipos
fundamentales)" del plan nuevo tiene ya un cimiento parcial en `avastd`, el
orden más barato no es reescribir desde cero sino:

1. **Fase 1 real:** crear `AvaMemory` (`ava_alloc`/`ava_realloc`/`ava_free`)
   como servicio de Ava Platform API (interfaz + Windows/Linux/BareKernel),
   y re-cablear el `operator new`/`delete` freestanding de `avastd` para
   que pase por `ava_alloc`/`ava_free` en vez de ir directo a
   `CorLib::malloc/free`. Esto le da control de memoria propio a *todo* lo
   que ya usa `avastd` sin tocar cada call site.
2. **Fase 2:** cerrar los huecos de superficie que bloquean migrar
   `builtins/*`, `proto_io`, `obfuscate`, `MemoryFileSystem` y `c_api.cpp` a
   `avastd` (`avastd::any`, `avastd::unique_ptr` ya existe — falta
   `avastd::ostringstream`/stream, `avastd::filesystem` si se usa fuera de
   PAL).
3. Migrar esos archivos uno por uno (mismo patrón que ya usó
   `PLAN_BAREKERNEL_STDCOMPAT.md` en su Fase 2 para `src/vm/*.cpp`).
4. Recién ahí evaluar si hace falta introducir `AvaValue`/`AvaString`/
   `AvaArray`/`AvaMap` como tipos de valor propios (sección 4) por encima de
   `avastd`, o si `avastd` + `AvaMemory` es suficiente para los objetivos de
   portabilidad/freestanding sin duplicar una segunda capa de contenedores.
   Esta decisión queda abierta para cuando se llegue ahí — no se resuelve
   en esta auditoría.

`src/ast/*`, `compiler.cpp` y `frontend_antlr.cpp` quedan fuera de esta
migración mientras dependan de ANTLR4 (no hay toolchain BareKernel con
ANTLR hoy).

## 7. Fase 3 -- cierre (builtins/proto_io/obfuscate/MemoryFileSystem)

Todo lo marcado "Runtime-critical" en la tabla del §2 salvo `c_api.cpp`
quedó migrado a `avastd::`:

- `src/builtins/builtin_strings.cpp`: no tenía equivalente a
  `std::string::replace(pos,count,str)` ni `compare(pos,count,str)` en
  `avastd::string` freestanding -- se agregaron ambos (mismo patrón que
  Fase 2: cerrar el hueco puntual, no reescribir toda la clase). `trim`
  se reescribió con índices en vez de `rbegin()/rend()` para no tener que
  agregar reverse_iterator a `avastd::string` por un único caller.
- `src/builtins/builtin_natives.cpp`: **no usaba `std::any`** pese a lo
  que decía la nota de la Fase 0 (ese hueco aplica a `ast_builder.cpp`/
  `frontend_antlr.cpp`, que son Tooling y no bloquean esta fase). Hueco
  real encontrado: faltaban `avastd::fabs`/`avastd::trunc` en
  `ava_math.h` (agregados, mismo patrón builtin-de-compilador que
  abs/round/floor/ceil/sqrt) y `avastd::string::pop_back()` (agregado).
  El único `try/catch` real (`AsNumber` sobre `avastd::stod`) quedó
  detrás de `#if AVA_HAVE_STD_LIBRARY`: en hosted `avastd::stod` es
  `std::stod` y sí puede lanzar con input no numérico; en freestanding
  es un parser manual que nunca lanza, así que ahí se llama directo sin
  try/catch (evita depender de excepciones C++ reales en el kernel).
- `src/ui/builtins.cpp`, `src/compiler/proto_io.*`,
  `src/compiler/obfuscate.*`: sin huecos nuevos, solo sustitución directa
  (ya cubiertos por Fase 1/2: `avastd::sstream`, `avastd::set`, etc).
- `platform/memory/MemoryFileSystem.*`: se verificó que ya estaba en
  `avastd::` de una sesión anterior -- no requirió cambios.

Único pendiente de la lista original: `api/src/c_api.cpp` + `main.cpp`
(82 usos, frontera ABI). No se movió en esta fase.

## 8. Fase 4 -- Platform API (sección 6 y 16 del plan nuevo)

Punto de partida distinto al resto de fases: la Ava Platform API descrita
en la sección 6 del plan (`platform/`, `IPlatform`/memoria/filesystem/
tiempo/threads/sync/consola) **ya existía** antes de este plan, con
implementación completa por interfaz (`IFileSystem`, `IThreadFactory`,
`IClock`, `ILibraryLoader`, `IConsole`, `IEnvironment`, `IProcess`,
`ITimer`, `IMutex`, agregadas en `IPlatform`) y un backend por plataforma
(`windows/`, `linux/`, `macos/`, `barekernel/`). `AvaMemory`
(`ava_alloc`/`ava_realloc`/`ava_free`, sección 5 del plan) también existe
ya, con las 4 implementaciones. No se trataba de crear la API desde cero.

Lo que sí eran huecos reales, cerrados en esta fase:

- **Memoria — inconsistencia entre `avastd::vector` y `avastd::string`.**
  `avastd::vector` freestanding ya alocaba vía `ava_alloc`/`ava_free`
  (`AvaMemory.h`). `avastd::string` freestanding no: usaba
  `::operator new`/`::operator delete` global crudo. `::operator new`
  global en este kernel resuelve a `CorLib::malloc`/`free` (ver
  `ava_new.h`), así que en la práctica ambos terminaban en el mismo
  allocator físico, pero por rutas distintas y sin pasar por el punto de
  control único que pide la sección 5 del plan. Se migró `alloc_bytes`/
  `free_if_owned` de `ava_string.h` a `ava_alloc`/`ava_free`, mismo patrón
  que `ava_vector.h`. `avastd::shared_ptr`/`avastd::function` siguen
  usando `new`/`delete` normal (no `::operator new` directo) a propósito:
  eso pasa por el operator new global del compilador, que es la rutina de
  soporte de lenguaje que ya provee el kernel -- redefinirla chocaría en
  el link (ver comentario en `ava_new.h`). No se tocó.
- **`avastd::mutex` (BareKernel, freestanding) era un no-op incluso con
  `AVA_HAVE_MUTEX`/`CKM_CAP_MUTEX` activo.** Tenía un `TODO Fase 4:
  delegar a BareKernelMutex` literal en el código. Se resolvió llamando
  directo a las mismas syscalls CKM que usa `BareKernelMutex.cpp`
  (`ckm_mutex_init/lock/unlock/trylock`) sobre un `CkmMutex` propio de
  `avastd::mutex`, en vez de pasar por la interfaz polimórfica
  `IMutex`/`IPlatform` (que sigue existiendo para quien la necesite como
  puntero a interfaz; `avastd::mutex` es un tipo de valor de más bajo
  nivel, usado por `vm.h` como `async_mutex_`). Con
  `CKM_CAP_MUTEX=0` (el estado actual de este kernel, ver
  `BareKernelCaps_target.h`) el comportamiento no cambia: sigue siendo
  no-op, que sigue siendo correcto porque `CKM_CAP_THREADS=0` también.
  Verificado por separado con `-DCKM_CAP_MUTEX=1` y sin la macro
  (`-fsyntax-only`): ambas ramas compilan.
- **`BareKernelTimer::ScheduleOnce` ignoraba `delayMs` en la rama
  `CKM_CAP_THREADS && CKM_CAP_TIMERS`** (spin off a un thread que
  disparaba el callback inmediatamente, con un `TODO: honour delay once
  ckm_timer is available` -- pero `ckm_sleep_ms` ya estaba disponible,
  solo no se usaba ahí). Se corrigió: el thread ahora hace
  `ckm_sleep_ms(delayMs)` antes de invocar el callback, igual que ya
  hacía la rama `CKM_CAP_TIMERS` sin threads.

No se tocó (fuera de alcance de esta fase, confirmado contra la sección 6
del plan, que dice explícitamente "no todas estas APIs tienen que
implementarse inmediatamente. La prioridad inicial es memoria."):

- **`network`** -- no existe `platform/*/  *Network.*` ni `INetwork` en
  ningún backend. El plan lo marca como "cuando sea necesario"; no hay
  caller hoy que lo necesite. Queda pendiente sin fecha.
- Los backends `linux/`, `macos/` siguen siendo mayormente STUB (`Clock`,
  `Process`, `Environment`, `Console`, `Thread`, `Library` con comentarios
  `TODO(Phase 6)`/`STUB`). Ese trabajo pertenece a hosted Linux/macOS, no
  a BareKernel ni a la migración de `avastd`, y ya está etiquetado en el
  propio código como una fase distinta (Fase 6 de `PAL_PROGRESS.md`, doc
  que no está en este zip). No se tocó para no mezclar alcance.
- `api/src/c_api.cpp` + `main.cpp` (82 usos `std::`, frontera ABI): sigue
  pendiente, como ya estaba documentado en §2/§7. Fase 4 (Platform API)
  no lo bloquea ni lo destraba.

Con esto, el checklist de la sección 16 del plan para Fase 4 (memoria,
filesystem, tiempo, threads, synchronization, consola definidos; network
diferido a propósito) queda cerrado sobre la base que ya existía, con los
tres huecos de consistencia interna resueltos.

## 9. Fase 5 -- GC (sección 8 y 16 del plan nuevo)

Aviso de nombres: `vm.h` ya tiene comentarios que dicen "Fase 5 (Async
Runtime)" (`set_timeout`/dispatcher sobre `ITimer`) -- es una numeración
histórica del proyecto, sin relación con la Fase 5 (GC) del plan nuevo.
No se toca nada de eso acá.

### 9.1 Modelo de ownership actual (ya existía, no se inventa en esta fase)

`value.h` ya tiene un diseño de ownership real, no es que "no había
nada": `Object` con `ref_count` atómico + `Retain`/`Release` manuales
(`value.cpp`). Cubre `StringObj`, `ListObj`, `DictObj`, `NativeObj`,
`ClassObj`, `InstanceObj`, `ModuleObj`, `BoundMethod`, `ExceptionObj` y
`Closure`/`Upvalue` (`closure.h`) -- todos los `ValueType` que
`Value::IsRefCounted()` marca como `true`. 54 call sites de
`Retain`/`Release` repartidos en `value.cpp`, `vm.cpp`, `vm_arith.cpp`,
`vm_call_op.cpp`, `vm_classes.cpp`, `vm_core.cpp`, `vm_import.cpp`,
`vm_task.cpp`, `c_api.cpp`.

`Closure` es un caso particular verificado en esta auditoría porque a
primera vista parece un bug: `CallFrame::closure` es
`avastd::shared_ptr<Closure>`, pero `Closure` también hereda `Object` y
se maneja con `Retain`/`Release` cuando vive detrás de un `Value`
(`ValueType::Function`). Dos mecanismos de refcounting sobre el mismo
objeto suena a doble-free. Verificado: **no lo es** -- los 5 sitios que
construyen ese `shared_ptr` (`vm.cpp`, `vm_call.cpp`, `vm_call_op.cpp`,
`vm_task.cpp`, `coroutine.cpp`) usan todos
`avastd::shared_ptr<Closure>(closure, [](Closure*) {})`: un deleter
no-op. Es el idiom estándar de "shared_ptr no-owning view" -- deja pasar
un `Closure*` por un campo tipado `shared_ptr<Closure>` sin que
`shared_ptr` participe nunca en la decisión de liberarlo. La única
autoridad real de liberación sigue siendo `Object::ref_count` vía
`Retain`/`Release`. Correcto, aunque no obvio a simple lectura -- vale la
pena dejarlo documentado acá para que nadie "simplifique" ese patrón
pensando que es redundante.

### 9.2 Gap real #1: Coroutine/TaskObj no participan del ownership

`Coroutine` (`coroutine.h`) y `TaskObj` (`task.h`) **no heredan
`Object`**. `Value::IsRefCounted()` no incluye `ValueType::Coroutine` ni
`ValueType::Task` -- `Retain`/`Release` son no-op para estos dos tipos.
Su ciclo de vida real: `new Coroutine()`/`new TaskObj()` en
`vm_core.cpp`/`vm_task.cpp`, registrados en `VM::created_coroutines_` /
`VM::created_tasks_`, y **liberados únicamente en `VM::~VM()`** (bucle
`for (auto* co : created_coroutines_) delete co;` en `vm_core.cpp`).

Esto es una estrategia válida y deliberada (arena de vida completa de la
VM), no un bug -- pero tiene una consecuencia real que el plan pide
señalar explícitamente en la sección 8 ("el diseño del GC debe considerar
desde el principio... Coroutines"): en un proceso de larga duración que
crea coroutines/tasks continuamente (el caso típico de un servicio
BareKernel, sección 20 del plan), la memoria de `Coroutine`/`TaskObj`
crece sin límite hasta que la VM entera se destruye. No es un leak en el
sentido de "memoria inalcanzable" -- es memoria retenida a propósito por
una estrategia de vida-completa-de-VM que no escala a procesos
long-running.

**No se corrige en esta fase.** Corregirlo bien requiere integrar
`Coroutine`/`TaskObj` al mismo modelo de refcounting que ya usa todo lo
demás (`CoroutineObj : Object`, `TaskObj : Object`, agregar los
`ValueType` correspondientes a `IsRefCounted()`, y auditar los mismos ~15
call sites de `vm_task.cpp`/`vm_async.cpp`/`coroutine.cpp`/
`vm_call.cpp`/`vm_call_op.cpp` que ya manejan `Retain`/`Release` para los
demás tipos, para no dejar ninguno sin actualizar). Es exactamente el
tipo de cambio que la sección 8 del plan marca como delicado (relaciones
`Task <-> Coroutine <-> awaiters` con forma de grafo, potencialmente
cíclicas) y este entorno no puede compilar ni correr el árbol completo
para validarlo (falta el toolchain ANTLR4 para el frontend, y el target
freestanding falla hoy por una limitación previa no relacionada en
`ava_error.h`, sección de manejo de errores i386-only -- ver nota
separada). Tocar el ciclo de vida de coroutines/tasks sin poder correr
`samples/test/fase2_*.ava`/`fase3_*.ava` (que existen específicamente
para cubrir este subsistema) es el tipo de cambio que no se hace a
ciegas. Queda como el ítem prioritario de una fase dedicada, con acceso a
build real.

Cambio hecho en esta fase, de bajo riesgo (no toca lifecycle, solo lo
hace observable): `VM::LiveCoroutineCount()` / `VM::LiveTaskCount()` en
`vm.h`, exponiendo el tamaño de `created_coroutines_`/`created_tasks_`.
Sirve para que un host (BareKernel u otro) pueda detectar crecimiento sin
límite hoy mismo, sin esperar a la integración completa. Verificado con
`g++ -fsyntax-only` en modo hosted.

### 9.3 Gap real #2 (abierto a propósito): ciclos

Bajo refcounting puro, un ciclo de referencias (p.ej. una `Instance` que
guarda un `Closure` cuyo `Upvalue` apunta de vuelta a un `Value` que
contiene esa misma `Instance`) nunca llega a `ref_count == 0` y nunca se
libera. Hoy no hay ningún mecanismo de detección/ruptura de ciclos. El
plan (sección 8) pide que el diseño del GC considere esto desde el
principio, pero también dice explícitamente en la sección 9.2/16 que "no
es necesario implementar el GC completo en la primera etapa". Se deja
como decisión de diseño abierta, no resuelta acá: la opción más barata
sobre la base actual sería un mark-sweep opcional que recorra roots
conocidos (`VM::globals_`, registros vivos en `frames_`/`saved_frames_`,
y los nuevos `created_coroutines_`/`created_tasks_` si se integran) para
romper ciclos periódicamente, sin reemplazar el refcounting como
mecanismo primario (modelo híbrido, común en runtimes con refcounting de
base -- Python usa la misma combinación). No implementado.

### 9.4 Recursos nativos (FFI): revisado, no requiere cambio

`vm_extern.cpp` mantiene `g_loaded_libs`, un cache global de librerías
nativas cargadas (`dlopen`/`LoadLibrary` vía `ILibraryLoader`), sin
`Unload()` explícito en ningún punto del ciclo de vida de la VM. Se
revisó si esto es un gap de GC -- **no lo es**: es una política
deliberada de vida-de-proceso para librerías nativas, la misma que usan
CPython, Node y Lua (nunca descargan una librería FFI una vez cargada,
porque no hay forma segura de saber si algún puntero a función de esa
librería sigue en uso). No se toca.

### 9.5 Resumen contra el checklist de la sección 16 (Fase 5)

- [x] Diseñar ownership -- ya existía (refcounting manual vía `Object`);
  auditado y confirmado correcto, incluyendo el patrón `shared_ptr`
  no-owning de `Closure` (§9.1).
- [x] Registrar objetos / Crear roots -- ya existía de forma implícita
  (`VM::globals_`, `frames_`, y ahora documentado explícitamente
  `created_coroutines_`/`created_tasks_` como root set de facto para
  Coroutine/Task bajo la estrategia actual). Formalizado con
  `LiveCoroutineCount()`/`LiveTaskCount()`.
- [ ] Implementar tracing -- no implementado, decisión de diseño abierta
  (§9.3).
- [ ] Manejar ciclos -- no manejado, mismo motivo (§9.3).
- [x] Integrar closures -- ya integrado y verificado correcto (§9.1).
- [ ] Integrar coroutines -- gap real identificado (§9.2), no corregido
  en esta fase por riesgo de tocar lifecycle sin poder compilar+correr
  los tests de async existentes.
- [x] Integrar native resources -- revisado; la política actual
  (vida-de-proceso) ya es la correcta, no hace falta integración con GC
  (§9.4).

## 10. Fase 3 -- cierre del último ítem: "Reducir std::shared_ptr"

Auditoría de los 19 sitios que usan `avastd::shared_ptr` en `src/vm/`
(los mismos que cuenta el plan §16, Fase 3), para decidir qué se puede
reducir de verdad y qué no.

### 10.1 Clasificación de los 19 sitios

**Grupo A -- vista no-dueña de `Closure` (5 sitios).** `vm.cpp:295`,
`vm_task.cpp:26`, `vm_call.cpp:84,121`, `vm_call_op.cpp:118`,
`coroutine.cpp:89`: todos construyen
`avastd::shared_ptr<Closure>(closure, [](Closure*) {})` -- deleter
no-op, ya auditado y confirmado correcto en §9.1 (Fase 5). El
`shared_ptr` nunca decide liberar nada acá; solo existe porque
`CallFrame::closure` está tipado `shared_ptr<Closure>`. No hay
asignación redundante que mover ni copia que evitar -- es una
construcción de un valor nuevo desde un puntero crudo, no una copia de
otro `shared_ptr` existente.

**Grupo B -- ownership real de `Proto` (14 sitios).** `value.h:153,313`,
`module.h:16,46,47,59`, `module.cpp:86,91`, `coroutine.h:21,22`,
`proto.h:31`, `vm.h:35`, `vm_core.cpp:209`. `Proto` no hereda `Object`
(no tiene `ref_count`/`Retain`/`Release`); su ciclo de vida depende
enteramente de `avastd::shared_ptr`, con dueños simultáneos reales:

- El árbol de compilación (`Proto::child_protos`, un `Proto` padre posee
  a sus hijos).
- `ModuleCache::modules_` (dueño mientras el módulo esté cacheado).
- `Closure::proto` (un `Closure` ya creado sigue vivo aunque su módulo se
  saque de la cache o el padre se destruya).
- `ClassObj::methods` (cada método de una clase es un `Proto` hijo,
  copiado al instanciar la clase -- ver `vm_classes.cpp:13`).
- `CallFrame::proto` (copia de seguridad para que el frame no dependa de
  que el `Closure`/módulo que lo originó siga vivo durante la llamada).

Esta necesidad de dueños múltiples es real y deliberada, no un descuido:
un `Proto` puede sobrevivir al `Closure` que lo compiló si otro
`Closure` lo sigue usando, y un `CallFrame` no puede arriesgarse a que su
`Proto` desaparezca a mitad de una llamada. Eliminar `shared_ptr` de este
grupo del todo requeriría darle a `Proto` el mismo ownership intrusivo
que ya tiene `Object` (`ref_count` + `Retain`/`Release`, ver §9.1) y
auditar los mismos ~14 call sites para no dejar ninguno sin convertir --
es un cambio de forma (Fase 5/GC), no de Fase 3, y toca lifecycle de la
misma manera que el gap de Coroutine/TaskObj que §9.2 dejó
deliberadamente sin tocar por no poder compilar+correr la suite
(`samples/test/fase2_*.ava`/`fase3_*.ava`) en este entorno. Se deja fuera
de alcance de Fase 3 a propósito, con la misma razón que ya usó Fase 5
para no migrar `shared_ptr<Upvalue>` al tracing del GC
(`GC_FASE5_OWNERSHIP_DESIGN.md` §7): mover ownership real sin poder
correr los tests que cubren ese subsistema no se hace a ciegas.

### 10.2 Lo que sí se redujo: copias que no necesitaban serlo

Dentro del Grupo B se encontraron 5 sitios donde el `shared_ptr` de
origen era una variable local que no se volvía a leer después de la
asignación -- una copia (incremento + eventual decremento atómico del
control block) donde un `avastd::move` alcanza:

| Archivo | Sitio | Antes | Después |
|---|---|---|---|
| `module.cpp` | `ModuleCache::Add` | `modules_[module_name] = proto;` | `modules_[module_name] = avastd::move(proto);` |
| `vm_import.cpp` | `VM::DoImport`, tras `CompileSource` | `module_cache_.Add(module_path, proto, resolved_path);` | `module_cache_.Add(module_path, avastd::move(proto), resolved_path);` |
| `vm_import.cpp` | `VM::DoImport`, armado del `CallFrame` | `frame.proto = proto; frame.registers.resize(proto->num_registers);` (usa `proto` de nuevo después de asignar) | orden invertido: `resize` primero, `frame.proto = avastd::move(proto);` al final |
| `vm.cpp` | `OpCode::CLOSURE`, alta de upvalue | `closure->upvalues.push_back(upval);` | `closure->upvalues.push_back(avastd::move(upval));` |
| `vm_call_op.cpp` | `OpClosure`, alta de upvalue | `closure->upvalues.push_back(upval);` | `closure->upvalues.push_back(avastd::move(upval));` |

Verificado por lectura (no por compilación, ver nota del pedido) que
ninguna de las 5 variables movidas se vuelve a usar después del punto de
move dentro de su función: `proto` en `ModuleCache::Add` es un parámetro
por valor no reusado tras la asignación; el `proto` de `DoImport` no
aparece en el resto de la función tras cada move (confirmado con grep
línea por línea); `upval` se crea y se descarta en cada iteración del
`for` de upvalues, sin lectura posterior. No se tocó ningún sitio del
Grupo A donde la variable de origen se sigue leyendo después (p.ej.
`closure->proto = child_proto;` en `OpCode::CLOSURE`, donde
`child_proto->upvalue_descs` se lee en la misma iteración) -- esos
siguen siendo copias reales a propósito.

Esto no cambia el conteo de 19 sitios (siguen siendo 19 lugares que
*declaran o construyen* un `shared_ptr`) ni el modelo de ownership de
`Proto` -- reduce el tráfico de refcounting atómico en el camino
caliente de import de módulos y creación de closures, sin riesgo
semántico. Con esto y la clasificación de §10.1 documentada, el ítem
"Reducir `std::shared_ptr`" del checklist de Fase 3 (plan §16) se cierra:
lo reducible sin rediseñar el ownership de `Proto` está reducido: lo que
queda es ownership real, con la razón de por qué documentada, no un
pendiente sin explicar.

## 11. Fase 6 -- Freestanding: auditoría y cierre

Punto de partida: el plan pedía "crear target freestanding" como si no
existiera. No es así -- ver §11.1. El trabajo real de esta fase fue
validar, por primera vez de punta a punta, que el árbol de `CORE_SOURCES`
más el backend real de BareKernel compila con los defines/capacidades que
declara `binding.cmake` para el target real, no solo con los defaults
"todo en 0" de `BareKernelCaps.h`.

### 11.1 "Crear target freestanding" -- ya existía

`AVA_TARGET_BAREKERNEL=ON` + `cmake/toolchain-i686-elf.cmake` +
`scripts/build_barekernel.bat` ya compilan `avalang` cruzado con
`i686-elf-g++`, seleccionando `PLATFORM_SOURCES` = los 11 archivos
`platform/barekernel/BareKernel*.cpp` (ver `runtime/avalang/CMakeLists.txt`
línea ~98). Es funcionalmente lo que el plan nuevo (§15) llama
`avalang-core-freestanding`, con otro nombre. No se creó nada nuevo --
se marca cerrado porque ya lo estaba.

### 11.2 Método de validación (limitación de entorno documentada)

No hay `i686-elf-g++` disponible en este entorno (paquete fuera de los
dominios de red permitidos), así que no se pudo compilar/linkear el
target real. Sustituto usado: `g++` del host en modo `-fsyntax-only
-ffreestanding -fno-exceptions -fno-rtti`, con:

- `-DAVA_TARGET_BAREKERNEL -DAVA_BAREKERNEL_TARGET_BINDING -D__i386__`
  (el `#error` de `ava_error.h` exige `__i386__` porque su
  setjmp/longjmp propio solo tiene implementación para esa arquitectura
  -- decisión correcta y ya documentada in situ, no un bug).
- Las `CKM_CAP_*` reales que declara
  `platform/barekernel/bindings/target/binding.cmake` (no los defaults en
  0 de `BareKernelCaps.h`), para ejercitar exactamente las ramas de
  código que el target real activa (`CKM_CAP_DYNAMIC_LOADING=1`,
  `CKM_CAP_ENVVARS=1`, `CKM_CAP_DIR_ENUM=1`, `CKM_CAP_COLOR=1`, resto 0).
- El include path real de `binding.cmake`
  (`platform/barekernel/bindings/target`), sin el cual
  `BareKernel*.cpp` no encuentra `ckm_syscall.h` -- confirmado que es un
  problema del comando de chequeo, no del código (CMake ya lo agrega).

Alcance verificado: las 44 unidades de `CORE_SOURCES`, las 11 de
`PLATFORM_SOURCES` (backend BareKernel), y `ckm_syscall.cpp` (el binding).
Todas pasan `-fsyntax-only` limpio tanto en la rama freestanding
(`CKM_CAP_LIBSTDCPP=0`) como en la hosted (`AVA_HAVE_STD_LIBRARY=1`,
sin los defines de arriba) -- se re-corrió esta última después de cada
fix para confirmar que ningún cambio rompió Windows/Linux/macOS.

Esto es una validación parcial pero real -- mejor que la que había
(el audit previo a esta fase corría `-fsyntax-only` en host x86_64 sin
ninguno de estos defines y chocaba de una con el `#error` de
`ava_error.h`, sin llegar a compilar nada del árbol real). No reemplaza
un link real contra el kernel: no prueba ABI de syscalls, layout de
stack para `setjmp`/`longjmp` en 32 bits real, ni el linker script.

### 11.3 Bugs reales encontrados y arreglados (7)

Todos son gaps genuinos de la capa `avastd::` freestanding o de un
`#include` que asume libc real -- ninguno cosmético, todos confirmados
por el compilador antes y después del fix.

1. **`avastd::shared_ptr<T>` -- tipo incompleto exigido en el lugar
   equivocado** (`ava_shared_ptr.h`). `ControlBlock<T>::release()` hacía
   `delete ptr` tipado en `T` directo. A diferencia de `std::shared_ptr`
   (que type-erasea el deleter en el momento de *construcción*),
   esto exigía `T` completo en cualquier TU que instanciara
   `~shared_ptr<T>()` -- no solo donde se construye. Rompía con
   `BoundMethod::proto` (`shared_ptr<Proto>`, `Proto` solo
   forward-declarado en `value.h`): cualquier archivo que incluyera
   `value.h` sin `proto.h` (la mayoría de `src/builtins/*.cpp`)
   instanciaba un `delete` sobre tipo incompleto. Fix: `ControlBlock<T>`
   ahora captura un thunk type-erased (`DefaultDelete<T>`, tomado por
   dirección en el constructor) -- el requisito de tipo completo queda
   confinado al sitio real de construcción, igual que `std::shared_ptr`.
2. **`avastd::unique_ptr<T>` -- sin conversión de upcast por move**
   (mismo archivo). `BareKernelPlatform::Create()` hace
   `return avastd::make_unique<BareKernelPlatform>();` contra un
   `unique_ptr<IPlatform>` de retorno -- patrón estándar de factory. Sin
   un constructor de conversión, esto no compilaba. Fix: constructor
   `template<class U> unique_ptr(unique_ptr<U>&&)`, igual que
   `std::unique_ptr`.
3. **`avastd::Hash<T*>` faltante** (`ava_unordered_map.h`).
   `gc_trace.cpp`/`gc_sweep.cpp` usan `unordered_set<Object*>` para el
   marcado del GC -- un caso real que la nota original del archivo decía
   que no existía ("AvaLang solo instancia con `avastd::string` +
   algunos enteros"). Fix: especialización genérica `Hash<T*>`
   (multiplicador Fibonacci sobre el valor del puntero, mismo patrón que
   `Hash<int64_t>`).
4. **`avastd::string::resize()` y constructor de rango de iteradores
   faltantes** (`ava_string.h`). `proto_io.cpp` (deserialización de
   bytecode) usa `s.resize(len)` antes de leer bytes crudos del stream, y
   `avastd::string s(bytes.begin(), bytes.end())` para reconstruir un
   string desde un `vector<uint8_t>`. Fix: ambos agregados (`resize` con
   relleno opcional; el constructor de rango solo cubre punteros crudos,
   que es lo único que produce `avastd::vector::begin()`, documentado
   como tal en el comentario).
5. **`avastd::to_string(double)` ambiguo** (`ava_string.h`).
   `ui/builtins.cpp::ToLogString` lo llama con un `double` (número no
   entero) y solo existían overloads de enteros -- ambiguaba entre
   `to_string(long long)`/`to_string(uint64_t)`. Fix: overload de
   `double` con formato fijo de 6 decimales (mismo default que
   `std::to_string(double)` real), sin `snprintf`/libc.
6. **`#include <new>` muerto en `BareKernelThread.cpp`**. Mismo patrón
   que los `<cstdint>` crudos ya corregidos en Fase 6 (turno anterior):
   en el toolchain real (`--without-headers`, sin libc) este include
   falla directo porque no hay `<new>` de sistema para envolver, y el
   archivo no usa placement-new en ningún lado -- confirmado con grep.
   Fix: eliminado (el placement-new real, cuando hace falta en otros
   archivos, ya lo provee `ava_new.h`, incluido transitivamente).
7. **`avastd::strlen` y `avastd::string::push_back` faltantes**
   (`ava_string.h`, usados por `BareKernelConsole.cpp`).
   `EmitAnsi` mide secuencias ANSI literales (`"\033[0m"` etc.) y
   `ReadLine` arma la línea leída del kernel carácter a carácter. El
   archivo además incluía `<cstring>` crudo para lo primero -- mismo
   problema de libc ausente que el punto 6. Fix: `strlen` agregado como
   función libre en `avastd` (freestanding: bucle manual; hosted:
   `using std::strlen`), `push_back` agregado a `avastd::string`
   (`append` de 1 carácter), y el `#include <cstring>` eliminado de
   `BareKernelConsole.cpp`.

### 11.4 Qué NO se tocó a propósito

- El asm inline de `ava_setjmp`/`ava_longjmp` (i386, ver `ava_error.cpp`)
  no se pudo ejercitar de verdad sin el cross-compiler -- el `-D__i386__`
  del syntax-check solo habilita la rama de tipos, no valida el código
  máquina generado. Eso solo se puede confirmar con un link real contra
  el kernel o el test standalone de 32 bits que ya se documentó en su
  momento (ver comentario en `ava_error.h`).
- `platform/barekernel/bindings/_template/` (plantilla para bindings de
  otros kernels) no se auditó -- no es parte del target actual.
- No se corrigió nada en `platform/barekernel/ckm_contract.h` ni en
  `ckm_syscall_numbers.h` -- son el contrato/ABI provisto por el kernel,
  no código de AvaLang.

### 11.5 Resumen contra el checklist de la sección 16 (Fase 6)

| Ítem | Estado |
|---|---|
| Crear target freestanding | Cerrado -- ya existía (§11.1) |
| Eliminar dependencias de STL del Core | Cerrado -- Fase 3 + confirmado por el syntax-check de esta fase |
| Eliminar APIs de OS del Core | Cerrado -- Fase 4 (Platform API) + confirmado acá |
| Reducir dependencias del runtime C++ | Cerrado -- sin cambios nuevos, ya reducido |
| Compilar Core sin host OS | Parcial -- syntax-check completo limpio (55 unidades + binding), sin link real por falta del cross-compiler (§11.2) |
| Crear tests freestanding | Pendiente -- no abordado en este turno |

## 12. Fase 7 -- BareKernel: auditoría y cierre parcial

Punto de partida real (no el que asumía el plan): la Fase 1 (`AvaMemory`),
la Fase 4 (Platform API) y la Fase 6 (Freestanding) ya habían dejado
conectados de verdad -- no como stub -- los cinco primeros ítems del
checklist de esta fase:

| Ítem del plan | Estado real encontrado |
|---|---|
| Conectar allocator del kernel | Ya conectado -- `BareKernelMemory.cpp` usa `ckm_malloc`/`ckm_free` desde Fase 1 |
| Implementar console | Ya conectado -- `BareKernelConsole.cpp` usa `ckm_write`/`ckm_read` desde Fase 2/4 |
| Implementar tiempo | Ya conectado -- `BareKernelClock.cpp` usa `ckm_now_ms`/`ckm_highres_now_ns`/`ckm_sleep_ms` |
| Implementar scheduler/threading | Ya conectado, con fallback documentado -- `BareKernelThread.cpp` usa hilos reales si `CKM_CAP_THREADS`, si no ejecuta inline (el target real tiene `CKM_CAP_THREADS=0`, así que hoy corre en modo inline single-threaded, a propósito, ver `binding-status.md`) |
| Implementar filesystem si existe | Ya conectado -- `BareKernelFileSystem.cpp` usa `ckm_open/read/write/close/stat/unlink/mkdir/rmdir` |
| Integrar C ABI | Ya conectado estructuralmente -- `api/src/c_api.cpp` ya formaba parte de `PUBLIC_SOURCES` para el target BareKernel desde antes de esta fase (`runtime/avalang/CMakeLists.txt`: `PUBLIC_SOURCES = CORE_SOURCES + api/src/c_api.cpp`, sin exclusión por target) |

Lo que faltaba de verdad, y era el hueco real de esta fase: **no existía
ningún punto de entrada que arrancara la VM en BareKernel.**
`scripts/build_barekernel.bat` sólo compilaba la librería `avalang`
(comentario explícito en el propio script: "Compila SOLO la libreria
core avalang"). Sin un `_start()`, "Ejecutar VM mínima" / "Ejecutar
bytecode real" / "Ejecutar programas AvaLang reales" no tenían ningún
código que los ejercitara, más allá de la librería en sí.

### 12.1 Trabajo de esta fase

1. **`runtime/avabare/`** (subproyecto nuevo, sólo agregado bajo
   `AVA_TARGET_BAREKERNEL` en el `CMakeLists.txt` raíz): contiene
   `ava_barekernel_runner.cpp`, un `_start()` freestanding que hace
   `ckm_dlopen("/system/lib/libavalang.so")` + `ckm_dlsym` sobre
   `ava_vm_create`/`ava_module_deserialize`/`ava_run`/
   `ava_module_destroy`/`ava_vm_destroy`/`ava_string_free`, lee
   `/apps/hello.avb` con `ckm_open`/`ckm_read` a un buffer estático, y
   ejecuta el módulo. Ver `runtime/avabare/README.md` para el detalle
   completo y las limitaciones (formato `AppHeader` del kernel vs. el
   `.a` que produce este target -- el wrapping final es una herramienta
   del propio kernel, fuera de este repositorio).
2. **Bytecode de muestra real, no un mock.** Se escribió
   `runtime/avalang/tools/gen_barekernel_sample.cpp` (mismo patrón sin
   framework que `test_proto_io_obfuscate.cpp`): construye un `Proto` a
   mano (llama a `print` con una constante string), lo corre con una
   `VM` real, lo serializa con `SerializeProto`, deserializa el
   resultado y lo vuelve a correr para confirmar el round-trip. Este
   turno sí pudo compilar y enlazar un binario **hosted** de verdad en
   este entorno (Linux x86_64, ver §12.2), así que esta herramienta se
   compiló y corrió de verdad contra `libavalang.a`, produciendo
   `runtime/avalang/platform/barekernel/samples/hello.avb` (143 bytes,
   verificado con round-trip de deserialización antes de escribirse a
   disco).
3. **Syntax-check del runner** con la misma receta de la Fase 6
   (`-fsyntax-only -ffreestanding -fno-exceptions -fno-rtti
   -DAVA_TARGET_BAREKERNEL -DAVA_BAREKERNEL_TARGET_BINDING -D__i386__` +
   las `CKM_CAP_*` reales de `binding.cmake`): limpio, sin warnings.
4. **`scripts/build_barekernel.bat`** actualizado para compilar también
   el target `ava_barekernel_runner`, no sólo `avalang`.

### 12.2 Build hosted real logrado en este entorno (hallazgo nuevo)

A diferencia de las fases anteriores, esta vez sí se pudo compilar
`avalang` de punta a punta en este entorno (no sólo `-fsyntax-only`):
se instalaron `cmake`/`ninja`/`antlr4` vía `apt` (dominios permitidos).
El jar de ANTLR4 empaquetado por Ubuntu (4.9.2) resultó incompatible con
el runtime C++ del sistema (4.10 -- cambio de API real,
`antlr4::internal::OnceFlag` no existe en el código generado por el jar
viejo contra el runtime nuevo). Se desinstaló el runtime C++ para forzar
la ruta ya prevista por el propio `CMakeLists.txt` (frontend stub,
`frontend_stub.cpp`) y la librería completa (44 unidades de
`CORE_SOURCES` + backend Linux + `c_api.cpp`) compiló y enlazó limpio
como `libavalang.a` estática. Esto no reemplaza el build freestanding
real contra `i686-elf-g++` (sigue sin estar disponible aquí, ver
`binding-status.md`), pero permitió generar y verificar bytecode real
en vez de sólo teorizar sobre su formato.

### 12.3 Qué sigue sin cerrar (honesto, no un olvido)

- **Build freestanding real contra `i686-elf-g++`.** Sigue sin estar
  disponible el cross-compiler en este entorno (paquete fuera de los
  dominios de red permitidos) -- mismo bloqueo que Fase 6.
- **Wrapping a `AppHeader` y despliegue en la imagen de disco.**
  Herramienta del propio proyecto del kernel (`ab.exe`), no de este
  repositorio -- `ava_barekernel_runner.a` queda listo para ese paso,
  no lo reemplaza.
- **Ejecución real contra el kernel (QEMU o hardware).** No realizada
  -- no hay kernel ni emulador disponibles en este entorno.
- **`libstdc++` / excepciones C++ portadas.** Sigue siendo el
  obstáculo crítico documentado desde `kernel.md` §6.1 -- no lo
  resuelve esta fase (el Core ya no las necesita gracias a `avastd`,
  pero el binding real todavía declara `CKM_CAP_LIBSTDCPP=0` /
  `CKM_CAP_STD_EXCEPTIONS=0`).
- El path fijo `/apps/hello.avb` / `/system/lib/libavalang.so` en el
  runner asume una convención de despliegue que nadie automatiza
  todavía en este build -- documentado en `runtime/avabare/README.md`,
  no oculto.

### 12.4 Resumen contra el checklist de la sección 16 (Fase 7)

| Ítem | Estado |
|---|---|
| Conectar allocator del kernel | Cerrado -- ya lo estaba desde Fase 1 (§12) |
| Implementar console | Cerrado -- ya lo estaba desde Fase 2/4 (§12) |
| Implementar tiempo | Cerrado -- ya lo estaba desde Fase 4 (§12) |
| Implementar scheduler/threading | Cerrado con fallback documentado -- inline mientras `CKM_CAP_THREADS=0` (§12) |
| Implementar filesystem si existe | Cerrado -- ya lo estaba desde Fase 4 (§12) |
| Integrar C ABI | Cerrado -- ya compilaba para el target, confirmado (§12) |
| Ejecutar VM mínima | Cerrado parcialmente -- `ava_barekernel_runner` escrito y syntax-checkeado (§12.1); no corrido contra un kernel real (§12.3) |
| Ejecutar bytecode real | Cerrado parcialmente -- bytecode real generado, corrido y verificado por round-trip en un build hosted de este entorno (§12.1, §12.2); no corrido todavía dentro de BareKernel mismo |
| Ejecutar programas AvaLang reales | Pendiente -- el programa de muestra es un `Proto` construido a mano, no un `.ava` compilado por el frontend ANTLR (ver §12.2, el jar disponible no es compatible con el runtime instalable en este entorno) |
