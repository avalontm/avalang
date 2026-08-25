#pragma once

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

LiveRenderResult BuildLiveRender(avalang::ui::ComponentTree* tree, int viewportWidth, int viewportHeight,
                                  const std::string& extends = "",
                                  const std::string& projectRoot = "");

}
