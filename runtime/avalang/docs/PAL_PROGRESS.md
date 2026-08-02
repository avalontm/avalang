# Platform Abstraction Layer — Progreso de Implementación

Checklist de las fases definidas en `docs/Platform_Foundation.md`
(sección "Development Strategy"), acotado **solo a avalang.dll**.

La Fase 6 (inicio de `avalang.ui.dll`) queda fuera de este documento a
propósito: no debe empezarse hasta que todas las fases de abajo estén
en ✅. Cuando llegue ese momento, se abrirá un `PAL_UI_PROGRESS.md`
propio.

## Alcance actual: solo Windows

Decisión de alcance: mientras el proyecto esté en esta etapa, el
trabajo activo del PAL (y de todo lo que se construya encima:
avahost, plugin loading, extern/FFI, async) se enfoca **exclusivamente
en el backend Windows**. Los backends de Linux (`core/platform/linux/`)
y macOS (`core/platform/macos/`) quedan **en estudio**: se dejan tal
cual (compilando como STUB, ver `CMakeLists.txt`), pausados a
propósito y sin recibir trabajo nuevo hasta que se decida retomarlos.
Ninguna fase de abajo se considera bloqueada por ellos — "completa"
en este documento significa "completa para Windows".

Leyenda: ✅ completa (Windows) · 🔄 parcial / en progreso · ⬜ no iniciada · 📚 en estudio (pausado)

| # | Estado | Fase | Objetivo | Notas |
|---|:---:|---|---|---|
| 0 | ✅ | **Estructura base** | Crear los puntos de extensión (stubs) que reservará el PAL para AvaUI, sin implementarlos aún. | `core/platform/interfaces/services/ui/` — `IWindow`, `IMouse`, `IKeyboard`, `ICursor`, `IClipboard`, `IRenderSurface`, `ITimer`, `IDisplay`, `IPlatformServices` (agregador: `UIPlatformInterfaces.h`). Se movió de `interfaces/ui/` a `interfaces/services/ui/` porque UI no es una plataforma (Windows/Linux/macOS lo son) sino un conjunto de *servicios* que las plataformas exponen; `PAL_UI.h` se renombró a `UIPlatformInterfaces.h` porque no es una PAL separada, sino una extensión de la misma. Ninguno está conectado a `IPlatform` todavía (intencional). |
| 1 | ✅ (Win) / 📚 (Lin/Mac) | **PAL completo para avalang.dll** | Interfaces pequeñas (`IFileSystem`, `IThread`, `IMutex`, `ILibrary`, `IClock`, `IConsole`, `IEnvironment`, `IProcess`) + implementación por SO. | Interfaces completas. Backend Windows funcional y es el único en desarrollo activo. Backends Linux y macOS existen como STUB y quedan en estudio (ver sección "Alcance actual" arriba). |
| 2 | 🔄 | **Eliminar código específico de plataforma fuera del PAL** | Ningún componente fuera de `core/platform/` debe llamar APIs de SO directamente (Windows). | ✅ `avahost/src/plugin/plugin_loader.cpp` migrado: ya no llama `LoadLibraryA`/`GetProcAddress`/`FreeLibrary` directo, usa `ava::platform::ILibraryLoader` / `WinLibraryLoader` (compilado directo en `avahost` vía `CMakeLists.txt`, sin ifdef Linux/macOS). ⬜ Pendiente: `avahost/src/web/transport/socket.cpp` sigue incluyendo `winsock2.h` directo — no hay todavía una interfaz PAL para sockets (`ISocket`); es un servicio nuevo a diseñar, no un simple swap como `ILibraryLoader`, se deja para una fase propia. |
| 3 | ✅ | **Estabilizar interfaces** | Congelar las firmas del PAL antes de construir más encima (Extern/FFI, Async, AvaUI). | Criterio de "estable" definido en `core/platform/interfaces/PAL_ABI.h` (nuevo): macro `AVA_PAL_ABI_VERSION`, regla de freeze (ninguna firma existente cambia, ningún método nuevo en interfaz STABLE), política de deprecation (`// [deprecated since ABI vN]`, un ABI completo de gracia antes de borrar) y de cuándo toca bump de versión. Las 9 interfaces core (`IFileSystem`, `IThread`/`IThreadFactory`, `IMutex`, `IClock`, `ILibrary`, `IConsole`, `IEnvironment`, `IProcess`, `IPlatform`) quedan marcadas `STABLE (Windows) since AVA_PAL_ABI_VERSION 1` con un banner que referencia `PAL_ABI.h`. Se verificó que los backends `Win*` implementan cada interfaz 1:1 sin miembros públicos extra. Las interfaces UI (`interfaces/services/ui/`) quedan explícitamente fuera de este contrato (Phase 0 stub, libres de cambiar hasta Fase 6). Comentario desactualizado en `IPlatform.h` ("Phase 3/5/6") y en `CMakeLists.txt` (numeración de fases por backend, que insinuaba que Linux/macOS tenían fases propias) corregidos para reflejar "solo Windows activo, Linux/macOS en estudio". |
| 4 | ✅ | **Extern/FFI vía ILibrary** | La carga de librerías nativas del lenguaje (`extern`) debe pasar por `ILibraryLoader`, no por `LoadLibrary`/`dlopen` directo. | `core/src/vm/vm_extern.cpp` ya usa `platform::ILibraryHandle` / `ILibraryLoader`. |
| 5 | ✅ | **Async Runtime** | Runtime de `async/await` construido sobre el PAL (requiere `ITimer` en el futuro). | Ver desglose de sub-fases 5.1-5.5 abajo. `ITimer` real sobre WinTimer + `set_timeout`/`sleep_async`/`clear_timeout` reemplazan lo cooperativo-puro por I/O real vía PAL. Compilación/validación final en Windows la hace el usuario en su equipo (fuera del alcance de esta sesión). |

### Fase 5 — sub-fases (Async Runtime)

Roadmap detallado para que cualquier sesión futura sepa exactamente dónde
retomar sin releer todo el código. Zips de checkpoint parcial:
`avalang_fase5_checkpoint1_pal_timer.zip`, `avalang_fase5_checkpoint2_wip.zip`
(este último **no compila todavía**, ver 5.2).

| Sub-fase | Estado | Qué es | Archivos | Notas |
|---|:---:|---|---|---|
| 5.1 | ✅ | **`ITimer` en el PAL** | `core/platform/interfaces/ITimer.h` (nuevo, distinto del stub `services/ui/ITimer.h` de Fase 0/6), `IPlatform.h` (accessor `Timer()`), `PAL_ABI.h` (bump a **`AVA_PAL_ABI_VERSION 2`** — regla 3 del contrato, agregar método a interfaz STABLE), `WinTimer.h/.cpp` (impl real: hilo worker + lista ordenada por deadline, ver comentario en el header sobre por qué NO se usó `CreateThreadpoolTimer`), `LinTimer`/`MacTimer` (stubs no funcionales solo para que compile, Linux/macOS siguen 📚 en estudio). Registrado en `CMakeLists.txt`. |
| 5.2 | ✅ | **Dispatcher `set_timeout` (delayed execution)** | `core/src/vm/vm.h`/`vm_async.cpp` (scheduler: `async_mutex_`, `async_ready_queue_`, `async_pending_timers_`, `PostAsyncTask`/`PumpAsyncEvents`/`HasPendingAsyncWork`/`OnAsyncTimerScheduled`/`OnAsyncTimerConsumed`), `core/src/builtins/builtin_async.cpp` (`builtin_set_timeout`), `builtin.h`, `builtin_registry.cpp` (`RegisterNative("set_timeout", ...)`), `CMakeLists.txt` (`vm_async.cpp` y `builtin_async.cpp` agregados a `CORE_SOURCES`), `public/src/main.cpp` (event loop: tras `ava_run` del script principal, `while (HasPendingAsyncWork()) { PumpAsyncEvents(); sleep 5ms; }`, igual que Node mantiene vivo el proceso mientras haya timers). Script de prueba: `scripts/set_timeout_demo.ava`. Diseño: el callback de `WinTimer` corre en su propio hilo worker → nunca llama `vm->Call()` directo (VM no reentrante entre hilos) → solo hace `PostAsyncTask`, y el hilo de la VM drena con `PumpAsyncEvents()`. Todo el código C++ nuevo de 5.2 pasó `g++ -fsyntax-only` (sanity check de sintaxis en Linux); **falta compilar y correr en Windows real con MSVC**, esta sesión no tiene ese toolchain. |
| 5.3 | ✅ | **Resume real de coroutine ligado a timer (`sleep_async`)** | `core/src/builtins/builtin_async.cpp` (`builtin_sleep_async`), `builtin.h`, `builtin_registry.cpp` (`RegisterNative("sleep_async", ...)`). `sleep_async(co, delayMs)` agenda el resume REAL de una coroutine ya suspendida en un `yield` (mismo mecanismo que `builtin_resume`: `vm->Call()` sobre un `Value` tipo `Coroutine`, ver `vm_call.cpp`), pero disparado por el `ITimer` del PAL en vez de sincrónicamente. No se tocó `coroutine.cpp`/`vm_call.cpp`/`vm_call_op.cpp` — se reusa la suspensión cooperativa que ya existía (Fase 1 de `AvaLang_Async_Await_Plan.md`); el PAL solo decide *cuándo* se dispara el resume. No hay palabra clave `await` en la gramática todavía — `sleep_async(co, ms)` es la forma explícita de lograr el mismo efecto. Script de prueba: `scripts/sleep_async_demo.ava`. |
| 5.4 | ✅ | **Cancelación / timeouts anidados** | `core/src/builtins/builtin_async.cpp` (`builtin_clear_timeout`), `builtin.h`, `builtin_registry.cpp`. `set_timeout`/`sleep_async` ahora devuelven el handle (`uint64_t` de `ITimer::ScheduleOnce`, como `ava_value_t` `AVA_NUMBER`); `clear_timeout(handle)` llama `ITimer::Cancel`. Caveat documentado en el código: no se decrementa `async_pending_timers_` al cancelar (carrera inherente cancelar-vs-disparar), en el peor caso `HasPendingAsyncWork()` reporta trabajo pendiente de más — no rompe nada, solo puede hacer que el event loop de `main.cpp` tarde algunos ciclos de más en salir. Script de prueba: `scripts/clear_timeout_demo.ava`. |
| 5.5 | ✅ | **Cierre de Fase 5** | — | Código de 5.1-5.4 completo y pasando `g++ -fsyntax-only`. Compilación/validación en Windows real (MSVC) la hace el usuario en su equipo. Quedan anotadas para el futuro, sin bloquear el cierre de esta fase: (a) `avahost` no dreana la cola async todavía (`PumpAsyncEvents()` solo está wireado en `public/src/main.cpp`, el CLI); (b) no hay azúcar sintáctica `await` en la gramática, solo las funciones nativas `set_timeout`/`sleep_async`/`clear_timeout`. |

## Cómo usar este documento

Al cerrar una fase, cambiar su ⬜/🔄 a ✅ y anotar en "Notas" qué se
hizo y qué archivo/commit lo respalda (igual que `AVAHOST_PROGRESS.md`).
No avanzar a la Fase 6 (AvaUI) mientras quede alguna fase de la 1 a la
5 sin marcar ✅ para Windows. Linux/macOS (📚) no cuentan para ese
criterio mientras sigan en estudio.
