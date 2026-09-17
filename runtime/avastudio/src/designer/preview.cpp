#include "designer/preview.h"

#include "layout/LayoutEngine.h"
#include "render_tree/IRenderTree.h"
#include "scene/ISceneGraph.h"

namespace studio::designer {

PreviewFrame BuildPreview(UiComponentTree* tree, const LayoutSize& viewport, const std::string& extends,
                           const std::string& projectRoot) {
    return studio::design::BuildLiveRender(tree, static_cast<int>(viewport.width), static_cast<int>(viewport.height),
                                            extends, projectRoot);
}

bool HasRect(const PreviewFrame& frame, const NodeId& id) {
    return frame.nodeIdToRect.find(id) != frame.nodeIdToRect.end();
}

LayoutRect RectOf(const PreviewFrame& frame, const NodeId& id) {
    auto it = frame.nodeIdToRect.find(id);
    return it != frame.nodeIdToRect.end() ? it->second : LayoutRect{};
}

}
