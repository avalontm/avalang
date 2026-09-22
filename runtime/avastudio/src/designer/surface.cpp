#include "designer/surface.h"

namespace studio::designer {

NodeId DesignerSurface::Pick(UiComponentTree* tree, const LayoutPoint& screenPoint) const {
    return PickCanvas(tree, viewport_.ToCanvas(screenPoint));
}

NodeId DesignerSurface::PickCanvas(UiComponentTree* tree, const LayoutPoint& canvasPoint) const {
    return HitTest(tree, layout_, canvasPoint);
}

std::vector<OverlayItem> DesignerSurface::Overlay() const {
    return Overlay(OverlayRectResolver());
}

std::vector<OverlayItem> DesignerSurface::Overlay(const OverlayRectResolver& resolver) const {
    if (mode_ == CanvasMode::Preview) {
        return {};
    }
    return BuildOverlay(selection_, layout_, resolver);
}

}
