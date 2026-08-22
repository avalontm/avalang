# Fase 5 (GC) — Sub-fase 1: Diseño de ownership

Ref: `avalang_runtime_stl_barekernel_plan.md`, §16 Fase 5, primer ítem
("Diseñar ownership"), y §8 (arquitectura general del futuro `AvaGC`).

## 1. Hallazgo: el modelo de ownership actual tiene un bug real

El runtime ya tiene un mecanismo de refcounting manual (`Object::ref_count`,
`Retain()`/`Release()` en `src/vm/value.cpp`). Funciona bien para valores
que viven en registros/frames de la VM, donde el propio bytecode hace
`Retain`/`Release` al empujar/pisar un registro.

**Pero para contenedores compuestos (`ListObj`, `DictObj`, `InstanceObj`,
`ClassObj`, `ModuleObj`, `Closure`/`Upvalue`, `BoundMethod`) el patrón no
se aplica de forma consistente.** Relevé cada sitio de escritura del árbol
completo (VM, compiler, C API, builtins) y confirmé:

- `ava_list_append` (`api/src/c_api.cpp:420`), el opcode de `list.append`
  (`vm_containers.cpp:25`), `ava_dict_set` (`c_api.cpp:467/470`), los
  opcodes de índice de dict (`vm_containers.cpp:108/111`), la asignación
  de atributos de instancia (`vm_classes.cpp:190/204`, `vm.cpp:229/233`,
  `vm_call_op.cpp:51/55`), y las claves de clase/módulo en el compiler
  (`compiler.cpp:2030-2225`) — **ninguno llama `Retain()` sobre el valor
  que insertan.**
- Ningún `Object` compuesto tiene destructor propio: todos usan
  `~Object() = default`. Cuando `Release()` hace `delete v.obj` sobre un
  `ListObj`/`DictObj`/etc., el `avastd::vector<Value>`/
  `avastd::unordered_map<..., Value>` interno sí destruye sus `Value`
  (verificado: `ava_vector.h`/`ava_unordered_map.h` hacen placement-new +
  `.~T()` explícito, no `memcpy` crudo asumiendo POD) — pero como `Value`
  no tiene destructor propio, ese "destruir" no libera nada.

**Consecuencia real, no hipotética:** un `Value` ref-counted guardado en
una lista/dict/instancia no tiene ninguna referencia retenida en su
nombre. Si el llamador libera su copia después de insertarlo, el
contenedor queda con un puntero colgante — no es un leak, es
use-after-free. Y a la inversa, si en algún punto sí se retiene al
insertar (ver más abajo) pero nunca se libera al destruir el contenedor,
sí hay leak.

**Dato clave que confirma que el propio equipo ya conocía y resolvió bien
este problema, solo que en un único lugar:** `VM::SetGlobal`/`GetGlobal`
(`vm_core.cpp:144-165`) sí hace `Retain` al guardar, `Release` al
sobrescribir, y `VM::~VM()` hace `Release` de todo `globals_` al destruir.
El comentario ahí mismo describe exactamente esta clase de bug
("dangling read... double-free... heap corruption that surfaces later as
an unrelated-looking crash"). `vm_arith.cpp:25/29` (concatenación de
listas) también lo hace bien, ad-hoc, solo para ese caso. El resto del
árbol —la gran mayoría— no.

## 2. Opciones de diseño

### Opción A — Parchear cada sitio de mutación a mano
Agregar `Retain()`/`Release()` en cada uno de los ~25 sitios relevados, y
agregar destructores explícitos a cada `Object` compuesto que iteren sus
miembros y hagan `Release()`.

- Contra: son ~25 sitios repartidos en VM, compiler, C API y builtins.
  Cualquier código nuevo que agregue un sitio de mutación (y los va a
  haber) puede repetir el mismo bug si no lo recuerda explícitamente. No
  escala.

### Opción B — Hacer que `Value` sea RAII (recomendada)
Dar a `Value` constructor de copia, operator= de copia, constructor/
operator= de movimiento, y destructor:

```cpp
Value(const Value& other) : type(other.type) {
    /* copiar union */ Retain(*this);
}
Value& operator=(const Value& other) {
    if (this == &other) return *this;
    Release(*this);
    /* copiar union */
    Retain(*this);
    return *this;
}
Value(Value&& other) noexcept { /* mover, other pasa a Nil */ }
Value& operator=(Value&& other) noexcept { Release(*this); /* mover */ }
~Value() { Release(*this); }
```

Como ya verifiqué que `avastd::vector<T>` y `avastd::unordered_map<K,V>`
construyen/destruyen elementos de verdad (no `memcpy`), y que en build
hosted son alias directos de `std::vector`/`std::unordered_map` (mismo
comportamiento estándar), **este cambio en un solo lugar (`value.h`/
`value.cpp`) corrige automáticamente los ~25 sitios sin tocarlos**: cada
`items.push_back(x)`, `attrs[k] = v`, `entries.emplace_back(...)` pasa a
retener correctamente porque copia un `Value`, y cada destrucción de
`ListObj`/`DictObj`/etc. libera correctamente porque destruye sus
`Value` miembro a miembro.

- Contra / riesgo real: **los sitios que YA hacen `Retain`/`Release`
  manual quedarían haciendo doble-retain / doble-release** una vez que
  la copia los haga automáticamente. Hay que auditarlos y quitar el
  manual en cada uno:
  - `vm_core.cpp`: `SetGlobal`/`GetGlobal`/`~VM()` (globals_)
  - `vm_arith.cpp:25,29` (concat de listas)
  - `vm.cpp`, `vm_call_op.cpp`, `vm_classes.cpp`, `vm_import.cpp`,
    `vm_task.cpp` (frames/upvalues/coroutines) — hay que revisar cada
    uno de los ~37 call sites de `Retain`/`Release` existentes, no solo
    borrarlos a ciegas: algunos podrían estar reteniendo un `Object*`
    crudo que no pasa por el constructor de copia de `Value` (ej. casts
    directos), esos sí quedan necesarios.
  - Esta auditoría es el trabajo real de la siguiente sub-fase, no es
    trivial y toca código de ejecución caliente (arithmetic, opcodes de
    llamada) — se hace con cuidado, un archivo a la vez, no en un solo
    parche.

## 3. Recomendación

Opción B. Es la que escala y es la que corresponde a "diseñar ownership"
en el sentido que pide el plan (una política uniforme, no fixes puntuales).
Pero se implementa en sub-fases propias, en este orden, cada una
verificable por separado:

1. **(esta)** Diseño + hallazgo documentado.
2. Agregar copy/move ctor/assign + destructor a `Value`, sin tocar todavía
   ningún call site existente de `Retain`/`Release` manual (va a haber
   doble-retain temporalmente — se documenta como conocido, no se corrige
   todavía).
3. Auditar y limpiar, archivo por archivo, los ~37 call sites manuales
   existentes (`vm_core.cpp`, `vm_arith.cpp`, `vm.cpp`, `vm_call_op.cpp`,
   `vm_classes.cpp`, `vm_import.cpp`, `vm_task.cpp`) para no dejar doble
   retain/release.
4. Agregar destructores explícitos donde haga falta liberar recursos que
   *no* son `Value` (p. ej. `NativeObj::user_data`, si aplica).
5. Con ownership ya sólido: recién ahí "Registrar objetos" / "Crear
   roots" / "Implementar tracing" (para ciclos) tienen una base
   correcta sobre la cual construirse — un tracing GC sobre un
   refcounting con punteros colgantes no sirve de nada.

## 3.1 Sub-fase 3 — cerrada

Auditados y corregidos, archivo por archivo, los ~37 call sites
manuales de `Retain`/`Release` que quedaban duplicando lo que el
constructor/destructor de `Value` (sub-fase 2) ya hace solo:
`vm_core.cpp`, `vm_import.cpp`, `vm.cpp`, `vm_call_op.cpp`,
`vm_classes.cpp`, `vm_task.cpp`, `vm_arith.cpp`. Verificado que no
quedó ningún call site fuera de `value.h`/`value.cpp` en todo `src/`
(grep sobre los 39 `.cpp` del runtime), y que los 36 que compilan en
este entorno (los 3 restantes fallan por dependencias ANTLR ausentes,
no relacionadas) siguen compilando limpio con
`g++ -std=c++17 -fsyntax-only`.

No todos eran leaks inofensivos como se esperaba al planear esta
sub-fase — varios eran **doble-release real**, más serio:

- `VM::~VM()` liberaba cada global manualmente y el destructor
  automático de `globals_` (ahora que `Value` es RAII) los volvía a
  liberar — doble release en cada apagado de VM.
- `VM::SetGlobal`, `SetNestedNamespace` (vm_import.cpp), `OpSetUpval`
  (vm.cpp/vm_call_op.cpp) y las 4 ramas de `OpSetAttr`
  (vm_classes.cpp) hacían `Release(x)` manual justo antes de una
  asignación de `Value` que ya libera el valor viejo por su cuenta —
  mismo patrón repetido: doble release sobre el valor reemplazado.
- El resto (retains manuales duplicados en `OpNewClass`,
  `OpNewInstance`, ramas de `OpGetAttr`, `OpClosure`/`CLOSURE`,
  `CreateCoroutine`, `StartAsyncCall`/`StartAsyncBoundCall`,
  `SettleTask`, concat de listas en `OpAdd`) sí eran solo leaks, como
  se esperaba: referencias de más que nunca se liberaban, sin riesgo
  de use-after-free.

Regla aplicada en cada sitio: si el valor llega **por copia** (parámetro
por valor, copia local desde un contenedor) su propia ownership ya
queda cubierta por el constructor/destructor automático de esa copia;
si se **asigna** dentro de un contenedor (`it->second = x`,
`emplace_back(...)`, `attrs[k] = x`), el `operator=`/constructor de
copia de `Value` ya retiene lo nuevo y libera lo viejo — el manual que
quedaba de la era pre-RAII se borra, no se "ajusta".

## 5. Sub-fase 4 — "Registrar objetos" — cerrada

Ref: checklist de Fase 5 en `avalang_runtime_stl_barekernel_plan.md`,
segundo ítem. Con ownership sólido (sub-fases 1-3), esto es la base real
para "Crear roots" / "Implementar tracing" / "Manejar ciclos": un
mark-sweep necesita poder recorrer *todos* los objetos vivos, no solo los
alcanzables desde donde arranca el marcado.

**Diseño: registro intrusivo**, no una lista aparte que cada sitio de
`new` tendría que alimentar a mano. `Object` (la base de
`StringObj`/`ListObj`/`DictObj`/`ClassObj`/`InstanceObj`/`ModuleObj`/
`NativeObj`/`BoundMethod`/`ExceptionObj`/`Closure`) ahora tiene
constructor/destructor propios (antes `~Object() = default`, sin
constructor declarado) que se registran/desregistran solos en una lista
doblemente enlazada global (`gc_prev`/`gc_next` en cada `Object`, cabeza y
mutex en `value.cpp`, TU-local). Igual que la sub-fase 2 hizo con `Value`
para no tocar los ~25 sitios de mutación uno por uno, esto evita tocar los
~35 sitios de `new StringObj(...)` / `new ListObj()` / etc. repartidos en
VM, compiler, C API y builtins: el registro pasa por la base de la
jerarquía, no por cada punto de creación.

API expuesta (`value.h`), pensada para que las próximas sub-fases la
consuman sin que este archivo necesite saber nada de roots/tracing:

- `GcLiveObjectCount()` — cuenta O(1) (contador aparte, no recorre la
  lista) de objetos vivos ahora mismo. Sirve ya mismo como chequeo de
  leaks: si baja a 0 cuando todo sale de scope, el refcounting de las
  sub-fases 2-3 sigue siendo correcto.
- `GcForEachObject(visit, ctx)` — snapshot de la lista bajo el mismo lock
  del registro. `visit` no debe crear ni destruir `Object` (no es
  reentrante). Esta es la función que "Crear roots"/"Implementar tracing"
  van a usar para el barrido (sweep): todo objeto no marcado durante el
  trace de roots, en este recorrido, es basura de un ciclo.

Protegido con `avastd::mutex` (existe en ambos targets: alias de
`std::mutex` en build hosted, syscalls CKM en barekernel; no-op segura si
`CKM_CAP_THREADS=0`, que es el estado actual del kernel) porque coroutines
o tasks async podrían crear/destruir objetos concurrentemente más
adelante -- más barato blindarlo ahora que auditar de nuevo bajo GC real.

Verificación (no solo compilación):

- `g++ -std=c++17 -fsyntax-only` sobre los 36 `.cpp` compilables del
  árbol (los mismos 3 de siempre fallan por ANTLR ausente, no relacionado)
  — limpio, cero cambios de comportamiento fuera de `value.h`/`value.cpp`.
- Smoke test en runtime (no solo sintaxis) contra `value.cpp` real:
  confirma que crear/destruir `Value` de tipo ref-counted sube/baja
  `GcLiveObjectCount()` 1 a 1, que **copiar** un `Value` (mismo `Object*`,
  refcount+1) no crea una entrada nueva en el registro, y que
  `GcForEachObject` visita exactamente los objetos vivos en cada momento.

Con esto, "Registrar objetos" queda cerrado. Lo que sigue en el checklist
es "Crear roots" (identificar el conjunto de `Value` raíz: registros de
cada `CallFrame` activo, `globals_`, pilas de coroutines/tasks suspendidas)
y después "Implementar tracing" (marcar desde esos roots recorriendo
`ListObj::items`, `DictObj::entries`, `InstanceObj::attrs`,
`ClassObj::attrs`/`instance_defaults`, upvalues de `Closure`, etc.) —
recién con roots + tracing tiene sentido "Manejar ciclos" (sweep de lo no
marcado usando exactamente `GcForEachObject`).

## 6. Sub-fase 5 — "Crear roots" — cerrada

Ref: checklist de Fase 5, tercer ítem. Con el registro de la sub-fase 4
(`GcForEachObject`) ya se puede recorrer *todo* objeto vivo -- lo que
falta para que un mark-sweep tenga sentido es el otro extremo: el
conjunto de `Value` que son raíz, es decir, que siguen vivos sin depender
del refcount de ningún otro `Object` del grafo. Todo lo alcanzable desde
ahí (siguiente sub-fase, "Implementar tracing") está vivo; lo que
`GcForEachObject` encuentre y el tracing no haya marcado es basura de un
ciclo -- esa es literalmente la sub-fase "Manejar ciclos" que sigue.

**Qué cuenta como raíz, y por qué:**

- `VM::globals_` -- cada `Value` ahí vive porque el nombre global lo
  referencia, no porque otro objeto lo contenga.
- Cada registro de cada `CallFrame` en `VM::frames_` (stack activo) y en
  `VM::saved_frames_` (stacks de llamadas async/coroutine anidadas que
  quedaron pausadas a mitad de un `await`, ver el comentario junto a ese
  campo en `vm.h`) -- un registro de frame es la raíz más directa que hay:
  literalmente lo que el bytecode tiene en la mano en este instante.
- `VM::pending_exception_` y `VM::yielded_values_` -- estado suelto de la
  VM que no vive dentro de ningún frame pero sí puede referenciar un
  objeto.
- Las `frames`/`yielded_values`/`entry` de cada `Coroutine` viva
  (`created_coroutines_`) y el `result`/`error` de cada `TaskObj` vivo
  (`created_tasks_`). Estos dos tipos **no son `Object`** -- no participan
  del refcounting (ver el comentario ya existente junto a
  `LiveCoroutineCount()`/`LiveTaskCount()` en `vm.h`), viven mientras vive
  la VM. Por eso cualquier `Value` que guarden es raíz igual que un
  global, nunca algo que el tracing vaya a encontrar por otro camino.

**Qué NO se agregó a propósito:** `current_coroutine_` y cada entrada de
`coroutine_resumers_`/`TaskObj::awaiters` son punteros a `Coroutine`/
`TaskObj` que *siempre* fueron empujados a `created_coroutines_`/
`created_tasks_` al crearse (verificado con grep: los tres `new
Coroutine()`/tres `new TaskObj()` del árbol hacen el push in situ) -- así
que recorrer esas dos listas ya cubre todo, sin necesidad de tratar esos
punteros sueltos como una fuente de raíces aparte.

**Implementación:** `VM::CollectGcRoots(vector<Value*>& out)` (nuevo,
`vm.h`/`vm_core.cpp`), apoyado en dos funciones libres nuevas en
`coroutine.h`/`coroutine.cpp` (`CollectFrameRoots`, reusada tanto para
`frames_` como para cada `Coroutine::frames`, y `CollectCoroutineRoots`) y
una en `task.h`/`coroutine.cpp` (`CollectTaskRoots`, junto a `TaskObj`
donde tiene sentido leerla). No toca ningún sitio de mutación existente --
es de solo lectura, junta punteros, no cambia ownership de nada.

Verificación (no solo compilación):

- `g++ -std=c++17 -fsyntax-only` sobre los 36 `.cpp` compilables -- limpio
  (mismos 3 de siempre fallan por ANTLR ausente, no relacionado).
- Los 20 `.cpp` de `src/vm/` compilan y **linkean** juntos con la
  implementación real de `platform/linux/` (no un stub) usando
  `-ffunction-sections -Wl,--gc-sections` para descartar en el link el
  único símbolo no resoluble en este entorno (`VM::RunFile` -> parser
  ANTLR, nunca invocado por el test).
- Smoke test en runtime contra el binario real: crea una `VM`, un global
  vía `SetGlobal` (API pública), y una `Coroutine` real vía
  `VM::CreateCoroutine` con una `Closure`/`Proto` mínima armada a mano
  (sin pasar por el compilador). Confirma que `CollectGcRoots` encuentra
  exactamente el `Value*` que apunta al `Object*` del global, al valor
  puesto a mano en `Coroutine::yielded_values`, y a la `Closure` misma
  (`Coroutine::entry`) -- 5 raíces en total, el número esperado para ese
  estado (1 global + `pending_exception_` + `yielded_values_` + 2 de la
  coroutine). No hay crash en destrucción de la `VM` con la coroutine
  todavía viva.

Con roots + el registro de la sub-fase 4 ya están las dos puntas que un
mark-sweep necesita. Sigue "Implementar tracing": desde cada raíz,
recorrer `ListObj::items`, `DictObj::entries`, `ClassObj::attrs`/
`instance_defaults`, `InstanceObj::attrs`, `ModuleObj::attrs`, y los
`upvalues` de cada `Closure` (via su `shared_ptr<Upvalue>`, ver
`closure.h`), marcando cada `Object` alcanzado. Recién con eso "Manejar
ciclos" (sub-fase siguiente) tiene sentido: barrer con `GcForEachObject`
lo que quedó sin marcar.

## 7. Sub-fase 6 — "Implementar tracing" — cerrada

Ref: checklist de Fase 5, cuarto ítem. Con registro (sub-fase 4) y roots
(sub-fase 5) ya resueltos, esto es la pieza que faltaba para poder
distinguir "vivo" de "basura de ciclo": recorrer desde cada raíz y marcar
todo `Object` alcanzado. Archivos nuevos: `src/vm/gc_trace.h`/
`gc_trace.cpp` (agregado a `CORE_SOURCES` en `runtime/avalang/CMakeLists.txt`
-- es una lista explícita de archivos, no un glob, así que un `.cpp`
nuevo no se compila solo si no se agrega ahí).

**`GcTraceMark(roots, out_marked)`**: por cada raíz, `MarkValue` chequea
`IsRefCounted()`/`obj` y despacha a `MarkObject(obj, type, marked)`. El
`type` viene del propio `Value` -- `Object` no sabe qué es sin ese tag
(mismo truco que ya usa `Value::IsRefCounted()`). `MarkObject` seguido
recorre según el tipo:

- `List`/`Dict`/`Module`/`Bound` -- recorren sus `Value` hijos directo
  (`items`, `entries`, `attrs`, `instance`).
- `Function` (`Closure`) -- recorre `upvalues` (`shared_ptr<Upvalue>`,
  `closure.h`). `Upvalue` no es un `ValueType` (se guarda por
  `shared_ptr`, no por `Value` con tag), así que se marca aparte, sin
  pasar por `MarkObject`. Su `location` (puntero al registro de frame
  mientras sigue "abierto") no se sigue -- no es una arista de ownership,
  ese `Value` ya es raíz por su cuenta o el upvalue ya tiene su copia en
  `value` cuando el frame cierra.
- `Instance`/`Class` -- recorren sus `attrs`/`instance_defaults` **y
  además marcan `InstanceObj::cls`/`ClassObj::base_class`**, que son
  punteros crudos sin retener (`vm_classes.cpp::OpNewInstance` hace
  `inst->cls = cls;` sin `Retain()` -- gap de ownership real, preexistente,
  no introducido acá). El tracing los trata igual como arista real del
  grafo de vida: si no se marcaran, un futuro sweep podría barrer un
  `ClassObj` con una instancia todavía viva apuntándolo. Esto NO arregla
  el gap en sí (eso es una auditoría de ownership aparte, con su propio
  `Retain()`/`Release()`, no de esta sub-fase) -- solo evita que el
  tracing lo agrave.
- `String`/`Exception` -- hojas, sin `Value` hijos.
- `Native` -- **a propósito no recorre** `NativeObj::primitive_this`
  (`ava_value_t` C ABI): convertirlo con `FromC()` construiría un `Value`
  temporal cuyo destructor llamaría `Release()` sobre un objeto que este
  marcado nunca retuvo -- bajaría un refcount de más como side effect de
  "solo mirar". Documentado como pendiente para la sub-fase "Integrar
  native resources" del checklist, que debe resolverlo con su propio
  mecanismo en vez de reusar `FromC`.
- `Coroutine`/`Task` -- nunca llegan acá como `Object` (no lo son, ver
  sub-fase 5); sus `Value` internos ya se recorren como raíces aparte.

Corta ciclos con el propio set de visitados (`marked.find(obj) !=
marked.end()` antes de insertar) -- es literalmente lo que hace posible
marcar un objeto autorreferenciado sin recursión infinita.

Verificación real, no solo compilación (mismo binario linkeado que la
sub-fase 5, `src/vm/*.cpp` + `platform/linux/*.cpp` reales):

- Ciclo **alcanzable**: una lista puesta en un global que se contiene a
  sí misma -- se marca una sola vez, sin loop infinito.
- Ciclo **huérfano**: dos listas que se referencian mutuamente
  (`la` ⇄ `lb`) sin que ningún root las apunte -- confirmado con
  `GcLiveObjectCount()` que siguen vivas (el refcounting nunca las libera
  solas, cada una tiene la ref de la otra: es exactamente el leak que
  motiva Fase 5) y confirmado con el resultado de `GcTraceMark` que
  **no** aparecen marcadas. Esta es la prueba real de que tracing hace lo
  que tiene que hacer: separar "vivo" de "basura de ciclo que el
  refcounting no puede ver".
- Arista estructural: `InstanceObj::cls` se marca aunque no sea un
  `Value` -- confirmado buscando el `ClassObj*` en el set de marcados sin
  que ningún `Value` lo referencie directo, solo alcanzable via
  `inst->cls`.
- `g++ -std=c++17 -fsyntax-only` sobre los 37 `.cpp` que compilan en este
  entorno (mismos 3 de siempre fuera, ANTLR ausente) -- limpio.

Con esto, "Registrar objetos" + "Crear roots" + "Implementar tracing"
(las tres piezas de un mark-sweep) están cerradas y verificadas cada una
por separado. Sigue **"Manejar ciclos"**: correr `GcTraceMark` sobre
`VM::CollectGcRoots()`, después recorrer `GcForEachObject` (sub-fase 4) y
`delete` lo que no esté en el set marcado -- más la decisión de diseño
que falta (cuándo correr esto: ¿threshold de objetos vivos, cada N
allocs, manual via API?) y qué pasa con las referencias externas vía C
API (`ava_value_t`/`AvaRef.id`) que un sweep no debería tocar sin que el
host las haya soltado.

## 8. Sub-fase 7 — "Manejar ciclos" — cerrada

`GcObjectKind` (`value.h`) es un tag nuevo guardado en el propio `Object`
(`gc_kind`), separado de `ValueType` (que alimenta la C ABI publica via
`ToC()`/`AvaValueType` y no se puede tocar sin riesgo de romper el
contrato binario). Hacia falta porque `GcForEachObject` (sub-fase 4) solo
da `Object*` sin tag, y el sweep necesita saber el tipo dinamico real de
cada objeto vivo para poder limpiarlo antes de `delete` -- no solo de los
alcanzables desde un root con `Value` tipado como `GcTraceMark` (sub-fase
6). Los 11 subtipos (`String`/`List`/`Dict`/`Function`/`Instance`/
`Class`/`Native`/`Bound`/`Exception`/`Module`/`Upvalue`) lo pasan a
`Object` en su constructor; `Object(GcObjectKind)` es `explicit` y sin
default, asi que un subtipo nuevo que se olvide de pasarlo no compila.

`gc_sweep.h`/`gc_sweep.cpp` implementan `GcCollectCycles(VM&)`:

1. `VM::CollectGcRoots()` + `GcTraceMark()` -- igual que sub-fase 6, arma
   el set de `Object*` alcanzables.
2. `GcForEachObject()` -- snapshot de todo lo vivo ahora mismo.
3. Basura = snapshot menos marcados, **excluyendo siempre `Upvalue`**:
   un `Upvalue` vive por `shared_ptr` (`Closure::upvalues`), no por este
   mecanismo -- borrarlo con `delete` directo, ademas del `shared_ptr`
   que ya lo posee, seria un double-free. Puede aparecer sin marcar (si
   ningun `Closure` vivo lo referencia) y de todos modos no es candidato:
   su ciclo de vida lo decide el ultimo `shared_ptr` que lo suelta, no el
   sweep.
4. Antes de tocar nada, cada objeto de la basura recibe un bias de
   `ref_count` +1. `GcClearRefs()` (switch sobre `gc_kind`) despues limpia
   de verdad los contenedores de `Value` de cada uno (`items`, `entries`,
   `attrs`, `instance_defaults`, `upvalues`, el `Value` suelto de
   `BoundMethod::instance`), lo que dispara `Release()` real sobre cada
   hijo -- incluidos los hijos que son *otros miembros del mismo ciclo de
   basura*. Sin el bias, esas liberaciones cruzadas podrian hacer que un
   miembro del ciclo llegue a refcount 0 y se autodestruya (via el
   `delete` que ya hace `Release()`) antes de que el propio loop del
   sweep llegue a el -- y cuando llegara, el `delete` explicito del paso
   siguiente seria un double-free sobre memoria ya liberada. El bias
   garantiza que ningun miembro del ciclo toque 0 durante este paso.
5. Recien ahi, `delete` explicito sobre cada objeto de la basura: como
   sus contenedores ya estan vacios, el destructor real no dispara
   ninguna cascada adicional.

Campos que **no** se tocan en `GcClearRefs` a proposito, documentado en
`gc_trace.cpp`/`gc_trace.h`: `InstanceObj::cls`/`ClassObj::base_class`
(punteros crudos, no retenidos, no son `Value`) y `NativeObj::
primitive_this`/`user_data` (C ABI / puntero opaco de host).

`VM::CollectGarbage()` (`vm.h`/`vm_core.cpp`) expone `GcCollectCycles`
sobre la propia VM, documentando por que sigue sin dispararse automatico:
durante la ejecucion de un opcode puede haber `Value` temporarios solo en
la pila nativa de C++ (no en `frames_`, no en ningun root) que un sweep a
mitad de ejecucion veria como basura y liberaria de mas. Es una decision
real, no un olvido -- pensado para correr entre ejecuciones completas, o
via `ava_vm_collect_garbage()` (C API, opcional) desde un host que sepa
que no hay temporarios de pila vivos en ese momento.

Con esto, "Manejar ciclos" cierra el mark-sweep completo sobre el
refcounting existente: `GcObjectKind`/`gc_kind` (registro tipado),
`CollectGcRoots` (sub-fase 5), `GcTraceMark` (sub-fase 6) y
`GcCollectCycles`/`VM::CollectGarbage` (esta sub-fase) juntos resuelven
exactamente el caso que motivo Fase 5: dos objetos que se referencian
mutuamente sin que ningun root los alcance, invisibles para el
refcounting solo, ahora detectables y liberables bajo demanda.

## 4. Qué NO cubre esto todavía

Refcounting (aunque esté bien implementado) **no resuelve ciclos**
(closure que se referencia a sí misma via upvalue, instancia con
referencia circular a otra instancia, etc.). Eso es un ítem aparte del
checklist de Fase 5 ("Manejar ciclos") y requiere un mark-sweep real por
encima de este refcounting, no reemplazarlo. Se aborda después de que el
refcounting esté correcto — hoy no tiene sentido diseñar el cycle
collector sobre una base que todavía tiene use-after-free.

## 9. Sub-fase 8 — "Integrar closures" — cerrada

Ref: checklist de Fase 5, sexto ítem. El caso motivador es el que quedó
anotado en §4: una `Closure` cuyo `Upvalue::value` termina apuntando,
directa o transitivamente, a esa misma `Closure` (closure que se
referencia a sí misma via upvalue) -- un ciclo invisible para el
refcounting puro, exactamente lo que Fase 5 existe para resolver.

Auditado el camino completo que ya existe desde las sub-fases 4-7 y
confirmado que ya cubre este caso sin necesitar código nuevo:

- `Closure` está registrada como `GcObjectKind::Function` (sub-fase 4,
  `value.h`/`closure.h`) y `Upvalue` como `GcObjectKind::Upvalue` propio,
  excluido a propósito del `delete` directo del sweep porque vive por
  `shared_ptr` (`Closure::upvalues`), no por este mecanismo.
- `GcTraceMark` (sub-fase 6, `gc_trace.cpp`) ya recorre `Closure::upvalues`
  cuando el `type` es `Function`: marca cada `Upvalue` (`shared_ptr`,
  aparte de `MarkObject` porque no es un `ValueType`) y llama `MarkValue`
  sobre `Upvalue::value` -- si ese `value` es a su vez de tipo `Function`
  y apunta a la misma `Closure` (o a otra que vuelve a apuntar a la
  primera), el corte de ciclos ya existente (`marked.find(obj) !=
  marked.end()` antes de insertar) evita la recursión infinita igual que
  para listas/dicts autorreferenciados -- no hay rama especial para
  `Closure` que falte.
- `GcClearRefs` (sub-fase 7, `gc_sweep.cpp`) ya tiene el caso `Function`:
  `static_cast<Closure*>(obj)->upvalues.clear()` suelta cada
  `shared_ptr<Upvalue>`; si esa `Closure` era el único dueño de un
  `Upvalue` cuyo `value` apuntaba de vuelta a ella, liberar el
  `shared_ptr` destruye el `Upvalue`, cuyo `~Value()` (RAII, sub-fase 2)
  dispara `Release()` sobre esa `Closure` -- exactamente el mismo `Value`
  que el bias de `ref_count +1` (paso 4 del sweep, sub-fase 7) ya protege
  contra un `delete` prematuro mientras el propio loop de basura sigue
  procesando el resto del ciclo.

Es decir: el mark-sweep genérico ya trata `Function`/`Upvalue` como
cualquier otro contenedor con hijos -- no hacía falta ninguna rama
especial de "closures" en tracing ni en sweep, porque ninguna de las dos
sub-fases anteriores dejó ese caso afuera. Lo que sí quedaba sin cerrar
formalmente era confirmar por escrito que el caso concreto que motivó
este ítem del checklist (§4) efectivamente cae dentro de lo ya
implementado, en vez de asumirlo.

**Qué queda fuera de esta sub-fase, a propósito:** el ciclo de vida de
`Coroutine`/`TaskObj` (y por lo tanto de cualquier `Closure` guardada en
`Coroutine::entry`) es independiente de este mark-sweep -- ninguno de los
dos es `Object`, y `VM::CollectGcRoots` trata *todo* `created_coroutines_`/
`created_tasks_` como raíz permanente sin importar `CoStatus::Dead` o
`TaskObj::done`, por diseño (viven mientras vive la VM, ver §6). Una
`Closure` alcanzable solo a través de una coroutine ya muerta no es basura
de ciclo para este sweep: sigue "viva" mientras la VM no se destruya. Eso
es exactamente el ítem siguiente del checklist, "Integrar coroutines", que
debe decidir si ese permanent-root es el comportamiento final deseado o si
hace falta soltar `created_coroutines_`/`created_tasks_` cuando terminan
-- no se toca acá para no mezclar los dos ítems.

## 10. Sub-fase 9 — "Integrar coroutines" — cerrada

Ref: checklist de Fase 5, séptimo ítem, y el gap que quedó anotado al
cerrar §9: `VM::CollectGcRoots` trata *todo* `created_coroutines_` como
raíz permanente (nunca se saca nada de ahí salvo en `~VM()`), incluidas
las `Coroutine` ya `CoStatus::Dead`. Relevé los tres flujos que resuman
una coroutine (`coroutine()`/`resume()` builtin en `coroutine.cpp` y
`vm_call.cpp`, y el driver interno de `async func` en `vm_task.cpp` --
`StartAsyncCall`, `StartAsyncBoundCall`, `ResumeAwaitingCoroutine`) y
confirmé que ninguno vuelve a leer `Coroutine::entry`/`frames`/
`yielded_values` una vez que `status` pasa a `Dead`: `OpResume`
(`vm_call.cpp`) revienta con "attempt to resume a dead coroutine" antes
de tocar `co->frames`, y `ResumeAwaitingCoroutine` (`vm_task.cpp`)
retorna de entrada si `status != Suspended`. `TaskObj::result`/`error` no
dependen de esto -- se copian a `SettleTask` desde el `result`/`error_val`
local de la propia función, no releídos de `co->frames` después.

**Es decir: una vez `Dead`, esos tres campos son basura real, con dueño
claro (la propia `Coroutine`) -- no basura de ciclo.** No hacía falta
tocar `GcCollectCycles`/`CollectGcRoots` para esto: alcanza con soltarlos
con el `Retain`/`Release` normal de `Value` (sub-fase 2) en el momento
exacto en que se sabe que ya no se van a usar, igual que ya hace
`VM::SetGlobal` al pisar un global.

`ReleaseDeadCoroutineState(Coroutine&)` (nuevo, `coroutine.h`/
`coroutine.cpp`): `entry = Value::Nil()` (dispara `Release()` sobre la
`Closure` vía el destructor de la copia vieja, hecho por el propio
`operator=` de `Value`), `frames.clear()` (cada `CallFrame::registers`/
`pending_await_error_value` se destruye Value por Value) y
`yielded_values.clear()`. Se llama justo después de cada uno de los 5
sitios donde `co->status` pasa a `Dead` (`coroutine.cpp:102`,
`vm_call.cpp:176`, y los tres de `vm_task.cpp:71/151/223` -- los tres
comparten la misma expresión ternaria, confirmado que ninguno vuelve a
leer `frames`/`entry` después).

Con `entry`/`frames`/`yielded_values` ya en `Nil`/vacíos para toda
`Coroutine` muerta, `CollectCoroutineRoots` (sub-fase 5) puede seguir
llamándose sin condición para *todo* `created_coroutines_` -- no hizo
falta filtrar por `status` ahí: un `Value::Nil()` no es ref-counted
(`IsRefCounted()`), así que pushearlo como raíz no cuesta nada y no
mantiene nada vivo. El resultado neto es el mismo que filtrar por
`status == Dead`, pero sin duplicar ese chequeo en dos lugares (el punto
de transición a `Dead` y el recorrido de roots) ni arriesgar un
mark-sweep tratando como "basura de ciclo" un objeto que en realidad
tenía dueño claro (`Coroutine::entry`) -- eso hubiera sido incorrecto:
soltarlo por sweep-delete en vez de por `Release()` normal habría dejado
el `ref_count` desincronizado si algo más también lo referenciaba.

**Qué queda fuera, a propósito:** `TaskObj` (`created_tasks_`) no se toca
-- `result`/`error` de un `TaskObj` ya asentado (`done == true`) siguen
siendo legítimamente accesibles después de asentarse (`await` puede
leerlos en cualquier momento posterior, no hay forma de saber si alguien
va a hacerlo), así que su raíz permanente sigue siendo el comportamiento
correcto documentado en `task.h`, no un gap. Eso deja **"Integrar native
resources"** como el único ítem restante de Fase 5.

## 11. Sub-fase 10 — "Integrar native resources" — cerrada

Ref: checklist de Fase 5, octavo y último ítem. El gap quedó anotado
desde sub-fase 6 (§7 de este documento, comentario en `gc_trace.cpp`):
`NativeObj::primitive_this` (el `this` de un método bindeado sobre un
primitivo, p. ej. `"abc".upper` -- armado en `vm_classes.cpp::OpGetAttr`)
se guarda como `ava_value_t` (C ABI), no como `Value`, así que nunca pasó
por `Retain()` al crearse. El tracing (sub-fase 6) ya documentaba por qué
no lo recorre (convertirlo con `FromC()` ahí sería un `Release()` no
balanceado, side effect de algo que se supone de solo lectura) pero eso
dejaba el problema real sin resolver: **un `NativeObj` de método
primitivo no retiene su `this`**, así que si el objeto original (el
`StringObj`/`ListObj`/`DictObj` sobre el que se llamó el método) se
libera mientras el `NativeObj` bindeado sigue vivo, `primitive_this`
queda colgando -- use-after-free real, no hipotético (confirmado
revisando el único call site que lo arma, `vm_classes.cpp:63`, que nunca
llamaba `Retain`).

**Fix, en tres partes:**

1. **Retener al crear** (`vm_classes.cpp::OpGetAttr`): `Retain(obj)`
   justo después de `native->primitive_this = ToC(obj)`. `ToC()` en sí
   sigue sin retener (no se toca -- es la conversión general Value→C ABI,
   usada en decenas de sitios que sí manejan su propio ownership; cambiar
   su contrato ahí rompería todo lo demás). El `Retain()` vive en el
   único call site que necesita esta semántica extra.
2. **Soltar en destrucción normal** (`value.h`/`value.cpp`): `NativeObj`
   gana destructor propio (antes usaba `~Object() = default` como todo
   el resto -- ver el hallazgo de sub-fase 1, §1, sobre por qué eso es
   seguro para `Value` pero no para un `ava_value_t` suelto sin RAII).
   `~NativeObj()` llama `Release(FromC(primitive_this))` solo si
   `is_primitive_method` -- la contraparte exacta del `Retain()` de
   arriba. `Object::~Object()` ya es `virtual` (sub-fase 4), así que
   `delete` sobre un `Object*` cuyo tipo dinámico es `NativeObj` despacha
   bien a este destructor sin necesitar cambiar ningún sitio de borrado
   existente.
3. **Soltar en sweep de ciclos, sin duplicar** (`gc_sweep.cpp`):
   `GcClearRefs` ahora tiene un caso `Native` -- si `is_primitive_method`,
   suelta ahí mismo (protegido por el bias de `ref_count +1` del paso 4
   de la sub-fase 7, igual que el resto de los casos del switch) y deja
   `is_primitive_method = false`. Es necesario: si soltara solo en el
   destructor, ese `Release()` correría en el paso 3 del sweep (el
   `delete` explícito, **sin** el bias activo para ese momento -- el bias
   protege el paso 2, no el 3), y si el objeto apuntado por
   `primitive_this` fuera *también* miembro del mismo ciclo de basura, un
   `Release()` ahí podría hacerlo `delete` una primera vez dentro del
   destructor y una segunda cuando el propio loop del sweep llegara a su
   turno -- double-free. Dejar `is_primitive_method = false` en
   `GcClearRefs` hace que `~NativeObj()` (que sí corre después, en el
   `delete` del paso 3) sea no-op para cualquier objeto que ya pasó por
   el sweep, sin duplicar el `Release()`.

`user_data` (el otro campo que sub-fase 6 dejó fuera) sigue sin tocarse a
propósito: es un puntero opaco de host (`void*`), nunca un `Object*` de
este runtime -- no hay nada que retener ni liberar acá, es responsabilidad
del propio host que lo registró (`VM::RegisterNative`).

Con esto se cierran las 8 sub-fases del checklist de Fase 5 (GC):
diseño de ownership, registro de objetos, roots, tracing, manejo de
ciclos, closures, coroutines y native resources.
