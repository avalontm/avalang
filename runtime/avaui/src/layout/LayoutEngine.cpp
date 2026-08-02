#include "layout/LayoutEngine.h"

#include "layout/LayoutEngineImpl.h"

namespace avalang {
namespace ui {

std::unique_ptr<LayoutEngine> LayoutEngine::Create() {
    return std::make_unique<layout::LayoutEngineImpl>();
}

} // namespace ui
} // namespace avalang
