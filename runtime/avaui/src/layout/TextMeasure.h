#ifndef AVA_UI_LAYOUT_TEXTMEASURE_H
#define AVA_UI_LAYOUT_TEXTMEASURE_H

#include <string>

namespace avalang {
namespace ui {
namespace layout {

// Text measurement used by intrinsic sizing (see
// LayoutEngineImpl::ComputeIntrinsicSize). Deliberately NOT a call
// into GDI or any other platform text-measuring API: the whole point
// of AvaLang's own LayoutEngine is that GdiRenderer (desktop) and
// HTMLRenderer (web) both draw the exact same coordinates it
// computes -- if a component's width came from an OS-specific glyph
// measurement, desktop and web could disagree on a button's size the
// moment fonts differ between the two environments, which is exactly
// the cross-platform consistency guarantee this engine exists to
// provide (see docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md, Fase A).
//
// This is therefore a deliberate heuristic, not a precise glyph
// measurement: an average-width-per-character approximation scaled by
// fontSize. It is good enough to turn a 0px or a 405px button into a
// reasonable natural size; it is NOT pixel-exact against what any
// given font actually renders. If a future phase needs pixel-exact
// measurement, that measurement needs to happen once (e.g. at
// compile/design time in AvaStudio) and be baked into the .avaui/
// layout data both renderers consume -- not measured independently,
// at runtime, by each renderer, or the two would drift apart again.

// Average width of one character at fontSize == 1.0, as a fraction of
// fontSize. Calibrated loosely for a generic UI sans-serif (Segoe
// UI/Arial-ish proportional font, the defaults this project's
// DefaultTheme already uses). Monospace or condensed/expanded fonts
// will be off by more than proportional fonts -- this is a heuristic,
// not a per-font metric table.
constexpr double kAverageCharWidthFactor = 0.55;

// Line height as a multiple of fontSize, for a single line of text --
// Phase 3's LayoutEngine has no text-wrapping model yet, so every
// text/button label is measured as exactly one line.
constexpr double kLineHeightFactor = 1.3;

// Estimates the on-screen width of `text` set at `fontSize`. `fontName`
// is accepted for a future per-font-family adjustment table, but is
// currently unused by the estimate -- see the heuristic note above.
// Returns 0.0 for empty text or a non-positive fontSize.
double EstimateTextWidth(const std::string& text, double fontSize, const std::string& fontName);

// Single-line height for a given fontSize (fontSize * kLineHeightFactor).
// Returns 0.0 for a non-positive fontSize.
double DefaultLineHeight(double fontSize);

} // namespace layout
} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_TEXTMEASURE_H
