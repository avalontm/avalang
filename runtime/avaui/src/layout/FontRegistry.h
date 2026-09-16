#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "Export.h"

namespace avalang {
namespace ui {
namespace layout {

class AVA_UI_API FontRegistry {
public:
    static FontRegistry& Instance();

    FontRegistry();
    ~FontRegistry();

    FontRegistry(const FontRegistry&) = delete;
    FontRegistry& operator=(const FontRegistry&) = delete;

    bool RegisterFont(const std::string& familyName, const unsigned char* ttfBytes,
                       std::size_t byteCount, bool copyBytes = true);

    bool RegisterFontFile(const std::string& familyName, const std::string& path);

    bool HasFont(const std::string& familyName) const;

    double MeasureTextWidth(const std::string& text, double fontSize, const std::string& fontName) const;

    double LineHeight(double fontSize, const std::string& fontName) const;

    bool GetFontBytes(const std::string& fontName, const unsigned char** outData,
                       std::size_t* outSize) const;

    std::vector<std::string> RegisteredFontNames() const;

private:
    struct LoadedFont;
    const LoadedFont* Resolve(const std::string& fontName) const;
    bool LoadInto(LoadedFont* slot, const unsigned char* ttfBytes, std::size_t byteCount);

    std::unordered_map<std::string, std::unique_ptr<LoadedFont>> fonts_;
    std::unique_ptr<LoadedFont> default_font_;
    std::unordered_map<std::string, std::vector<unsigned char>> owned_bytes_;
};

}
}
}