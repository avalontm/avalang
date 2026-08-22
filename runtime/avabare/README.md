# ava_barekernel_runner

Bootstrap userspace mínimo para BareKernel (Fase 7 de
`avalang_runtime_stl_barekernel_plan.md`). Es el equivalente al
`ava_runner` descrito en `docs/kernel/kernel.md` §5: un `_start()`
freestanding que:

1. `ckm_dlopen("/system/lib/libavalang.so", CKM_RTLD_NOW)`.
2. Resuelve por `ckm_dlsym` la C API que necesita: `ava_vm_create`,
   `ava_vm_destroy`, `ava_module_deserialize`, `ava_module_destroy`,
   `ava_run`, `ava_string_free`.
3. Lee `/apps/hello.avb` con `ckm_open`/`ckm_read` a un buffer estático
   (sin heap propio -- este bootstrap vive fuera de `AvaMemory`, igual
   que el resto de la infraestructura previa a `dlopen`).
4. `ava_vm_create` → `ava_module_deserialize` → `ava_run`.
5. Reporta por `ckm_write(CKM_STDERR, ...)` y sale con `ckm_exit`.

No incluye `avastd`, STL, ni el resto de la PAL -- usa `ckm_contract.h`/
`ckm_syscall.h` directamente porque este archivo corre *antes* de que
`libavalang.so` esté cargado, así que no puede depender de símbolos que
viven adentro de esa librería.

## Por qué es una librería STATIC y no un ejecutable

El formato de ejecutable de este kernel es `AppHeader` (no ELF, ver
`docs/kernel/kernel.md` §2.2), producido por las herramientas propias del
kernel (`ab.exe`), no por este árbol de CMake. Este target deja
`ava_barekernel_runner.a` (un único objeto reubicable con `_start` como
símbolo exportado) listo para que ese paso externo lo envuelva; no
reemplaza esa herramienta porque no forma parte de este repositorio.

## Validado

`-fsyntax-only` limpio con la misma receta usada en la auditoría de Fase 6
(`docs/architecture/RUNTIME_CORE_AUDIT.md` §11.2): `-ffreestanding
-fno-exceptions -fno-rtti -D__i386__` más las `CKM_CAP_*` reales de
`platform/barekernel/bindings/target/binding.cmake`.

## No validado (limitación de entorno, ya documentada para el resto de
BareKernel en `docs/kernel/binding-status.md`)

- Compilación y enlace real con `i686-elf-g++` (no hay ese cross-compiler
  en este entorno).
- El wrapping a formato `AppHeader` y el despliegue en `disk.ima`.
- Ejecución real contra el kernel (QEMU o hardware).
- El path fijo `/apps/hello.avb` asume que ese archivo fue copiado al
  disco de la imagen del kernel junto a `libavalang.so` en
  `/system/lib/` -- ninguno de los dos se copia automáticamente por este
  build.

## Bytecode de muestra

`platform/barekernel/samples/hello.avb` (dentro de `runtime/avalang/`) es
bytecode real, no un mock: se generó con
`runtime/avalang/tools/gen_barekernel_sample.cpp`, que construye un
`Proto` a mano (mismo patrón que `test_proto_io_obfuscate.cpp`), lo
corre con una `VM` real invocando `print`, lo serializa con
`SerializeProto`, y vuelve a correr el resultado deserializado para
confirmar el round-trip -- todo esto compilado y enlazado de verdad
contra `libavalang.a` (build hosted en Linux, frontend stub, sin ANTLR)
en este mismo entorno.
