#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Fwd.h"
#include "components/ComponentTree.h"
#include "layout/LayoutTypes.h"
#include "layout/ILayoutNode.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"

namespace studio::design {

struct LiveRenderResult {
    std::unique_ptr<avalang::ui::LayoutEngine> layoutEngine;
    std::unique_ptr<avalang::ui::render::IRenderTree> renderTree;
    std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph;

    std::unordered_map<std::string, avalang::ui::LayoutRect> nodeIdToRect;

    std::unique_ptr<avalang::ui::ComponentTree> layoutTree;
    avalang::ui::LayoutRect slotRect;
    bool hasSlot = false;

    bool ok = false;
    std::string error;
};

// Evaluates a raw property expression (`"status: " + count`, `agreed`, ...)
// against the design-time state. Used by the layout engine (text
// measurement) and by the render tree (the text that actually gets painted).
// Without it both fall back to the raw source text, which is what used to show
// up on the canvas: the literal `"status: " + count` instead of `status: 0`.
using TextEvaluator = std::function<std::string(const std::string&)>;

// `evalText` only has to stay valid for the duration of the call: the
// evaluators are detached from the layout engine / render tree before
// returning, so nothing in the result keeps a pointer into the caller's VM.
LiveRenderResult BuildLiveRender(avalang::ui::ComponentTree* tree, int viewportWidth, int viewportHeight,
                                  const std::string& extends = "",
                                  const std::string& projectRoot = "",
                                  TextEvaluator evalText = nullptr);

}
