#include "layout/FontRegistry.h"

#include <cstdio>
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include "resources/fonts/DefaultFontData.h"

namespace avalang {
namespace ui {
namespace layout {

struct FontRegistry::LoadedFont {
    stbtt_fontinfo info{};
    bool valid = false;
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    int unitsPerEm = 1000; // stb_truetype default fallback; overwritten on load
};

FontRegistry& FontRegistry::Instance() {
    static FontRegistry instance;
    return instance;
}

FontRegistry::FontRegistry() {
    default_font_ = std::make_unique<LoadedFont>();
    const fonts::FontBytes bytes = fonts::DefaultRegular();
    LoadInto(default_font_.get(), bytes.data, bytes.size);
}

FontRegistry::~FontRegistry() = default;

bool FontRegistry::LoadInto(LoadedFont* slot, const unsigned char* ttfBytes, std::size_t byteCount) {
    if (slot == nullptr || ttfBytes == nullptr || byteCount == 0) {
        return false;
    }
    const int offset = stbtt_GetFontOffsetForIndex(ttfBytes, 0);
    if (offset < 0) {
        return false;
    }
    if (!stbtt_InitFont(&slot->info, ttfBytes, offset)) {
        return false;
    }
    stbtt_GetFontVMetrics(&slot->info, &slot->ascent, &slot->descent, &slot->lineGap);
    // unitsPerEm isn't exposed as a direct getter in this stb_truetype
    // version; scale is computed per-call via
    // stbtt_ScaleForPixelHeight, which already divides by the font's
    // internal units-per-em for us, so we don't need to store it
    // separately -- kept as a documented field in case a future stb
    // update needs it.
    slot->valid = true;
    return true;
}

bool FontRegistry::RegisterFont(const std::string& familyName, const unsigned char* ttfBytes,
                                 std::size_t byteCount, bool copyBytes) {
    if (copyBytes) {
        // stb_truetype keeps pointers INTO the buffer it was given (it
        // doesn't copy glyph data out at InitFont time), so the copy
        // has to outlive the LoadedFont -- store it in owned_bytes_
        // before/alongside the slot, not as a temporary.
        std::vector<unsigned char> owned(ttfBytes, ttfBytes + byteCount);
        auto slot = std::make_unique<LoadedFont>();
        const bool ok = LoadInto(slot.get(), owned.data(), owned.size());
        if (!ok) {
            return false;
        }
        owned_bytes_[familyName] = std::move(owned);
        fonts_[familyName] = std::move(slot);
        return true;
    }

    auto slot = std::make_unique<LoadedFont>();
    if (!LoadInto(slot.get(), ttfBytes, byteCount)) {
        return false;
    }
    fonts_[familyName] = std::move(slot);
    return true;
}

bool FontRegistry::RegisterFontFile(const std::string& familyName, const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }
    return RegisterFont(familyName, buffer.data(), buffer.size(), /*copyBytes=*/true);
}

bool FontRegistry::HasFont(const std::string& familyName) const {
    return fonts_.find(familyName) != fonts_.end();
}

const FontRegistry::LoadedFont* FontRegistry::Resolve(const std::string& fontName) const {
    if (!fontName.empty()) {
        const auto it = fonts_.find(fontName);
        if (it != fonts_.end() && it->second->valid) {
            return it->second.get();
        }
    }
    // Unregistered/empty family name falls back to the built-in
    // default rather than reverting to the old per-character guess --
    // every text control gets real metrics even with zero font
    // configuration.
    return (default_font_ && default_font_->valid) ? default_font_.get() : nullptr;
}

double FontRegistry::MeasureTextWidth(const std::string& text, double fontSize,
                                       const std::string& fontName) const {
    if (text.empty() || fontSize <= 0.0) {
        return 0.0;
    }
    const LoadedFont* font = Resolve(fontName);
    if (font == nullptr) {
        return 0.0;
    }

    // Must scale by the font's em-square (unitsPerEm), NOT by
    // stbtt_ScaleForPixelHeight's (ascent-descent) span. CSS/HTML
    // `font-size: Npx` -- which is what every renderer this measurement
    // has to agree with (HTMLRenderer's inline style, GdiRenderer's
    // point size) actually means -- maps 1 em to N px, i.e.
    // stbtt_ScaleForMappingEmToPixels. Using ScaleForPixelHeight instead
    // silently divides every glyph's advance by (ascent-descent)/unitsPerEm
    // instead of by 1: for a font like Poppins (ascent 1050, descent
    // -350, unitsPerEm 1000) that's a scale of 48/1400 instead of
    // 48/1000, i.e. every measured width comes out ~28.6% too narrow.
    // That understates intrinsic width for EVERY piece of text (not
    // just long/bound ones), so LayoutEngine boxes elements too small
    // and the renderer's unconditional `white-space: nowrap; overflow:
    // hidden; text-overflow: ellipsis` (see HTMLRenderer::OnDrawText)
    // then clips text that would otherwise have fit fine -- this is
    // why even short static labels like "500"/"Linea"/"Columna" were
    // getting cut off, not just long dynamic values.
    const float scale = stbtt_ScaleForMappingEmToPixels(const_cast<stbtt_fontinfo*>(&font->info),
                                                          static_cast<float>(fontSize));
    double width = 0.0;
    for (std::size_t i = 0; i < text.size();) {
        // ASCII fast path; the embedded default (JetBrains Mono) and
        // typical UI fonts are Latin-1-range for control labels, which
        // is what GetGlyphRangesDefault() covers on the ImGui side too
        // (see embedded_font.cpp). Multi-byte UTF-8 sequences are
        // walked byte-by-byte here deliberately conservative: unknown
        // continuation bytes fall back to the codepoint's own advance
        // via stb, which treats them as their raw codepoint value --
        // acceptable for the ASCII/Latin-1 labels AvaUI controls use
        // today; full UTF-8 decoding is a follow-up if non-Latin text
        // shows up in practice.
        const unsigned char c = static_cast<unsigned char>(text[i]);
        int advanceWidth = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(const_cast<stbtt_fontinfo*>(&font->info), c, &advanceWidth,
                                    &leftSideBearing);
        width += advanceWidth * scale;

        if (i + 1 < text.size()) {
            const int kerning = stbtt_GetCodepointKernAdvance(
                const_cast<stbtt_fontinfo*>(&font->info), c, static_cast<unsigned char>(text[i + 1]));
            width += kerning * scale;
        }
        ++i;
    }
    return width;
}

bool FontRegistry::GetFontBytes(const std::string& fontName, const unsigned char** outData,
                                 std::size_t* outSize) const {
    const LoadedFont* font = Resolve(fontName);
    if (font == nullptr || outData == nullptr || outSize == nullptr) {
        return false;
    }
    // stbtt_fontinfo::data already points at the exact TTF buffer this
    // font was loaded from (see stb_truetype.h) -- no need to keep a
    // second copy of the pointer/size around.
    const auto sizeIt = owned_bytes_.find(fontName);
    if (sizeIt != owned_bytes_.end()) {
        *outData = font->info.data;
        *outSize = sizeIt->second.size();
    } else if (!fontName.empty() && fonts_.find(fontName) != fonts_.end()) {
        // Registered without copyBytes=true: caller-owned buffer, size
        // isn't tracked here (RegisterFont's non-copy path is meant
        // for callers who already know their own buffer's lifetime and
        // size) -- fall back to the default font's known size instead
        // of returning a wrong size.
        *outData = font->info.data;
        const fonts::FontBytes defaultBytes = fonts::DefaultRegular();
        *outSize = defaultBytes.size;
    } else {
        const fonts::FontBytes defaultBytes = fonts::DefaultRegular();
        *outData = defaultBytes.data;
        *outSize = defaultBytes.size;
    }
    return true;
}

std::vector<std::string> FontRegistry::RegisteredFontNames() const {
    std::vector<std::string> names;
    names.reserve(fonts_.size());
    for (const auto& entry : fonts_) {
        names.push_back(entry.first);
    }
    return names;
}

double FontRegistry::LineHeight(double fontSize, const std::string& fontName) const {
    if (fontSize <= 0.0) {
        return 0.0;
    }
    const LoadedFont* font = Resolve(fontName);
    if (font == nullptr) {
        return 0.0;
    }
    // Same em-square scale as MeasureTextWidth above, for the same
    // reason: WrapTextLines/LayoutEngineImpl reserve vertical space
    // using this value alongside horizontal widths from
    // MeasureTextWidth, and HTMLRenderer positions each wrapped line's
    // y-offset by it too -- all three have to agree with the browser's
    // own (em-based) `font-size: Npx` line-height, not a
    // ScaleForPixelHeight-flavored one.
    const float scale = stbtt_ScaleForMappingEmToPixels(const_cast<stbtt_fontinfo*>(&font->info),
                                                          static_cast<float>(fontSize));
    return (font->ascent - font->descent + font->lineGap) * scale;
}

} // namespace layout
} // namespace ui
} // namespace avalang
