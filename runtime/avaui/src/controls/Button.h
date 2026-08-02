#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>
#include <memory>
#include <functional>

namespace avalang::ui::controls {

/**
 * Fase 17 — Button Control
 * 
 * First real control, validating Theme (Fase 16) + Resources (Fase 15).
 * 
 * Design:
 * - Extends IComponent for basic tree functionality
 * - Maps to theme roles: buttonPrimary, button (font), text (color)
 * - Type: "Button"
 * - Properties:
 *   * text: string (label shown on button)
 *   * isEnabled: bool (default true; disabled -> opacity/color change)
 *   * style: "primary" | "secondary" (default: "primary")
 * 
 * Theme Integration (before Layout):
 * - Background: theme.Color("buttonPrimary") [for primary]
 * - Text Color: theme.Color("text") or theme.Color("textInverted")
 * - Font: theme.Font("button") [12pt, w600]
 * - Padding: theme.Spacing().paddingPx
 * - Border Radius: theme.Spacing().borderRadiusPx
 * 
 * Layout & Rendering:
 * - Expands to simple container: Rectangle + Text child
 * - Hit-testing: click detection via IEventDispatcher (Phase 5)
 * - Events: Click -> callback (if bound)
 * 
 * ABI Version: 15 (matches UIModule)
 */

/**
 * Callback signature for click events.
 * Called when user clicks the button (after Theme + Layout + RenderTree + Scene processed).
 */
using ButtonClickCallback = std::function<void(ComponentId buttonId)>;

/**
 * Create a Button component and add it to a ComponentTree.
 * 
 * @param tree The ComponentTree to add to (or nullptr to create a standalone button)
 * @param text Label text to display
 * @return Newly created Button component (owned by tree if provided, else heap-allocated)
 * 
 * Usage:
 *   auto tree = CreateComponentTree();
 *   auto button = CreateButton(tree.get(), "Click Me");
 *   button->SetProperty("isEnabled", PropertyValue(true));
 *   
 *   // Bind click callback (before rendering pipeline)
 *   BindButtonClick(button->Id(), [](ComponentId id) {
 *       std::cout << "Button " << id << " clicked\\n";
 *   });
 */
AVA_UI_API IComponent* CreateButton(ComponentTree* tree, const std::string& text);

/**
 * Bind a callback to a Button's click event.
 * 
 * Callback is invoked when Click event reaches this button.
 * Only one callback per button; setting again overwrites.
 * 
 * @param buttonId Component ID of Button (from IComponent::Id())
 * @param callback Function to invoke on click
 */
AVA_UI_API void BindButtonClick(ComponentId buttonId, ButtonClickCallback callback);

/**
 * Unbind click callback from a Button.
 * 
 * @param buttonId Component ID of Button
 */
AVA_UI_API void UnbindButtonClick(ComponentId buttonId);

} // namespace avalang::ui::controls
