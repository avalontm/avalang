#pragma once

#include "imgui.h"

// Ava Studio brand & syntax palette. Centralized so theme.cpp (UI chrome)
// and panels/syntax_highlight.cpp (editor tokens) always agree on the
// exact colors from the AvaLang brand guide.
namespace studio::palette {

inline ImVec4 FromHex(unsigned int hex, float alpha = 1.0f) {
    const float r = ((hex >> 16) & 0xFF) / 255.0f;
    const float g = ((hex >> 8) & 0xFF) / 255.0f;
    const float b = (hex & 0xFF) / 255.0f;
    return ImVec4(r, g, b, alpha);
}

inline ImU32 U32FromHex(unsigned int hex, float alpha = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(FromHex(hex, alpha));
}

// --- Brand (primary = orange) -------------------------------------------
constexpr unsigned int kPrimary      = 0xFF7A00;
constexpr unsigned int kPrimaryHover = 0xFF8F1F;
constexpr unsigned int kPrimaryLight = 0xFFB347;
constexpr unsigned int kPrimaryDark  = 0xD95F02;
constexpr unsigned int kAccentGold   = 0xFFC857;

// --- Backgrounds (cool near-black, OpenCode-style) -------------------------
constexpr unsigned int kBackground = 0x0B0B0D;
constexpr unsigned int kSurface    = 0x101013;
constexpr unsigned int kCard       = 0x1C1C20;
constexpr unsigned int kBorder     = 0x29292E;

// --- Text -----------------------------------------------------------------
constexpr unsigned int kTextPrimary   = 0xE6E6E8;
constexpr unsigned int kTextSecondary = 0xB4B4B8;
constexpr unsigned int kTextMuted     = 0x8A8A90;
constexpr unsigned int kTextDisabled  = 0x6B6B70;

// --- Status -----------------------------------------------------------
constexpr unsigned int kSuccess = 0x22C55E;
constexpr unsigned int kWarning = 0xF59E0B;
constexpr unsigned int kError   = 0xEF4444;
constexpr unsigned int kInfo    = 0x38BDF8;

// --- Syntax (used by panels/syntax_highlight.cpp) --------------------------
constexpr unsigned int kSynKeyword  = 0xFF8F1F;
constexpr unsigned int kSynFunction = 0xFFD166;
constexpr unsigned int kSynType     = 0x59C3FF;
constexpr unsigned int kSynVariable = 0xF8FAFC;
constexpr unsigned int kSynString   = 0x98E06A;
constexpr unsigned int kSynNumber   = 0x7CC6FE;
constexpr unsigned int kSynComment  = 0x6B7280;
constexpr unsigned int kSynOperator = 0xF8FAFC;

} // namespace studio::palette
