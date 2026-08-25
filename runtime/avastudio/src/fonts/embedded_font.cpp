#include "fonts/embedded_font.h"

#include <cstdio>

#include "fonts/jetbrains_mono_bold_ttf.h"
#include "fonts/jetbrains_mono_regular_ttf.h"

namespace {

ImFont* g_code_font = nullptr;

}

namespace studio {

ImFont* LoadDefaultFont(float size_px) {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig config;

    config.FontDataOwnedByAtlas = false;
    std::snprintf(config.Name, sizeof(config.Name), "JetBrains Mono, %.0fpx", size_px);

    ImFont* font = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(fonts::kJetBrainsMonoRegularTTF),
        static_cast<int>(fonts::kJetBrainsMonoRegularTTFLen),
        size_px,
        &config,
        io.Fonts->GetGlyphRangesDefault());

    io.FontDefault = font;
    return font;
}

ImFont* LoadBoldFont(float size_px) {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig config;

    config.FontDataOwnedByAtlas = false;
    std::snprintf(config.Name, sizeof(config.Name), "JetBrains Mono Bold, %.0fpx", size_px);

    ImFont* font = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(fonts::kJetBrainsMonoBoldTTF),
        static_cast<int>(fonts::kJetBrainsMonoBoldTTFLen),
        size_px,
        &config,
        io.Fonts->GetGlyphRangesDefault());

    g_code_font = font;
    return font;
}

ImFont* GetCodeFont() {
    return g_code_font;
}

}
