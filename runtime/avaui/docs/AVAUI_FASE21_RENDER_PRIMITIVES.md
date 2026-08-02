# AVAUI — Fase 21: primitiva de elipse + borderRadius passthrough

Alcance: solo Windows (ver "Alcance actual" en `AVALANG_UI_PROGRESS.md`).
Trabajo hecho bajo `AVAUI_ARCHITECTURE_FREEZE_PLAN.md` (categoría
"Complete Internal Implementations" / "Finish Render Commands" /
"Finish Renderer") -- no se creó ningún módulo, folder ni abstracción
nueva; se completó lo que ya existía y se cerraron dos gaps que la
Fase 18 dejó documentados a propósito en vez de resolver en el momento
(`AVAUI_FASE18_CONTROLS.md`, sección 2).

## 0. Punto de partida: los dos gaps tal como quedaron documentados

`docs/AVAUI_FASE18_CONTROLS.md` sección 2 y la fila 18 de
`AVALANG_UI_PROGRESS.md` dejaron anotado, sin tocar ninguna interfaz
congelada en ese momento:

1. **Sin primitiva de círculo/elipse.** `RenderCommand`,
   `IRenderCommandSink` e `IRenderer` solo tenían
   `DrawRectangle`/`DrawText`/`DrawImage` (+`DrawPath`, no-op
   reservado). CheckBox y RadioButton se dibujaban ambos como
   cuadrados -- mismo lenguaje visual, RadioButton sin el círculo que
   visualmente se espera de un radio button.
2. **`borderRadius` no llegaba a ningún lado.** El Theme (Fase 16) ya
   calculaba `theme.Spacing().borderRadiusPx` y lo escribía en el
   property bag de Button/Image (`RenderTheme.cpp`, líneas ~41 y ~85),
   pero `RenderCommand.drawRect` / `IRenderer::DrawRectangle` /
   `IRenderCommandSink::DrawRectangle` no tenían parámetro de radio, y
   nada en `RenderTree::BuildComponent` leía la propiedad hacia
   `IRenderNode::BorderRadius()` (que sí existía desde la Fase 6,
   simplemente nunca se llenaba). `HTMLRenderer::OnDrawRectangle`
   tampoco emitía `border-radius` en el CSS aunque hubiera recibido el
   valor.

Ambos gaps tocaban interfaces congeladas desde la Fase 13
(`RenderCommand`, `IRenderer` -- 2 de las 9). Esta fase los cierra de
forma explícitamente aditiva, sin romper ninguna firma publicada, y
hace el bump de ABI correspondiente que la Fase 18 dejó pendiente a
propósito.

## 1. Diseño: aditivo, no ruptura

Regla seguida en cada cambio (`AVAUI_ARCHITECTURE_FREEZE_PLAN.md`:
"Preferir extender ... de forma aditiva ... en vez de romper lo
existente"):

- **Enums nuevos valores, siempre al final.** `RenderCommandType::DrawEllipse`
  y `RenderNodeType::Ellipse` se agregaron como el último enumerador de
  cada enum, no intercalados entre los existentes -- así ningún
  enumerador ya emitido en código compilado cambia de valor integral.
- **Structs nuevos campos, nunca removidos/reordenados.**
  `RenderCommand.drawRect` gana `borderRadius` (float, al final del
  struct); se agrega el struct hermano `drawEllipse` completo (nuevo,
  no reemplaza nada).
- **Métodos con parámetro nuevo, siempre al final y con default.**
  `DrawRectangle` (en `IRenderCommandSink`, `IRenderer`, `RenderCommandSink`,
  `BaseRenderer`) gana `float borderRadius = 0.0f` como último
  parámetro -- cualquier llamador preexistente que pase solo los 7
  argumentos originales sigue compilando igual que antes, con
  `borderRadius` implícito en 0 (mismo comportamiento visual que antes
  de esta fase: esquinas rectas).
- **`DrawEllipse` es un método nuevo, no una sobrecarga de
  `DrawRectangle`.** Nueva primitiva de comando ("variant" en el
  vocabulario de `RenderCommand`), tal como pedía el freeze plan en vez
  de forzar el radio en el rectángulo (que no serviría para el círculo
  real de un RadioButton, solo para esquinas redondeadas).

Estos son cambios de firma a 2 de las 9 interfaces congeladas
(`RenderCommand`, `IRenderer`) según la política de
`AVAUI_FASE13_INTERFACE_FREEZE.md` ("cualquier cambio de firma exige
bump de `UIModule::AbiVersion()` y nota en el progreso, no un edit
silencioso") -- de ahí el bump 18 -> 19 (ver sección 4).

## 2. Cambios por archivo

### `commands/RenderCommand.h`
- `RenderCommandType::DrawEllipse` agregado al final del enum.
- `drawRect.borderRadius` (float) agregado al final del struct
  anónimo existente.
- `drawEllipse` (struct anónimo nuevo): `cx, cy, rx, ry` (centro +
  radios, no bounding box -- permite óvalos sin variant adicional) +
  `fillColor`/`borderColor`/`borderWidth`, mismo patrón que `drawRect`.

### `commands/IRenderCommandSink.h` + `commands/RenderCommandSink.h/.cpp`
- `DrawRectangle(...)` gana `float borderRadius = 0.0f` como último
  parámetro.
- `DrawEllipse(cx, cy, rx, ry, fillColor, borderColor, borderWidth)`
  nuevo, pure virtual en la interfaz, implementado en
  `RenderCommandSink` construyendo un `RenderCommand` con
  `type = DrawEllipse`.

### `renderer/IRenderer.h`
- Mismo tratamiento que `IRenderCommandSink`: `DrawRectangle` gana
  `borderRadius` con default, `DrawEllipse` nuevo pure virtual.

### `renderer/BaseRenderer.h/.cpp`
- `ProcessCommands()` gana el `case RenderCommandType::DrawEllipse`
  (dispatch a `OnDrawEllipse`), y el `case DrawRectangle` ahora pasa
  `cmd.drawRect.borderRadius` a `OnDrawRectangle`.
- `DrawRectangle()`/`DrawEllipse()` públicos (llamados directo, sin
  pasar por `RenderCommand`, como hace algún test) forwardean a los
  `OnDraw*` protegidos igual que antes.
- `OnDrawRectangle` (protegido, pure virtual) gana el parámetro
  `borderRadius`; `OnDrawEllipse` (protegido, pure virtual) nuevo. Las
  3 subclases (`NullRenderer`, `HTMLRenderer`, `GdiRenderer`) se
  actualizaron para matchear.

### `renderer/NullRenderer.h/.cpp`
- `OnDrawRectangle` con el parámetro nuevo, `OnDrawEllipse` nuevo --
  ambos no-op, sin cambio de comportamiento (sigue siendo el stub que
  no dibuja nada).

### `renderer/HTMLRenderer.h/.cpp` (backend validado en esta fase)
- `OnDrawRectangle`: si `borderRadius > 0.0f`, emite
  `border-radius: <N>px;` en el `style=""` inline. Si es 0 (default),
  el output es byte-a-byte idéntico al de antes de esta fase (no se
  emite la propiedad).
- `OnDrawEllipse` (nuevo): un único `<div>` posicionado por su bounding
  box (`left = cx - rx`, `top = cy - ry`, `width = 2*rx`,
  `height = 2*ry`) con `border-radius: 50%` -- el mismo truco CSS
  estándar para dibujar círculos/óvalos con un elemento rectangular;
  funciona para óvalos porque el porcentaje se resuelve por eje de
  forma independiente, no como una sola elipse inscrita cuadrada.

### `renderer/GdiRenderer.h/.cpp` (Windows, actualizado en código, NO
compilado con MSVC ni con mingw en esta sesión -- ver sección 5)
- `OnDrawRectangle`: si `borderRadius > 0`, usa `RoundRect(hdc, left,
  top, right, bottom, diameter, diameter)` en vez de `Rectangle(...)`.
  `RoundRect` toma diámetro de la elipse de esquina, no radio, de ahí
  el `borderRadius * 2`; GDI clampa internamente si el diámetro excede
  el tamaño del rectángulo, no hizo falta clamping manual.
- `OnDrawEllipse` (nuevo): `Ellipse(hdc, cx-rx, cy-ry, cx+rx, cy+ry)`
  con el mismo patrón brush/pen (crear, seleccionar, dibujar,
  restaurar, liberar) que ya usaba `OnDrawRectangle`.

### `render_tree/IRenderNode.h`
- `RenderNodeType::Ellipse` agregado al final del enum (después de
  `Custom`), no intercalado en el bloque "Visual primitives" -- mismo
  criterio de no correr valores existentes que `RenderCommandType`.
  `IRenderNode::BorderRadius()`/`RenderNode::SetBorderRadius()` **no
  cambiaron** -- ya existían desde la Fase 6, el gap nunca fue la
  interfaz sino que nada la usaba (ver siguiente punto).

### `render_tree/RenderTree.cpp` -- el fix central de los dos gaps
- `BuildComponent()`: nuevo bloque que lee la propiedad `"borderRadius"`
  del property bag (la misma que `RenderTheme.cpp` ya escribía para
  Button/Image desde la Fase 16) hacia `renderNode->SetBorderRadius(...)`,
  mismo patrón que el bloque preexistente para `borderWidth` justo
  arriba. Esto es lo que hace que el valor calculado por Theme
  finalmente llegue al Render Tree en vez de perderse ahí.
- `DecomposeRadioButton()`: el `boxNode` (el indicador, antes
  `RenderNodeType::Rectangle`) pasa a `RenderNodeType::Ellipse`. Sigue
  siendo su propio método (no un alias de `DecomposeCheckBox`, como ya
  documentaba la Fase 18) -- ahora también porque su indicador es un
  tipo de nodo distinto, no solo porque lee nombres de propiedad
  distintos.
- `DecomposeCheckBox()`: sin cambios de comportamiento, solo
  actualización del comentario (ya no comparte "el mismo lenguaje
  visual" con RadioButton -- el cuadrado ahora es una decisión
  explícita de CheckBox, no una limitación compartida).

### `commands/SceneCommandWalker.cpp` -- el único productor real de
`RenderCommand`s (Fase 14), donde ambos gaps se cerraban en la
práctica
- El bloque que antes llamaba incondicionalmente a
  `sink.DrawRectangle(x, y, w, h, fill, border, borderWidth)` ahora
  rama por `renderNode->Type()`:
  - `RenderNodeType::Ellipse` -> `sink.DrawEllipse(cx, cy, rx, ry, ...)`,
    con `cx = x + w/2`, `cy = y + h/2`, `rx = w/2`, `ry = h/2` --
    el rect geométrico que ya calculaba LayoutEngine se reinterpreta
    como bounding box de la elipse, sin tocar Layout.
  - cualquier otro tipo -> `sink.DrawRectangle(..., renderNode->BorderRadius())`,
    ahora pasando el radio en vez de perderlo.

### `UIModule.h` / `UIModule.cpp`
- `AbiVersion()`: 18 -> **19**. Comentario nuevo en `UIModule.h`
  documentando exactamente qué cambió y por qué es aditivo (ver
  sección 4).

## 3. Test / Validación

Ejecutado de verdad en esta sesión (headless, `g++ -std=c++20`,
`libglm-dev` vía apt -- mismo método que las Fases 14/18/19):

- **47 `.cpp` de `ui/src/` independientes de plataforma** (todo salvo
  `platform/*` y `renderer/GdiRenderer.cpp`, que requiere
  `<windows.h>`) compilan limpio y enlazan en `libavalang_ui.a`.
- **`ava_button_demo`** (Fase 17, extendido en esta sesión): nuevo
  assert -- `button->GetProperty("borderRadius")` existe (viene del
  Theme) y el HTML final contiene `border-radius: 4px` (4 = 
  `DefaultTheme::spacing_.borderRadiusPx`). Corrido: **PASS**, 10/10
  (bloques de test originales) + la aserción nueva del bloque 8.
- **`ava_controls_demo`** (Fase 18, extendido en esta sesión): nuevo
  bloque "Test 6" -- el HTML final contiene `border-radius: 50%`
  (el marcador único de `HTMLRenderer::OnDrawEllipse`). Corrido:
  **PASS**, 6/6 bloques. Verificado a mano además (no solo por el
  assert) que el CheckBox (16x16) no lleva `border-radius` y ambos
  RadioButton (16x16) sí, confirmando que dejaron de compartir
  lenguaje visual.
- **`ava_ui_pipeline_demo`** (Fase 14, regresión): mismos números que
  la Fase 14 documentó (7 componentes, layout 800x600, 7 render
  commands, 1577 bytes de HTML) -- sin cambios, porque ese fixture no
  pasa por `RenderTheme::Apply` (nunca tuvo `borderRadius` que
  perder). Corrido: **PASS**.
- **`ava_animation_demo`** (Fase 19, regresión): mismos números que la
  Fase 19 documentó (opacidad 1.0 -> 0.9 -> 0.6 -> 0.2 en los frames
  simulados, 1049 bytes de HTML en cada snapshot). Corrido: **PASS**.

Nota honesta sobre un detalle *no* introducido por esta fase, visto al
inspeccionar el HTML generado a mano: cuando `ShouldFill()`/
`ShouldStroke()` es `false`, `SceneCommandWalker` igual pasa
`Color{0,0,0,0}` como `fillColor`/`borderColor`, y
`HTMLRenderer::ColorToHex()` ignora el canal alfa -- el resultado es
`background-color: #000000` opaco en vez de "sin relleno". Esto ya
pasaba antes de esta fase para el cuadrado de CheckBox/RadioButton sin
seleccionar (mismo código de cálculo de color, sin tocar); se deja
anotado acá para que no se confunda con una regresión de la Fase 21,
pero **no se corrigió** -- está fuera del alcance de "elipse +
borderRadius" y toca la Fase 8/9 (fuera de las dos cosas pedidas).

## 4. ABI

`UIModule::AbiVersion()` pasa de **18** a **19**. Motivo (documentado
también en `UIModule.h`): cambio de firma aditivo en 2 de las 9
interfaces congeladas desde la Fase 13 (`RenderCommand`: nuevo
enumerador + nuevo campo + nuevo struct; `IRenderer`: nuevo parámetro
con default en `DrawRectangle` + nuevo método `DrawEllipse`). Ningún
llamador ni implementación preexistente se rompió -- se verificó
compilando las 47 fuentes independientes de plataforma sin tocar nada
fuera de los archivos listados en la sección 2.

## 5. Alcance Windows / limitaciones

Igual que todas las fases anteriores: sin toolchain Windows (MSVC) ni
mingw disponibles en este sandbox. `renderer/GdiRenderer.cpp` se
actualizó en código (`RoundRect`/`Ellipse`, sección 2) pero **no se
compiló ni se corrió** en esta sesión -- validado solo por lectura
contra la API real de GDI y el mismo patrón que ya usaba
`OnDrawRectangle`/`OnDrawImage` antes de esta fase. El backend HTML
(Fase 10) sí se validó de punta a punta, tal como pidió la persona
("cerrar primero en el backend HTML, y de ahí propagar a GdiRenderer
si el tiempo alcanza").

Linux/macOS: sin trabajo nuevo, siguen en STUB a propósito (ver
"Alcance actual" en `AVALANG_UI_PROGRESS.md`).

## 6. Entregable

- `docs/AVAUI_FASE21_RENDER_PRIMITIVES.md` (este archivo).
- `commands/RenderCommand.h`, `commands/IRenderCommandSink.h`,
  `commands/RenderCommandSink.h/.cpp` -- `DrawEllipse` + `borderRadius`.
- `renderer/IRenderer.h`, `renderer/BaseRenderer.h/.cpp`,
  `renderer/NullRenderer.h/.cpp`, `renderer/HTMLRenderer.h/.cpp`,
  `renderer/GdiRenderer.h/.cpp` -- implementaciones actualizadas.
- `render_tree/IRenderNode.h` (`RenderNodeType::Ellipse`),
  `render_tree/RenderTree.cpp` (borderRadius passthrough +
  RadioButton -> Ellipse).
- `commands/SceneCommandWalker.cpp` -- branch Ellipse/Rectangle +
  borderRadius al walker real.
- `UIModule.h/.cpp` -- `AbiVersion()` = 19.
- `tests/ButtonDemo.cpp`, `tests/ControlsDemo.cpp` -- aserciones nuevas
  de regresión para que estos dos gaps no se reabran en silencio.

Con esto, los dos gaps que `AVAUI_FASE18_CONTROLS.md` dejó anotados
quedan cerrados para el backend HTML (validado de punta a punta) y
propagados en código al backend nativo de Windows (sin validar por
falta de toolchain, mismo límite de siempre).
