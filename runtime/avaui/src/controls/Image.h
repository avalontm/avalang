#pragma once

#include "components/IComponent.h"
#include "Export.h"
#include <string>

namespace avalang::ui::controls {

/**
 * Fase 18 -- Image Control
 *
 * Properties:
 *   * source: string -- logical path resolved later by IResourceProvider
 *     (Fase 15: "@icons/...", "@local/...", or a plain filesystem path).
 *     Image itself does not resolve it; RenderTree's DecomposeImage
 *     (Fase 6/13) copies it verbatim onto the render node's ImagePath(),
 *     and the active IRenderer decides how to load/draw it (Fase 15's
 *     documented decision: loading lives in the renderer, not Resources).
 *   * alt: string (accessibility text; not yet consumed by any
 *     renderer -- reserved so authors don't have to add it later)
 *
 * Theme-filled (RenderTheme::Apply):
 *   * borderRadius: theme.Spacing().borderRadiusPx
 */
AVA_UI_API IComponent* CreateImage(ComponentTree* tree, const std::string& src);

/** Update the image source of an existing Image component in place. */
AVA_UI_API void SetImageSource(IComponent* imageComponent, const std::string& src);

} // namespace avalang::ui::controls
