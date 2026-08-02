#include "layout/TextMeasure.h"

namespace avalang {
namespace ui {
namespace layout {

double EstimateTextWidth(const std::string& text, double fontSize, const std::string& /*fontName*/) {
    if (text.empty() || fontSize <= 0.0) {
        return 0.0;
    }
    // See TextMeasure.h for why this is an average-width heuristic
    // and not a real glyph measurement.
    return static_cast<double>(text.size()) * fontSize * kAverageCharWidthFactor;
}

double DefaultLineHeight(double fontSize) {
    if (fontSize <= 0.0) {
        return 0.0;
    }
    return fontSize * kLineHeightFactor;
}

} // namespace layout
} // namespace ui
} // namespace avalang
