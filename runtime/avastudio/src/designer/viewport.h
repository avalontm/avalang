#pragma once

#include "designer/types.h"

namespace studio::designer {

class DesignerViewport {
public:
    double Zoom() const { return zoom_; }
    double PanX() const { return panX_; }
    double PanY() const { return panY_; }

    LayoutPoint ToCanvas(const LayoutPoint& screenPoint) const;
    LayoutPoint ToScreen(const LayoutPoint& canvasPoint) const;
    LayoutRect ToScreen(const LayoutRect& canvasRect) const;

    void Pan(double dx, double dy);
    void SetPan(double x, double y);
    void SetZoom(double zoom, const LayoutPoint& anchorScreenPoint);
    void Reset();
    void FitToScreen(const LayoutSize& contentSize, const LayoutSize& screenSize);
    void Center(const LayoutSize& contentSize, const LayoutSize& screenSize);

private:
    double zoom_ = 1.0;
    double panX_ = 0.0;
    double panY_ = 0.0;
};

}
