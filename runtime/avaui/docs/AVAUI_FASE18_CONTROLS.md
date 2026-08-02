# Fase 18 — Controls: resto del set base

**Objetivo:** Text, TextBox, Column/Row/Stack, Image, CheckBox, RadioButton
— el resto de la lista del Toolbox (`AVAUI_PLAN_FASE12_PLUS.md`), mismo
patrón que Button (Fase 17): crear → theme → pipeline completo.

**Status:** ✅ **CLOSED** (Windows/generic pipeline; ver "Alcance" abajo)

---

## 0. Antes de escribir controles: Fase 17 no estaba realmente cerrada

Al abrir esta fase, el primer paso fue compilar y correr de verdad lo
que Fase 17 dejó documentado como "ALL TESTS PASSED". No lo estaba:

- **`ui/tests/ButtonDemo.cpp` nunca compiló.** Llamaba a cinco funciones
  libres que no existen en ningún lado del código
  (`avalang::ui::CreateComponentTree()`, `CreateLayoutEngine()`,
  `CreateRenderTree()`, `CreateSceneGraph()`, `CreateHTMLRenderer()`) y
  a un método `IRenderer::Render(sceneGraph, ostream)` que tampoco
  existe. Las factories reales son métodos estáticos `Create()` en cada
  interfaz — el patrón correcto ya estaba validado en
  `ui/tests/AvauiPipelineDemo.cpp` (Fase 14), que sí compila y corre.
  Reescrito con ese mismo patrón; corre y pasa sus 10 tests de verdad
  (verificado con g++, ver sección 5).
- **`ui/src/render/` e `ui/include/avalang/ui/render/` seguían en el
  repo**, duplicados de `render_tree/` (el rename cosmético de la Fase
  13), y estaban rotos (`IRenderNode.h` sin sus includes de
  `ComponentId`/`LayoutRect`). No estaban en `CMakeLists.txt`, así que
  nadie lo notó. Eliminados — sin referencias activas, confirmado con
  `grep`.
- **Bug real en `PropertyValue`**: `PropertyValue("FFFFFF")` (usado en
  `RenderTheme::ApplyTypeDefaults` para el `textColor` de Button)
  invocaba el constructor `PropertyValue(bool)`, no
  `PropertyValue(std::string)` — `const char* → bool` es una conversión
  estándar, `const char* → std::string` es una conversión definida por
  el usuario, y la resolución de sobrecarga siempre prefiere la
  primera, sin importar que ambos constructores sean `explicit`.
  Consecuencia real: `Button::textColor` quedaba guardado como `false`,
  y `AsString()` devolvía `""`. Confirmado con un repro mínimo
  (`/tmp/t.cpp`, tres constructores explicit, `PV p("FFFFFF")` imprime
  `"bool ctor"`). Arreglado agregando `PropertyValue(const char*)`
  (declarado en el header, implementado en `PropertyValue.cpp`) — no
  rompe ninguna firma existente, cierra el hueco de raíz para todo el
  código futuro que construya `PropertyValue` desde un literal (que es
  casi todo `controls/`).

Ninguno de estos tres hallazgos estaba en el plan original. Se corrigen
acá porque bloqueaban validar Fase 18 con confianza — construir
controles nuevos sobre una demo que nunca compiló, o sobre un bug de
overload resolution que silencia colores, hubiera repetido el mismo
patrón de "cerrado sin correr".

---

## 1. Controles nuevos

Mismo patrón que Button en los tres frentes (creación, theme,
render/decomposición):

### Text (`controls/Text.h/.cpp`)
- `CreateText(tree, text)`, `SetTextValue(component, text)`.
- Ya tenía tratamiento completo en Theme (rol `"body"`) y RenderTree
  (`DecomposeText`) desde Fases 16/6 — este control solo agrega la
  factory tipada.

### Image (`controls/Image.h/.cpp`)
- `CreateImage(tree, src)`, `SetImageSource(component, src)`.
- Propiedad `alt` reservada (no consumida por ningún renderer todavía).
- Igual que Text: Theme (`borderRadius`) y RenderTree (`DecomposeImage`)
  ya existían.

### Container: Column / Row / Stack (`controls/Container.h/.cpp`)
- `CreateColumn(tree)`, `CreateRow(tree)`, `CreateStack(tree)`,
  `SetContainerSpacing(container, px)`.
- Ya eran `TypeName()`s de primera clase para LayoutEngine
  (`ArrangeRowOrColumn`/`ArrangeStack`, Fase 3) y RenderTree
  (`DecomposeContainer`, Fase 6). Lo que faltaba era el punto de
  entrada tipado — antes cada demo hacía
  `tree->CreateComponent("Column")` a mano.

### TextBox (`controls/TextBox.h/.cpp`)
- Primer control con **input real** de los tres. Modelo de datos:
  `text` / `placeholder` / `isFocused` / `isEnabled`.
- `CreateTextBox(tree, placeholder)`, `SetTextBoxValue(component, value)`
  (dispara el callback de cambio), `GetTextBoxValue(component)`,
  `BindTextBoxChange`/`UnbindTextBoxChange` (mismo registro global por
  `ComponentId` que ya validó Button para clicks).
- **No** está enganchado a teclado real de `IEventDispatcher`/
  `WinKeyboard` — `IEventDispatcher` (Fase 5) solo tiene hit-testing +
  evento de Click, no un modelo de cursor/selección de texto. Eso queda
  para la Fase 20 (integración con AvaHost/Studio), que sí tiene una
  ventana real y un loop de teclado contra el cual probarlo. Ver
  `RenderTree::DecomposeTextBox`: dibuja el `text` actual, o el
  `placeholder` en gris si está vacío.

### CheckBox (`controls/CheckBox.h/.cpp`)
- `CreateCheckBox(tree, label, isChecked=false)`,
  `SetCheckBoxChecked`, `ToggleCheckBox` (conveniencia para un
  handler de click), `GetCheckBoxChecked`,
  `BindCheckBoxChange`/`UnbindCheckBoxChange`.
- `RenderTree::DecomposeCheckBox`: cuadrado pequeño (borde siempre,
  relleno sólido si `isChecked`) + texto de `label` a la derecha.

### RadioButton (`controls/RadioButton.h/.cpp`)
- `CreateRadioButton(tree, label, group, isSelected=false)`,
  `SelectRadioButton` (deselecciona a todo el resto del grupo),
  `GetRadioButtonSelected`, `BindRadioButtonChange`/`Unbind...`.
- `ComponentTree` (Fase 2) no tiene concepto de "grupo", así que
  RadioButton mantiene su propio registro `group name -> vector<IComponent*>`
  a nivel de proceso (mismo estilo que el registro de callbacks de
  Button), documentado en el header como una limitación a revisar en
  Fase 20 si en algún momento se destruyen/recrean árboles en caliente.
- `RenderTree::DecomposeRadioButton`: mismo lenguaje visual que
  CheckBox (cuadrado, no círculo — ver siguiente sección).

---

## 2. Gap de renderer que se dejó documentado, no "arreglado"

> **Actualización (Fase 21):** los dos gaps de esta sección quedaron
> **cerrados** -- ver `docs/AVAUI_FASE21_RENDER_PRIMITIVES.md` para el
> reporte completo. Se dejan las notas originales de la Fase 18 tal
> cual estaban (contexto histórico de por qué no se resolvieron en su
> momento), la actualización es solo este párrafo.

Dos cosas salieron a la luz al implementar CheckBox/RadioButton que
tocan fases ya congeladas (Fase 8/9, freeze desde Fase 13) y que
**no** se tocaron en esta fase:

1. **Sin primitiva de círculo/elipse.** `RenderCommand`,
   `IRenderCommandSink` e `IRenderer` solo tienen
   `DrawRectangle`/`DrawText`/`DrawImage` (+`DrawPath`, documentado como
   no-op reservado). CheckBox y RadioButton se dibujan ambos como
   cuadrados — RadioButton no tiene el círculo que visualmente se
   espera de un radio button. Arreglarlo de raíz significa agregar una
   primitiva nueva a una interfaz congelada (bump de ABI + cambio de
   firma en 3 sitios), que es una decisión de la fase de Renderer, no
   de Controls — queda anotado para cuando se retome.
2. **`borderRadius` no llega a ningún lado.** El Theme (Fase 16) ya
   calcula `theme.Spacing().borderRadiusPx` y lo escribe en el
   property bag de Button/Image, pero `RenderCommand.drawRect` e
   `IRenderer::DrawRectangle`/`IRenderCommandSink::DrawRectangle` no
   tienen parámetro de radio — se pierde entre RenderTree y el
   Renderer. `HTMLRenderer::OnDrawRectangle` tampoco emite
   `border-radius` en el CSS. Mismo caso que el anterior: cambiar esas
   firmas es zona congelada, se documenta para no tocarla sin
   avisar.

Ninguno de los dos rompe ningún test de esta fase (ninguna aserción
verifica esquinas redondeadas o círculos), pero conviene que quede
escrito para que la próxima persona no lo redescubra desde cero.

---

## 3. Test/Validación (`ui/tests/ControlsDemo.cpp`)

Mismo formato que `ButtonDemo.cpp`. 5 bloques de test:

1. Construye un árbol con los 8 controles (Text, Image, Row, Stack,
   TextBox, CheckBox, 2× RadioButton en el mismo grupo).
2. Comportamiento sin renderizar todavía: `SetTextValue`, callback de
   cambio de TextBox, `ToggleCheckBox`, exclusión mutua de
   RadioButton (seleccionar B debe deseleccionar A).
3. `RenderTheme::Apply()` sobre el árbol completo — verifica que cada
   control nuevo recibió sus propiedades de theme.
4. Pipeline completo: Layout → RenderTree → SceneGraph →
   `SceneCommandWalker` → `HTMLRenderer`.
5. Verifica que el HTML final contiene el texto actualizado, el valor
   del TextBox, las labels de CheckBox/RadioButton y el `src` de
   Image.

### Resultado real (g++ -std=c++20, headless, mismo criterio de
validación que ya usó la Fase 14)

```
[ControlsDemo] === Fase 18: Controls Demo (rest of base set) ===
...
[ControlsDemo]   OK all 8 controls created (Text, Image, Row, Stack, TextBox, CheckBox, 2x RadioButton)
[ControlsDemo]   OK Text::SetTextValue
[ControlsDemo]   OK TextBox value + change callback
[ControlsDemo]   OK CheckBox toggle
[ControlsDemo]   OK RadioButton group mutual exclusion
[ControlsDemo]   OK Theme applied to all Fase 18 controls
[ControlsDemo]   OK pipeline produced 3065 bytes of HTML
[ControlsDemo]   OK HTML contains all expected control output
[ControlsDemo]
=== ALL TESTS PASSED ===
Phase 18 validation: PASS
```

`ButtonDemo` (reescrito) y `AvauiPipelineDemo` (Fase 14, regresión) se
corrieron en la misma sesión con el mismo binario de librería —
ambos siguen pasando.

---

## 4. Archivos

**Nuevos:**
- `ui/include/avalang/ui/controls/{Text,Image,Container,TextBox,CheckBox,RadioButton}.h`
- `ui/src/controls/{Text,Image,Container,TextBox,CheckBox,RadioButton}.cpp`
- `ui/tests/ControlsDemo.cpp`
- `docs/AVAUI_FASE18_CONTROLS.md` (este archivo)
- `docs/AVAUI_FASE18_SUMMARY.md`

**Modificados:**
- `ui/src/render_tree/RenderTree.h/.cpp` — despacho + 3 métodos
  `Decompose{TextBox,CheckBox,RadioButton}` nuevos.
- `ui/include/avalang/ui/components/PropertyValue.h` +
  `ui/src/components/PropertyValue.cpp` — overload
  `PropertyValue(const char*)`.
- `ui/tests/ButtonDemo.cpp` — reescrito con API real (ver sección 0).
- `ui/CMakeLists.txt` — 6 fuentes nuevas + target `ava_controls_demo`.
- `ui/include/avalang/ui/UIModule.h` / `ui/src/UIModule.cpp` — ABI
  16 → 17.

**Eliminados:**
- `ui/src/render/`, `ui/include/avalang/ui/render/` (duplicado muerto,
  ver sección 0).

---

## 5. Cómo se validó (alcance real de esta sesión)

Sin toolchain de Windows/MSVC disponible en este sandbox — igual que
todas las fases anteriores desde la 12. A diferencia de Fase 17 (que
se declaró cerrada sin correr nada), esta vez sí se compiló y **corrió**
todo lo independiente de plataforma con `g++ -std=c++20`:

```
libglm-dev instalado vía apt (dependencia de Scene Graph, Fase 7)
35 -> 41 archivos .cpp de ui/src/ (excluyendo platform/windows,
     platform/linux, platform/macos, GdiRenderer) compilan limpio
     y se linkean en una libavalang_ui.a estática
ava_button_demo    -> corre, 10/10 tests OK
ava_controls_demo  -> corre, 5/5 bloques OK
ava_ui_pipeline_demo (Fase 14, regresión) -> corre, sin cambios de
     comportamiento
```

Pendiente, igual que en todas las fases previas: compilar
`platform/windows/*` + `GdiRenderer.cpp` con MSVC real y correr un
`.exe` que efectivamente abra una ventana Win32. Linux/macOS
(`platform/linux/`, `platform/macos/`) siguen como STUB a propósito,
sin trabajo nuevo — ver "Alcance actual: solo Windows" en
`docs/AVALANG_UI_PROGRESS.md`.

---

## 6. Siguiente fase

**Fase 19 — Animation**: transform/opacity interpolation sobre
`ISceneNode` (Fase 7, ya tiene los campos). Ahora sí hay controles
reales para animar (fade-in de un CheckBox, slide de un TextBox, etc.).
