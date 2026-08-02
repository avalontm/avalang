#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang::ui::controls {

/**
 * Fase 18 -- Text Control
 *
 * Simplest control in the base set: a single label. RenderTheme (Fase 16)
 * already has a role mapping for TypeName() == "Text" (font "body",
 * theme.Color("text")); RenderTree (Fase 6/13) already decomposes it to
 * a single Text render node (DecomposeText). Nothing new needed on those
 * two fronts -- this control only adds the create-time convenience the
 * rest of controls/ has (a typed factory instead of hand-rolling
 * tree->CreateComponent("Text") + SetProperty("text", ...) at every
 * call site).
 *
 * Properties:
 *   * text: string (the displayed text)
 *
 * Theme-filled (RenderTheme::Apply, before Layout):
 *   * fontName / fontSize: theme.Font("body")
 *   * textColor: theme.Color("text")
 */
AVA_UI_API IComponent* CreateText(ComponentTree* tree, const std::string& text);

/** Update the text of an existing Text component in place. */
AVA_UI_API void SetTextValue(IComponent* textComponent, const std::string& text);

} // namespace avalang::ui::controls
