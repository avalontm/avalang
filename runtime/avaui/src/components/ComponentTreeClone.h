#pragma once

#include <memory>

#include "Export.h"
#include "Fwd.h"
#include "components/ComponentTree.h"

namespace avalang {
namespace ui {

AVA_UI_API std::unique_ptr<ComponentTree> CloneComponentTree(ComponentTree* source);

}
}
