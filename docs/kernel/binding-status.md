# Estado del binding BareKernel `target`

Este documento registra el estado real del binding en
`runtime/avalang/platform/barekernel/bindings/target/` frente al patrón
descrito en [`barekernel.md`](./barekernel.md). Se actualiza cada vez que
cambia el estado, no es un plan -- para el plan ver [`kernel.md`](./kernel.md).

## Resumen

El binding `target/` implementa el Contrato Kernel Mínimo (CKM) sobre el
kernel provisto (`litekernel.zip`), sin usar su nombre en la Capa B
kernel-agnóstica (`platform/barekernel/*.cpp`), tal como exige el patrón.

`bindings/litekernel/` (una carpeta huérfana de un intento anterior, con
capability flags que contradecían a `target/` y sin implementación propia de
`ckm_syscall`) fue eliminada. `target/` es la única fuente de verdad.

## Qué está resuelto

| Pieza | Estado |
|---|---|
| ABI de syscall (`int 0x80`, eax=número, ebx/ecx/edx/esi/edi=args 1-5) | Confirmado contra `src/arch/x86/syscall/syscall_arch_x86.cpp` y `libs_asm/x86/syscall_entry.asm` del kernel |
| Tabla de números `SYS_*` | Copiada de `include/syscall/syscall_types.hpp` (`enum SyscallNumber`) del kernel. Ya no hay `#error` de números faltantes en `ckm_syscall_numbers.h` |
| Capability flags (`BareKernelCaps_target.h`) | Alineados con lo que el kernel realmente implementa |
| Definición de `AVA_BAREKERNEL_TARGET_BINDING` en `binding.cmake` | Agregada -- sin esto el build fallaba en el linker (declaraciones sin definición) |
| Compilación sintáctica | Los 9 archivos de la Capa B + `ckm_syscall.cpp` compilan limpio (`g++ -fsyntax-only`, sin warnings) contra los números y capability flags reales |

## Bugs encontrados y corregidos al leer el código fuente del kernel

- **Polaridad de `ckm_readdir` invertida.** El kernel (`sys_readdir` en
  `src/core/syscall/categories/io_syscalls.cpp`) devuelve `1` = entrada
  leída, `0` = fin de directorio, `<0` = error. El contrato CKM define lo
  contrario (`0` = entrada rellenada, `1` = fin). El binding original no
  traducía esto -- ya corregido en `ckm_syscall.h`.
- **Firma real de `sys_spawn`.** Es `sys_spawn(path, flags, target_uid)`,
  no `(path, argv, argc)` como asume el contrato CKM. No hay forma de pasar
  argv al kernel tal como está implementado hoy. Como
  `CKM_CAP_PROCESS_EXEC=0`, esto no se invoca actualmente; queda documentado
  en el binding para cuando se active esa capability.
- **`ckm_waitpid` sin definición en la ruta del binding.** `sys_waitpid`
  existe registrado en el kernel pero retorna `ENOSYS` fijo (no hay
  `SYS_WAITPID` en el enum). Se agregó un stub que devuelve el mismo
  `ENOSYS` en vez de dejarlo indefinido, para que activar
  `CKM_CAP_PROCESS_EXEC` sin querer falle con un error claro y no con un
  error de enlazado o un número de syscall inventado.

## Qué sigue sin implementar (por capability flag en 0)

- Hilos y mutex de kernel (`CKM_CAP_THREADS=0`, `CKM_CAP_MUTEX=0`)
- Ejecución de procesos vía `waitpid` (`CKM_CAP_PROCESS_EXEC=0`)
- Timers asíncronos (`CKM_CAP_TIMERS=0`)
- Excepciones C++ confirmadas (`CKM_CAP_STD_EXCEPTIONS=0`)
- `libstdc++` portado (`CKM_CAP_LIBSTDCPP=0`) -- ver `kernel.md` §6.1, sigue
  siendo el obstáculo crítico para que AvaLang compile contra este kernel

## Build cruzado (`scripts/build_barekernel.bat`)

Agregado un script de build (`scripts/build_barekernel.bat`) y un toolchain
file de CMake (`cmake/toolchain-i686-elf.cmake`) que configuran y compilan
`avalang` (solo la librería core, sin `ava_cli`/UI/Studio/AvaHost/Pack)
contra el cross-compiler `i686-elf-gcc/g++` en la carpeta indicada (por
defecto `D:\litekernel\tools\x86`).

El script localiza los binarios del compilador buscándolos recursivamente
dentro de esa carpeta, arma el build con Ninja (requerido -- el generador de
Visual Studio no puede invocar un cross-compiler GCC), y compila solo el
target `avalang`.

**No forcé `-ffreestanding`/`-nostdlib` en el toolchain file.** Si el
`libstdc++` que trae ese toolchain no está realmente portado, el build va a
fallar en el link con una pared de `undefined reference` -- eso es la
respuesta real a si la Fase 1 Opción A de `kernel.md` funciona con este
toolchain en particular, no un bug del script. Aún no se corrió este script
de verdad (sin acceso a `D:\litekernel\tools\x86` desde este entorno), así
que el resultado real del link sigue sin verificarse.

## Pendiente real (no resuelto por este trabajo)

1. **Build real con cross-compiler i686.** Solo se verificó sintaxis con
   `-D__i386__` sobre un host x86_64 -- no había toolchain `i686-elf-g++`
   disponible en este entorno. Falta compilar y enlazar de verdad.
2. **Prueba contra el kernel real** (QEMU o hardware) -- no realizada.
3. **`libstdc++` / excepciones C++** -- obstáculo crítico ya documentado en
   `kernel.md`, no resuelto por el binding en sí.
4. **Deriva del enum `SyscallNumber`.** Si el proyecto del kernel cambia esos
   números, `ckm_syscall_numbers.h` debe actualizarse a mano -- no hay
   generación automática todavía.
