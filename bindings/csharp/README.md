# AvaLang csharp binding

Wrapper managed sobre `include/ava.h` via P/Invoke.

## Estructura

- `AvaLang.Interop/` — libreria class-library (`AvaLang.Interop.dll`) que
  expone `AvaVM`, `AvaModule`, `AvaValue`, `AvaException`. Coloca tu
  `avalang.dll` compilado en `AvaLang.Interop/runtimes/win-x64/native/`.
- `AvaLang.Tests/` — proyecto de consola que corre los `.ava` en
  `AvaLang.Tests/scripts/` contra la VM y reporta PASS/FAIL/PENDING.

## Build y test

```
cd AvaLang.Interop
dotnet build -c Release
cd ../AvaLang.Tests
dotnet run
```

Mientras el frontend siga siendo el stub (sin antlr4-runtime, ver
DESIGN.md pasos 2-6), todos los scripts van a reportar PENDING con el
mensaje "AvaLang frontend not built: ...". Es el comportamiento esperado
hoy, no un fallo del binding — confirma que la DLL nativa y el wrapper
managed estan bien conectados.

Ver DESIGN.md seccion 6 para el diseño original de este binding.
