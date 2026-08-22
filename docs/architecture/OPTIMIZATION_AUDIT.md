# Fase 8 -- Optimizacion: auditoria de RTTI y excepciones

Ver `avalang_runtime_stl_barekernel_plan.md`, seccion "Fase 8 --
Optimizacion", items 3 (Revisar RTTI) y 4 (Revisar C++ exceptions). A
diferencia de LTO / dead-code elimination / symbol stripping (items 1, 2,
5 -- ver `cmake/OptimizationOptions.cmake`), estos dos no son un flag que
se pueda aplicar a ciegas a todo el repo: "revisar" significaba
efectivamente decidir, target por target, si `-fno-rtti`/`-fno-exceptions`
son seguros. Este documento es el resultado de esa revision.

## Core hosted (`avalang`, `avalang_ui`, `ava_cli`, `avahost`, `avapack`)

**Conclusion: RTTI y excepciones se mantienen habilitados (default del
compilador). No se agrega `-fno-rtti`/`-fno-exceptions` a estos targets.**

Motivo -- el codigo los usa activamente, no serian flags "gratis":

- **Excepciones**: `throw` aparece en 5 archivos del core (`vm.cpp`,
  `vm_extern.cpp`, `ast_builder.cpp`, `compiler.cpp`,
  `frontend_antlr.cpp`), con manejo via `catch` en varios puntos del VM y
  de la API C (`c_api.cpp` envuelve llamadas del compilador/VM en
  `try/catch` para convertir excepciones C++ en el `char** out_error` de
  la API C -- ese es justamente el mecanismo por el que errores de
  compilacion/runtime cruzan la frontera C API sin abortar el proceso).
  Desactivar excepciones ahi rompe la compilacion directamente.
- **RTTI**: `dynamic_cast` y `typeid` se usan en `ast_builder.cpp` y
  `compiler.cpp` (frontend ANTLR: recorrido del arbol de parseo
  distinguiendo tipos concretos de nodo). Tambien rompe la compilacion si
  se desactiva sin antes reescribir ese recorrido con un mecanismo propio
  (p.ej. un tag de tipo manual en cada nodo del AST).

Quitar RTTI/excepciones del core es un refactor real (reemplazar
`dynamic_cast`/`typeid` por dispatch manual, y el `throw`/`catch` del VM
y de `c_api.cpp` por un `Result<T, Error>` propio), no un flag de CMake.
Fuera de alcance de la Fase 8 -- si se decide encarar, es candidato a su
propia fase dentro de la migracion STL/AvaValue mas amplia que ya cubre
este plan (ver seccion "17. Orden recomendado").

## Freestanding / BareKernel (`ava_barekernel_runner`, runtime/avabare)

**Ya esta hecho -- de la Fase 10 (Freestanding), no de esta.**

`runtime/avabare/CMakeLists.txt` ya compila con `-fno-exceptions
-fno-rtti -ffreestanding -nostdlib`, y sus defines (`CKM_CAP_STD_EXCEPTIONS=0`,
`CKM_CAP_LIBSTDCPP=0`) reflejan que ahi no hay soporte de excepciones/RTTI
del compilador cruzado ni libstdc++ disponible -- no es una opcion, es un
requisito del entorno (correr dentro de un kernel sin runtime de C++
completo). Este documento solo lo confirma como ya cerrado; no se toco en
esta pasada.

## avastudio (IDE, `AVA_BUILD_STUDIO=OFF` por default)

No revisado en esta pasada -- linkea contra ImGui/GLFW via FetchContent
(dependencias de terceros con su propia postura sobre RTTI/excepciones,
no controlada por este repo) y esta apagado por default, asi que no es
donde el tamano/rendimiento importan para el runtime en si. Si se
retoma, revisar por separado.

## Resumen

| Target                  | RTTI          | Excepciones   | Motivo |
|--------------------------|---------------|---------------|--------|
| `avalang` / `avalang_ui` | habilitado    | habilitado    | usados por frontend/VM/C-API, ver arriba |
| `ava_cli` / `avahost` / `avapack` | habilitado (heredado) | habilitado (heredado) | linkean contra `avalang` |
| `ava_barekernel_runner`  | deshabilitado | deshabilitado | ya cerrado en Fase 10, toolchain freestanding lo exige |
| `ava_studio`             | sin revisar   | sin revisar   | fuera de alcance (IDE, OFF por default) |
