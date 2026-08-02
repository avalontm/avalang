#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <functional>

namespace avalang::ui::controls {

/**
 * Fase 18 -- CheckBox Control
 *
 * Properties:
 *   * label: string (text shown next to the box; may be empty)
 *   * isChecked: bool (default false)
 *   * isEnabled: bool (default true)
 *
 * Theme-filled (RenderTheme::Apply):
 *   * borderColor: theme.Color("border")
 *   * borderWidth: theme.Spacing().borderWidthPx
 *
 * Rendering note: the render pipeline (Fase 8/9, frozen since Fase 13)
 * only has DrawRectangle/DrawText/DrawImage primitives -- no ellipse,
 * no arbitrary path fill (IRenderer::DrawPath is a documented no-op).
 * DecomposeCheckBox (RenderTree, this phase) therefore draws the box
 * as a small square that fills solid when isChecked is true, same
 * visual language Button already uses for its own background
 * rectangle. See docs/AVAUI_FASE18_CONTROLS.md for why this isn't
 * fixed here (it would mean breaking RenderCommand/IRenderer's frozen
 * signatures, which is out of scope for a controls/ phase).
 */
using CheckBoxChangeCallback = std::function<void(ComponentId checkBoxId, bool isChecked)>;

AVA_UI_API IComponent* CreateCheckBox(ComponentTree* tree, const std::string& label, bool isChecked = false);

/** Sets checked state explicitly (invokes the bound change callback, if any). */
AVA_UI_API void SetCheckBoxChecked(IComponent* checkBoxComponent, bool isChecked);

/** Flips the current checked state; returns the new state. Convenience for a click handler. */
AVA_UI_API bool ToggleCheckBox(IComponent* checkBoxComponent);

AVA_UI_API bool GetCheckBoxChecked(IComponent* checkBoxComponent);

AVA_UI_API void BindCheckBoxChange(ComponentId checkBoxId, CheckBoxChangeCallback callback);
AVA_UI_API void UnbindCheckBoxChange(ComponentId checkBoxId);

} // namespace avalang::ui::controls
