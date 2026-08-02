#include "render_tree/IRenderTree.h"
#include "render_tree/RenderTree.h"

namespace avalang {
namespace ui {
namespace render {

IRenderTree* IRenderTree::Create() {
    return new RenderTree();
}

} // namespace render
} // namespace ui
} // namespace avalang
