#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang::ui::controls {

/**
 * Fase 18 -- RadioButton Control
 *
 * Unlike CheckBox, selecting one RadioButton must clear every other
 * button in the same `group`. ComponentTree (Fase 2) has no group/
 * registry concept of its own, so this control keeps a small
 * process-wide group registry (same style as Button's click-callback
 * registry) mapping group name -> member ComponentIds, scoped for the
 * lifetime of the process (not per-tree) -- acceptable for now since
 * nothing in the pipeline destroys+recreates trees at runtime yet
 * (Fase 20 will need to revisit this if/when it does).
 *
 * Properties:
 *   * label: string
 *   * group: string (radio group name; buttons in the same group are
 *     mutually exclusive)
 *   * isSelected: bool (default false)
 *   * isEnabled: bool (default true)
 *
 * Theme-filled (RenderTheme::Apply):
 *   * borderColor: theme.Color("border")
 *   * borderWidth: theme.Spacing().borderWidthPx
 *
 * Rendering note: same caveat as CheckBox -- no ellipse primitive in
 * the render pipeline yet, so DecomposeRadioButton draws a small
 * square indicator, not a circle. See docs/AVAUI_FASE18_CONTROLS.md.
 */
using RadioButtonChangeCallback = std::function<void(ComponentId radioButtonId, bool isSelected)>;

AVA_UI_API IComponent* CreateRadioButton(ComponentTree* tree, const std::string& label,
                                          const std::string& group, bool isSelected = false);

/** Selects this radio button and deselects every other member of its group. */
AVA_UI_API void SelectRadioButton(IComponent* radioButtonComponent);

AVA_UI_API bool GetRadioButtonSelected(IComponent* radioButtonComponent);

AVA_UI_API void BindRadioButtonChange(ComponentId radioButtonId, RadioButtonChangeCallback callback);
AVA_UI_API void UnbindRadioButtonChange(ComponentId radioButtonId);

} // namespace avalang::ui::controls
