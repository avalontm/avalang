# Plan: capa de compatibilidad `avastd` para el target BareKernel

## Decisión tomada (reemplaza kernel.md §6.1 "Opción A")

Se descarta portar `libstdc++` real. Evidencia contra esa opción, encontrada
al inspeccionar el kernel (`litekernel.zip`):

- El toolchain instalado (`i686-elf-g++ 15.2.0`) fue configurado con
  `--without-headers`: no trae `libstdc++`, ni siquiera los headers C++
  (`<functional>`, `<cstdint>`, etc.). No es un problema de instalación, es
  la configuración estándar de un cross-compiler "solo kernel" de OSDev.
- **Ningún componente de litekernel usa hoy `libstdc++`**, ni el kernel ni
  las apps de userspace (`userspace/apps/test_dlopen/build.bat` y
  `userspace/libs/libtest/build.bat` compilan con
  `-ffreestanding -fno-exceptions -fno-rtti -nostdlib`, igual que el kernel).
  No hay precedente ni infraestructura (`__cxa_throw` en `cxx_runtime.cpp`
  hace panic-loop; no hay unwinding real) para excepciones C++ reales.
- Portar `libstdc++` + unwinding real sería el primer componente de todo el
  ecosistema litekernel en depender de eso — mucho riesgo para desbloquear
  un solo consumidor (AvaLang).

**Se adopta la Opción B de `kernel.md` §6.1** ("extender la mini-STL"), pero
implementada como una **capa de compatibilidad propia de AvaLang**
(`avastd`) en vez de tocar `include/std/` del kernel — así no se acopla
AvaLang al kernel `litekernel` específico, y la capa sirve para cualquier
target freestanding futuro (otro kernel, otro RTOS, etc.), no solo este.

## Diseño

`runtime/avalang/platform/barekernel/stdcompat/ava_stdcompat.h` es el punto
de entrada único. Define el namespace `avastd` con dos implementaciones
posibles, elegidas en tiempo de compilación vía el capability flag que ya
existe (`CKM_CAP_LIBSTDCPP`, en `BareKernelCaps_target.h`):

```cpp
#if CKM_CAP_LIBSTDCPP
  // avastd::vector<T> == std::vector<T>, alias directo, cero costo.
#else
  // avastd::vector<T> == implementación propia, freestanding, sin excepciones.
#endif
```

**Regla de oro**: el código de AvaLang (VM, compiler, builtins) usa
`avastd::` en vez de `std::` y **no necesita saber en qué target corre**.
Los builds de Windows/Linux/macOS (`CKM_CAP_LIBSTDCPP=1` ahí siempre) siguen
usando la implementación real de libstdc++/MSVC STL con cero overhead — el
`#if` se resuelve en compilación, no hay indirección en runtime.

Migrar un archivo es mecánico: cambiar los `#include <vector>` etc. por
`#include ".../ava_stdcompat.h"` y `std::` por `avastd::` (ver script de
migración en Fase 3).

## Manejo de errores (reemplaza excepciones)

`CKM_CAP_STD_EXCEPTIONS=0` en este kernel. AvaLang usa 186 `throw`/`catch`.
En vez de un refactor masivo a códigos de error (rompería la forma en que
está escrito hoy el compiler/frontend, que sí puede seguir usando
excepciones reales porque **no corre en el kernel** — ver AVA_TARGET_BAREKERNEL
en CMakeLists, que ya excluye `ava_cli`/UI/Studio del build barekernel),
se usan macros que cambian de implementación según la capability:

```cpp
AVA_TRY {
    AVA_THROW(AvaRuntimeError("mensaje"));
} AVA_CATCH(AvaRuntimeError& e) {
    ...
}
```

- Si `CKM_CAP_STD_EXCEPTIONS=1`: expande a `try`/`throw`/`catch` reales.
- Si `CKM_CAP_STD_EXCEPTIONS=0`: expande a un mecanismo basado en
  `setjmp`/`longjmp` con una pila de "manejadores" por hilo lógico.

**Limitación documentada y aceptada**: `longjmp` no ejecuta destructores de
objetos automáticos entre el `AVA_TRY` y el `AVA_THROW`. Para el VM esto es
aceptable porque el único caso donde se dispara es un error fatal de
ejecución (la ruta de error, no la de éxito) — el equivalente a "abortar la
tarea", no a "recuperarse y seguir compartiendo memoria con el resto del
sistema". Si en algún momento se necesita cleanup determinístico ahí,
Fase 2 puede introducir un escape valve explícito (`AVA_CLEANUP_ON_UNWIND`)
antes de tocar esto.

## Fases

- **Fase 0 (esta entrega)**: andamiaje completo de `avastd` (tipos, utility,
  atomic, vector, string, shared_ptr/unique_ptr, unordered_map/set,
  function, manejo de errores) + prueba de concepto migrando los dos
  archivos que fallan hoy en tu log (`vm.h`, `value.h`) y toda la cadena de
  headers que arrastran (`vm_helpers.h`, `proto.h`, `closure.h`, `module.h`,
  `coroutine.h`, `task.h`) — son los que se incluyen transitivamente desde
  `vm_helpers.cpp`, el primer archivo que falló en tu build.
- **Fase 1**: migrar el resto de headers de `src/vm/*.h` y compilar de
  nuevo contra el toolchain real (`build_barekernel.bat`) para encontrar
  huecos de API en `avastd` que el Fase 0 no haya cubierto (es normal que
  aparezcan — la superficie de `std::vector`/`std::string` es grande, esta
  entrega cubre lo que realmente usa la cadena de `vm.h`, no el 100% de la
  API de la STL).
- **Fase 2**: migrar los `.cpp` de `src/vm/*` uno por uno (son 20 archivos,
  ver `CORE_SOURCES` en `runtime/avalang/CMakeLists.txt`).
- **Fase 3**: migrar `src/builtins/*`, `src/compiler/proto_io.cpp`,
  `src/compiler/obfuscate.cpp`, `src/ui/builtins.cpp` y `avaui`
  (`target_link_libraries(avalang PRIVATE avalang_ui)` — el target
  barekernel también linkea `avalang_ui` hoy, hay que decidir si eso sigue
  siendo cierto o si `avaui` se excluye del build barekernel, ya que un
  framework de UI probablemente no tiene sentido corriendo dentro del
  kernel sin un compositor gráfico detrás).
- **Fase 4**: limpieza — reemplazar el `std::mutex`/`std::atomic` directos
  que queden por las interfaces PAL (`IMutex`) donde corresponda, y
  eliminar cualquier `#include <...>` de libstdc++ que haya quedado suelto.

Cada fase se entrega en zip, vos compilás con `build_barekernel.bat` y
reportás los errores — mismo patrón que siempre.
