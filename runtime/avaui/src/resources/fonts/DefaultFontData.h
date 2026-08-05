#pragma once

// Single accessor point for "what is AvaUI's canonical default font".
// Both the regular and bold TTF byte arrays are checked in (see
// default_font_regular_ttf.h / default_font_bold_ttf.h) and are the same
// bytes AvaStudio's editor loads into ImGui (fonts/embedded_font.cpp) --
// one font file, one owner (AvaUI), every consumer (layout measurement,
// GdiRenderer, HTMLRenderer, the AvaStudio editor/preview) reads or draws
// from the same source instead of picking its own font by name and
// silently drifting from what AvaUI measured. See layout/TextMeasure.h
// for the fuller rationale.

#include <cstddef>

namespace avalang::ui::fonts {

struct FontBytes {
    const unsigned char* data = nullptr;
    std::size_t size = 0;
};

// JetBrains Mono Regular -- AvaUI's canonical default font. Used for
// layout measurement whenever a component doesn't resolve to a
// project-supplied custom font (see FontRegistry).
FontBytes DefaultRegular();

// JetBrains Mono Bold -- canonical default bold weight.
FontBytes DefaultBold();

} // namespace avalang::ui::fonts
