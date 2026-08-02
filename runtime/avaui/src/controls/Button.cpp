#include "controls/Button.h"
#include "components/ComponentTree.h"
#include "components/PropertyValue.h"
#include <unordered_map>
#include <mutex>

namespace avalang::ui::controls {

// Global registry of button click callbacks, protected by mutex
namespace {

std::unordered_map<ComponentId, ButtonClickCallback> g_buttonCallbacks;
std::mutex g_callbackMutex;

}  // anonymous namespace

/**
 * Create a Button and set up its properties.
 * 
 * Button properties (set before layout):
 *   - "text": the label displayed on the button (PropertyValue::String)
 *   - "isEnabled": bool (PropertyValue::Bool), default true
 *   - "style": "primary" (default) or "secondary" (PropertyValue::String)
 *   - "type": "Button" (set automatically)
 * 
 * Theme integration happens in RenderTheme::Apply() (Phase 16),
 * which walks the tree and fills empty properties with theme defaults
 * based on component type.
 * 
 * Button doesn't call RenderTheme here -- that's the caller's
 * responsibility via the full pipeline (Parser -> Layout -> RenderTheme -> RenderTree).
 */
IComponent* CreateButton(ComponentTree* tree, const std::string& text) {
    if (!tree) {
        // Standalone button (no tree context)
        // For now, just return nullptr -- Button must be part of a tree
        // to participate in the full pipeline. A standalone button is not useful.
        return nullptr;
    }

    // Create base component via ComponentTree::CreateComponent
    // (which assigns a unique ID and owns the component)
    IComponent* button = tree->CreateComponent("Button");
    if (!button) {
        return nullptr;
    }

    // Set properties that Button needs before layout/rendering
    button->SetProperty("text", PropertyValue(text));
    button->SetProperty("isEnabled", PropertyValue(true));
    button->SetProperty("style", PropertyValue(std::string("primary")));

    // Properties filled by RenderTheme::Apply() later:
    //   - background: theme.Color("buttonPrimary") [for primary style]
    //   - color (text): theme.Color("text")
    //   - font: theme.Font("button")
    //   - padding, borderRadius: theme.Spacing()
    //   - opacity: 0.5 if not isEnabled

    return button;
}

/**
 * Bind a click callback to a Button.
 * 
 * The callback is invoked by:
 *   1. IEventDispatcher detects a Click event targeting this button
 *   2. EventDispatcher::Dispatch() -> button's event handler
 *   3. Button's handler looks up callback in g_buttonCallbacks
 *   4. Callback invoked if present
 * 
 * Currently, integration with IEventDispatcher is handled externally
 * (e.g., in AvauiPipelineDemo or in a platform-specific event loop).
 * Button just stores the callback; the dispatcher retrieves it as needed.
 * 
 * Thread-safe via g_callbackMutex.
 */
void BindButtonClick(ComponentId buttonId, ButtonClickCallback callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_buttonCallbacks[buttonId] = callback;
}

void UnbindButtonClick(ComponentId buttonId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_buttonCallbacks.erase(buttonId);
}

/**
 * Global accessor for button callbacks (internal use by EventDispatcher).
 * Called by Phase 5 (IEventDispatcher) when a Click event is dispatched.
 * Returns nullptr if no callback is bound.
 */
namespace internal {

ButtonClickCallback* GetButtonClickCallback(ComponentId buttonId) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    auto it = g_buttonCallbacks.find(buttonId);
    return (it != g_buttonCallbacks.end()) ? &it->second : nullptr;
}

}  // namespace internal

}  // namespace avalang::ui::controls
