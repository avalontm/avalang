#pragma once

#include "components/ComponentTree.h"
#include "ITheme.h"
#include "Export.h"
#include <memory>

namespace avalang::ui {

/**
 * Helper to apply theme defaults to a ComponentTree.
 * 
 * Usage:
 *   auto theme = CreateDefaultThemeProvider()->Current();
 *   ComponentTree* tree = parser.Parse(avaui_file);
 *   RenderTheme::Apply(tree, theme);
 *   // Now tree's components have filled-in properties from theme
 *   layoutEngine.Layout(tree);  // ... continues with pipeline
 * 
 * Design:
 * - Walks all components in the tree
 * - For each component:
 *   - Checks component type (Button, Text, etc.)
 *   - Applies theme roles for that type (e.g., Button -> buttonPrimary + button font)
 *   - Only fills EMPTY properties (component-specific values are never overwritten)
 * 
 * This ensures theme acts as a "default stylesheet", not a "forced override".
 */
class AVA_UI_API RenderTheme {
public:
    /**
     * Apply theme defaults to all components in tree.
     * 
     * @param tree ComponentTree to modify (in-place)
     * @param theme Theme providing defaults
     * @return true if application succeeded
     */
    static bool Apply(ComponentTree* tree, ITheme* theme);

    /**
     * Apply theme to a single component node.
     * 
     * @param component Component to theme
     * @param theme Theme providing defaults
     * @return true if application succeeded
     */
    static bool ApplyToComponent(IComponent* component, ITheme* theme);
};

} // namespace avalang::ui
