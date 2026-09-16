#pragma once

#include <string>
#include <vector>

#include "Export.h"

namespace avalang {
namespace ui {
namespace layout {

AVA_UI_API double EstimateTextWidth(const std::string& text, double fontSize, const std::string& fontName);

AVA_UI_API double DefaultLineHeight(double fontSize, const std::string& fontName = std::string());

AVA_UI_API std::vector<std::string> WrapTextLines(const std::string& text, double fontSize,
                                        const std::string& fontName, double maxWidth);

AVA_UI_API double WrappedLineHeight(double fontSize, const std::string& fontName = std::string());

}
}
}