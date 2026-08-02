# ui/src/controls

## Fase 17 — Button Control ✅

**Status:** CLOSED

**Files:**
- `Button.cpp` — Button component creation and click callback registry
- `ButtonInternal.h` — Internal API for EventDispatcher integration

**Public API:**
- `#include <avalang/ui/controls/Button.h>`
- `CreateButton(tree, text)` — Create and add Button to ComponentTree
- `BindButtonClick(id, callback)` — Bind click event handler
- `UnbindButtonClick(id)` — Unbind click event handler

**Test:** `ui/tests/ButtonDemo.cpp` (executable: ava_button_demo)

**Theme Integration:** Button is fully styled by Theme (Fase 16):
- backgroundColor from "buttonPrimary"
- textColor from "text"  
- fontName/fontSize from "button" font role
- padding/borderRadius from spacing

**Event Integration:** Click events dispatched via Phase 5 (IEventDispatcher)

## Fase 18+ — Additional Controls

Reserved for: Text, TextBox, Column/Row/Stack, Image, CheckBox, RadioButton.

See docs/AVAUI_PLAN_FASE12_PLUS.md for schedule.

## ComboBox

Cross-platform control name (not tied to the HTML `<select>` tag — same
component works whichever backend ends up drawing it).

**Files:**
- `ComboBox.h` / `ComboBox.cpp` — ComboBox component (`selectedValue`/`isEnabled` properties, Option children via `AddOption`, change callback registry)

**Public API:**
- `CreateComboBox(tree)`
- `AddOption(tree, comboBox, value, label)`
- `SetSelectedValue(comboBox, value)` / `GetSelectedValue(comboBox)` / `GetSelectedLabel(comboBox)`
- `BindComboBoxChange(id, callback)` / `UnbindComboBoxChange(id)`

**Rendering per backend:**
- `HTMLRenderer`: RenderTree serializes Option children into `RenderNode::OptionsData()`; `SceneCommandWalker` emits a native `<select>`/`<option>` through `DrawHtmlFragment` (same escape hatch `Slot` already uses for backend-specific markup).
- `GdiRenderer`: `OnDrawHtmlFragment` is already a no-op there, same as every other control GDI doesn't implement yet — a real Win32 dropdown (`ComboBoxEx`) is future work, out of scope for this phase.
