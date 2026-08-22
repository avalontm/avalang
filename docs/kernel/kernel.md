# Plan de Portado: AvaLang → selected kernel

Documento de investigación y plan para portar el runtime de AvaLang al kernel de
selected kernel (`D:\selected kernel`), permitiendo ejecutar scripts `.ava` como
aplicaciones de usuario (Ring 3) en el SO.

---

## 1. Resumen ejecutivo

**Veredicto: Compatible, requiere trabajo de portado intermedio.**

AvaLang está diseñado con una capa de abstracción de plataforma (PAL) limpia que
aisla todo el acceso al SO. selected kernel ya provee los syscalls POSIX-like
necesarios (incluyendo `dlopen`/`dlsym`/`dlclose`). El trabajo consiste en crear
un nuevo backend PAL (`platform/barekernel/`) que use los syscalls del kernel en
vez de las APIs de Linux/Windows.

**El obstáculo principal es `libstdc++`**: AvaLang usa intensivamente
`std::string`, `std::vector`, `std::shared_ptr`, excepciones C++, y
`std::unordered_map`. El kernel compila con `-fno-exceptions -ffreestanding
-nostdlib` y no tiene `libstdc++`. Resolver esto es el primer paso crítico.

---

## 2. Arquitectura de selected kernel (relevante para el port)

### 2.1 Arquitectura objetivo

- **Arquitectura**: x86 32-bit (i686)
- **Modo**: Ring 3 (userspace) con privilegios separados
- **Dirección base de apps**: `0x40000000` (1GB virtual)
- **Espacio de usuario**: `0x00000000` – `0xBFFFFFFF`
- **Espacio kernel**: `0xC0000000` – `0xFFFFFFFF`

### 2.2 Formato de ejecutables

selected kernel usa un formato propio (`AppHeader`) para ejecutables, NO ELF estándar:

```c
struct AppHeader {
    uint32_t magic;         // 0x43455845 ("EXEC")
    uint32_t version;       // 1
    uint32_t entry_point;   // offset desde base
    uint32_t total_size;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t stack_size;
    uint32_t flags;
    uint32_t checksum;
    uint32_t reserved[4];
} __attribute__((packed));
```

**Sin embargo**, para librerías dinámicas (`.so`), el kernel usa **ELF32
estándar** (`ET_DYN`, `EM_386`, `ELFCLASS32`, little-endian). El loader en
`src/filesystem/lib_loader.cpp` parsea secciones ELF, tablas de símbolos
(`SHT_SYMTAB`, `SHT_DYNSYM`), y aplica relocations (`R_386_32`, `R_386_PC32`,
`R_386_RELATIVE`, `R_386_GLOB_DAT`, `R_386_JMP_SLOT`).

**Conclusión**: AvaLang debería compilarse como `.so` (ELF32 compartido) y
cargarse con `dlopen`, NO como ejecutable `AppHeader`.

### 2.3 Syscalls disponibles

Los syscalls usan `int 0x80` con hasta 6 argumentos en registros
(`eax`=num, `ebx`, `ecx`, `edx`, `esi`, `edi`).

| Categoría | Syscalls relevantes para AvaLang |
|---|---|
| **I/O** (`io_syscalls.hpp`) | `read`, `write`, `open`, `close`, `lseek`, `stat`, `fstat`, `access`, `getcwd`, `chdir`, `mkdir`, `rmdir`, `unlink`, `rename`, `pipe`, `dup`, `dup2` |
| **Memoria** (`memory_syscalls.hpp`) | `malloc`, `free`, `realloc`, `memalign`, `mmap`, `munmap`, `mprotect`, `brk` |
| **Procesos** (`process_syscalls.hpp`) | `exit`, `yield`, `getpid`, `getppid`, `sleep`, `fork`, `exec`, `waitpid`, `spawn`, `getpriority`, `setpriority` |
| **Dinámico** (`dynlink_syscalls.hpp`) | `dlopen` (230), `dlsym` (231), `dlclose` (232), `dlerror` (233) |
| **Tiempo** (`time_syscalls.hpp`) | `sleep`, timers RTC/APIC |

Stdin=0, stdout=1, stderr=2 (POSIX estándar).

### 2.4 CorLib (libc del userspace)

El kernel provee `corlib` — una libc de userspace en `corlib/`:

| Módulo | Cobertura |
|---|---|
| `Console` | Write/WriteLine/ReadLine/ReadKey, colores, cursor, clear |
| `String` | Clase completa (length, append, substring, indexof, trim, split, conversión numérica) |
| `File` | Operaciones de archivo |
| `Memory` | malloc/free wrappers |
| `Process` | getpid, sleep, exit, spawn |
| `CString` | strlen, strcpy, strcmp, memcpy, memset, strtok, atoi, etc. |
| `Format` | Formato tipo printf con templates variádicos |
| `DateTime` | Fecha/hora |

### 2.5 C++ runtime actual (`cxx_runtime.cpp`)

El kernel provee un runtime C++ mínimo en `corlib/src/cxx_runtime.cpp`:

- `operator new` / `operator delete` / `operator new[]` / `operator delete[]` → CorLib::malloc/free
- `__dso_handle`, `__cxa_atexit` (no-op), `__cxa_pure_virtual` (panic loop)
- `__cxa_throw` (panic loop — las excepciones C++ **no funcionan**)
- Stubs de RTTI (`_ZTVN10__cxxabiv1...`) — `dynamic_cast` retorna 0

**Estado**: El runtime soporta C++ básico (clases, virtuales, new/delete) pero
**no soporta excepciones** (`__cxa_throw` entra en loop infinito) ni RTTI real.

### 2.6 STL del kernel (`include/std/`)

El kernel tiene su propia mini-STL en `include/std/`:

| Header | Contenido |
|---|---|
| `types.hpp` | int8_t..uint64_t, size_t, nullptr, `forward()`, `remove_reference` |
| `cstring.hpp` | strlen, strcpy, strcmp, memcpy, memset, memmove, strtok, atoi, strdup, tolower, toupper, etc. |
| `string.hpp` | Clase `String` del kernel (NO `std::string`) |
| `format.hpp` | Templates variádicos `f()`, `concat()`, `sprintf()`, conversión int/hex/float→str |
| `list.hpp` | Lista enlazada genérica `List<T>` + placement new |
| `Queue.hpp` | Cola FIFO genérica `Queue<T>` |
| `math.hpp` | Funciones matemáticas |
| `console.hpp`, `debug.hpp`, `file.hpp`, `panic.hpp` | Utilidades del kernel |

**NO tiene**: `std::vector`, `std::map`, `std::unordered_map`, `std::string`,
`std::shared_ptr`, `std::function`, `std::thread`, `std::mutex`,
`std::chrono`, `std::fstream`, `std::sstream`, `std::iostream`.

### 2.7 Toolchain de compilación

- **Cross-compiler esperado**: `i686-elf-gcc` / `i686-elf-g++` / `i686-elf-ld`
- **Ensamblador**: NASM (`-f elf32`)
- **Build tool kernel**: `kb.exe` (busca toolchain en `tools/x86/bin/`)
- **Build tool userspace**: `ab.exe` (busca toolchain en `<ab.exe_dir>/tools/bin/`)
- **Estado actual**: El cross-compiler `i686-elf` **NO está instalado** en el
  sistema. Hay `corlib.a` precompilado (cache) en `corlib/build/`, lo que indica
  que el toolchain estuvo disponible en el pasado.

**Flags de compilación del kernel** (de `build_config.json`):
```
-ffreestanding -nostdlib -fno-exceptions -fno-builtin -Wall -Wextra -c
```

**Flags de userspace** (de `userspace/apps/test_dlopen/build.bat`):
```
-m32 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -O2 -Wall -Wextra
```
Link: `i686-elf-ld -m elf_i386 -nostdlib -Ttext 0x40000000`

### 2.8 Aplicación userspace de referencia

`userspace/apps/test_dlopen/` muestra el patrón exacto para apps userspace:

1. **`syscall_stubs.asm`**: Stubs de syscall en ASM (`syscall0`..`syscall5`
   usando `int 0x80`).
2. **Código C++**: Define wrappers (`dlopen`, `dlsym`, `write`, `exit`) usando
   los stubs ASM.
3. **Entry point**: `extern "C" void _start()` (no `main`).
4. **Compilación**: `i686-elf-g++ -c` → `i686-elf-ld` (link a `.bin` con
   `AppHeader` wrapper).

El patrón para cargar AvaLang sería: un `.so` (ELF32 compartido) cargado con
`dlopen`, que exporta `ava_run_file()` y `ava_run_string()`.

---

## 3. Dependencias de AvaLang (inventario completo)

### 3.1 Headers STL usados

```
algorithm, atomic, cassert, cctype, chrono, cmath, cstddef, cstdint,
cstdio, cstdlib, cstring, filesystem, fstream, functional, iomanip,
iostream, istream, memory, mutex, ostream, set, sstream, stdexcept,
string, thread, unordered_map, unordered_set, utility, vector
```

### 3.2 Componentes STL usados (por frecuencia)

| Componente | Uso (ocurrencias) | Crítico |
|---|---|---|
| `std::string` | 368 | **Sí** — omnipresente |
| `std::vector` | 206 | **Sí** — listas, bytecode, AST |
| `std::shared_ptr` | 195 | **Sí** — gestión de memoria del VM |
| `std::move` | 13 | Sí |
| `std::unique_ptr` | 8 | Sí |
| `std::to_string` | 6 | Sí |
| `std::any` | 6 | Sí — AST visitor |
| `std::function` | 5 | Sí — callbacks |
| `std::pair` | 1 | Sí |
| `std::unordered_map` | — | **Sí** — `globals_` del VM, tablas de símbolos |
| `std::unordered_set` | — | Sí |
| `std::exception_ptr` | 1 | Sí — propagación de excepciones |

### 3.3 Excepciones C++

AvaLang usa **excepciones C++ extensivamente** (186 referencias a
`throw`/`catch`/`try`):
- `throw std::runtime_error(...)` en el compiler y AST builder
- `throw AvaError(...)` y `throw CompileError(...)` errores del lenguaje
- `catch` en el frontend y API C

Esto es **incompatible** con `-fno-exceptions` del kernel. Las excepciones
C++ son parte fundamental de la arquitectura de AvaLang.

### 3.4 iostream / fstream / sstream

29 usos de `std::cout`/`std::cerr`/`std::ifstream`/`std::ofstream`/
`std::stringstream`. Principalmente en:
- Lectura de archivos `.ava` (`vm_file.cpp`, `module.cpp`)
- Logging de debug (`vm_core.cpp`, `vm_helpers.cpp`)
- Proto IO (`proto_io.cpp`, `obfuscate.cpp`)

Estos pueden reemplazarse por wrappers sobre `sys_read`/`sys_write` en el PAL.

### 3.5 Threads y mutex

AvaLang usa `std::thread` / `std::mutex` para:
- Async runtime (`vm_async.cpp`, `LinTimer.cpp`)
- Coroutines (cooperativas, no requieren hilos reales)
- `IThreadFactory` / `IMutex` interfaces del PAL

selected kernel **no tiene pthreads** en userspace. El scheduler es preemptivo a
nivel kernel pero no hay hilos POSIX en Ring 3.

### 3.6 ANTLR4 (solo para compilar, no para ejecutar)

El frontend (parser) requiere:
- **ANTLR4 Java** (solo para generar `AvaLangLexer.cpp`, `AvaLangParser.cpp`,
  `AvaLangBaseListener.cpp` desde `AvaLang.g4`)
- **antlr4-runtime C++** (librería para usar el parser generado)

Los archivos `.cpp` generados son código C++ normal — se pueden pre-generar en
el host y compilar en el kernel sin Java ni antlr4-runtime.

### 3.7 libffi (opcional)

Para FFI nativo (`vm_extern.cpp`). Se puede compilar sin él
(`AVA_HAVE_LIBFFI` no definido). No es crítico para el funcionamiento básico.

---

## 4. Análisis de la capa PAL de AvaLang

AvaLang tiene una **Platform Abstraction Layer (PAL)** que aísla todo el
acceso al SO. El PAL se define en `runtime/avalang/platform/`:

### 4.1 Interfaces (`platform/interfaces/`)

| Interfaz | Métodos | Syscalls de selected kernel equivalentes |
|---|---|---|
| `IFileSystem` | ReadFile, WriteFile, DeleteFile, CreateDirectory, DeleteDirectory, EnumerateDirectory, Exists, IsDirectory, FileSize, GetExecutableDirectory | `sys_open`/`sys_read`/`sys_write`/`sys_close`/`sys_stat`/`sys_mkdir`/`sys_rmdir`/`sys_unlink`/`sys_getcwd` |
| `IThreadFactory` | CreateThread, SleepMs, CurrentThreadId | **No soportado** — necesitar stub |
| `IMutex` | Lock, Unlock, TryLock | **No soportado** — necesitar stub o spinlock |
| `IClock` | NowMs, HighResNowNs, SleepMs | `sys_sleep` + RTC (requiere syscall de tiempo) |
| `ILibraryLoader` | Load, Unload, ResolveSymbol | `sys_dlopen` (230) / `sys_dlsym` (231) / `sys_dlclose` (232) |
| `IConsole` | Write, WriteLine, WriteError, ReadLine, SetForegroundColor, ResetColor | `sys_write`(stdout/stderr) / `sys_read`(stdin) |
| `IEnvironment` | Get, Set, Enumerate | **No soportado** — stub vacío |
| `IProcess` | CurrentProcessId, Execute | `sys_getpid` / `sys_fork`+`sys_exec`+`sys_waitpid` |
| `ITimer` | Start, Cancel, callback | `sys_sleep` + callback (simulación) |

### 4.2 Backends existentes

| Backend | Estado | Ubicación |
|---|---|---|
| **Windows** | Funcional (único en desarrollo activo) | `platform/windows/` |
| **Linux** | Funcional (POSIX + pthread + dl) | `platform/linux/` |
| **macOS** | Stub | `platform/macos/` |
| **Memory** | Para tests | `platform/memory/` |

Cada backend son ~10 archivos pequeños, uno por interfaz.

### 4.3 Patrón de implementación (ejemplo Linux)

```cpp
// LinFileSystem.cpp usa: open, read, write, close, stat, opendir, readdir
// LinThread.cpp usa: std::thread, std::this_thread::sleep_for
// LinLibrary.cpp usa: dlopen, dlsym, dlclose
// LinConsole.cpp usa: fwrite, fputs, fgets, fflush (stdout/stderr/stdin)
// LinClock.cpp usa: clock_gettime, nanosleep
// LinProcess.cpp usa: fork, execvp, pipe, waitpid
```

---

## 5. Plan de portado

### Fase 0: Preparar el toolchain

**Prerrequisito**: Instalar el cross-compiler `i686-elf-gcc/g++` en
`D:\selected kernel\tools\x86\bin\`.

Opciones:
1. **Descargar de OSDev** (recomendado): build precompilado de
   https://wiki.osdev.org/GCC_Cross-Compiler
2. **Compilar desde source**: GCC + binutils con target `i686-elf`
3. **Usar el que ya tenía el proyecto**: El `corlib.a` cacheado sugiere que
   existía — verificar backups o repos original.

**Verificación**: `i686-elf-g++ --version` y `kb build` deben funcionar.

### Fase 1: Resolver `libstdc++` (obstáculo crítico)

AvaLang necesita `std::string`, `std::vector`, `std::shared_ptr`,
`std::unordered_map`, excepciones C++, y RTTI. Opciones:

#### Opción A: Portar `libstdc++` (ideal pero complejo)

Compilar `libstdc++` de GCC con target `i686-elf` en modo freestanding.
Requiere:
- `--enable-libstdcxx` en el build de GCC
- Implementar los syscalls que `libstdc++` espera (malloc, abort, etc.)
- Habilitar excepciones (`-fexceptions` en vez de `-fno-exceptions`)

**Ventaja**: Compatibilidad total, sin cambios en código de AvaLang.
**Desventaja**: Trabajo significativo, `libstdc++` es grande.

#### Opción B: Implementar mini-STL en el kernel (pragmático)

Extender `include/std/` del kernel con los componentes que AvaLang necesita:

| Componente | Esfuerzo | Notas |
|---|---|---|
| `std::vector` | Medio | Plantilla sobre `Allocator` — similar a `List<T>` existente |
| `std::string` | Medio | Ya existe `CorLib::String` — adaptar o crear wrapper `std::string` |
| `std::shared_ptr` | Medio | Conteo de referencias sobre `Allocator` |
| `std::unordered_map` | Alto | Hash table — no existe nada similar en el kernel |
| `std::function` | Medio | Type-erased callable |
| Excepciones C++ | **Alto** | Requiere implementar `__cxa_throw`, `__cxa_begin_catch`, unwinding, personalidad |

**Ventaja**: Control total, sin dependencias externas.
**Desventaja**: Mucho trabajo, especialmente excepciones y hash maps.

#### Opción C: Compilar AvaLang con `-fno-exceptions` y refactorizar (no recomendado)

Reemplazar las 186 referencias a `throw`/`catch` con códigos de error.
Implicaría reescribir gran parte del compiler y frontend.

**Ventaja**: Sin cambios en el kernel.
**Desventaja**: Refactor masivo de AvaLang, pérdida de robustez.

#### Recomendación: Opción A (portar libstdc++)

Es la opción que menos cambia el código de AvaLang. El cross-compiler
`i686-elf-g++` con `libstdc++` ya soporta `-fexceptions` en freestanding.
Los símbolos que falta (`malloc`, `abort`, `__errno_location`) se implementan
como wrappers de los syscalls de selected kernel.

### Fase 2: Crear backend PAL `platform/barekernel/`

Crear ~10 archivos en `runtime/avalang/platform/barekernel/`:

```
platform/barekernel/
├── BareKernelPlatform.h          ← class BareKernelPlatform : public IPlatform
├── BareKernelPlatform.cpp        ← Platform::Create() returns BareKernelPlatform
├── BareKernelFileSystem.h/.cpp   ← IFileSystem via ckm_open/read/write/close/stat
├── BareKernelConsole.h/.cpp      ← IConsole via ckm_write(CKM_STDOUT)/ckm_read(CKM_STDIN)
├── BareKernelClock.h/.cpp        ← IClock via ckm_now_ms / ckm_highres_now_ns / ckm_sleep_ms
├── BareKernelLibrary.h/.cpp      ← ILibraryLoader via ckm_dlopen/dlsym/dlclose
├── BareKernelProcess.h/.cpp      ← IProcess via ckm_getpid/ckm_spawn/ckm_waitpid
├── BareKernelThread.h/.cpp       ← IThreadFactory (real si CKM_CAP_THREADS, inline si no)
├── BareKernelMutex.h/.cpp        ← IMutex (futex si CKM_CAP_MUTEX, spinlock/no-op si no)
├── selected kernelEnvironment.h/.cpp  ← IEnvironment (real si CKM_CAP_ENVVARS, stub si no)
└── selected kernelTimer.h/.cpp        ← ITimer (real si CKM_CAP_TIMERS, emulado si no)
```

#### Syscall stubs

Crear un archivo ASM (`syscall_stubs.asm`) similar al de `test_dlopen`:

```asm
; syscall0..syscall5 usando int 0x80
; Mismo patrón que userspace/apps/test_dlopen/syscall_stubs.asm
```

Y un header C++ (`ckm_syscall_stubs.h`) con los wrappers:

```cpp
extern "C" int syscall0(int num);
extern "C" int syscall1(int num, int arg1);
// ...

// Números reales -- ver include/syscall/syscall_types.hpp (enum
// SyscallNumber) del kernel, y el estado actual del binding en
// binding-status.md
#define SYS_OPEN    40
#define SYS_CLOSE   41
#define SYS_READ    42
#define SYS_WRITE   43
#define SYS_DLOPEN  230
// ...

inline int sys_write(int fd, const void* buf, int count) {
    return syscall3(SYS_WRITE, fd, (int)buf, count);
}
// ...
```

> **Nota**: la tabla completa ya está resuelta en
> `runtime/avalang/platform/barekernel/bindings/target/ckm_syscall_numbers.h`.
> Ver [`binding-status.md`](./binding-status.md) para el estado actual del
> binding.

#### Mapeo PAL → syscalls

| Método PAL | Implementación |
|---|---|
| `IFileSystem::ReadFile` | `ckm_open(O_RDONLY)` → `ckm_read` en bucle → `ckm_close` |
| `IFileSystem::WriteFile` | `ckm_open(O_WRONLY\|O_CREAT\|O_TRUNC)` → `ckm_write` → `ckm_close` |
| `IFileSystem::Exists` | `ckm_stat` (retorna 0 si existe) |
| `IFileSystem::DeleteFile` | `ckm_unlink` |
| `IFileSystem::CreateDirectory` | `ckm_mkdir` |
| `IFileSystem::EnumerateDirectory` | Requiere `ckm_opendir`/`ckm_readdir` — verificar si existe |
| `IConsole::Write` | `ckm_write(CKM_STDOUT, ...)` |
| `IConsole::ReadLine` | `ckm_read(CKM_STDIN, ...)` |
| `IClock::NowMs` | RTC syscall (verificar `time_syscalls.hpp`) |
| `IClock::SleepMs` | `ckm_sleep_ms(ms)` |
| `ILibraryLoader::Load` | `ckm_dlopen(path, RTLD_NOW)` |
| `ILibraryLoader::ResolveSymbol` | `ckm_dlsym(handle, name)` |
| `IProcess::CurrentProcessId` | `ckm_getpid()` |
| `IProcess::Execute` | `ckm_spawn` + `ckm_waitpid` (o `ckm_fork`/`ckm_exec`) |
| `IThreadFactory::CreateThread` | Stub: ejecutar inline (Fase 2) |
| `IMutex::Lock` | Spinlock con `__asm__("pause")` o no-op |

### Fase 3: Integración con el build de AvaLang

Modificar `runtime/avalang/CMakeLists.txt` para añadir el target BareKernel:

```cmake
if(AVA_TARGET_BAREKERNEL)
    set(PLATFORM_SOURCES
        platform/barekernel/BareKernelFileSystem.cpp
        platform/barekernel/BareKernelThread.cpp
        platform/barekernel/BareKernelMutex.cpp
        platform/barekernel/BareKernelClock.cpp
        platform/barekernel/BareKernelLibrary.cpp
        platform/barekernel/BareKernelConsole.cpp
        platform/barekernel/BareKernelEnvironment.cpp
        platform/barekernel/BareKernelProcess.cpp
        platform/barekernel/BareKernelTimer.cpp
        platform/barekernel/BareKernelPlatform.cpp
        # Binding del kernel concreto:
        platform/barekernel/bindings/selected kernel/ckm_binding.cpp
    )
    # Compilar con i686-elf-g++, linkar como .so (ELF32 ET_DYN)
endif()
```

**Output**: `libavalang.so` (ELF32 compartido, posicionable).

### Fase 4: Pre-generar el parser ANTLR4

En el host (Linux/WSL con Java + ANTLR4 jar):
```bash
java -jar antlr4.jar -Dlanguage=Cpp -visitor -o generated/ grammar/AvaLang.g4
```

Los archivos generados (`AvaLangLexer.cpp`, `AvaLangParser.cpp`,
`AvaLangBaseListener.cpp`) se copian al árbol de build del kernel y se
compilan con `i686-elf-g++`.

**Nota**: `antlr4-runtime` (la librería C++) también necesita compilarse para
`i686-elf`. Es una librería C++ estándar — debería compilar con
`libstdc++` portado.

### Fase 5: App runner de AvaLang

Crear una app userspace mínima (`ava_runner`) que:

1. Carga `libavalang.so` con `dlopen("libavalang.so", RTLD_NOW)`
2. Resuelve `ava_run_file` con `dlsym(handle, "ava_run_file")`
3. Lee el archivo `.ava` del filesystem
4. Llama `ava_run_file(ava_code)` → ejecuta el script
5. `dlclose(handle)` → `exit(0)`

Estructura:
```
userspace/apps/ava_runner/
├── ava_runner.cpp          ← _start() que dlopen+dlsym+run
├── syscall_stubs.asm       ← int 0x80 stubs (copiar de test_dlopen)
├── build.bat               ← build script (copiar patrón de test_dlopen)
└── build/                  ← output .bin
```

### Fase 6: Desplegar en la imagen de disco

1. Copiar `libavalang.so` a `/system/lib/` en `disk.ima` (o `initrd.img`)
2. Copiar `ava_runner.bin` a `/apps/` en el disco
3. Copiar scripts `.ava` de prueba al disco
4. Boot el kernel en QEMU y ejecutar `ava_runner` desde el shell

---

## 6. Obstáculos y riesgos

### 6.1 `libstdc++` (CRÍTICO)

**Riesgo**: Alto. Sin `libstdc++` (o equivalente), AvaLang no compila.
**Mitigación**: Portar `libstdc++` con el cross-compiler. Verificar que
`i686-elf-g++` incluye `libstdc++` o compilarla desde source.

### 6.2 Excepciones C++ (CRÍTICO)

**Riesgo**: Alto. El runtime actual del kernel (`__cxa_throw` = panic loop)
no soporta excepciones. AvaLang las usa en 186 lugares.
**Mitigación**: Con `libstdc++` portado y `-fexceptions`, el unwinding de
GCC debería funcionar. Requiere verificar que el linker incluye las
secciones `.eh_frame` / `.eh_frame_hdr` en el ELF.

### 6.3 `std::unordered_map` (ALTO)

**Riesgo**: Medio. El VM usa `std::map<string, Value>` para `globals_` y
tablas de símbolos. No hay hash table en el kernel.
**Mitigación**: `libstdc++` incluye `std::unordered_map`. Si se porta
correctamente, funciona sin cambios.

### 6.4 Threads en userspace (MEDIO)

**Riesgo**: Medio. El async runtime de AvaLang usa `std::thread`.
**Mitigación**: Fase 2 con stub single-threaded. Las coroutines de AvaLang
son cooperativas (no necesitan hilos reales). El async runtime puede
simularse con el scheduler preemptivo del kernel (un "thread" = un proceso).

### 6.5 Enumeración de directorios (BAJO)

**Riesgo**: Bajo. `IFileSystem::EnumerateDirectory` necesita
`opendir`/`readdir` o `sys_getdents`. No se vio explícitamente en los
headers de syscalls.
**Mitigación**: Implementar con `sys_stat` + path guessing, o añadir un
syscall `sys_getdents` al kernel (trabajo futuro del kernel).

### 6.6 `std::any` (BAJO)

**Riesgo**: Bajo. Usado solo en el AST visitor (6 ocurrencias).
**Mitigación**: `libstdc++` lo incluye. Alternativa: reemplazar con
`void*` + type tag.

---

## 7. Verificación de compatibilidad por componente

| Componente AvaLang | Depende de | selected kernel tiene | Estado |
|---|---|---|---|
| Compiler (parser→AST→bytecode) | `std::string`, `std::vector`, `std::any`, excepciones, ANTLR4-runtime | No tiene STL ni excepciones | **Requiere libstdc++** |
| VM (bytecode interpreter) | `std::string`, `std::vector`, `std::shared_ptr`, `std::unordered_map`, `std::function` | No tiene STL | **Requiere libstdc++** |
| Coroutines | Cooperativas, no necesitan hilos | Scheduler preemptivo | **Compatible** |
| Async runtime | `std::thread`, `std::mutex` | No tiene pthreads | **Stub en Fase 2** |
| File I/O | `IFileSystem` PAL | `sys_open`/`read`/`write`/`close`/`stat` | **Compatible** |
| Dynamic loading | `ILibraryLoader` PAL | `sys_dlopen`/`dlsym`/`dlclose` | **Compatible** |
| Console I/O | `IConsole` PAL | `sys_write`/`sys_read` | **Compatible** |
| Time/clock | `IClock` PAL | `sys_sleep` + RTC | **Compatible** |
| Process exec | `IProcess` PAL | `sys_fork`/`exec`/`waitpid`/`spawn` | **Compatible** |
| Environment vars | `IEnvironment` PAL | No tiene | **Stub vacío** |
| FFI (libffi) | `vm_extern.cpp` | No tiene | **Omitir (no crítico)** |

---

## 8. Orden recomendado de ejecución

1. **Instalar cross-compiler** `i686-elf-gcc/g++` en `tools/x86/bin/`
2. **Verificar/build del kernel** con `kb build` — asegurar que compila
3. **Verificar `libstdc++`** — compilar un hello world C++ con
   `std::string` y `throw`/`catch` para `i686-elf`
4. **Si `libstdc++` no funciona**: evaluar Opción B (mini-STL) u Opción C
   (refactor sin excepciones)
5. **Crear backend PAL** `platform/barekernel/` (Fase 2)
6. **Pre-generar ANTLR4** en el host (Fase 4)
7. **Compilar AvaLang como `.so`** para `i686-elf` (Fase 3)
8. **Crear `ava_runner`** app userspace (Fase 5)
9. **Desplegar en `disk.ima`** y probar en QEMU (Fase 6)
10. **Iterar**: probar scripts `.ava` simples → corregir bugs → escalar

---

## 9. Archivos clave de referencia

### selected kernel

| Archivo | Contenido |
|---|---|
| `include/core/process/process_types.hpp` | `AppHeader`, `ProcessState`, `DEFAULT_BASE_ADDRESS` |
| `include/syscall/categories/io_syscalls.hpp` | Syscalls de I/O (read, write, open, close, stat, etc.) |
| `include/syscall/categories/memory_syscalls.hpp` | Syscalls de memoria (malloc, mmap, etc.) |
| `include/syscall/categories/process_syscalls.hpp` | Syscalls de procesos (fork, exec, waitpid, etc.) |
| `include/syscall/categories/dynlink_syscalls.hpp` | dlopen/dlsym/dlclose (230-233) |
| `include/syscall/arch/syscall_arch_x86.hpp` | Arquitectura de syscalls x86 (int 0x80, 6 args) |
| `src/filesystem/lib_loader.cpp` | Loader de .so ELF32 (parseo, relocations, símbolos) |
| `src/arch/x86/process/loader.cpp` | Loader de ejecutables (AppHeader) a Ring 3 |
| `corlib/src/cxx_runtime.cpp` | Runtime C++ mínimo (new/delete, sin excepciones) |
| `corlib/include/string.hpp` | `CorLib::String` (libc string del userspace) |
| `include/std/list.hpp` | `List<T>` (lista enlazada del kernel) |
| `userspace/apps/test_dlopen/` | App de referencia: syscall stubs + dlopen + dlsym |
| `build_config.json` | Flags de compilación del kernel |

### AvaLang

| Archivo | Contenido |
|---|---|
| `runtime/avalang/platform/interfaces/IPlatform.h` | Interfaz raíz del PAL |
| `runtime/avalang/platform/interfaces/IFileSystem.h` | Interfaz de filesystem |
| `runtime/avalang/platform/interfaces/IThread.h` | Interfaz de threads |
| `runtime/avalang/platform/interfaces/ILibrary.h` | Interfaz de dlopen/dlsym |
| `runtime/avalang/platform/linux/LinFileSystem.cpp` | Backend Linux (referencia para selected kernel) |
| `runtime/avalang/platform/linux/LinConsole.cpp` | Console backend Linux |
| `runtime/avalang/CMakeLists.txt` | Build system (selección de backend PAL) |
| `runtime/avalang/api/include/avalang.h` | API C pública (`ava_run_file`, `ava_run_string`) |

---

## 10. Conclusión

AvaLang **es compatible** con selected kernel a nivel de arquitectura — la PAL
limpia y los syscalls POSIX-like del kernel cubren el 90% de las necesidades.
El trabajo de portado es **intermedio**: crear ~10 archivos de backend PAL
es directo, pero el obstáculo real es **`libstdc++`** y el soporte de
**excepciones C++** en el entorno freestanding del kernel.

Si el cross-compiler `i686-elf-g++` incluye `libstdc++` con soporte de
excepciones (lo estándar para GCC), el port es factible en el orden de
días-semanas. Si no, se requiere evaluar alternativas (mini-STL propia o
refactor sin excepciones), lo que escalaría el trabajo a semanas-meses.

El resultado final sería: scripts `.ava` ejecutándose como aplicaciones de
usuario en selected kernel, con el runtime cargado dinámicamente vía `dlopen`.
