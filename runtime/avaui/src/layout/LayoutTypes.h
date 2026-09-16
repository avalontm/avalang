#pragma once

namespace avalang {
namespace ui {

struct LayoutRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct LayoutPoint {
    double x = 0.0;
    double y = 0.0;
};

struct LayoutSize {
    double width = 0.0;
    double height = 0.0;
};

struct LayoutConstraints {
    double minWidth = 0.0;
    double maxWidth = 0.0;
    double minHeight = 0.0;
    double maxHeight = 0.0;

    double ClampWidth(double width) const {
        return width < minWidth ? minWidth : (width > maxWidth ? maxWidth : width);
    }

    double ClampHeight(double height) const {
        return height < minHeight ? minHeight : (height > maxHeight ? maxHeight : height);
    }
};

enum class LayoutAlignment {
    Start,
    Center,
    End,
    Stretch,
};

}
}