#include "layout/TextMeasure.h"

#include "layout/FontRegistry.h"

namespace avalang {
namespace ui {
namespace layout {

double EstimateTextWidth(const std::string& text, double fontSize, const std::string& fontName) {
    if (text.empty() || fontSize <= 0.0) {
        return 0.0;
    }
    return FontRegistry::Instance().MeasureTextWidth(text, fontSize, fontName);
}

double DefaultLineHeight(double fontSize, const std::string& fontName) {
    if (fontSize <= 0.0) {
        return 0.0;
    }
    return FontRegistry::Instance().LineHeight(fontSize, fontName);
}

} // namespace layout
} // namespace ui
} // namespace avalang
