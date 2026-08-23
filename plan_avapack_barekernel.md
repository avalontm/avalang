# Plan: apps AvaLang para litekernel — `libavalang.so` compartido + `app.exe` (`AppHeader`) que depende de él

> Contraparte de `runtime/avapack/README.md` (empacador desktop, Fase 0-7) pero
> para el target `AVA_TARGET_BAREKERNEL`. Este documento asume que ya leíste
> `docs/kernel/kernel.md`, `docs/kernel/barekernel.md` y `docs/kernel/binding-status.md`.
>
> **Decisión ya tomada, sin alternativa detrás de flags:** el runtime de AvaLang
> vive en `libavalang.so` (un solo archivo, instalado una vez en `/system/lib/`).
> Cualquier app hecha con AvaLang se compila como `app.exe` (formato `AppHeader`
> propio de litekernel — **no PE**, mismo struct que ya arma
> `ExecutableHeaderCreator.cs` de AppBuilder) y depende de ese `libavalang.so` en
> runtime vía `ckm_dlopen`. No hay modo estático/monolítico en este plan — se
> descarta a propósito, ver §2.
>
> **Alcance de formatos, para no confundir:** los scripts de AvaLang tienen
> dos extensiones — `.ava` (lógica) y `.avaui` (UI declarativa) — pero de
> momento **solo `.ava` funciona bien**; `.avaui` todavía no está a la par en
> estabilidad. Por eso este plan asume `.ava` como único entry soportado por
> `ava_barekernel_pack` — ver nota en §3.

> **Actualización — verificado contra el código real de `litekernel.zip`.**
> La versión anterior de este documento inferí varias cosas de
> `docs/kernel/kernel.md` (documentación del lado AvaLang) sin haber leído
> nunca el kernel en sí. Ya se subió `litekernel.zip` y se revisaron
> `src/filesystem/lib_loader.cpp`, `src/core/syscall/categories/dynlink_syscalls.cpp`,
> `src/arch/x86/process/loader.cpp` e `include/core/process/process_types.hpp`
> directamente. Resultado: el modelo "`libavalang.so` compartido + `ckm_dlopen`"
> que este plan asume como ya resuelto **tiene dos bugs reales en el loader
> del kernel** que son bloqueadores más fundamentales que
> `CKM_CAP_LIBSTDCPP=0` — ver Fase B0.5 (§4) para el detalle y la fase nueva
> que agregan antes de construir nada de `avapack_barekernel`. También se
> encontró una inconsistencia de nombres entre `.avb` (barekernel) y `.avbc`
> (avapack desktop, Fase 6) para el mismo formato binario — ver nota en §3.

## 0. Lo que ya existe (para no reabrir por error)

Antes de proponer nada, esto es lo que el repo **ya tiene resuelto** y por qué:

| Pieza | Estado | Dónde |
|---|---|---|
| PAL de AvaLang sobre CKM (9 interfaces) | Implementado, compila `-fsyntax-only` limpio | `runtime/avalang/platform/barekernel/` |
| Binding concreto contra litekernel (`target/`) | Syscalls y capability flags confirmados contra el código fuente del kernel | `runtime/avalang/platform/barekernel/bindings/target/` |
| `avalang` compilado STATIC para este target | Forzado siempre STATIC (CMake `Generic` no sabe armar `.so`) | `runtime/avalang/CMakeLists.txt:186-246` |
| `libavalang.so` (ELF32 `ET_DYN`) | Paso de link manual con `ld -shared --whole-archive` a partir del `.a` de arriba | `runtime/avalang/CMakeLists.txt:397-419`, target `avalang_barekernel_so` |
| Bootstrap `ava_barekernel_runner` | `_start()` freestanding: `ckm_dlopen(libavalang.so)` → `dlsym` de la C API → lee `.avb` de disco → `ava_run` | `runtime/avabare/` |
| Bytecode de muestra real | `.avb` generado con `ava_module_serialize`, no un mock | `runtime/avalang/platform/barekernel/samples/hello.avb` |
| Decisión de formato | Apps = `AppHeader` propio del kernel (igual al que arma `ExecutableHeaderCreator.cs` de AppBuilder). Librerías = ELF32 `ET_DYN` porque el loader del kernel (`lib_loader.cpp`) solo sabe parsear ELF para `.so` | `docs/kernel/kernel.md §2.2` |
| Bloqueador crítico pendiente (AvaLang) | `CKM_CAP_LIBSTDCPP=0` — sin libstdc++ portada, nada de esto corre de verdad en el kernel todavía, solo compila | `docs/kernel/binding-status.md` |
| **Bloqueador crítico pendiente (litekernel, nuevo)** | `LibraryLoader::ApplyRelocations` resuelve `R_386_32`/`PC32`/`RELATIVE`/`GLOB_DAT`/`JMP_SLOT` usando `lib->base_addr` **físico**, antes de que `sys_dlopen` decida `lib->mapped_base` (dirección **virtual**, distinta, elegida después). Cualquier dato absoluto horneado por una relocation (vtable, puntero a string, tabla de function pointers) queda apuntando a la dirección física, no a donde el proceso realmente lo puede leer — `sys_dlsym` sí compensa esto con un offset, pero las relocations ya aplicadas no se tocan más | `src/filesystem/lib_loader.cpp` (`ApplyRelocation`, `ApplyRelocations`), `src/core/syscall/categories/dynlink_syscalls.cpp` (`sys_dlopen`) |
| **Bloqueador operativo pendiente (litekernel, nuevo)** | `process_libraries[MAX_LIBS_PER_PROCESS = 32]` se indexa directo con `proc->get_pid()`, y `get_next_pid()` es un contador monotónico (`next_pid++`, nunca se reutiliza) que cuenta *todos* los procesos lanzados desde el boot, no solo los concurrentes. Pasado el proceso #32 desde el arranque, `AddLibraryToProcess` devuelve `false` para siempre y `sys_dlopen` empieza a fallar para cualquier app — el kernel necesita reiniciarse | `src/core/syscall/categories/dynlink_syscalls.cpp` (`process_libraries`, `AddLibraryToProcess`), `include/core/process/scheduler_base.hpp:71` (`get_next_pid`) |
| Integridad del `AppHeader` | El campo `checksum` (`process_types.hpp:43`) se parsea y se imprime en el log de debug de `Loader::load_from_memory`, pero `Loader::validate_header()` **nunca lo verifica** — hoy es puramente decorativo del lado del kernel | `src/arch/x86/process/loader.cpp` (`validate_header`) |

**Conclusión de la Fase 0 de este plan:** el modelo "runtime compartido
(`libavalang.so`) + `app.exe` delgado que depende de él" **ya es una decisión
tomada y con trabajo real invertido** del lado de AvaLang, no un valor por
defecto que nadie evaluó — y este plan la confirma como la única forma de
hacerlo, sin dejar una variante estática como opción (ver §2 para el motivo).
Lo que cambia con la verificación contra `litekernel.zip` es que **el
mecanismo de `dlopen` del que depende ese modelo no funciona todavía tal
como está implementado hoy en el kernel**, independientemente de que AvaLang
haga todo bien de su lado — ver Fase B0.5.

Lo que **no existe todavía**, y es lo que de verdad falta para que una app
AvaLang se sienta "como una app normal" al estilo AppBuilder, es: **el script
embebido dentro del `app.exe`** en vez de un `.avb` suelto que hay que copiar
aparte al disco.

---

## 1. AppBuilder vs. AvaLang barekernel — comparación exacta

| | **AppBuilder** (tus apps C++) | **AvaLang barekernel hoy** |
|---|---|---|
| Qué se embebe en el `.exe` (`AppHeader`, no PE) | Código de la app + `corlib.a` completo (estático) | Nada — el `.avb` vive suelto en `/apps/` |
| Runtime compartido | Ninguno — `corlib` se linkea entero por app | `libavalang.so`, cargado por `dlopen` en runtime |
| Formato final | `AppHeader` (`ExecutableHeaderCreator.cs`) | `AppHeader` para el runner, `.so` ELF32 para el runtime |
| Tamaño por app | `crt0.o` + objetos de la app + `corlib.a` completo | Runner (~mínimo, unas pocas funciones) + `.avb` aparte |
| Dependencia externa en disco | Ninguna | `libavalang.so` en `/system/lib/`, tiene que estar presente |
| Por qué esa elección | `corlib` es chico (una libc mínima), no vale la pena compartirlo, y así cada app queda 100% autosuficiente | El runtime de AvaLang (VM + intérprete + builtins, eventualmente STL) es un orden de magnitud más grande que `corlib` — replicarlo por app sería carísimo en disco |

La comparación deja ver que **no es la misma pregunta** en los dos casos: `corlib`
y "el VM completo de un lenguaje" no pesan lo mismo. AppBuilder puede darse el
lujo de estático-siempre porque lo que arrastra es chico. Este es el análisis
concreto para AvaLang.

---

## 2. Por qué se descarta estático — y qué queda fijo

Se evaluó (y se descarta a propósito, sin dejarlo como flag) el equivalente a
lo que hace AppBuilder: linkear `libavalang.a` directo dentro de cada app,
sin `.so` ni `dlopen`. Motivo concreto, no genérico: `corlib` (lo que
AppBuilder embebe por app) es una libc mínima — chica. El runtime de AvaLang
(VM + intérprete + builtins, y más adelante STL real cuando se resuelva
`CKM_CAP_LIBSTDCPP=0`, `kernel.md §6.1`) es un orden de magnitud más grande y
**todavía no tocó su piso de tamaño** — va a crecer. Multiplicar ese costo
por cada app instalada, justo antes de que crezca más, es la dirección
equivocada. Por eso queda descartado, no relegado a un flag "por si acaso".

**Lo que queda fijo, sin alternativa:**

- Un solo `libavalang.so`, instalado una vez en `/system/lib/` (ya se construye
  hoy: `runtime/avalang/CMakeLists.txt:397-419`, target
  `avalang_barekernel_so`).
- Cualquier app hecha con AvaLang es un `app.exe` (`AppHeader`) que en su
  `_start` hace `ckm_dlopen("/system/lib/libavalang.so")` + `dlsym` de la C API
  que necesita — igual que ya hace `ava_barekernel_runner` hoy
  (`runtime/avabare/`), sin variante que la salte.
- Consecuencia asumida, no un descuido: `libavalang.so` es una dependencia real
  del sistema. Si no está en `/system/lib/` o la ABI no matchea, ninguna app
  AvaLang arranca. Se lo trata como parte de la imagen base de litekernel
  (como `corlib` hoy para las apps AppBuilder), no como algo opcional.
- Consecuencia asumida también: cada arranque paga el costo de `dlopen`
  (parseo ELF32, relocations) contra saltar directo a `_start`. A cambio,
  actualizar el VM (parchear el intérprete, agregar un builtin) es
  reemplazar **un solo archivo** sin recompilar ni redistribuir ninguna app,
  y N apps instaladas comparten una sola copia del runtime en disco.
- Queda como camino crítico que el loader de `.so` del kernel
  (`lib_loader.cpp`) esté sólido — a diferencia de AppBuilder, que nunca
  necesitó tocarlo.

---

## 3. Lo que hace falta sí o sí: embeber el script en el `AppHeader`

Hoy `ava_barekernel_runner` lee `/apps/hello.avb` como archivo de disco
separado (`runtime/avabare/README.md`, sección "Bytecode de muestra"). Para
que se sienta "como una app normal" (un solo archivo, se copia y corre), el
`.avb` tiene que quedar **dentro** del `AppHeader`, no al lado.

> **Alcance: solo `.ava`, no `.avaui`.** AvaLang tiene dos extensiones de
> script — `.ava` (lógica) y `.avaui` (UI declarativa, parseada por
> `AvauiParser`/`AvauiWriter`) — pero de momento **solo `.ava` funciona
> bien**; `.avaui` todavía no está a la par en estabilidad, ni siquiera en
> el empaquetador desktop (`avapack` Fase 1 sí lo embebe como texto, pero eso
> no dice nada sobre qué tan sólido está el resto del pipeline `.avaui`). Por
> eso `ava_barekernel_pack` (§3-4) asume **`.ava` como único entry
> soportado** — nada de este plan intenta compilar o embeber `.avaui` para
> litekernel. Soportarlo sería agregar superficie nueva sobre un formato que
> ni en desktop está terminado, y además no hay todavía ningún renderer de
> `avaui` pensado para correr sobre el CKM freestanding (hoy solo existen
> `GdiRenderer`/`HTMLRenderer`) — es un problema aparte, no parte de este
> plan.

Buena noticia: **este es exactamente el problema que `avapack` (desktop) ya
resolvió**, solo que empaquetando contra el formato del `.exe` nativo del
host (PE en Windows) en vez de `AppHeader` de litekernel.

**Aclaración importante, para no confundir los dos `.exe`:** el `.exe` de
litekernel comparte *nombre de extensión* con el `.exe` de Windows, pero es
un formato binario completamente distinto y propio tuyo — el struct
`AppHeader` (magic `"EXEC"`, `entry_point`, `code_offset`, `checksum`, etc.,
ver `docs/kernel/kernel.md §2.2`), sin nada de PE (no hay
`IMAGE_DOS_HEADER`, ni `IMAGE_NT_HEADERS`, ni tabla de secciones PE, ni
import table de Windows). Es el mismo formato que ya arma
`ExecutableHeaderCreator.cs` de AppBuilder hoy para tus apps C++. Todo lo que
sigue en este documento sobre el `.exe` de barekernel se refiere a *ese*
formato — cuando haga falta distinguirlo del `.exe`/PE que sí arma `avapack`
desktop, se aclara explícitamente.

El componente `embedded_project.h`/`embedded_project.cpp` + `main.cpp` de
`runtime/avapack/` es, conceptualmente, el mismo `ExecutableHeaderCreator.cs`
de AppBuilder pero para AvaLang — la pieza nueva es "generar lo mismo, pero
envuelto en `AppHeader` (litekernel) en vez de en PE (Windows, el host)".

### Diseño propuesto: `ava_barekernel_pack` (nuevo componente)

Un cuarto tipo de artefacto, hermano de `avapack_gen`/`avapack`, específico
para este target:

```
runtime/avapack/
├── src/
│   ├── main.cpp                    (ya existe — target desktop)
│   ├── main_zerodisk.cpp           (ya existe — target desktop)
│   └── barekernel/                 (NUEVO)
│       ├── main_barekernel.cpp     ← reemplaza a ava_barekernel_runner.cpp:
│       │                             en vez de leer /apps/hello.avb del
│       │                             filesystem del kernel, lee el .avb
│       │                             desde un símbolo embebido en el propio
│       │                             binario (mismo patrón que
│       │                             embedded_project.h, sin la parte de
│       │                             cifrado/hooks de imports — no aplica,
│       │                             ver "qué SÍ y qué NO reusar" abajo)
│       └── apphdr_writer.h/.cpp    ← equivalente a ExecutableHeaderCreator.cs:
│                                     toma el .bin final (post-objcopy) y le
│                                     antepone el struct AppHeader
│                                     (magic/entry_point/sizes/checksum) —
│                                     mismo layout que docs/kernel/kernel.md §2.2
```

### Qué SÍ reusar de `avapack` desktop, y qué NO

| Pieza de `avapack` desktop | Reusar para barekernel? | Por qué |
|---|---|---|
| `embedded_project.h` (contrato `EmbeddedFile[]`) | **Sí**, adaptado | Mismo problema: "N archivos van embebidos como arrays de bytes en un `.cpp`". Para barekernel probablemente sea un solo archivo (el `.avb` del entry) en vez de un árbol con imports — ver nota de imports abajo. |
| Cifrado AES-256-CTR (Fase 3) | **No, no por ahora** | El modelo de amenaza de Fase 3 (`README.md`, "Modelo de amenaza") es "que alguien no lea el código con `strings` sobre el `.exe`" (el PE de
Windows que arma `avapack` desktop). En litekernel eso es secundario mientras el bloqueador real siga siendo que el binding compile y corra contra hardware/QEMU real. Agregarlo es trivial después (es el mismo AES-CTR, mismo header) — no lo hagas ahora, es trabajo que no vas a poder validar hasta tener el kernel corriendo apps de verdad. |
| Verificación de integridad HMAC (Fase 5) | No por ahora, mismo motivo | Ídem — validalo cuando tengas builds reales corriendo, no antes. |
| Hooks `SetBeforeModuleReadHook`/`SetAfterModuleReadHook` (Fase 4, imports que tocan disco) | **Probablemente no aplica** | Están para no dejar el `.ava` de un import en texto plano en el disco del host. En litekernel, si una app AvaLang importa otro módulo, ese import se resuelve contra el filesystem del kernel de la app corriendo — pero como recién estás en la Fase 5 del CKM (`CKM_CAP_*` de threads/timers en 0), probablemente **la primera versión de este componente no soporte imports en absoluto** — un solo entry, sin `import`, embebido entero. Agregar imports es una fase aparte, después. |
| `kEntryIsBytecode` (Fase 6, entry precompilado a `.avbc`) | **Sí, y es el único modo** | En barekernel **no existe compilar en el propio kernel** — no hay ANTLR (`frontend_stub.cpp`, ver `binding.cmake` comment). El `.avb` siempre llega precompilado desde el host (mismo flujo que ya usa `gen_barekernel_sample.cpp`: compilar con una `VM` real corriendo en el host, serializar con `ava_module_serialize`). Este componente nunca necesita la rama "compilar `.ava` en runtime" que sí existe en `main.cpp` desktop. **Ojo con la extensión al portar este código**: `avapack` desktop llama a este mismo formato binario `.avbc` (`embedded_project.h`, `main.cpp` de Fase 6), mientras que todo el lado barekernel (`avabare`, `gen_barekernel_sample.cpp`, y este documento) lo llama `.avb`. Es el mismo formato — mismo magic `"AVBC"`, mismo `ava_module_serialize`/`ava_module_deserialize` — nada de contenido cambia, pero si se copia el código de Fase 6 tal cual, sus mensajes de error y comentarios van a decir `.avbc` en un componente que en todos lados dice `.avb`. `avapack_barekernel_gen` debe estandarizar en una sola extensión (`.avb`, para no romper la convención ya usada en `avabare/README.md` y en `hello.avb`) y corregir los strings copiados de Fase 6 que digan `.avbc`. |
| `ExecutableHeaderCreator.cs` (AppBuilder, C#) | **Sí, como referencia de layout** | El struct `AppHeader` es literalmente el mismo (`docs/kernel/kernel.md §2.2` es una copia del struct `ExecutableHeader` de AppBuilder). Portar esa clase a C++ dentro de `avapack/src/barekernel/apphdr_writer.cpp` es casi mecánico. |
| `EntryPointDetector.cs` (heurística de firma de bytes) | **No** | Esa heurística existe en AppBuilder porque el toolchain no le da el entry point de otra forma confiable tras `objcopy -O binary`. Acá lo sabemos con certeza: es el símbolo `_start` del propio `main_barekernel.cpp`, disponible en el `.elf` antes de aplanarlo — mejor resolverlo con `nm`/el propio linker que reinventar detección de patrones de bytes. |

### Flujo de build resultante

1. `ava_cli build --target barekernel --project <dir> --entry app.ava --out app.exe`
   (nuevo flag `--target barekernel` en `avacli/src/build_command.cpp`, hoy
   ese comando solo conoce el target desktop).
2. Internamente: compila `app.ava` con una `VM` de host (misma técnica que
   `gen_barekernel_sample.cpp`) → `ava_module_serialize` → bytes `.avb`.
3. Un nuevo `avapack_barekernel_gen` (análogo a `avapack_gen`) escribe esos
   bytes como un `embedded_project.cpp` minimalista (sin cifrado, ver tabla
   arriba).
4. Se compila `main_barekernel.cpp` + ese `.cpp` generado, cross-compilado
   con el toolchain `i686-elf` (mismo patrón que `Crt0Builder`/
   `ApplicationCompiler` de AppBuilder: `-ffreestanding -nostdlib -m32`),
   contra el mismo `crt0.o`/`app.ld` que ya usa AppBuilder para las apps C++
   — **una app AvaLang en este modelo usa el mismo linker script y el mismo
   arranque que cualquier app AppBuilder**, no hace falta un `app.ld` nuevo.
5. `objcopy -O binary` → `.bin` plano.
6. `apphdr_writer` antepone el `AppHeader` (magic/entry/checksum) → `.exe`
   final (tu formato propio, no PE), listo para copiar a `/apps/`.
7. El `.exe` resultante hace `ckm_dlopen(libavalang.so)` en su `_start` —
   eso no cambia respecto a `ava_barekernel_runner` hoy, solo que ya no lee
   un `.avb` de `/apps/`, lo tiene embebido. Esto no tiene variante sin
   `dlopen` — es el único flujo.

---

## 4. Plan de fases

Numerando como continuación natural de las fases ya documentadas en
`runtime/avapack/README.md` (que van de Fase 0 a Fase 7 para el target
desktop), pero como una rama paralela — no se pisan:

### Fase B0 — Confirmar el bloqueador crítico antes de invertir acá
No tiene sentido construir el empaquetador si `CKM_CAP_LIBSTDCPP=0` sigue
bloqueando que el binding corra de verdad. Antes de B1: cerrar o al menos
tener un plan concreto de fecha para `kernel.md §6.1` (port de libstdc++) y
`§6.2` (excepciones C++). Si eso sigue lejos, este plan igual se puede
prototipar y validar con `-fsyntax-only` (mismo criterio que usó
`avabare/README.md` para `ava_barekernel_runner`), pero no esperes correrlo
en QEMU todavía.

### ✅ Fase B0.5 — Arreglar el loader de litekernel (bloqueador real, no de AvaLang) — IMPLEMENTADO, pendiente de compilar/probar en QEMU
Verificado contra `lib_loader.cpp` y `dynlink_syscalls.cpp` reales (no contra
docs): construir `avapack_barekernel` sobre el `dlopen` de hoy es trabajo
desperdiciado, porque son bugs de runtime que ningún `-fsyntax-only` va a
detectar. Cambios ya escritos (`lib_loader.cpp`, `lib_loader.hpp`,
`dynlink_syscalls.cpp` — entregados como parche, sin build system en el zip
para compilarlos acá):

- [x] **Relocations contra la dirección física, no la virtual mapeada.**
   `LoadSharedLibrary` llama `ApplyRelocations` inmediatamente después de
   `ParseSections`, usando `lib->base_addr` (físico, recién salido de
   `Allocator::AllocPages`). Recién después, en `sys_dlopen`, se elige
   `lib->mapped_base` (dirección virtual, incrementando de a `0x10000000`
   por cada `dlopen`) y se mapean ahí las páginas físicas dentro del proceso
   llamante. Cualquier relocation absoluta (`R_386_32`, `R_386_RELATIVE`,
   `R_386_GLOB_DAT`, `R_386_JMP_SLOT`) ya quedó horneada contra la dirección
   física y no se vuelve a tocar — el proceso, corriendo en modo usuario, no
   tiene esa dirección física mapeada en ningún lado.
   **Arreglo aplicado:** cada nombre de librería recibe una dirección
   virtual fija y estable (`GetOrAssignVirtualBase`, `lib_loader.cpp`),
   asignada la primera vez que se carga y reutilizada por todos los
   procesos. `ApplyRelocations` corre contra esa dirección fija
   (`lib->mapped_base`), no contra `base_addr`. `sys_dlopen` ya no elige una
   dirección nueva por llamada — reutiliza `lib->mapped_base`.
- [x] **`R_386_PC32` de yapa, mismo bug.** Restaba la dirección física del
   propio sitio de la relocation en vez de la virtual. Arreglado en la misma
   pasada (usa `base + rel->r_offset` en vez de `(uintptr_t)reloc_addr`).
- [x] **`process_libraries[32]` indexado por PID monotónico.** `get_next_pid()`
   nunca se reutiliza y cuenta *todos* los procesos desde el boot, no solo
   los concurrentes — pasado el proceso #32, `dlopen` empezaba a fallar para
   siempre. **Arreglo aplicado:** se reemplazó el arreglo fijo por una lista
   enlazada única filtrada por `pid`, sin límite de procesos históricos.
- [x] **RELA descartaba el addend explícito (`r_addend`).** `ApplyRelocation`
   asumía siempre la convención REL (addend implícito en el buffer). Si el
   linker emite RELA con addend ≠ 0 (típico en GOT/PLT), se perdía en
   silencio. **Arreglo aplicado:** `ApplyRelocation` ahora recibe
   `rela_addend` explícito y lo usa cuando `is_rela == true`.
- [ ] **Validación de host pendiente** (test con un `.so` sintético + un dato
   absoluto conocido, para confirmar el puntero final antes de correr esto
   en QEMU) — sigue sin hacerse, es el próximo paso antes de dar por cerrada
   la fase.

Sin esto, `ava_barekernel_runner` de hoy probablemente ya esté rompiendo en
runtime real apenas `libavalang.so` tenga algún dato global no trivial (y
lo va a tener: la VM tiene estado). No se detectó antes porque nunca se
ejecutó contra el kernel real, solo se validó con `-fsyntax-only`.

### ✅ Fase B1 — `apphdr_writer` + entry embebido, sin cifrado — IMPLEMENTADO, validado con el mismo criterio que ya tenía `avabare` (no ejecución real)
- [x] **`embedded_avb.h`** (`runtime/avapack/src/barekernel/`) — contrato
  freestanding-safe (solo `<cstddef>`/`<cstdint>`, sin STL) entre el
  generador y `main_barekernel.cpp`: `kAvbBytes`/`kAvbSize`/`kEntryName`.
  Deliberadamente NO es `avapack::EmbeddedFile[]` (eso es un array de N
  archivos con cifrado AES-256-CTR + HMAC) — es un solo blob sin cifrar,
  siguiendo la tabla de §3 ("Qué SÍ reusar... y qué NO"): sin cifrado, sin
  integridad, sin imports, siempre bytecode (nunca hay modo "compilar
  `.ava` en el kernel", a diferencia de `kEntryIsBytecode` que en desktop
  es una opción — acá es el único modo).
- [x] **`avapack_barekernel_gen`** (`src/barekernel/gen/main.cpp`) — tool de
  host standalone: `--avb <entry.avb> --out <cpp> [--entry-name <n>]` →
  escribe `embedded_avb.cpp`. A diferencia de `avapack_gen`, NO linkea
  `avalang` — el `.avb` siempre llega ya serializado desde afuera (ver
  tabla §3, fila `kEntryIsBytecode`); compilar `.ava` es responsabilidad
  de `ava_cli build --target barekernel` (Fase B2), no de este generador.
  **Corrido de verdad** (no solo compilado) contra el
  `platform/barekernel/samples/hello.avb` real que ya existía en el repo
  → el resultado quedó comiteado como
  `src/barekernel/samples/embedded_hello_avb.cpp` (143 bytes, magic
  `AVBC` visible en el array), mismo criterio de "no mocks" que ya usa
  `gen_barekernel_sample.cpp` para `hello.avb`.
- [x] **`apphdr_writer.h/.cpp`** (`src/barekernel/`) — struct `AppHeader`
  portado campo por campo desde `kernel.md §2.2` (verificado:
  `static_assert(sizeof(AppHeader) == 64)`, `#pragma pack(1)`) +
  `WriteAppHeaderWrapped()`. **Verificado contra el `ExecutableHeaderCreator.cs`
  real** (traído en `AppBuilder.zip`): el algoritmo de `checksum` NO es
  CRC-32 (eso era una suposición de un intento anterior sin el original a
  mano) — es una suma simple de bytes mod 2³², y solo sobre el payload; el
  `AppHeader` nunca participa del cálculo. `total_size` es `payload.size()`
  nada más, no header+payload. `data_offset` es
  `sizeof(AppHeader) + payload.size()` (justo después del código) aunque
  `data_size` sea 0. `bss_size`/`stack_size`/`flags` usan los defaults
  fijos reales de AppBuilder (4096 / 65536 / 0x01) vía
  `kDefaultBssSize`/`kDefaultStackSize`/`kAppHeaderFlags`, sobreescribibles
  si hace falta. `entry_point`: la semántica (offset dentro del `.bin`) ya
  coincidía con el diseño existente (confirmado contra
  `EntryPointDetector.cs`, que usa heurísticas de patrones de bytes — este
  componente no las reimplementa, usa el offset real de `_start` vía
  `nm`/el linker, más preciso que adivinar). Validado con un test
  funcional end-to-end (`.bin` de juguete → `WriteAppHeaderWrapped` →
  comparación byte a byte contra lo que calcularía el C# real, incluyendo
  el checksum) — no solo compilación limpia. Como ya establece la Fase 0
  (`Loader::validate_header()` nunca verifica este campo hoy), esto nunca
  bloqueó correr nada en QEMU — pero ahora si bloqueaba (antes de este
  fix) dar por sentada compatibilidad binaria con AppBuilder si algún día
  se comparan checksums bit a bit.
- [x] **`ava_apphdr_writer`** (`src/barekernel/apphdr_cli/main.cpp`) — CLI
  de host sobre `apphdr_writer`: `--bin --entry-offset [--stack-size]
  [--bss-size] --out`. `--stack-size`/`--bss-size` ahora son opcionales de
  verdad (antes `--stack-size` era obligatorio pese a que la ayuda de
  `ava_cli build --target barekernel --help` decía que tenía default —
  bug encontrado y corregido junto con el fix de checksum): sin estos
  flags, caen a los defaults reales de AppBuilder
  (`kDefaultStackSize`/`kDefaultBssSize`). Probado de verdad con un `.bin`
  de juguete: header de 64 bytes, magic `EXEC` (`45 58 45 43`),
  `code_offset=0x40`, `total_size`/`code_size`/`stack_size`/`bss_size`/
  `checksum` coinciden byte a byte con el cálculo equivalente al C# real.
- [x] **`main_barekernel.cpp`** (`src/barekernel/`) — fork explícito de
  `ava_barekernel_runner.cpp`: mismo `_start`, mismo `dlopen`+`dlsym` de
  `libavalang.so`, mismas capability flags — la única diferencia es que
  `ava_module_deserialize` lee de `avapack_barekernel::kAvbBytes/kAvbSize`
  (ya en `.rodata` del binario) en vez de `ckm_open("/apps/hello.avb")` +
  `ckm_read` a un buffer estático. Ya no hay un `.avb` suelto que copiar
  aparte a `/apps/`.
- [x] **Validación**: `g++ -fsyntax-only -ffreestanding -fno-exceptions
  -fno-rtti -D__i386__` + las mismas `CKM_CAP_*` de
  `bindings/target/binding.cmake` (idéntica receta a
  `avabare/README.md`) — limpio para `main_barekernel.cpp` y para
  `samples/embedded_hello_avb.cpp`. `avapack_barekernel_gen` y
  `ava_apphdr_writer` (herramientas de host, C++ portable normal) se
  compilaron, enlazaron **y corrieron de verdad** contra CMake real
  (target nuevo en `runtime/avapack/CMakeLists.txt`), no solo
  sintaxis — son más fáciles de validar en este entorno porque no
  dependen del binding freestanding.
- [x] **CMake**: `runtime/avapack/CMakeLists.txt` gana `avapack_barekernel_gen`
  y `ava_apphdr_writer` (bajo `AVA_BUILD_PACK`, HOST siempre — ver nota
  de B2 abajo sobre por qué). `runtime/avapack/src/barekernel/CMakeLists.txt`
  (nuevo) define `avapack_barekernel_app` (STATIC, mismo patrón que
  `avabare/CMakeLists.txt`), agregado desde la raíz junto a
  `add_subdirectory(runtime/avabare)` bajo `AVA_TARGET_BAREKERNEL`.
  Confirmado que `AVA_BUILD_PACK=ON` (host) sigue configurando y
  compilando limpio, incluyendo `avapack_gen` (target desktop, sin
  regresión) — y que `AVA_TARGET_BAREKERNEL=ON` sigue fallando en
  `binding.cmake` por falta del cross-compiler `i686-elf`, exactamente la
  misma limitación ya documentada para `avabare` en
  `docs/kernel/binding-status.md`, no algo nuevo introducido acá.
- [ ] **Nuevo, descubierto al cablear el CMake**: `ava_cli build --target
  barekernel` (Fase B2) no puede simplemente "agregar un flag" al build
  cruzado existente — `avapack_barekernel_gen` tiene que correr **en el
  host** (produce texto/C++ a partir de bytes, es una herramienta de
  build) pero el resto del target (`main_barekernel.cpp` +
  `embedded_avb.cpp` compilado) tiene que compilarse **cruzado** con
  `i686-elf-g++`. Si `ava_cli build --target barekernel` fuera una sola
  invocación de CMake con el toolchain file de `i686-elf` activo (como
  hace hoy `scripts/build_barekernel.bat` para `avalang`), ese mismo
  toolchain también intentaría cross-compilar `avapack_barekernel_gen` —
  y un binario `i686-elf` no corre en el host para generar el `.cpp` que
  el resto del build necesita. B2 tiene que invocar
  `avapack_barekernel_gen` como un binario de HOST ya compilado por
  separado (mismo build que ya deja `AVA_BUILD_PACK=ON` sin cross), antes
  de lanzar el `cmake --build` cruzado — no es un detalle menor, es
  justo el tipo de bug de "está compilado pero no corre donde hace
  falta" que ya pasó una vez con el loader (Fase B0.5). Ninguna otra
  fase de este plan lo menciona todavía; queda anotado acá para que B2 lo
  resuelva de entrada, no lo descubra a mitad de camino.

### Fase B2 — Integración con `ava_cli build --target barekernel`
- Flag nuevo en `runtime/avacli/src/build_command.cpp` (hoy ese comando solo
  arma el árbol `build_pack` desktop, ver `runtime/avapack/CMakeLists.txt`
  comentario "Fase 2 las fija dinámicamente").
- Reusa el toolchain `i686-elf` ya definido en
  `cmake/toolchain-i686-elf.cmake` (usado hoy por
  `scripts/build_barekernel.bat` para compilar la lib `avalang` sola) — este
  target lo reusa para compilar también `main_barekernel.cpp` + el `.cpp`
  generado.
- Salida: `app.exe` en formato `AppHeader`, listo para `disk.ima`.

### Fase B3 — Despliegue end-to-end en `disk.ima` + QEMU
- Equivalente a la Fase 6 de `kernel.md` (`Desplegar en la imagen de disco`),
  pero ya con apps generadas por `ava_cli build --target barekernel` en vez
  de copiadas a mano.
- Primer punto real donde se puede confirmar tamaño real en disco con 2-3
  apps de prueba, y validar que compartir `libavalang.so` entre varias
  realmente se comporta como se espera (una sola copia en disco/RAM).

### Fase B4 (opcional, después de medir B3) — Imports entre módulos AvaLang
- Solo si algún proyecto real de litekernel necesita más de un archivo
  `.ava`. Ahí sí conviene mirar de nuevo el esquema de hooks de Fase 4
  (`avapack` desktop) — adaptado a que en barekernel el filesystem donde se
  "materializa" un import temporalmente es el del kernel, no el del host.

---

## 5. Resumen para decidir ahora

- Es una sola forma de hacerlo: **`libavalang.so` compartido, instalado una
  vez, y `app.exe` (`AppHeader`) delgado que depende de él vía `ckm_dlopen`**
  — sin variante estática detrás de ningún flag.
- Lo que falta construir de verdad es B1/B2: que el `.avb` quede embebido
  dentro del `app.exe` en vez de suelto en `/apps/`. Eso es lo único nuevo
  del lado de AvaLang; el resto (`libavalang.so`, el binding CKM, el runner)
  ya existe y no se toca.
- **Nuevo, verificado contra `litekernel.zip` real:** antes de B1, hay dos
  bugs del lado del kernel (`lib_loader.cpp`/`dynlink_syscalls.cpp`, no
  código de AvaLang) que hoy hacen que `ckm_dlopen` sea poco confiable en
  runtime — relocations horneadas contra la dirección física en vez de la
  virtual mapeada, y un límite de 32 procesos *totales desde el boot* (no
  concurrentes) antes de que `dlopen` empiece a fallar para todo el mundo.
  Es la Fase B0.5. Arreglarlos es trabajo de `litekernel`, no de
  `avapack_barekernel`, pero bloquea que cualquier prueba end-to-end de este
  plan sea confiable.
- Menor: unificar `.avb` (nombre usado en barekernel) vs `.avbc` (nombre
  usado en avapack desktop, Fase 6) para el mismo formato binario antes de
  portar ese código — mismo contenido, dos nombres.