# Patrón BareKernel: Adaptador de Kernel Reutilizable para AvaLang

Especificación del patrón **BareKernel** — un adaptador PAL reusable que permite
portar el runtime de AvaLang a **cualquier kernel** (no solo selected kernel) como un
backend más, paralelo a `windows/`, `linux/`, `macos/`.

> **Relación con `kernel.md`**: Este documento define **el patrón abstracto**.
> El plan concreto AvaLang→selected kernel vive en [`kernel.md`](./kernel.md) y se
> trata como *caso de aplicación* de este patrón. No se repite aquí.

---
## 0. tools
https://github.com/lordmilko/i686-elf-tools/

## 1. Resumen ejecutivo

**Veredicto del patrón: BareKernel es un adaptador de tres capas sobre un
Contrato Kernel Mínimo (CKM).**

AvaLang ya aisla todo acceso al SO tras una Platform Abstraction Layer (PAL)
con interfaces congeladas (`IFileSystem`, `IThread`, `IClock`, `ILibrary`,
`IConsole`, `IEnvironment`, `IProcess`, `IMutex`, `ITimer`). Los backends
existentes (`windows/`, `linux/`, `macos/`) implementan esas interfaces
directamente contra las APIs del SO host.

**BareKernel** añade una **tercera familia de backends** para kernels que **no**
son Windows/Linux/macOS — entornos freestanding, custom OS, RTOS, kernels
educativos como selected kernel. En vez de implementar las 9 interfaces PAL contra
cada kernel nuevo (trabajo repetitivo y propenso a errores), BareKernel:

1. Define un **Contrato Kernel Mínimo (CKM)** — ~12 primitivas POSIX-like que
   cualquier kernel razonablemente moderno puede cumplir.
2. Implementa las 9 interfaces PAL **una sola vez** (`platform/barekernel/`)
   sobre el CKM. Este código es **kernel-agnóstico** y nunca se reescribe.
3. Cada kernel concreto solo aporta un **binding** (~1 archivo) que traduce el
   CKM a sus syscalls reales. Adaptar AvaLang a un kernel nuevo = escribir un
   binding, no reimplementar la PAL.

**Costo de portado a un kernel nuevo**: 1 binding (~200-400 líneas) + 1
tabla de capability flags + 1 bloque en `CMakeLists.txt`. El adaptador
BareKernel y las interfaces PAL no se tocan.

---

## 2. Motivación y contexto

### 2.1 Por qué un patrón de kernel-portability

Los backends `windows/`, `linux/`, `macos/` son **óptimos para SOs mainstream**:
reutilizan la libc, pthreads, libstdc++ y el linker del host. Implementar
`LinFileSystem` es 30 líneas sobre `open`/`read`/`close` POSIX.

Un kernel custom (selected kernel, SerenityOS, un RTOS, un kernel educativo) **no
tiene libc estándar ni pthreads**, usa syscalls propios (`int 0x80`, `syscall`,
`svc`), formatos de ejecutable propios, y compila con `-ffreestanding
-nostdlib`. Escribir un backend PAL completo por kernel sería duplicar esfuerzo
y divergencias.

BareKernel **factoriza lo común** (las 9 interfaces PAL sobre POSIX-like) y
**aísla lo variable** (los syscalls concretos) tras un contrato pequeño.

### 2.2 Cuándo usar BareKernel vs. backend SO nativo

| Target | Backend recomendado | Razón |
|---|---|---|
| Windows, Linux, macOS (mainstream) | `windows/` / `linux/` / `macos/` | Reutilizan libc/pthreads/libstdc++ del host |
| selected kernel, kernels educativos, custom OS freestanding | **`barekernel/`** | No hay libc estándar; el CKM es el punto de encuentro |
| RTOS (FreeRTOS, Zephyr) con POSIX shim | `barekernel/` (binding sobre la shim) | El CKM cabe en una shim POSIX minimalista |
| Microcontrollers bare-metal sin syscalls | No aplicable | Sin syscalls no hay CKM; usar `memory/` para tests |

### 2.3 Principios de diseño heredados de la PAL

BareKernel respeta las reglas ya fijadas en `PAL_ABI.h` y `Platform.h`:

- **Selección compile-time, no runtime OS detection** (`Platform.h:18`): el
  backend se elige en `CMakeLists.txt`, no con `#ifdef` dinámicos.
- **Una sola definición de `Platform::Create()`** por build (`windows/README.md:19`):
  BareKernel define `Platform::Create()` cuando es el backend seleccionado.
- **Interfaces congeladas**: BareKernel **nunca** añade métodos a `I*.h`. Toda
  variabilidad va en el CKM o en capability flags.
- **No dependencia ascendente**: el adaptador no referencia compiler/VM/AST.

---

## 3. Modelo conceptual: tres capas

```
┌─────────────────────────────────────────────────────────────┐
│  Runtime AvaLang (compiler, VM, async, FFI)                 │
│  solo ve: ava::platform::IPlatform + 9 sub-interfaces      │
└──────────────────────────┬──────────────────────────────────┘
                           │ (interfaces congeladas, PAL_ABI.h)
┌──────────────────────────▼──────────────────────────────────┐
│  CAPA A — Interfaces PAL (no se tocan)                       │
│  runtime/avalang/platform/interfaces/I*.h                   │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│  CAPA B — Adaptador BareKernel (kernel-agnóstico, 1 sola)   │
│  runtime/avalang/platform/barekernel/                       │
│  Implementa las 9 interfaces PAL sobre el CKM.             │
│  Nunca llama a un syscall concreto.                       │
└──────────────────────────┬──────────────────────────────────┘
                           │ (Contrato Kernel Mínimo, ckm_contract.h)
┌──────────────────────────▼──────────────────────────────────┐
│  CAPA C — Binding del kernel concreto (1 por kernel)         │
│  runtime/avalang/platform/barekernel/bindings/<target>/    │
│  Implementa las ~12 primitivas ckm_* con los syscalls reales.│
└─────────────────────────────────────────────────────────────┘
```

### 3.1 Capa A — Interfaces PAL (existente, congelada)

Definidas en `runtime/avalang/platform/interfaces/`. BareKernel las usa **sin
modificación**:

| Interfaz | Header | Accesor en `IPlatform` |
|---|---|---|
| `IFileSystem` | `IFileSystem.h` | `FileSystem()` |
| `IThreadFactory` (+ `IThread`) | `IThread.h` | `Threads()` |
| `IMutex` | `IMutex.h` | `CreateMutex()` (owned) |
| `IClock` | `IClock.h` | `Clock()` |
| `ILibraryLoader` (+ `ILibraryHandle`) | `ILibrary.h` | `Libraries()` |
| `IConsole` | `IConsole.h` | `Console()` |
| `IEnvironment` | `IEnvironment.h` | `Environment()` |
| `IProcess` (+ opcional `IProcessStream`) | `IProcess.h`, `IProcessStream.h` | `Process()` |
| `ITimer` | `ITimer.h` | `Timer()` |

`PAL_ABI.h` fija `AVA_PAL_ABI_VERSION = 2`. BareKernel es un backend más y debe
respetar la misma política de versionado: cualquier cambio en el set de
accessors de `IPlatform` requiere bump de ABI.

### 3.2 Capa B — Adaptador BareKernel (lo nuevo, 1 sola vez)

`runtime/avalang/platform/barekernel/` contiene **un único adaptador** que
implementa las 9 interfaces PAL sobre el CKM. Este código:

- **No incluye** headers de ningún kernel concreto.
- **No llama** a `int 0x80`, `syscall`, ni a libc del host.
- **Solo** invoca funciones `ckm_*` declaradas en `ckm_contract.h`.
- **Decide** stubs vs. implementación real leyendo `BareKernelCaps.h` en
  compile-time (ver §5).

Una vez escrito, **no se vuelve a tocar** al portar a un kernel nuevo.

### 3.3 Capa C — Binding del kernel concreto (lo variable)

`runtime/avalang/platform/barekernel/bindings/<target>/` contiene la
implementación del CKM para un kernel concreto. Para selected kernel, esto vive en
`bindings/selected kernel/` y su plan detallado está en [`kernel.md`](./kernel.md)
§5 Fase 2 (syscall stubs + wrappers).

Un binding típico son 2-3 archivos:
- `ckm_binding.cpp` — los `extern "C"` que el adaptador invoca.
- `ckm_syscall_stubs.asm` — si el kernel usa una instrucción de syscall
  específica (`int 0x80`, `syscall`, `svc`), los stubs ASM.
- `BareKernelCaps_<target>.h` — los flags de capacidad reales del kernel.

**Adaptar AvaLang a un kernel nuevo = escribir un binding.** El adaptador
(Capa B) y las interfaces (Capa A) son invariantes del patrón.

---

## 4. El Contrato Kernel Mínimo (CKM)

El **CKM** es la pieza central del patrón BareKernel. Es un conjunto **pequeño
y deliberadamente minimalista** de primitivas POSIX-like que un kernel debe
proveer para que el adaptador funcione. El objetivo es que **cualquier kernel
con syscalls de I/O, memoria, proceso y carga dinámica** pueda cumplirlo.

### 4.1 Filosofía del CKM

- **12 primitivas, no 30.** Si un syscall no es estrictamente necesario para
  implementar las 9 interfaces PAL, no está en el CKM. (Decisión por defecto
  §0 del plan: minimalista.)
- **`extern "C"` con firma fija.** El binding las implementa; el adaptador las
  invoca. Sin name mangling C++, sin clases, sin plantillas.
- **Códigos de error estilo POSIX.** Retornan `int` negativo (`-errno`) en
  fallo, no lanzan excepciones. El adaptador traduce a `bool`/valor PAL.
- **Sin dependencias de libc ni libstdc++.** El binding las implementa sobre
  los syscalls del kernel, no sobre `glibc`/`musl`.
- **Sin hilos ni mutex obligatorios.** Threads/mutex son *capabilities*
  opcionales (§5). Un kernel sin pthreads puede cumplir el CKM y el adaptador
  degrada a single-threaded.

### 4.2 Las 12 primitivas del CKM

| # | Primitiva | Categoría | Interfaz PAL que la usa |
|---|---|---|---|
| 1 | `ckm_open` | I/O | `IFileSystem` |
| 2 | `ckm_close` | I/O | `IFileSystem`, `IConsole` |
| 3 | `ckm_read` | I/O | `IFileSystem`, `IConsole` |
| 4 | `ckm_write` | I/O | `IFileSystem`, `IConsole` |
| 5 | `ckm_stat` | I/O | `IFileSystem` |
| 6 | `ckm_opendir` / `ckm_readdir` / `ckm_closedir` | I/O | `IFileSystem::EnumerateDirectory` |
| 7 | `ckm_dlopen` / `ckm_dlsym` / `ckm_dlclose` | Dinámico | `ILibraryLoader` |
| 8 | `ckm_malloc` / `ckm_free` | Memoria | Todas (strings, vectors) |
| 9 | `ckm_now_ms` / `ckm_highres_now_ns` / `ckm_sleep_ms` | Tiempo | `IClock`, `ITimer` |
| 10 | `ckm_getpid` / `ckm_spawn` / `ckm_waitpid` / `ckm_exit` | Proceso | `IProcess` |
| 11 | `ckm_unlink` / `ckm_mkdir` / `ckm_rmdir` | I/O FS | `IFileSystem` (mutación) |
| 12 | `ckm_getcwd` / `ckm_chdir` | I/O FS | `IEnvironment` |

> **Nota de granularidad**: 12 *familias*. Algunas agrupan 2-3 funciones
> relacionadas (`opendir`/`readdir`/`closedir`). El recuento de funciones
> individuales en `ckm_contract.h` es ~24, pero conceptualmente son 12
> capacidades.

### 4.3 Constantes del CKM

El adaptador y el binding comparten estas constantes (definidas en
`ckm_contract.h`):

```c
/* Flags de apertura para ckm_open */
#define CKM_O_RDONLY   0x0000
#define CKM_O_WRONLY   0x0001
#define CKM_O_RDWR     0x0002
#define CKM_O_CREAT    0x0040
#define CKM_O_TRUNC    0x0200
#define CKM_O_APPEND   0x0400

/* Descriptores estándar */
#define CKM_STDIN      0
#define CKM_STDOUT     1
#define CKM_STDERR     2

/* Modo de creación de archivos */
#define CKM_S_IRUSR    0400
#define CKM_S_IWUSR    0200
#define CKM_S_IRWXU    0700

/* Flags para ckm_dlopen */
#define CKM_RTLD_NOW   0x0002
#define CKM_RTLD_LAZY  0x0001

/* Campos de ckm_stat */
struct CkmStat {
    uint64_t size;          /* tamaño en bytes */
    uint32_t is_directory;  /* 0 = archivo, 1 = directorio */
    uint32_t mode;          /* bits CKM_S_* */
};

/* Entrada de ckm_readdir */
struct CkmDirEntry {
    char name[256];
    uint32_t is_directory;
};
```

### 4.4 Semántica de cada primitiva

Cada `ckm_*` tiene semántica POSIX-estándar. La firma canónica (la que el
adaptador invoca) se detalla en §7. Aquí solo la semántica:

| Primitiva | Semántica | `errno`-style en fallo |
|---|---|---|
| `ckm_open(path, flags, mode)` | Abre/crea archivo, retorna fd ≥ 0 | `-ENOENT`, `-EACCES`, `-EEXIST` |
| `ckm_close(fd)` | Cierra fd, retorna 0 | `-EBADF` |
| `ckm_read(fd, buf, count)` | Lee hasta `count` bytes, retorna n leídos (0 = EOF) | `-EBADF`, `-EFAULT` |
| `ckm_write(fd, buf, count)` | Escribe `count` bytes, retorna n escritos | `-EBADF`, `-ENOSPC` |
| `ckm_stat(path, CkmStat*)` | Rellena `CkmStat`, retorna 0 | `-ENOENT` |
| `ckm_opendir(path)` | Abre directorio, retorna handle ≥ 0 (o `nullptr`) | `-ENOTDIR`, `-ENOENT` |
| `ckm_readdir(handle, CkmDirEntry*)` | Rellena siguiente entrada, retorna 0; 1 = fin | `-EBADF` |
| `ckm_closedir(handle)` | Cierra directorio, retorna 0 | `-EBADF` |
| `ckm_dlopen(path, flags)` | Carga `.so`/`.dll`, retorna handle (o `nullptr`) | ver `ckm_dlerror` |
| `ckm_dlsym(handle, name)` | Resuelve símbolo, retorna puntero (o `nullptr`) | ver `ckm_dlerror` |
| `ckm_dlclose(handle)` | Descarga, retorna 0 | `-EINVAL` |
| `ckm_malloc(size)` / `ckm_free(ptr)` | Asignación de memoria (estilo POSIX) | `nullptr` en OOM |
| `ckm_now_ms()` | Tiempo wall-clock en ms desde Unix epoch | siempre retorna |
| `ckm_highres_now_ns()` | Tiempo monotónico en ns (solo duraciones) | siempre retorna |
| `ckm_sleep_ms(ms)` | Bloquea `ms` milisegundos | siempre retorna |
| `ckm_getpid()` | Retorna PID del proceso actual | siempre retorna |
| `ckm_spawn(path, argv, argc)` | Lanza proceso hijo, retorna PID (o `-1`) | `-ENOENT`, `-ENOMEM` |
| `ckm_waitpid(pid, exit_code*)` | Espera hijo, retorna 0; rellena `exit_code` | `-ECHILD` |
| `ckm_exit(code)` | Termina el proceso actual (no retorna) | n/a |
| `ckm_unlink(path)` | Borra archivo, retorna 0 | `-ENOENT` |
| `ckm_mkdir(path)` | Crea directorio, retorna 0 | `-EEXIST`, `-ENOENT` |
| `ckm_rmdir(path)` | Borra directorio vacío, retorna 0 | `-ENOTEMPTY` |
| `ckm_getcwd(buf, size)` | Rellena `buf` con cwd, retorna 0 | `-ERANGE` |
| `ckm_chdir(path)` | Cambia cwd, retorna 0 | `-ENOENT` |

---

## 5. Capacidades opcionales (Capability flags)

No todos los kernels cumplen las 12 primitivas por completo. BareKernel usa
**flags de capacidad compile-time** para que el adaptador degrade limpiamente
cuando un kernel no soporta algo.

> **Decisión por defecto §0 del plan**: flags en **compile-time** (`#if
> CKM_CAP_*` en `BareKernelCaps.h`), **no** nullptr-check en runtime. Esto
> elimina overhead y sigue el patrón "no runtime OS detection" de
> `Platform.h:18`.

### 5.1 Flags de capacidad

| Flag | Significa | Degradación si `0` |
|---|---|---|
| `CKM_CAP_THREADS` | El kernel tiene hilos en userspace | `IThreadFactory` corre el callback **inline** (single-threaded) |
| `CKM_CAP_MUTEX` | El kernel provee mutex/futex atómico | `IMutex` se implementa con spinlock + `__asm__("pause")`, o no-op si `!CKM_CAP_THREADS` |
| `CKM_CAP_ENVVARS` | El kernel tiene variables de entorno | `IEnvironment::GetEnvVar` retorna `false`, `SetEnvVar` retorna `false` |
| `CKM_CAP_DIR_ENUM` | El kernel tiene `opendir`/`readdir` | `IFileSystem::EnumerateDirectory` retorna `false` (sin enumeración) |
| `CKM_CAP_COLOR` | La consola soporta códigos ANSI de color | `IConsole::SetForegroundColor`/`ResetColor` son no-op |
| `CKM_CAP_PROCESS_EXEC` | El kernel puede lanzar procesos (`ckm_spawn`) | `IProcess::Execute` retorna `false` |
| `CKM_CAP_TIMERS` | El kernel tiene timers asíncronos reales | `ITimer::ScheduleOnce` se emula con `ckm_sleep_ms` en un hilo (o inline si `!CKM_CAP_THREADS`) |
| `CKM_CAP_STD_EXCEPTIONS` | El runtime C++ del kernel soporta excepciones | Requiere refactor del caller; ver `kernel.md` §5 Fase 1 |
| `CKM_CAP_LIBSTDCPP` | `libstdc++` está portado al kernel | Sin esto, AvaLang no compila (ver `kernel.md` §6.1) |

### 5.2 Tabla de capabilities (`BareKernelCaps.h`)

Cada binding rellena su propia tabla de flags. Ejemplo para selected kernel (referencia
detallada en [`kernel.md`](./kernel.md) §2.3-2.7):

```c
/* bindings/selected kernel/BareKernelCaps_selected kernel.h */
#define CKM_CAP_THREADS          0   /* selected kernel no tiene pthreads en Ring 3 */
#define CKM_CAP_MUTEX            0   /* spinlock fallback */
#define CKM_CAP_ENVVARS          0   /* no hay envvars en el kernel */
#define CKM_CAP_DIR_ENUM         0   /* no hay opendir/readdir (ver kernel.md §6.5) */
#define CKM_CAP_COLOR            1   /* consola soporta color */
#define CKM_CAP_PROCESS_EXEC     1   /* sys_spawn disponible */
#define CKM_CAP_TIMERS            0   /* emular con sys_sleep */
#define CKM_CAP_STD_EXCEPTIONS   0   /* __cxa_throw = panic loop (kernel.md §2.5) */
#define CKM_CAP_LIBSTDCPP         0   /* obstaculo critico (kernel.md §6.1) */
```

### 5.3 Patrón de uso en el adaptador

```cpp
// BareKernelThread.cpp — compilado una sola vez, sin tocar por kernel
std::unique_ptr<IThread> BareKernelThreadFactory::CreateThread(ThreadFunc func) {
#if CKM_CAP_THREADS
    return std::make_unique<BareKernelRealThread>(std::move(func));
#else
    /* Degradacion single-threaded: corre inline y retorna un handle "joinable=false". */
    func();
    return std::make_unique<BareKernelInlineThread>();
#endif
}
```

El binding **no** ve este código — solo aporta `CKM_CAP_THREADS = 0` o `1`. El
adaptador (Capa B) ya sabe qué hacer en cada caso.

---

## 6. Estructura de directorios del adaptador BareKernel

```
runtime/avalang/platform/barekernel/
├── BareKernelPlatform.h/.cpp      IPlatform (agrega los 9 sub-adaptadores)
├── BareKernelFileSystem.h/.cpp    IFileSystem sobre ckm_open/read/write/close/stat
├── BareKernelConsole.h/.cpp      IConsole sobre ckm_write(CKM_STDOUT/STDERR), ckm_read(CKM_STDIN)
├── BareKernelClock.h/.cpp        IClock sobre ckm_now_ms / ckm_highres_now_ns / ckm_sleep_ms
├── BareKernelLibrary.h/.cpp      ILibraryLoader sobre ckm_dlopen/dlsym/dlclose
├── BareKernelProcess.h/.cpp      IProcess sobre ckm_spawn/waitpid (+ IProcessStream opcional)
├── BareKernelThread.h/.cpp       IThreadFactory (real si CKM_CAP_THREADS, inline si no)
├── BareKernelMutex.h/.cpp        IMutex (futex si CKM_CAP_MUTEX, spinlock/no-op si no)
├── BareKernelEnvironment.h/.cpp  IEnvironment (real si CKM_CAP_ENVVARS, stub si no)
├── BareKernelTimer.h/.cpp        ITimer (real si CKM_CAP_TIMERS, emulado si no)
├── ckm_contract.h                Declaraciones extern "C" del CKM + constantes (§7)
├── BareKernelCaps.h              Capa de indireccion: incluye el binding Caps_<target>.h
├── barel.h                       Alias publico: namespace barel = ::ava::platform::barekernel
├── README.md                     Como implementar el CKM para tu kernel (este patron)
└── bindings/                     Implementaciones del CKM por kernel concreto (Capa C)
    └── selected kernel/                Caso de aplicacion (ver kernel.md, no duplicado aqui)
        ├── ckm_binding.cpp        extern "C" ckm_* sobre syscalls de selected kernel
        ├── ckm_syscall_stubs.asm  int 0x80 stubs (patron de test_dlopen, kernel.md §2.8)
        └── BareKernelCaps_selected kernel.h
```

### 6.1 Convenciones de nombres

- **Adaptador** (Capa B): `BareKernel<PAL-Interface>.{h,cpp}` — paralelo a
  `Win*.cpp`, `Lin*.cpp`, `Mac*.cpp`.
- **Binding** (Capa C): `ckm_binding.cpp` + `ckm_syscall_stubs.asm` (si aplica)
  + `BareKernelCaps_<target>.h`. El nombre del directorio `bindings/<target>/`
  identifica el kernel y se selecciona con `AVA_BAREKERNEL_TARGET` (§9).
- **Alias público**: `barel.h` expone `namespace barel =
  ::ava::platform::barekernel;` para uso externo (FFI, CLI, AvaUI). Decisión
  por defecto §0 del plan.

### 6.2 Relación con los backends existentes

BareKernel es un **backend más** en `platform/`. No reemplaza ni modifica
`windows/`, `linux/`, `macos/` ni `memory/`. El mecanismo de selección en
`CMakeLists.txt` (§9) lo trata como una rama adicional del `if/elseif`
existente.

---

## 7. API completa del Contrato Kernel Mínimo

Este es el contenido canónico de `ckm_contract.h`. Listo para copiar y que el
binding lo implemente. Todas las funciones son `extern "C"` con C-linkage.

```c
/* ckm_contract.h — Contrato Kernel Minimo para BareKernel
 *
 * El adaptador BareKernel invoca SOLO estas funciones. Un binding de kernel
 * concreto las implementa sobre sus syscalls reales. Nunca llamar a libc del
 * host desde un binding BareKernel.
 */
#ifndef AVA_PLATFORM_BAREKERNEL_CKM_CONTRACT_H
#define AVA_PLATFORM_BAREKERNEL_CKM_CONTRACT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Constantes (ver §4.3) ===== */
#define CKM_O_RDONLY   0x0000
#define CKM_O_WRONLY   0x0001
#define CKM_O_RDWR     0x0002
#define CKM_O_CREAT    0x0040
#define CKM_O_TRUNC    0x0200
#define CKM_O_APPEND   0x0400

#define CKM_STDIN      0
#define CKM_STDOUT     1
#define CKM_STDERR     2

#define CKM_S_IRUSR    0400
#define CKM_S_IWUSR    0200
#define CKM_S_IRWXU    0700

#define CKM_RTLD_NOW   0x0002
#define CKM_RTLD_LAZY  0x0001

struct CkmStat {
    uint64_t size;
    uint32_t is_directory;
    uint32_t mode;
};

struct CkmDirEntry {
    char     name[256];
    uint32_t is_directory;
};

/* ===== I/O de archivos ===== */
int   ckm_open(const char* path, int flags, int mode);
int   ckm_close(int fd);
long  ckm_read(int fd, void* buf, long count);
long  ckm_write(int fd, const void* buf, long count);
int   ckm_stat(const char* path, struct CkmStat* out);
int   ckm_unlink(const char* path);
int   ckm_mkdir(const char* path);
int   ckm_rmdir(const char* path);

/* ===== Enumeracion de directorios ===== */
void* ckm_opendir(const char* path);
int   ckm_readdir(void* handle, struct CkmDirEntry* out);
int   ckm_closedir(void* handle);

/* ===== Carga dinamica ===== */
void* ckm_dlopen(const char* path, int flags);
void* ckm_dlsym(void* handle, const char* symbol);
int   ckm_dlclose(void* handle);
const char* ckm_dlerror(void);

/* ===== Memoria ===== */
void* ckm_malloc(size_t size);
void  ckm_free(void* ptr);

/* ===== Tiempo ===== */
int64_t ckm_now_ms(void);
int64_t ckm_highres_now_ns(void);
void    ckm_sleep_ms(uint32_t milliseconds);

/* ===== Procesos ===== */
int     ckm_getpid(void);
int     ckm_spawn(const char* path, const char* const* argv, int argc);
int     ckm_waitpid(int pid, int* out_exit_code);
void    ckm_exit(int code);

/* ===== CWD ===== */
int   ckm_getcwd(char* buf, size_t size);
int   ckm_chdir(const char* path);

/* ===== Variables de entorno (opcional, ver CKM_CAP_ENVVARS) ===== */
int   ckm_getenv(const char* name, char* buf, size_t size);
int   ckm_setenv(const char* name, const char* value);

/* ===== Hilos / Mutex (opcional, ver CKM_CAP_THREADS / CKM_CAP_MUTEX) ===== */
typedef void (*CkmThreadFunc)(void* arg);
int   ckm_thread_create(CkmThreadFunc func, void* arg);
int   ckm_thread_join(int thread_handle);

typedef struct CkmMutex CkmMutex;
int   ckm_mutex_init(CkmMutex* m);
int   ckm_mutex_lock(CkmMutex* m);
int   ckm_mutex_unlock(CkmMutex* m);
int   ckm_mutex_trylock(CkmMutex* m);

#ifdef __cplusplus
}
#endif

#endif /* AVA_PLATFORM_BAREKERNEL_CKM_CONTRACT_H */
```

> **Convención de error**: todas las funciones que retornan `int` devuelven
> `0` en éxito y un valor negativo (`-errno`-style) en fallo. Las que
> retornan puntero devuelven `nullptr` en fallo (detalles vía `ckm_dlerror`
> para carga dinámica). El adaptador traduce estos códigos a `bool`/valores
> de las interfaces PAL — **nunca** los propaga al runtime AvaLang.

---

## 8. Mapeo PAL → CKM

Tabla bidireccional: cada método de cada interfaz PAL → qué primitiva CKM usa
y qué capability flag rige la degradación. Esta es la **receta** que el
adaptador (Capa B) implementa una sola vez.

### 8.1 `IFileSystem`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `ReadFile(path, out)` | `ckm_open(O_RDONLY)` → bucle `ckm_read` → `ckm_close` | — | n/a (obligatorio) |
| `WriteFile(path, content)` | `ckm_open(O_WRONLY\|O_CREAT\|O_TRUNC)` → `ckm_write` → `ckm_close` | — | n/a |
| `DeleteFile(path)` | `ckm_unlink` | — | n/a |
| `CreateDirectory(path)` | `ckm_mkdir` | — | n/a |
| `DeleteDirectory(path)` | `ckm_rmdir` | — | n/a |
| `EnumerateDirectory(path, out)` | `ckm_opendir` → bucle `ckm_readdir` → `ckm_closedir` | `CKM_CAP_DIR_ENUM` | retorna `false`, `out` vacío |
| `Exists(path)` | `ckm_stat` (retorna 0 = existe) | — | n/a |
| `IsDirectory(path)` | `ckm_stat` → `CkmStat.is_directory` | — | n/a |
| `FileSize(path)` | `ckm_stat` → `CkmStat.size` | — | n/a |
| `GetExecutableDirectory()` | `ckm_getcwd` o path fijo del binding | — | fallback a `"/"` |

### 8.2 `IConsole`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `Write(utf8)` | `ckm_write(CKM_STDOUT, ...)` | — | n/a |
| `WriteLine(utf8)` | `ckm_write(CKM_STDOUT, ...)` + `"\n"` | — | n/a |
| `WriteError(utf8)` | `ckm_write(CKM_STDERR, ...)` | — | n/a |
| `ReadLine(out)` | bucle `ckm_read(CKM_STDIN, ...)` hasta `'\n'` | — | n/a |
| `SetForegroundColor(color)` | emite código ANSI (`\033[3xm`) | `CKM_CAP_COLOR` | no-op |
| `ResetColor()` | emite `\033[0m` | `CKM_CAP_COLOR` | no-op |

### 8.3 `IClock`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `NowMs()` | `ckm_now_ms` | — | n/a (obligatorio) |
| `HighResNowNs()` | `ckm_highres_now_ns` | — | fallback a `ckm_now_ms * 1e6` |
| `SleepMs(ms)` | `ckm_sleep_ms` | — | n/a |

### 8.4 `ILibraryLoader` / `ILibraryHandle`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `Load(name)` | `ckm_dlopen(name, CKM_RTLD_NOW)` | — | n/a (core de AvaLang) |
| `ResolveSymbol(sym)` | `ckm_dlsym(handle, sym)` | — | n/a |
| `~ILibraryHandle` | `ckm_dlclose(handle)` | — | n/a |

### 8.5 `IProcess`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `CurrentProcessId()` | `ckm_getpid` | — | n/a |
| `Execute(cmd, args, out)` | `ckm_spawn` + redirección de stdout/stderr vía pipes + `ckm_waitpid` | `CKM_CAP_PROCESS_EXEC` | retorna `false` |
| `ExecuteStreaming` (opcional, `IProcessStream`) | igual + callback por chunk | `CKM_CAP_PROCESS_EXEC` | `dynamic_cast` retorna `nullptr` |

### 8.6 `IThreadFactory` / `IThread`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `CreateThread(func)` | `ckm_thread_create` | `CKM_CAP_THREADS` | **ejecuta inline**, retorna `InlineThread{joinable=false}` |
| `SleepMs(ms)` | `ckm_sleep_ms` | — | n/a |
| `CurrentThreadId()` | `ckm_getpid` (single-thread) o `ckm_thread_self` | `CKM_CAP_THREADS` | retorna `ckm_getpid` |

### 8.7 `IMutex`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `Lock()` | `ckm_mutex_lock` | `CKM_CAP_MUTEX` | spinlock con `__asm__("pause")` (o no-op si `!CKM_CAP_THREADS`) |
| `Unlock()` | `ckm_mutex_unlock` | `CKM_CAP_MUTEX` | spinlock unlock |
| `TryLock()` | `ckm_mutex_trylock` | `CKM_CAP_MUTEX` | spinlock `xchg` |

### 8.8 `IEnvironment`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `GetEnvVar(name, out)` | `ckm_getenv` | `CKM_CAP_ENVVARS` | retorna `false` |
| `SetEnvVar(name, value)` | `ckm_setenv` | `CKM_CAP_ENVVARS` | retorna `false` |
| `GetCurrentDirectory()` | `ckm_getcwd` | — | fallback a `"."` |
| `SetCurrentDirectory(path)` | `ckm_chdir` | — | retorna `false` |
| `GetCommandLineArgs()` | argv del binding (en `_start`) | — | vector vacío |

### 8.9 `ITimer`

| Método PAL | Primitivas CKM | Flag | Degradación si `0` |
|---|---|---|---|
| `ScheduleOnce(delayMs, cb)` | timer nativo o `ckm_thread_create` + `ckm_sleep_ms` | `CKM_CAP_TIMERS` | inline: `ckm_sleep_ms(delayMs); cb();` (bloquea) |
| `Cancel(handle)` | cancel timer nativo | `CKM_CAP_TIMERS` | no-op (si inline, ya se ejecutó) |

---

## 9. Integración con el build de AvaLang

BareKernel es una rama nueva en el selector de backend PAL de
`runtime/avalang/CMakeLists.txt`. Paralelo a `windows`/`linux`/`macos`.

### 9.1 Variables de build nuevas

| Variable | Tipo | Default | Significado |
|---|---|---|---|
| `AVA_TARGET_BAREKERNEL` | bool | `OFF` | Selecciona BareKernel como backend PAL |
| `AVA_BAREKERNEL_TARGET` | string | `selected kernel` | Qué binding de `bindings/` se compila |
| `AVA_BAREKERNEL_LINK_AS_SO` | bool | `ON` | Si `ON`, linka como `.so` ELF32 compartido (carga vía `dlopen`) |

### 9.2 Bloque CMake nuevo (a añadir en el `if/elseif` existente)

```cmake
# En runtime/avalang/CMakeLists.txt, despues del bloque linux/macos:

if(AVA_TARGET_BAREKERNEL)
    if(NOT AVA_BAREKERNEL_TARGET)
        message(FATAL_ERROR "AVA_TARGET_BAREKERNEL=ON requiere AVA_BAREKERNEL_TARGET=<target>")
    endif()

    set(BAREKERNEL_BINDING_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/barekernel/bindings/${AVA_BAREKERNEL_TARGET})

    if(NOT EXISTS ${BAREKERNEL_BINDING_DIR})
        message(FATAL_ERROR "Binding BareKernel no encontrado: ${BAREKERNEL_BINDING_DIR}")
    endif()

    set(PLATFORM_SOURCES
        platform/barekernel/BareKernelFileSystem.cpp
        platform/barekernel/BareKernelConsole.cpp
        platform/barekernel/BareKernelClock.cpp
        platform/barekernel/BareKernelLibrary.cpp
        platform/barekernel/BareKernelProcess.cpp
        platform/barekernel/BareKernelThread.cpp
        platform/barekernel/BareKernelMutex.cpp
        platform/barekernel/BareKernelEnvironment.cpp
        platform/barekernel/BareKernelTimer.cpp
        platform/barekernel/BareKernelPlatform.cpp
        # Binding del kernel concreto (Capa C):
        ${BAREKERNEL_BINDING_DIR}/ckm_binding.cpp
    )

    target_compile_definitions(avalang PRIVATE
        AVA_PAL_BACKEND_BAREKERNEL=1
        AVA_BAREKERNEL_TARGET="${AVA_BAREKERNEL_TARGET}"
        AVA_BAREKERNEL_BINDING="${AVA_BAREKERNEL_TARGET}"
    )
    target_include_directories(avalang PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/barekernel
        ${BAREKERNEL_BINDING_DIR}
    )

    if(AVA_BAREKERNEL_LINK_AS_SO)
        # Linkar como .so ELF32 compartido (carga con dlopen).
        # Ver kernel.md §5 Fase 3 para el caso selected kernel.
        set_target_properties(avalang PROPERTIES
            OUTPUT_NAME "avalang"
            SUFFIX ".so"
        )
    endif()
endif()
```

### 9.3 Selección del binding en compile-time

`BareKernelCaps.h` (Capa B) es una **capa de indirección** que incluye la
tabla del binding seleccionado por `AVA_BAREKERNEL_TARGET`:

```c
/* platform/barekernel/BareKernelCaps.h — no editar a mano */
#ifndef AVA_PLATFORM_BAREKERNEL_CAPS_H
#define AVA_PLATFORM_BAREKERNEL_CAPS_H

/* El build inyecta -DAVA_BAREKERNEL_TARGET="selected kernel". */
#define XSTR(x) STR(x)
#define STR(x)  #x
#define CAPS_HEADER XSTR(./bindings/AVA_BAREKERNEL_TARGET/BareKernelCaps_AVA_BAREKERNEL_TARGET.h)

/* En la practica, el CMake añade el include dir del binding, asi que: */
#include "BareKernelCaps_ava_barekernel_target.h"

#endif
```

> **Alternativa más simple** (recomendada para el primer port): el CMake ya
> añade `${BAREKERNEL_BINDING_DIR}` a los includes (§9.2), así que el binding
> solo crea un archivo `BareKernelCaps.h` (sin sufijo) en su propio directorio
> y el adaptador lo encuentra vía include path. Esto evita macros de
> stringificación frágiles.

### 9.4 Toolchain y cross-compilation

BareKernel normalmente requiere un cross-compiler (p.ej. `i686-elf-g++` para
selected kernel). La configuración del toolchain es responsabilidad del binding, no
del adaptador. El caso selected kernel se documenta en [`kernel.md`](./kernel.md)
§5 Fase 0 (instalación del cross-compiler) y §2.7 (flags de compilación).

El `CMakeLists.txt` de BareKernel **no** hardcodea el compilador: usa
`${CMAKE_CXX_COMPILER}` y los flags que el toolchain file del binding provea.

---

## 10. Algoritmo de portado: secuencia canónica

Pasos para adaptar AvaLang a un kernel nuevo usando el patrón BareKernel. **El
orden importa**: cada paso depende del anterior.

### Paso 1 — Auditar el CKM

Usar el checklist de §11 para verificar que el kernel cumple (o puede cumplir)
las 12 primitivas del CKM. Si más de 3 primitivas obligatorias faltan,
BareKernel no es viable para ese kernel en su estado actual; reevaluar.

### Paso 2 — Preparar el toolchain de cross-compilation

Instalar/configurar el cross-compiler para el target del kernel (p.ej.
`i686-elf-g++` para selected kernel x86 32-bit). Para el toolchain de selected kernel
ver §0. Compilar un "hello world" C++ con `std::string` y `throw`/`catch`
para verificar `libstdc++`. Referencia detallada del flujo:
[`kernel.md`](./kernel.md) §5 Fase 0 (caso selected kernel).

### Paso 3 — Crear el binding `bindings/<target>/`

Dentro de `runtime/avalang/platform/barekernel/bindings/<target>/`:

- `BareKernelCaps_<target>.h` — tabla de capabilities (§5.2). **Una vez
  escritas, son invariantes** — no se cambian sin re-auditar el kernel.
- `ckm_binding.cpp` — los `extern "C" ckm_*` de §7. Implementar solo las
  primitivas; usar la plantilla de §12 como punto de partida.
- `ckm_syscall_stubs.asm` — solo si el kernel usa una instrucción de syscall
  específica (`int 0x80`, `syscall`, `svc`). El patrón es el mismo que
  `userspace/apps/test_dlopen/syscall_stubs.asm` en selected kernel
  ([`kernel.md`](./kernel.md) §2.8).

### Paso 4 — Resolver `libstdc++` (obstáculo crítico)

AvaLang usa `std::string`, `std::vector`, `std::shared_ptr`, excepciones C++,
RTTI. Si el kernel no tiene `libstdc++` portada, evaluar las 3 opciones
documentadas en [`kernel.md`](./kernel.md) §5 Fase 1:

- **A**: Portar `libstdc++` (ideal, sin cambios en AvaLang).
- **B**: Implementar mini-STL en el kernel (pragmático).
- **C**: Refactor sin excepciones (no recomendado, mucho trabajo).

Si el resultado no es `CKM_CAP_LIBSTDCPP=1`, el port se bloquea aquí.

### Paso 5 — Pre-generar el parser ANTLR4

El parser de AvaLang se genera con ANTLR4 + Java. En el host:

```bash
java -jar antlr4.jar -Dlanguage=Cpp -visitor -o generated/ grammar/AvaLang.g4
```

Los `.cpp` generados se incluyen en el build de BareKernel. Referencia:
[`kernel.md`](./kernel.md) §5 Fase 4.

### Paso 6 — Compilar el adaptador y el binding

Con `AVA_TARGET_BAREKERNEL=ON` y `AVA_BAREKERNEL_TARGET=<target>`:

```bash
cmake -B build -DAVA_TARGET_BAREKERNEL=ON \
               -DAVA_BAREKERNEL_TARGET=<target>
cmake --build build
```

El output es `libavalang.so` (si `AVA_BAREKERNEL_LINK_AS_SO=ON`), un ELF32
compartido que se carga vía `dlopen`. Verificar con `readelf -h libavalang.so`
que es `ET_DYN`.

### Paso 7 — Crear el app runner

App userspace mínima que carga `libavalang.so` con `dlopen` y resuelve
`ava_run_file`/`ava_run_string` con `dlsym`. Estructura paralela a
`userspace/apps/test_dlopen/` (ver [`kernel.md`](./kernel.md) §2.8 y §5 Fase 5).

### Paso 8 — Desplegar y probar

Copiar `libavalang.so` al directorio de librerías del kernel (`/system/lib/`
en selected kernel), `ava_runner.bin` a `/apps/`, y scripts `.ava` de prueba.
Bootear el kernel en QEMU y ejecutar:

```
/apps/ava_runner /scripts/hello.ava
```

Iterar: scripts `.ava` simples → corregir bugs en el binding → escalar a
scripts más complejos.

---

## 11. Checklist de conformidad CKM

Lista marcable para auditar si un kernel cumple el Contrato Kernel Mínimo
antes de empezar el portado. Imprimir y tachar cada ítem.

### 11.1 Primitivas obligatorias (bloquean el port si faltan)

**I/O de archivos**:
- [ ] Existe syscall para abrir archivo y retornar descriptor (≥0).
- [ ] Existe syscall para cerrar descriptor.
- [ ] Existe syscall para leer bytes de un descriptor.
- [ ] Existe syscall para escribir bytes a un descriptor.
- [ ] Existe syscall para consultar tamaño/tipo de un archivo (stat).
- [ ] Existe syscall para borrar un archivo.
- [ ] Existe syscall para crear un directorio.
- [ ] Existe syscall para borrar un directorio vacío.

**Enumeración de directorios** (opcional pero necesaria para `IFileSystem::EnumerateDirectory`):
- [ ] Existe syscall para abrir un directorio y obtener handle.
- [ ] Existe syscall para leer la siguiente entrada del directorio.
- [ ] Existe syscall para cerrar el handle del directorio.

**Memoria**:
- [ ] Existe syscall para asignar memoria dinámica (malloc-style).
- [ ] Existe syscall para liberar memoria dinámica.

**Tiempo**:
- [ ] Existe syscall para obtener tiempo wall-clock en milisegundos.
- [ ] Existe syscall para dormir N milisegundos.

**Procesos**:
- [ ] Existe syscall para obtener PID del proceso actual.
- [ ] Existe syscall para terminar el proceso actual con código de salida.
- [ ] Existe mecanismo para lanzar procesos hijo (`spawn`/`fork+exec`).

**CWD**:
- [ ] Existe syscall para obtener el directorio de trabajo actual.
- [ ] Existe syscall para cambiar el directorio de trabajo.

### 11.2 Primitivas opcionales (degradables vía capability flags)

- [ ] El kernel provee carga dinámica de `.so`/`.dll` (`CKM_CAP_THREADS` para `dlopen`).
- [ ] El kernel soporta hilos en userspace (`CKM_CAP_THREADS`).
- [ ] El kernel provee primitivas de mutex/futex (`CKM_CAP_MUTEX`).
- [ ] El kernel expone variables de entorno (`CKM_CAP_ENVVARS`).
- [ ] El kernel soporta códigos ANSI de color en la consola (`CKM_CAP_COLOR`).
- [ ] El kernel provee timers asíncronos reales (`CKM_CAP_TIMERS`).
- [ ] El runtime C++ del kernel soporta excepciones (`CKM_CAP_STD_EXCEPTIONS`).

### 11.3 Compatibilidad del toolchain

- [ ] Existe cross-compiler para el target del kernel (p.ej. `i686-elf-g++`).
- [ ] El cross-compiler incluye `libstdc++` o se puede compilar.
- [ ] ANTLR4 (Java) está disponible en el host para pre-generar el parser.
- [ ] El kernel puede cargar `.so` ELF32 (o el formato equivalente para su arch).

### 11.4 Veredicto

- **Todas las obligatorias + libstdc++ OK** → BareKernel viable, proceder al Paso 2.
- **Alguna obligatoria falta** → Reevaluar BareKernel; considerar un backend
  PAL ad-hoc.
- **Opcionales faltan** → Marcar `CKM_CAP_* = 0` en `BareKernelCaps_<target>.h`
  y aceptar la degradación correspondiente.

---

## 12. Plantilla de binding (`ckm_binding.cpp`)

Esqueleto mínimo que un nuevo binding copia y rellena. Reemplazar cada
`TODO: tu kernel aquí` con la llamada al syscall concreto. La plantilla asume
que los stubs ASM de §7 ya están en `ckm_syscall_stubs.asm` (si el kernel usa
`int 0x80`/`syscall`/`svc`).

```cpp
/* bindings/<target>/ckm_binding.cpp — Plantilla de binding BareKernel
 *
 * Copiar este archivo en runtime/avalang/platform/barekernel/bindings/<target>/
 * y rellenar cada TODO con la llamada al syscall concreto del kernel.
 *
 * Convenciones (ver §7):
 *   - Retornar 0 en exito, negativo estilo -errno en fallo.
 *   - Retornar nullptr en fallo para ckm_dlopen / ckm_dlsym / ckm_opendir.
 *   - ckm_* NO debe llamar a libc del host (glibc, musl, etc).
 *   - ckm_* NO debe lanzar excepciones.
 */
#include "../ckm_contract.h"

/* Si tu kernel usa stubs ASM, declara sus prototipos aqui:
 *   extern "C" int syscall0(int num);
 *   extern "C" int syscall1(int num, int arg1);
 *   extern "C" int syscall3(int num, int arg1, int arg2, int arg3);
 *   extern "C" int syscall6(int num, int arg1, int arg2, int arg3,
 *                            int arg4, int arg5, int arg6);
 *
 * Si tu kernel expone libc-like wrappers directamente (caso POSIX shim),
 * usa esos wrappers en vez de los stubs.
 */

/* ===== I/O de archivos ===== */
int ckm_open(const char* path, int flags, int mode) {
    /* TODO: invocar syscall_open del kernel con (path, flags, mode).
     *   Ejemplo Linux: return open(path, flags, mode);  // direct libc
     *   Ejemplo bare kernel: return syscall3(SYS_OPEN, (int)path, flags, mode);
     */
    return -1;
}

int ckm_close(int fd) {
    /* TODO: SYS_CLOSE(fd) */
    return -1;
}

long ckm_read(int fd, void* buf, long count) {
    /* TODO: SYS_READ(fd, buf, count) -- retorna bytes leidos, 0 = EOF */
    return -1;
}

long ckm_write(int fd, const void* buf, long count) {
    /* TODO: SYS_WRITE(fd, buf, count) */
    return -1;
}

int ckm_stat(const char* path, struct CkmStat* out) {
    /* TODO: SYS_STAT(path, out) -- rellenar out->size, is_directory, mode */
    return -1;
}

int ckm_unlink(const char* path) { /* TODO: SYS_UNLINK(path) */ return -1; }
int ckm_mkdir(const char* path)   { /* TODO: SYS_MKDIR(path)  */ return -1; }
int ckm_rmdir(const char* path)   { /* TODO: SYS_RMDIR(path)  */ return -1; }

/* ===== Enumeracion de directorios ===== */
void* ckm_opendir(const char* path)  { /* TODO */ return nullptr; }
int   ckm_readdir(void* h, struct CkmDirEntry* out) { /* TODO */ return -1; }
int   ckm_closedir(void* h)          { /* TODO */ return -1; }

/* ===== Carga dinamica ===== */
static char last_dl_error[128];
void* ckm_dlopen(const char* path, int flags) { /* TODO: SYS_DLOPEN */ return nullptr; }
void* ckm_dlsym(void* h, const char* sym)     { /* TODO: SYS_DLSYM  */ return nullptr; }
int   ckm_dlclose(void* h)                    { /* TODO: SYS_DLCLOSE */ return -1; }
const char* ckm_dlerror(void) {
    /* TODO: retornar ultimo mensaje de error del kernel. El adaptador
     * nunca llama a esto si ckm_dlopen/dlsym no retornaron nullptr. */
    return last_dl_error;
}

/* ===== Memoria ===== */
void* ckm_malloc(size_t size) { /* TODO: SYS_MALLOC(size) */ return nullptr; }
void  ckm_free(void* ptr)     { /* TODO: SYS_FREE(ptr)    */ }

/* ===== Tiempo ===== */
int64_t ckm_now_ms(void)        { /* TODO: SYS_NOW_MS */       return 0; }
int64_t ckm_highres_now_ns(void){ /* TODO: SYS_HIGHRES_NS */    return 0; }
void    ckm_sleep_ms(uint32_t ms){ /* TODO: SYS_SLEEP_MS(ms) */ }

/* ===== Procesos ===== */
int ckm_getpid(void) { /* TODO: SYS_GETPID */ return 0; }
int ckm_spawn(const char* path, const char* const* argv, int argc) {
    /* TODO: SYS_SPAWN o SYS_FORK+SYS_EXEC */
    return -1;
}
int ckm_waitpid(int pid, int* out_exit_code) {
    /* TODO: SYS_WAITPID(pid, out_exit_code) */
    return -1;
}
void ckm_exit(int code) { /* TODO: SYS_EXIT(code) -- no retorna */ }

/* ===== CWD ===== */
int ckm_getcwd(char* buf, size_t size) { /* TODO: SYS_GETCWD(buf, size) */ return -1; }
int ckm_chdir(const char* path)        { /* TODO: SYS_CHDIR(path)      */ return -1; }

/* ===== Variables de entorno (solo si CKM_CAP_ENVVARS=1) ===== */
int ckm_getenv(const char* name, char* buf, size_t size) {
    /* TODO */ return -1;
}
int ckm_setenv(const char* name, const char* value) {
    /* TODO */ return -1;
}

/* ===== Hilos (solo si CKM_CAP_THREADS=1) ===== */
int ckm_thread_create(CkmThreadFunc func, void* arg) {
    /* TODO: lanzar hilo en el kernel. El adaptador nunca llama a esto si
     * CKM_CAP_THREADS=0. */
    return -1;
}
int ckm_thread_join(int thread_handle) { /* TODO */ return -1; }

/* ===== Mutex (solo si CKM_CAP_MUTEX=1) ===== */
/* CkmMutex es un tipo opaco; el binding define su tamaño y layout. */
struct CkmMutex { uint32_t state; /* padding segun kernel */ };
int ckm_mutex_init(CkmMutex* m)     { /* TODO */ return -1; }
int ckm_mutex_lock(CkmMutex* m)     { /* TODO */ return -1; }
int ckm_mutex_unlock(CkmMutex* m)   { /* TODO */ return -1; }
int ckm_mutex_trylock(CkmMutex* m)  { /* TODO */ return -1; }
```

### 12.1 Checklist de relleno del binding

Por cada `ckm_*` rellenado, verificar:

- [ ] El syscall del kernel existe y su semántica coincide con §4.4.
- [ ] La traducción de flags POSIX (CKM_O_*, CKM_RTLD_*, CKM_S_*) a flags
      nativos del kernel está documentada (suele ser un `#define` o `switch`).
- [ ] Los códigos de error del kernel se mapean a `-errno`-style estándar.
- [ ] La función no llama a libc del host (verificar con
      `nm --undefined-only ckm_binding.o` — no debería listar `malloc`,
      `strlen`, etc., excepto `_Unwind_*` si se usa libstdc++).
- [ ] `ckm_highres_now_ns` es monotónico (no salta hacia atrás en NTP/RTC
      updates).
- [ ] `ckm_exit` no retorna al caller (verificar con `__builtin_unreachable()`
      después de la syscall si el compilador se queja).

---

## 13. Referencias

### Documentación del proyecto

| Documento | Propósito |
|---|---|
| [`kernel.md`](./kernel.md) | Plan **concreto** AvaLang→selected kernel. Caso de aplicación de este patrón. |
| `runtime/avalang/platform/Platform.h` | Entry point `Platform::Create()` por backend. |
| `runtime/avalang/platform/interfaces/IPlatform.h` | Interfaz raíz del PAL. |
| `runtime/avalang/platform/interfaces/PAL_ABI.h` | Política de versionado y freeze del PAL. |
| `runtime/avalang/platform/interfaces/I*.h` | 9 sub-interfaces PAL (referencia para el mapeo §8). |
| `runtime/avalang/platform/windows/README.md` | Convención de READMEs por backend (paralelo). |
| `runtime/avalang/platform/linux/LinFileSystem.cpp` | Backend Linux — referencia de implementación `IFileSystem` sobre POSIX. |
| `runtime/avalang/platform/linux/LinLibrary.cpp` | Backend Linux — referencia de implementación `ILibraryLoader` sobre `dlopen`. |
| `runtime/avalang/platform/linux/LinThread.cpp` | Backend Linux — referencia de `IThreadFactory` con `std::thread`. |
| `runtime/avalang/CMakeLists.txt` | Selector de backend PAL (punto de extensión BareKernel §9). |

### Documentación del kernel (caso selected kernel)

| Documento | Propósito |
|---|---|
| `D:\selected kernel\README.md` | Visión general, arquitectura, factory pattern multi-arch. |
| `D:\selected kernel\include\arch/arch_config.hpp` | Patrón de macros de detección de arquitectura (referencia para BareKernel). |
| `D:\selected kernel\include/syscall/categories/io_syscalls.hpp` | Syscalls I/O (open/read/write/close/stat). |
| `D:\selected kernel\include/syscall/categories/memory_syscalls.hpp` | Syscalls memoria. |
| `D:\selected kernel\include/syscall/categories/process_syscalls.hpp` | Syscalls proceso. |
| `D:\selected kernel\include/syscall/categories/dynlink_syscalls.hpp` | Syscalls `dlopen`/`dlsym`/`dlclose`. |
| `D:\selected kernel\src\filesystem/lib_loader.cpp` | Loader ELF32 compartido (referencia binding §12). |
| `D:\selected kernel\corlib/src/cxx_runtime.cpp` | Runtime C++ mínimo del kernel (sin excepciones). |
| `D:\selected kernel\userspace/apps/test_dlopen/` | App userspace de referencia para syscall stubs + `dlopen`. |

### Estándares y recursos externos

| Recurso | Para qué sirve |
|---|---|
| [POSIX.1-2017 — System Interfaces](https://pubs.opengroup.org/onlinepubs/9699919799/) | Semántica canónica de `open`/`read`/`write`/`stat`/`dlopen` (el CKM sigue este estándar). |
| [System V ABI x86](https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html) | ELF32/64, relocations, formato de `.so` (necesario para `dlopen`). |
| [ELF Specification](https://refspecs.linuxfoundation.org/elf/elf.pdf) | Formato de ejecutables y shared objects. |
| [OSDev Wiki — GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler) | Cómo instalar `i686-elf-g++` (referencia §0). |
| [ANTLR4 Documentation](https://github.com/antlr/antlr4/blob/master/doc/index.md) | Pre-generar el parser C++ de AvaLang (§10 Paso 5). |

---

## 14. Apéndice — BareKernel vs. backend SO nativo

### Cuándo elegir cada uno

| Pregunta | Backend SO nativo | BareKernel |
|---|---|---|
| ¿El target es Windows, Linux, macOS? | **Sí** | No |
| ¿El target tiene libc estándar (`glibc`/`musl`/CRT MSVC)? | **Sí** | No necesariamente |
| ¿Hay pthreads/libdispatch en userspace? | **Sí** | No necesariamente (degradable) |
| ¿Hay libstdc++ o libc++ en el target? | **Sí** | Necesita portar (ver `kernel.md` §5 Fase 1) |
| ¿El runtime C++ soporta excepciones? | **Sí** | Necesita portar o refactor (ver `kernel.md` §6.2) |
| ¿El kernel expone syscalls documentados? | Sí (POSIX/Win32) | Sí (propios) |
| ¿El kernel carga `.so`/`.dll`? | **Sí** | Necesario (sin esto, FFI de AvaLang no funciona) |
| ¿El kernel es custom/educativo/RTOS? | No | **Sí** |

### Costo estimado de portado (BareKernel)

| Kernel nuevo (con libstdc++ ya portada) | Esfuerzo |
|---|---|
| Escribir `BareKernelCaps_<target>.h` (tabla de ~10 flags) | 30 min |
| Rellenar `ckm_binding.cpp` (~24 funciones) sobre los syscalls del kernel | 1-3 días |
| Stubs ASM si el kernel usa `int 0x80`/`syscall`/`svc` | 0.5 día |
| Bloque en `CMakeLists.txt` (copiar plantilla §9.2) | 30 min |
| Toolchain file para el cross-compiler | 0.5-1 día |
| Pre-generar ANTLR4 + compilar + iterar bugs | 1-2 días |
| App runner + desplegar en imagen de disco | 0.5 día |
| **Total** | **~5-8 días hábiles** |

Comparado con reimplementar las 9 interfaces PAL desde cero (~3-4 semanas),
BareKernel reduce el port a una **semana** y elimina divergencias entre
backends: si todos los bindings BareKernel cumplen el CKM, AvaLang se comporta
exactamente igual sobre cualquier kernel soportado.

### ¿Por qué no factorizar `windows/`/`linux/`/`macos/` también?

Es **explícitamente no deseable**. Los backends SO nativos son más rápidos
(reutilizan `std::thread`, `dlopen` de glibc, etc.) y están escritos contra
APIs maduras y estables. Refactorizarlos tras BareKernel añadiría capas de
indirección inútiles para un host mainstream.

La regla: **Usa el backend SO nativo cuando puedas, BareKernel cuando debas.**

### ¿Por qué no usar `memory/` para kernels?

`memory/` (ver `runtime/avalang/platform/memory/MemoryFileSystem.cpp`) es un
backend **mock** para tests unitarios — los archivos viven en un `std::map` en
memoria. No tiene noción de procesos, threads ni carga dinámica. Es útil
para CI pero **no** para ejecutar AvaLang en un kernel real.
