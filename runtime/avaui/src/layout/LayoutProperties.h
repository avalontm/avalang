#ifndef AVA_UI_LAYOUT_LAYOUTPROPERTIES_H
#define AVA_UI_LAYOUT_LAYOUTPROPERTIES_H

#include <string>

#include "components/IComponent.h"
#include "LayoutTypes.h"

namespace avalang {
namespace ui {
namespace layout {

// Property names the Layout Engine recognizes on IComponent (Phase
// 2's generic property bag -- Phase 3 is the first consumer that
// gives these names meaning). A missing or wrong-typed property
// behaves exactly as if it were absent and falls back to the
// documented default -- IComponent has no schema/validation of its
// own, so this file *is* the schema for layout-related properties.
//
//   width, height                   (Number) explicit size on that
//                                    axis. Absent means auto: fill the
//                                    space the parent offers if this
//                                    component's alignment on that
//                                    axis is "stretch" (the default);
//                                    otherwise fall back to this
//                                    component's intrinsic/content-
//                                    based size (see
//                                    LayoutEngineImpl::ComputeIntrinsicSize
//                                    and layout/TextMeasure.h) -- a
//                                    Button/Text/Label sizes to its
//                                    label text, a Row/Column sizes to
//                                    the sum/max of its children.
//                                    Falls back to 0 only when no
//                                    intrinsic size can be derived at
//                                    all (e.g. an empty container with
//                                    no text and no children).
//   margin                          (Number) uniform outer spacing,
//                                    default 0.
//   margin-left/top/right/bottom    (Number) per-side override of
//                                    `margin`.
//   padding                         (Number) uniform inner spacing
//                                    applied to this component's own
//                                    children, default 0.
//   padding-left/top/right/bottom   (Number) per-side override of
//                                    `padding`.
//   align                           (String: "start" | "center" |
//                                    "end" | "stretch") cross-axis
//                                    alignment used when the parent is
//                                    a Row or Column (horizontal for a
//                                    Column parent, vertical for a Row
//                                    parent). Default "stretch".
//   align-h, align-v                (String, same values) alignment
//                                    used when the parent is a Stack
//                                    (or any TypeName the engine
//                                    doesn't otherwise recognize,
//                                    which falls back to Stack's
//                                    overlay behavior). Default
//                                    "stretch" each.
//   spacing                         (Number) gap the engine inserts
//                                    between consecutive children.
//                                    Read off the *parent* (Row/Column)
//                                    component, not the child. Default
//                                    0 -- LayoutEngine itself never
//                                    invents a value; the real
//                                    Parser -> RenderTheme -> Layout
//                                    pipeline fills margin/padding/
//                                    spacing in on layout containers
//                                    that don't set them explicitly
//                                    (see RenderTheme.cpp's
//                                    ApplyTypeDefaults and
//                                    ThemeSpacing::containerPaddingPx/
//                                    containerGapPx), so this 0 only
//                                    shows up when the engine is used
//                                    directly, bypassing RenderTheme.

// Defaults used by intrinsic sizing (LayoutEngineImpl::ComputeIntrinsicSize,
// see layout/TextMeasure.h for the text-measurement side of it) when a
// component doesn't carry an explicit width/height. Kept here, next
// to the rest of this file's property defaults, rather than inside
// LayoutEngineImpl.cpp, so every layout-related default lives in one
// place.

// Fallback fontSize used only when a component reaches intrinsic
// sizing with no "fontSize" property at all (e.g. LayoutEngine used
// directly, outside the normal Parser -> RenderTheme -> Layout
// pipeline, where RenderTheme::Apply always fills this in first).
constexpr double kDefaultFontSizePx = 14.0;

// Horizontal/vertical padding a Button's intrinsic size assumes around
// its label text when the author hasn't set an explicit "padding" (or
// explicit width/height) -- gives it a natural hit-target instead of
// an intrinsic size that hugs the label text exactly.
constexpr double kDefaultButtonPaddingX = 16.0;
constexpr double kDefaultButtonPaddingY = 8.0;

// Floor on a Button's intrinsic height, so a very small/short label
// (e.g. "-") still gets a reasonable, clickable height.
constexpr double kDefaultButtonMinHeight = 32.0;

// TextBox/ComboBox: both render as a native form control (<input>/
// <select>) with no label-driven content the way Button/Text do, so
// intrinsic sizing can't derive a size from measured text the same
// way -- these are the same floor/padding idea as Button's, just
// tuned for a form-field height instead of a button's.
constexpr double kDefaultInputPaddingX = 10.0;
constexpr double kDefaultInputMinHeight = 36.0;
constexpr double kDefaultInputMinWidth = 160.0;

// CheckBox/RadioButton: fixed-size box (see RenderTree::DecomposeCheckBox/
// DecomposeRadioButton, which caps the same box at 16px) plus the gap
// before the label text those functions also use.
constexpr double kDefaultCheckboxBoxSize = 16.0;
constexpr double kDefaultCheckboxLabelGap = 6.0;

// Icon/Image: both are leaf nodes with no text to measure and no
// children, so without an explicit width/height they'd otherwise
// intrinsic-size to 0x0. Icon defaults small (inline-glyph-sized);
// Image defaults larger, closer to a typical thumbnail -- either is
// just a sane floor, meant to be overridden by an explicit
// width/height on components that need a specific size.
constexpr double kDefaultIconSize = 24.0;
constexpr double kDefaultImageSize = 120.0;

// Resolved outer/inner spacing for one component: either from the
// per-side property (e.g. "margin-left") or, if absent, the uniform
// property (e.g. "margin").
struct EdgeInsets {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

// Writes `component`'s Number property `name` into `*out` and returns
// true, or leaves `*out` untouched and returns false if the property
// is absent or not a Number.
bool TryReadNumber(const IComponent* component, const std::string& name, double* out);

// Same as TryReadNumber, but returns `defaultValue` instead of a bool
// when the property is absent or not a Number.
double ReadNumber(const IComponent* component, const std::string& name, double defaultValue);

// Reads `baseName` (uniform) and `baseName`-left/top/right/bottom
// (per-side overrides) into an EdgeInsets. Used for both "margin" and
// "padding".
EdgeInsets ReadEdgeInsets(const IComponent* component, const std::string& baseName);

// Parses `component`'s String property `name` ("start"/"center"/
// "end"/"stretch") into a LayoutAlignment. Anything absent, the wrong
// type, or an unrecognized string value resolves to
// LayoutAlignment::Stretch.
LayoutAlignment ReadAlignment(const IComponent* component, const std::string& name);

} // namespace layout
} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_LAYOUTPROPERTIES_H
