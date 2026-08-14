#ifndef AVA_UI_LAYOUT_FONTREGISTRY_H
#define AVA_UI_LAYOUT_FONTREGISTRY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace avalang {
namespace ui {
namespace layout {

// AvaUI's single source of truth for "what does this font actually
// measure to". Replaces the old average-char-width / 1.3x-line-height
// guesses (see TextMeasure.h history) with real glyph metrics read out
// of an actual TTF via stb_truetype -- the same library Dear ImGui uses
// internally to build its own font atlas, so this isn't a new kind of
// dependency, just one AvaUI now owns directly instead of only
// benefiting the editor's own chrome.
//
// Design goals this exists to satisfy (see conversation / design notes
// in docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md):
//   1. AvaUI is the ONLY place that computes layout. GdiRenderer,
//      HTMLRenderer, and AvaStudio's designer_canvas must never
//      recompute a size or position -- they only draw the rect AvaUI
//      already decided. This registry is what makes that rect
//      trustworthy: it's derived from a real font instead of a guess.
//   2. A project can use the built-in default font (JetBrains Mono,
//      embedded -- see DefaultFontData.h) with zero configuration, or
//      register a custom font (embedded TTF bytes, typically copied
//      into the project's assets/fonts/ folder by AvaStudio's font
//      picker) under whatever family name the theme/component uses.
//   3. Every renderer that PAINTS text (GdiRenderer, HTMLRenderer, the
//      AvaStudio editor) must end up drawing the exact same font file
//      this registry measured against, or the measured rect and the
//      painted glyphs will disagree again. This class only owns
//      measurement; wiring each renderer to the same font *file* is
//      the renderer's job (see GdiRenderer::LoadFontFile /
//      HTMLRenderer's @font-face emission).
class FontRegistry {
public:
    // Global instance used by TextMeasure.cpp's free functions. Tests
    // and embedders that want isolation can still construct their own
    // FontRegistry directly.
    static FontRegistry& Instance();

    FontRegistry();
    ~FontRegistry();

    FontRegistry(const FontRegistry&) = delete;
    FontRegistry& operator=(const FontRegistry&) = delete;

    // Registers `ttfBytes` (must stay valid for the lifetime of this
    // registry -- pass bytes from a static embed array, or from a
    // buffer this registry copies internally if `copyBytes` is true)
    // under `familyName`. A later call with the same familyName
    // replaces the previous registration (e.g. AvaStudio's font picker
    // re-pointing a family at a newly-copied project asset).
    //
    // Returns false if the bytes don't parse as a valid TTF/OTF --
    // callers should keep using whatever was registered before (or the
    // built-in default) rather than silently losing measurement.
    bool RegisterFont(const std::string& familyName, const unsigned char* ttfBytes,
                       std::size_t byteCount, bool copyBytes = true);

    // Convenience: reads the file at `path` and registers it. Returns
    // false if the file can't be read or doesn't parse.
    bool RegisterFontFile(const std::string& familyName, const std::string& path);

    // True if `familyName` has been explicitly registered (as opposed
    // to falling back to the default embedded font).
    bool HasFont(const std::string& familyName) const;

    // Real advance-width sum for `text` set at `fontSize` px, using the
    // font registered under `fontName` if present, else the built-in
    // default (JetBrains Mono Regular). Never guesses per-character
    // averages -- this is the actual font's per-glyph advance widths.
    double MeasureTextWidth(const std::string& text, double fontSize, const std::string& fontName) const;

    // Real single-line height for `fontSize` px against `fontName` (or
    // the default font), derived from the font's own ascent/descent/
    // lineGap metrics instead of a flat 1.3x multiplier.
    double LineHeight(double fontSize, const std::string& fontName) const;

    // Raw TTF bytes backing `fontName` (or the default font if
    // unregistered/empty). Used by renderers that need to load the
    // EXACT font this registry measured against -- GdiRenderer via
    // AddFontMemResourceEx, HTMLRenderer via a served @font-face file
    // -- instead of resolving a font independently by name and risking
    // a different file (see TextMeasure.h). Returns false only if even
    // the built-in default failed to load (should not happen in
    // practice).
    bool GetFontBytes(const std::string& fontName, const unsigned char** outData,
                       std::size_t* outSize) const;

    // Family names of every explicitly-registered custom font (never
    // includes the built-in default). HTMLRenderer needs this in
    // addition to its own per-frame usedFontNames_: when a page is
    // composed from more than one HTMLRenderer instance (page fragment
    // + layout, see ui_pipeline_dynamic_renderer.cpp's
    // RenderTreeFragment), a font referenced only inside a spliced-in
    // fragment never shows up in the FINAL renderer's own
    // usedFontNames_ (that renderer never drew that text itself), so
    // its @font-face rule would otherwise be silently missing from the
    // assembled HTML even though the font is correctly registered.
    // Emitting @font-face for every registered font, not just the
    // ones this particular instance happened to draw, closes that gap.
    std::vector<std::string> RegisteredFontNames() const;

private:
    struct LoadedFont; // pImpl: keeps stb_truetype.h out of this header
    const LoadedFont* Resolve(const std::string& fontName) const;
    bool LoadInto(LoadedFont* slot, const unsigned char* ttfBytes, std::size_t byteCount);

    std::unordered_map<std::string, std::unique_ptr<LoadedFont>> fonts_;
    std::unique_ptr<LoadedFont> default_font_;
    // Backing storage for registrations that asked to be copied (so
    // callers can pass a temporary buffer, e.g. bytes just read from
    // disk, without keeping it alive themselves).
    std::unordered_map<std::string, std::vector<unsigned char>> owned_bytes_;
};

} // namespace layout
} // namespace ui
} // namespace avalang

#endif // AVA_UI_LAYOUT_FONTREGISTRY_H
