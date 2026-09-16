#pragma once

#include <string>

#include "components/IComponent.h"
#include "LayoutTypes.h"

namespace avalang {
namespace ui {
namespace layout {

constexpr double kDefaultFontSizePx = 14.0;
constexpr double kDefaultButtonPaddingX = 16.0;
constexpr double kDefaultButtonPaddingY = 8.0;
constexpr double kDefaultButtonMinHeight = 32.0;
constexpr double kDefaultInputPaddingX = 10.0;
constexpr double kDefaultInputMinHeight = 36.0;
constexpr double kDefaultInputMinWidth = 160.0;
constexpr double kDefaultCheckboxBoxSize = 16.0;
constexpr double kDefaultCheckboxLabelGap = 6.0;
constexpr double kDefaultIconSize = 24.0;
constexpr double kDefaultImageSize = 120.0;

struct EdgeInsets {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

bool TryReadNumber(const IComponent* component, const std::string& name, double* out);

double ReadNumber(const IComponent* component, const std::string& name, double defaultValue);

EdgeInsets ReadEdgeInsets(const IComponent* component, const std::string& baseName);

LayoutAlignment ReadAlignment(const IComponent* component, const std::string& name);

}
}
}