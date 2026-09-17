#include "designer/surface.h"

namespace studio::designer {

NodeId DesignerSurface::Pick(UiComponentTree* tree, const LayoutPoint& screenPoint) const {
    LayoutPoint canvasPoint = viewport_.ToCanvas(screenPoint);
    return HitTest(tree, layout_, canvasPoint);
}

std::vector<OverlayItem> DesignerSurface::Overlay() const {
    if (mode_ == CanvasMode::Preview) {
        return {};
    }
    return BuildOverlay(selection_, layout_);
}

}
