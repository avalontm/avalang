#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang::ui::controls {

/**
 * Fase 18 -- TextBox Control
 *
 * First control in the base set with real user input (not just
 * output), per the plan's own note: "TextBox es el mas nuevo (input de
 * usuario, no solo output)". Fase 18 wires up the data model
 * (value/placeholder/focus properties + change callback registry,
 * same global-registry pattern Button already validated in Fase 17
 * for click) and the visual decomposition (RenderTree). It does NOT
 * wire real keyboard input from IEventDispatcher/WinKeyboard -- that
 * needs a text-editing cursor/selection model IEventDispatcher (Fase
 * 5) doesn't have yet (it only has hit-testing + a Click event, per
 * ButtonInternal's callback pattern). Driving TextBox from real
 * keystrokes is left for Fase 20 (AvaHost/Studio integration), where
 * there's an actual window and keyboard loop to test against; see
 * docs/AVAUI_FASE18_CONTROLS.md.
 *
 * Properties:
 *   * text: string (current value, empty by default)
 *   * placeholder: string (shown when text is empty; not yet rendered
 *     differently from `text` -- RenderTree just picks whichever of
 *     the two is non-empty, see DecomposeTextBox)
 *   * isFocused: bool (default false; no visual change from it yet --
 *     reserved for when Fase 20 wires real keyboard focus)
 *   * isEnabled: bool (default true)
 *
 * Theme-filled (RenderTheme::Apply):
 *   * backgroundColor: theme.Color("inputBackground")
 *   * borderColor: theme.Color("inputBorder")
 *   * borderWidth: theme.Spacing().borderWidthPx
 *   * fontSize: theme.Font("body").sizePoints
 */
using TextBoxChangeCallback = std::function<void(ComponentId textBoxId, const std::string& newValue)>;

AVA_UI_API IComponent* CreateTextBox(ComponentTree* tree, const std::string& placeholder = "");

/** Programmatically set the value (also invokes the bound change callback, if any). */
AVA_UI_API void SetTextBoxValue(IComponent* textBoxComponent, const std::string& value);

/** Read the current value (empty string if not set or component is null). */
AVA_UI_API std::string GetTextBoxValue(IComponent* textBoxComponent);

AVA_UI_API void BindTextBoxChange(ComponentId textBoxId, TextBoxChangeCallback callback);
AVA_UI_API void UnbindTextBoxChange(ComponentId textBoxId);

} // namespace avalang::ui::controls
