#include "designer/responsive_inspector.h"

#include <algorithm>
#include <sstream>

#include "designer/responsive.h"

namespace studio::designer {

namespace {

using avalang::ui::theme::BreakpointOverride;
using avalang::ui::theme::ControlStyleOverride;

struct StyleField {
    const char* name;
    std::optional<std::string> (*read)(const ControlStyleOverride&);
};

std::string FormatNumber(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

std::optional<std::string> ReadNumber(const std::optional<double>& value) {
    if (!value) return std::nullopt;
    return FormatNumber(*value);
}

const StyleField kStyleFields[] = {
    {"backgroundColor", [](const ControlStyleOverride& s) { return s.backgroundColor; }},
    {"textColor", [](const ControlStyleOverride& s) { return s.textColor; }},
    {"borderColor", [](const ControlStyleOverride& s) { return s.borderColor; }},
    {"fontName", [](const ControlStyleOverride& s) { return s.fontName; }},
    {"fontSize", [](const ControlStyleOverride& s) { return ReadNumber(s.fontSize); }},
    {"borderWidth", [](const ControlStyleOverride& s) { return ReadNumber(s.borderWidth); }},
    {"borderRadius", [](const ControlStyleOverride& s) { return ReadNumber(s.borderRadius); }},
    {"padding", [](const ControlStyleOverride& s) { return ReadNumber(s.padding); }},
    {"margin", [](const ControlStyleOverride& s) { return ReadNumber(s.margin); }},
    {"spacing", [](const ControlStyleOverride& s) { return ReadNumber(s.spacing); }},
};

bool IsActive(const BreakpointOverride& breakpoint, double viewportWidth) {
    return static_cast<double>(breakpoint.minWidthPx) <= viewportWidth;
}

ControlStyleOverride BreakpointContribution(const BreakpointOverride& breakpoint, const std::string& typeLower) {
    ControlStyleOverride contribution;
    if (breakpoint.hasGlobal) {
        breakpoint.global.MergeOnto(contribution);
    }
    const auto it = breakpoint.perType.find(typeLower);
    if (it != breakpoint.perType.end()) {
        it->second.MergeOnto(contribution);
    }
    return contribution;
}

}

bool IsPureLayoutTypeName(const std::string& typeLower) {
    return typeLower == "row" || typeLower == "column" || typeLower == "stack" || typeLower == "hstack" ||
           typeLower == "vstack" || typeLower == "flex";
}

std::optional<uint32_t> HighestActiveBreakpoint(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                 double viewportWidth) {
    std::optional<uint32_t> highest;
    for (const BreakpointOverride& breakpoint : styles.Breakpoints()) {
        if (!IsActive(breakpoint, viewportWidth)) continue;
        if (!highest || breakpoint.minWidthPx > *highest) highest = breakpoint.minWidthPx;
    }
    return highest;
}

std::vector<ResponsiveStyleRow> ResolveResponsiveRows(const avalang::ui::theme::ProjectStyleSheet& styles,
                                                       const std::string& typeLower, double viewportWidth) {
    const ControlStyleOverride resolved =
        ResolveResponsiveStyle(styles, typeLower, viewportWidth, IsPureLayoutTypeName(typeLower));

    std::vector<std::optional<uint32_t>> sources(std::size(kStyleFields));
    for (const BreakpointOverride& breakpoint : styles.Breakpoints()) {
        if (!IsActive(breakpoint, viewportWidth)) continue;
        const ControlStyleOverride contribution = BreakpointContribution(breakpoint, typeLower);
        for (size_t i = 0; i < std::size(kStyleFields); ++i) {
            if (kStyleFields[i].read(contribution)) sources[i] = breakpoint.minWidthPx;
        }
    }

    std::vector<ResponsiveStyleRow> rows;
    for (size_t i = 0; i < std::size(kStyleFields); ++i) {
        const std::optional<std::string> value = kStyleFields[i].read(resolved);
        if (!value) continue;
        rows.push_back(ResponsiveStyleRow{kStyleFields[i].name, *value, sources[i]});
    }
    return rows;
}

}
