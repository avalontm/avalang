#include "scene/ISceneGraph.h"
#include "scene/SceneGraph.h"

namespace avalang {
namespace ui {
namespace scene {

ISceneGraph* ISceneGraph::Create() {
    return new SceneGraph();
}

} // namespace scene
} // namespace ui
} // namespace avalang
