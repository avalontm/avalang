# Fase 22 — Compatibility Matrix

Construida por inspección directa del código fuente en `runtime/avaui/src`, no por inferencia. Cada celda referencia el archivo y símbolo verificado.

Leyenda: `OK` implementado y verificado. `FALTA` sin implementación (fallback vacío o ausente). `PARCIAL` implementado con alcance reducido.

| Control | RenderTree | Commands | GDI | Web | Android | Linux | macOS | HitTest | Focus | Keyboard | Theme | Accessibility | Tests |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Text | OK | OK | OK | OK | OK | FALTA | FALTA | OK | FALTA | FALTA | OK | OK | OK |
| Image | OK | OK | OK | OK | OK | FALTA | FALTA | OK | FALTA | FALTA | OK | OK | FALTA |
| Button | OK | OK | OK | OK | OK | FALTA | FALTA | OK | OK | OK | OK | OK | OK |
| TextBox | OK | OK | OK | FALTA | FALTA | FALTA | FALTA | OK | OK | OK | OK | OK | OK |
| CheckBox | OK | OK | OK | FALTA | OK | FALTA | FALTA | OK | OK | OK | OK | OK | OK |
| RadioButton | OK | OK | OK | FALTA | OK | FALTA | FALTA | OK | OK | OK | OK | OK | OK |
| ComboBox | OK | OK | OK | FALTA | FALTA | FALTA | FALTA | OK | OK | OK | OK | OK | PARCIAL |
| ScrollView | OK | OK | OK | OK | OK | FALTA | FALTA | OK | FALTA | FALTA | OK | OK | FALTA |
| Dialog | OK | OK | OK | OK | OK | FALTA | FALTA | OK | PARCIAL | PARCIAL | OK | OK | FALTA |

## Evidencia por columna

### GDI / Web / Android

`BaseRenderer.h` declara `OnDrawInput`, `OnDrawCheckBox`, `OnDrawRadioButton`, `OnDrawComboBox` como virtuales con cuerpo vacío (`{}`), no puros. Un renderer que no los sobrescribe compila pero no dibuja nada para ese control.

- `GdiRenderer` (`platform/windows/GdiRenderer.cpp`): sobrescribe las cuatro. Cobertura completa de los 9 controles.
- `HTMLRenderer` (`renderer/HTMLRenderer.cpp/.h`): sobrescribe `OnDrawButton`/`OnDrawLink`/`OnDrawPath` pero no `OnDrawInput`, `OnDrawCheckBox`, `OnDrawRadioButton` ni `OnDrawComboBox`. TextBox, CheckBox, RadioButton y ComboBox no se pintan en Web.
- `AndroidCanvasRenderer` (`platform/android/AndroidCanvasRenderer.cpp/.h`): sobrescribe `OnDrawCheckBox` y `OnDrawRadioButton`, pero no `OnDrawInput` ni `OnDrawComboBox`. TextBox y ComboBox no se pintan en Android.
- ScrollView y Dialog no dependen de estas cuatro: emiten `DrawRectangle`, que es puro virtual e implementado en los tres renderers, así que quedan `OK` en Web y Android pese a los huecos anteriores.

### Linux / macOS

`platform/linux/LinPlatform.cpp` y `platform/macos/MacPlatform.cpp` solo registran la plataforma y devuelven `new stub::StubAppSurface()`. No existe un renderer real (no hay ningún `.cpp` de renderer bajo `platform/linux/` ni `platform/macos/`). Ningún control puede marcarse `OK` en estas dos columnas.

### HitTest

`HitTest.cpp` resuelve hit-testing por bounds del `RenderTree` para cualquier tipo de nodo, no solo los interactivos (`HitTestTests.cpp::ClickOnLabelHitsTheLabel` prueba esto sobre `Text`). `OwnsInteractionSubtree` decide cuáles absorben el evento como propietarios interactivos (`Button`, `Link`, `TextBox`, `CheckBox`, `RadioButton`, `ComboBox`); los demás siguen siendo hit-testables pero delegan a hijos o al padre.

### Focus

`HitTest.cpp::OwnsInteractionSubtree` es también la lista que consume `EventDispatcher::CollectFocusableDescendants` para foco por teclado. Focusables reales: `Button`, `Link`, `TextBox`, `CheckBox`, `RadioButton`, `ComboBox`. `Text`, `Image` y `ScrollView` no son focusables. `Dialog` no es focusable en sí mismo, pero como overlay controla el focus trap de sus descendientes vía `EventDispatcher::EnforceFocusTrap` — de ahí `PARCIAL`.

### Keyboard

De los controles focusables, `TextBoxEditingController.cpp` y `ComboBoxController.cpp` suscriben `EventType::KeyDown` y traducen teclas a comportamiento (edición de texto, navegación de opciones). `ButtonController.cpp` (nuevo), `CheckBoxController.cpp` y `RadioButtonController.cpp` ahora también suscriben `KeyDown`: Espacio/Enter (`controls::IsActivationKey`, en `ActivationKey.h`) activa el control igual que un click de mouse — para Button despachando un `Click` sintético (consumido por el mismo camino que ya usan `WireVmEventHandlers`/`WireHrefNavigation` en avahost), para CheckBox/RadioButton reutilizando el mismo `Activate()` que ya corre en el caso `Click`. `ScrollView` no tiene scroll por flechas. `Dialog` obtiene cierre con Escape solo por el manejo genérico en `EventDispatcher.cpp` (overlay + `dismissible`), no por lógica propia — de ahí `PARCIAL`.

Hallazgo real de bug durante esta subfase (no simulado, en `EventDispatcher::PollInput`): la detección de `Tab` para `CycleFocus` estaba anidada dentro de `if (focusedComponent_ != 0)`, así que un usuario que navega solo con teclado, sin haber hecho click antes con el mouse, no podía llegar a ningún control — el primer Tab no hacía nada porque nada estaba enfocado todavía. Reestructuré `PollInput` para que la detección de teclas nuevas (y el disparo de `Tab`/`CycleFocus`) corra siempre; solo el despacho de los eventos `KeyDown`/`KeyUp` en sí (que necesitan un target) sigue condicionado a que haya un componente enfocado. Verificado con `ControlActivationTests.cpp`, que enfoca exclusivamente vía `Tab` (sin click de mouse) antes de probar Espacio/Enter.

### Theme

`RenderTheme.cpp::ResolveForViewport` tiene rama explícita para `button`, `text`/`body`, `link`, `combobox`, `checkbox`, `dialog`, `radiobutton`, `scrollview`/`scroll`/`container`/`flex`, e imagen vía el resuelto genérico. Los 9 controles resuelven estilo.

### Accessibility

`AccessibilityMappings.cpp` mapea `AccessibilityRole` para los 9 tipos (`Button`, `Text`, `Image`, `TextInput`, `CheckBox`, `RadioButton`, `ComboBox`, `Dialog`, `ScrollView`) en las cuatro funciones de mapeo (rol de plataforma, traits, nombre de vista Android, string de rol).

### Tests

Cobertura real en `tests/*.cpp` (`RenderCommandTests`, `HitTestTests`, `EventTests`, `SnapshotTests`):

- `Text`: `RenderCommand.TextEmitsDrawText`, `HitTest.ClickOnLabelHitsTheLabel`.
- `Image`: sin test dedicado en ningún archivo. `FALTA`.
- `Button`: `RenderCommand.ButtonEmitsDrawButton`, `HitTest.ClickOnButtonHitsTheButton`, `ControlActivation.SpaceOnFocusedButtonFiresClick`, `ControlActivation.EnterOnFocusedButtonFiresClick`.
- `TextBox`: `RenderCommand.TextBoxEmitsDrawInput`, `Event.TextInputFiresOnFocusedTextBox`.
- `CheckBox`: `RenderCommand.CheckBoxEmitsDrawCheckBox`, `HitTest.ClickOnCheckBoxHitsTheCheckBox`, `ControlActivation.SpaceOnFocusedCheckBoxTogglesIt`.
- `RadioButton`: `RenderCommand.RadioButtonEmitsDrawRadioButton`, `HitTest.ClickOnRadioButtonHitsTheRadioButton`, `ControlActivation.EnterOnFocusedRadioButtonSelectsIt`.
- `ComboBox`: solo `RenderCommand.ComboBoxEmitsDrawComboBox`; sin test de HitTest/Event/Snapshot. `PARCIAL`.
- `ScrollView`: sin test dedicado. `FALTA`.
- `Dialog`: sin test dedicado. `FALTA`.

## Bloqueadores para marcar un control como completo

Según la regla de la Fase 22 ("no marcar un control como completo si falta una columna crítica"), ningún control de los 9 puede considerarse completo hoy:

1. Linux y macOS no tienen renderer real — bloquea a los 9 controles por igual.
2. Image, ScrollView y Dialog no tienen test dedicado.

Cerrado en esta iteración (ver `runtime/avaui/src/renderer/HTMLRenderer.cpp/.h`, `runtime/avaui/src/platform/android/AndroidCanvasRenderer.cpp/.h` y `AndroidJNI.cpp/.h`, más `tests/HTMLRendererTests.cpp`):

- TextBox, CheckBox, RadioButton y ComboBox ya se pintan en Web (antes caían en el default vacío de `BaseRenderer`).
- TextBox y ComboBox ya se pintan en Android (bridges JNI `Bridge_CanvasDrawInput` y `Bridge_CanvasDrawComboBox` agregados).
- Se agregó cobertura de test dedicada para `HTMLRenderer` sobre estos cuatro controles.

Cerrado en esta iteración (ver `runtime/avaui/src/controls/ButtonController.cpp/.h` (nuevo), `CheckBoxController.cpp`, `RadioButtonController.cpp`, `ActivationKey.h` (nuevo), `EventDispatcher.cpp`, más `tests/ControlActivationTests.cpp`, wireado en `runtime/avahost/src/native/native_app_host.cpp`):

- Button, CheckBox y RadioButton responden a Espacio/Enter estando enfocados, igual que a un click de mouse.
- Se corrigió un bug real en `EventDispatcher::PollInput`: `Tab` no movía el foco si todavía no había ningún componente enfocado, dejando a un usuario de solo-teclado sin forma de llegar a un control. Ahora `CycleFocus` corre para `Tab` sin depender de que ya haya foco previo.
- Se agregó cobertura de test dedicada (`ControlActivationTests.cpp`, 4 tests) que enfoca exclusivamente vía `Tab` y activa vía Espacio/Enter, sin click de mouse.
