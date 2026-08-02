#include "components/ComponentTree.h"

#include "components/ComponentTreeImpl.h"

namespace avalang {
namespace ui {

std::unique_ptr<ComponentTree> ComponentTree::Create() {
    return std::make_unique<components::ComponentTreeImpl>();
}

} // namespace ui
} // namespace avalang
