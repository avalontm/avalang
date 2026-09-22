#pragma once

#include <vector>

#include "designer/hit_test.h"
#include "designer/layout_core.h"
#include "designer/overlay.h"
#include "designer/preview.h"
#include "designer/selection_manager.h"
#include "designer/types.h"
#include "designer/viewport.h"

namespace studio::designer {

enum class DesignerLayer {
    Background,
    Grid,
    Component,
    Selection,
    Guide,
    DragDrop,
    Interaction,
};

class DesignerSurface {
public:
    NodeId Pick(UiComponentTree* tree, const LayoutPoint& screenPoint) const;
    NodeId PickCanvas(UiComponentTree* tree, const LayoutPoint& canvasPoint) const;
    std::vector<OverlayItem> Overlay() const;
    std::vector<OverlayItem> Overlay(const OverlayRectResolver& resolver) const;

    LayoutCore& Layout() { return layout_; }
    const LayoutCore& Layout() const { return layout_; }

    DesignerViewport& Viewport() { return viewport_; }
    const DesignerViewport& Viewport() const { return viewport_; }

    SelectionManager& Selection() { return selection_; }
    const SelectionManager& Selection() const { return selection_; }

    CanvasMode Mode() const { return mode_; }
    void SetMode(CanvasMode mode) { mode_ = mode; }

private:
    LayoutCore layout_;
    DesignerViewport viewport_;
    SelectionManager selection_;
    CanvasMode mode_ = CanvasMode::Design;
};

}
