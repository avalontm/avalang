#ifndef AVA_UI_RENDER_IRENDER_TREE_H
#define AVA_UI_RENDER_IRENDER_TREE_H

#include "render_tree/IRenderNode.h"
#include "Export.h"
#include <memory>
#include <functional>
#include <string>

namespace avalang {
namespace ui {

class IComponent;
class LayoutEngine;

namespace render {

// Converts Component Tree + Layout Tree into a Render Tree.
// Each Component becomes zero or more RenderNodes (decomposition).
// E.g., Button component -> Rectangle (bg) + Rectangle (border) + Text (label) + Image (icon).
class AVA_UI_API IRenderTree {
public:
    virtual ~IRenderTree() = default;

    // Factory
    static IRenderTree* Create();

    // Build render tree from component tree + layout.
    // Clears previous tree; recreates from scratch.
    virtual void Build(IComponent* componentRoot, LayoutEngine* layoutEngine) = 0;

    // Access render tree
    virtual std::shared_ptr<IRenderNode> Root() const = 0;
    virtual std::shared_ptr<IRenderNode> FindNode(ComponentId componentId) const = 0;

    // Rebuild on component/layout changes (future: incremental updates)
    virtual void Invalidate() = 0;
    virtual bool IsDirty() const = 0;

    // Iterate all nodes (depth-first)
    virtual void ForEach(std::function<void(const std::shared_ptr<IRenderNode>&)> visitor) = 0;

    // Gap D / Fase C: optional property-evaluator. If set, the next
    // Build() passes every property's raw text through this fn before
    // using it (mirrors html_renderer.h::RenderOptions::evalText).
    // Default = identity (no evaluation). Setter is additive; existing
    // callers that never call it see identical behavior.
    virtual void SetEvalText(std::function<std::string(const std::string&)> evalText) = 0;
};

} // namespace render
} // namespace ui
} // namespace avalang

#endif // AVA_UI_RENDER_IRENDER_TREE_H
