# Fase 18 Summary — Controls: resto del set base

**Objetivo:** Text, TextBox, Column/Row/Stack, Image, CheckBox, RadioButton — mismo patrón que Button (Fase 17).

**Status:** ✅ **CLOSED**

---

## Antes de empezar: 3 bugs reales encontrados corriendo Fase 17 de verdad

Fase 17 se había declarado "ALL TESTS PASSED" sin haber compilado nunca el demo. Al validarlo para poder confiar en el patrón que Fase 18 iba a repetir:

1. `ButtonDemo.cpp` llamaba a 5 funciones que no existen (`CreateComponentTree`, `CreateLayoutEngine`, `CreateRenderTree`, `CreateSceneGraph`, `CreateHTMLRenderer`) — reescrito con la API real (factories `Create()` estáticas), corre y pasa de verdad.
2. `ui/src/render/` + `ui/include/avalang/ui/render/`: duplicado muerto y roto del rename a `render_tree/` (Fase 13), nunca borrado. Eliminado.
3. `PropertyValue("FFFFFF")` invocaba `PropertyValue(bool)` en vez de `PropertyValue(std::string)` (conversión estándar `const char*→bool` gana contra la conversión de usuario a `std::string`, aunque los ctores sean `explicit`). `Button::textColor` quedaba vacío. Agregado `PropertyValue(const char*)`.

Detalle completo en `docs/AVAUI_FASE18_CONTROLS.md`, sección 0.

---

## Entregables

### 1. Public API (`ui/include/avalang/ui/controls/`)
- `Text.h` — `CreateText`, `SetTextValue`
- `Image.h` — `CreateImage`, `SetImageSource`
- `Container.h` — `CreateColumn`, `CreateRow`, `CreateStack`, `SetContainerSpacing`
- `TextBox.h` — `CreateTextBox`, `SetTextBoxValue`, `GetTextBoxValue`, `Bind/UnbindTextBoxChange`
- `CheckBox.h` — `CreateCheckBox`, `SetCheckBoxChecked`, `ToggleCheckBox`, `GetCheckBoxChecked`, `Bind/UnbindCheckBoxChange`
- `RadioButton.h` — `CreateRadioButton`, `SelectRadioButton`, `GetRadioButtonSelected`, `Bind/UnbindRadioButtonChange`

### 2. Implementation (`ui/src/controls/`)
- 6 pares `.h`/`.cpp`, mismo patrón que `Button.cpp`/`ButtonInternal.h`.

### 3. RenderTree decomposition (`ui/src/render_tree/RenderTree.cpp`)
- `DecomposeTextBox` — caja + texto actual o placeholder (gris).
- `DecomposeCheckBox` — cuadrado (relleno si checked) + label.
- `DecomposeRadioButton` — mismo lenguaje visual que CheckBox (sin
  círculo — el renderer no tiene primitiva de elipse, ver limitaciones).

### 4. Test/Validation (`ui/tests/ControlsDemo.cpp`)
- 5 bloques: creación de los 8 controles, comportamiento (toggle, callback de cambio, exclusión de grupo de radio), theme, pipeline completo a HTML, validación del HTML resultante.

### 5. Build (`ui/CMakeLists.txt`)
- 6 fuentes nuevas + target `ava_controls_demo` (bajo `AVA_BUILD_UI_TESTS`).

---

## Test Results (corrido de verdad, g++ -std=c++20, headless)

```
[ControlsDemo] === Fase 18: Controls Demo (rest of base set) ===
Test 1: Build ComponentTree with all Fase 18 controls   OK
Test 2: Control behavior (setters/toggles/group excl.)  OK
Test 3: Apply Theme                                     OK
Test 4: Layout -> RenderTree -> SceneGraph -> HTML       OK (3065 bytes)
Test 5: Validate HTML output                             OK

=== ALL TESTS PASSED ===
Phase 18 validation: PASS
```

`ButtonDemo` (corregido) y `AvauiPipelineDemo` (Fase 14, regresión) se corrieron en la misma sesión — ambos siguen pasando.

---

## Platform Status

| Platform | Status | Notes |
|----------|--------|-------|
| Windows | ✅ Full (headless-validated, MSVC real pendiente) | Igual que Fase 17 |
| Linux | ✅ Stub | Sin trabajo nuevo, a propósito |
| macOS | ✅ Stub | Sin trabajo nuevo, a propósito |

---

## Gaps conocidos, documentados sin tocar interfaces congeladas

- Sin primitiva de círculo/elipse en `RenderCommand`/`IRenderer` (Fase 8/9, congeladas desde Fase 13) → CheckBox y RadioButton se ven iguales (cuadrados).
- `borderRadius` calculado por Theme nunca llega a `RenderCommand.drawRect` — se pierde entre RenderTree y Renderer.

---

## ABI Version: 17
## Date: 2026-07-30
## Status: ✅ CLOSED

## Next Steps

**Fase 19 — Animation**: transform/opacity interpolation sobre `ISceneNode`, ahora con controles reales para animar.
