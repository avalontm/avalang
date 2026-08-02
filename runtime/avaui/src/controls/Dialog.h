#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang::ui::controls {

/**
 * Dialog Control (modal)
 *
 * Design:
 * - Extends IComponent for basic tree functionality
 * - Type: "Dialog"
 * - Properties:
 *   * title: string
 *   * isOpen: bool (default false; closed dialogs decompose no children
 *     and paint nothing -- see RenderTree::DecomposeDialog)
 *   * dismissible: bool (default true; click-outside-to-close wiring is
 *     part of the interactivity phase, not this control)
 *   * overlay / backdrop: bool, defaulted by RenderTheme::Apply() to
 *     true/true for type "dialog" -- these are the same generic
 *     properties any component can set (see RenderTree.cpp), Dialog is
 *     just the first consumer.
 *
 * Closing is not a dedicated callback: the standard pattern is a
 * "Cerrar" button inside the dialog's children with its own `click`
 * handler that sets isOpen = false in application state, same as any
 * other button.
 */

AVA_UI_API IComponent* CreateDialog(ComponentTree* tree, const std::string& title);

AVA_UI_API void SetDialogOpen(IComponent* dialog, bool isOpen);

} // namespace avalang::ui::controls
