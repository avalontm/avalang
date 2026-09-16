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
    slot->valid = true;
    return true;
}

bool FontRegistry::RegisterFont(const std::string& familyName, const unsigned char* ttfBytes,
                                 std::size_t byteCount, bool copyBytes) {
    if (copyBytes) {
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
    return RegisterFont(familyName, buffer.data(), buffer.size(), true);
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

    const float scale = stbtt_ScaleForMappingEmToPixels(const_cast<stbtt_fontinfo*>(&font->info),
                                                          static_cast<float>(fontSize));
    double width = 0.0;
    for (std::size_t i = 0; i < text.size();) {
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
    const auto sizeIt = owned_bytes_.find(fontName);
    if (sizeIt != owned_bytes_.end()) {
        *outData = font->info.data;
        *outSize = sizeIt->second.size();
    } else if (!fontName.empty() && fonts_.find(fontName) != fonts_.end()) {
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
    const float scale = stbtt_ScaleForMappingEmToPixels(const_cast<stbtt_fontinfo*>(&font->info),
                                                          static_cast<float>(fontSize));
    return (font->ascent - font->descent + font->lineGap) * scale;
}

}
}
}