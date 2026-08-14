#ifndef AVA_UI_LAYOUT_TEXTMEASURE_H
#define AVA_UI_LAYOUT_TEXTMEASURE_H

#include <string>
#include <vector>

namespace avalang {
namespace ui {
namespace layout {

// Text measurement used by intrinsic sizing (see
// LayoutEngineImpl::ComputeIntrinsicSize). AvaUI is the single place
// that computes layout -- GdiRenderer (desktop), HTMLRenderer (web),
// and AvaStudio's designer_canvas only ever draw the rect AvaUI hands
// them (see docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md, Fase A) -- so this
// measurement has to be trustworthy on its own, not a rough guess that
// each renderer then has to visually "catch up" to.
//
// As of the FontRegistry pass, this is REAL glyph measurement (via
// stb_truetype against an actual embedded TTF -- see FontRegistry.h),
// not the old average-width-per-character / flat-1.3x-line-height
// heuristic. Cross-platform consistency is preserved the same way it
// always was meant to be: one font file is the source of truth
// (AvaUI's embedded default, or a project's custom font registered by
// family name), and every renderer is responsible for painting that
// *same* font file rather than resolving its own by name -- see
// GdiRenderer::LoadFontFile and HTMLRenderer's @font-face emission.

// Estimates the on-screen width of `text` set at `fontSize` using the
// font registered under `fontName` in FontRegistry::Instance() (falls
// back to AvaUI's embedded default font if `fontName` isn't
// registered, e.g. a plain "Arial"/"Segoe UI" reference with no custom
// asset behind it -- measured for real against the default font rather
// than guessed). Returns 0.0 for empty text or a non-positive fontSize.
double EstimateTextWidth(const std::string& text, double fontSize, const std::string& fontName);

// Single-line height for `fontSize` px, using the same font resolution
// as EstimateTextWidth (real ascent/descent/lineGap metrics, not a
// flat multiplier). `fontName` may be empty to use the default font.
// Returns 0.0 for a non-positive fontSize.
double DefaultLineHeight(double fontSize, const std::string& fontName = std::string());

// Greedy word-wrap of `text` into lines that each fit within maxWidth
// px, measured with the same real glyph metrics EstimateTextWidth
// uses (same font resolution: `fontName`, falling back to AvaUI's
// embedded default). Words are never split mid-word -- a single word
// wider than maxWidth on its own still gets its own line rather than
// being cut, so wrapping never loses characters the way the old
// fixed `white-space: nowrap` + no-width OnDrawText did (see
// LayoutEngineImpl::ComputeIntrinsicSize's "wrap" handling and
// HTMLRenderer/GdiRenderer's OnDrawText).
//
// Existing newline characters ('\n') in `text` always force a line
// break, same as a real paragraph would, in addition to the
// width-driven wrapping.
//
// Returns a single-element vector containing `text` unchanged if
// maxWidth <= 0, text is empty, or text already fits within
// maxWidth -- callers that only care about "does this need to wrap
// at all" can check `result.size() > 1`.
std::vector<std::string> WrapTextLines(const std::string& text, double fontSize,
                                        const std::string& fontName, double maxWidth);

// Vertical distance between the baselines of two consecutive wrapped
// lines (DefaultLineHeight with a bit of extra leading, same
// convention as a browser's default `line-height: normal` typically
// landing a little above 1.0x the font's raw ascent+descent). Used by
// both LayoutEngineImpl (to size a wrapped Text's intrinsic height)
// and every OnDrawText implementation (to position each wrapped line)
// -- kept as one function so the two stay in lockstep instead of two
// renderers independently guessing a line-height multiplier that
// could drift apart from what LayoutEngineImpl already reserved
// space for.
double WrappedLineHeight(double fontSize, const std::string& fontName = std::string());

} // namespace layout
} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_TEXTMEASURE_H
