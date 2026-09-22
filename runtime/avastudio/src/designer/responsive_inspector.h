#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "theme/ProjectStyleOverrides.h"

namespace studio::designer {

struct ResponsiveStyleRow {
    std::string key;
    std::string value;
    std::optional<uint32_t> breakpointMinWidth;
};

bool IsPureLayoutTypeName(const std::string& typeLower);

std::optional<uint32_t> HighestActiveBreakpoint(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                 double viewportWidth);

std::vector<ResponsiveStyleRow> ResolveResponsiveRows(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                       const std::string& typeLower, double viewportWidth);

}
