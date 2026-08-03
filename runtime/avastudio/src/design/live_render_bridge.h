#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Fwd.h"
#include "components/ComponentTree.h"
#include "design/design_document.h"
#include "layout/LayoutEngine.h"
#include "layout/LayoutTypes.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"

namespace studio::design {

struct LiveRenderResult {
    std::unique_ptr<avalang::ui::ComponentTree> componentTree;
    std::unique_ptr<avalang::ui::LayoutEngine> layoutEngine;
    std::unique_ptr<avalang::ui::render::IRenderTree> renderTree;
    std::unique_ptr<avalang::ui::scene::ISceneGraph> sceneGraph;

    std::unordered_map<std::string, avalang::ui::ComponentId> uidToComponentId;
    std::unordered_map<std::string, avalang::ui::LayoutRect> uidToRect;

    bool ok = false;
    std::string error;
};

LiveRenderResult BuildLiveRender(const DesignNode& root, int viewportWidth, int viewportHeight);

} // namespace studio::design
