#pragma once

#include "components/IComponent.h"
#include "Export.h"

namespace avalang::ui::controls {

/**
 * Fase 18 -- Container Controls: Column, Row, Stack
 *
 * These three were already first-class TypeName()s as far as
 * LayoutEngine (Fase 3: ArrangeRowOrColumn / ArrangeStack) and
 * RenderTree (Fase 6/13: DecomposeContainer) are concerned -- every
 * previous demo (ButtonDemo, AvauiPipelineDemo) already created them
 * directly via tree->CreateComponent("Column"/"Row"/"Stack"). What was
 * missing was the typed factory the rest of controls/ has, so callers
 * don't hand-roll CreateComponent + SetProperty at every call site and
 * so RenderTheme's container role ("surface" background, see
 * RenderTheme.cpp's `type == "container" || "row" || "column" ||
 * "stack"` branch) has a single, documented entry point.
 *
 * Properties common to all three (optional, read by LayoutEngine):
 *   * spacing: number (gap between children, main axis, Row/Column only)
 *   * padding: number or edge-insets string (see LayoutProperties.h)
 *
 * Theme-filled (RenderTheme::Apply):
 *   * backgroundColor: theme.Color("surface") (only if not set explicitly)
 *
 * Children are added the usual IComponent way after creation:
 *   auto row = CreateRow(tree.get());
 *   row->AddChild(CreateText(tree.get(), "Hello"));
 */
AVA_UI_API IComponent* CreateColumn(ComponentTree* tree);
AVA_UI_API IComponent* CreateRow(ComponentTree* tree);
AVA_UI_API IComponent* CreateStack(ComponentTree* tree);

/** Sets the main-axis gap between children (Row/Column only; ignored by Stack). */
AVA_UI_API void SetContainerSpacing(IComponent* container, double spacingPx);

} // namespace avalang::ui::controls
