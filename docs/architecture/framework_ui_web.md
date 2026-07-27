# Framework UI Web — Bugs y fixes

Historial de bugs / arreglos del bridge `AvaLang.UI` ↔ `avalang.dll` y de los renderers de UI web. Complementa a `AVAUI_FRAMEWORK.md` y `11_COMPONENT_TREE.md`.

## Contexto rápido

- `AvaLang.UI/Native/NativeMethods.cs` — simulación en memoria del bridge (no usa `[DllImport]`). El bridge real con `[DllImport("avalang")]` vive en `AvaLang.Interop/NativeMethods.cs`. El plan de migración está en `avalang-dotnet/PLAN_CENTRALIZACION.md`.
- `AvaLang.UI/Core/Component.cs` — wrapper de alto nivel que el renderer (`HtmlRenderer`) consume.
- `AvaLang.UI/Rendering/HtmlRenderer.cs` — convierte un `ComponentTree` a HTML.

## Tasks

### [COMPLETED] Bug: Properties de tipo string se serializaban como número
- **Date**: 2026-07-26
- **File**: `avalang-dotnet/AvaLang.UI/Native/NativeMethods.cs:53-64`, `avalang-dotnet/AvaLang.UI/Core/Component.cs:72-76`
- **Problem**: `ava_ui_set_property` (en la simulación) recibía solo `double value, int valueType`. Para `valueType=3` (string), guardaba `((int)value).ToString()`, perdiendo el string original. Resultado: `SetProperty("onclick", "handleClick()")` terminaba guardando `"0"` (porque `"handleClick()"` no parsea como double y se convertía a `0.0`). El HTML emitido era `onclick="0"` en lugar de `onclick="handleClick()"`.
- **Solution**:
  1. Añadido overload `ava_ui_set_property(IntPtr, string, double, int, string?)` que usa el string crudo cuando está presente.
  2. `Component.SetProperty` ahora pasa el `value as string` como quinto parámetro, preservando el string original.
  3. Tests añadidos en `RenderEventTests.cs` cubren: `onclick`, `oninput`, `data-action`, y verificación de que el handler aparece en el HTML (no el número `0`).
