#include "designer/viewport.h"

#include <algorithm>

namespace studio::designer {

namespace {
constexpr double kMinZoom = 0.05;
constexpr double kMaxZoom = 16.0;
}

LayoutPoint DesignerViewport::ToCanvas(const LayoutPoint& screenPoint) const {
    return LayoutPoint{(screenPoint.x - panX_) / zoom_, (screenPoint.y - panY_) / zoom_};
}

LayoutPoint DesignerViewport::ToScreen(const LayoutPoint& canvasPoint) const {
    return LayoutPoint{canvasPoint.x * zoom_ + panX_, canvasPoint.y * zoom_ + panY_};
}

LayoutRect DesignerViewport::ToScreen(const LayoutRect& canvasRect) const {
    LayoutPoint origin = ToScreen(LayoutPoint{canvasRect.x, canvasRect.y});
    return LayoutRect{origin.x, origin.y, canvasRect.width * zoom_, canvasRect.height * zoom_};
}

void DesignerViewport::Pan(double dx, double dy) {
    panX_ += dx;
    panY_ += dy;
}

void DesignerViewport::SetPan(double x, double y) {
    panX_ = x;
    panY_ = y;
}

void DesignerViewport::SetZoom(double zoom, const LayoutPoint& anchorScreenPoint) {
    double clamped = std::clamp(zoom, kMinZoom, kMaxZoom);
    LayoutPoint anchorCanvas = ToCanvas(anchorScreenPoint);
    zoom_ = clamped;
    panX_ = anchorScreenPoint.x - anchorCanvas.x * zoom_;
    panY_ = anchorScreenPoint.y - anchorCanvas.y * zoom_;
}

void DesignerViewport::Reset() {
    zoom_ = 1.0;
    panX_ = 0.0;
    panY_ = 0.0;
}

void DesignerViewport::FitToScreen(const LayoutSize& contentSize, const LayoutSize& screenSize) {
    if (contentSize.width <= 0.0 || contentSize.height <= 0.0) {
        Reset();
        return;
    }
    double fitZoom = std::min(screenSize.width / contentSize.width, screenSize.height / contentSize.height);
    zoom_ = std::clamp(fitZoom, kMinZoom, kMaxZoom);
    Center(contentSize, screenSize);
}

void DesignerViewport::Center(const LayoutSize& contentSize, const LayoutSize& screenSize) {
    panX_ = (screenSize.width - contentSize.width * zoom_) / 2.0;
    panY_ = (screenSize.height - contentSize.height * zoom_) / 2.0;
}

}
