#pragma once

#include "Export.h"
#include <cstdint>
#include <string>
#include <memory>

namespace avalang::ui {

/**
 * Color descriptor for theme palette.
 * Format: "RRGGBB" hex string (e.g., "FF5733")
 */
struct ThemeColor {
    std::string hex;  // "RRGGBB" or "RRGGBBAA" format
    
    ThemeColor() = default;
    explicit ThemeColor(const std::string& h) : hex(h) {}
};

/**
 * Font descriptor for theme typography.
 */
struct ThemeFont {
    std::string name;        // Font family name (e.g., "Segoe UI", "Arial") --
                              // also the key layout::FontRegistry measures
                              // real glyph metrics under (see FontRegistry.h).
    uint32_t sizePoints = 12; // Font size in points
    uint32_t weight = 400;    // 400=normal, 700=bold (CSS weights)
    bool italic = false;

    // Optional path (relative to the project root, e.g.
    // "assets/fonts/Inter-Regular.ttf") to an embedded/custom TTF that
    // backs `name`. Empty means "use AvaUI's built-in default font
    // (JetBrains Mono) for measurement" -- `name` is then purely a
    // display label passed through to renderers, NOT something
    // FontRegistry can measure against (a bare system font name has no
    // guaranteed-identical file on every target platform/browser; see
    // FontRegistry.h). When AvaStudio's font picker resolves a system
    // font, it copies that font's file into the project's assets and
    // fills this in, so `name` and `filePath` end up describing the
    // exact same bytes everywhere the project is built or previewed.
    std::string filePath;

    ThemeFont() = default;
    ThemeFont(const std::string& n, uint32_t s) 
        : name(n), sizePoints(s) {}
    ThemeFont(const std::string& n, uint32_t s, const std::string& file)
        : name(n), sizePoints(s), filePath(file) {}
};

/**
 * Spacing/sizing descriptor.
 */
struct ThemeSpacing {
    uint32_t paddingPx = 8;
    uint32_t marginPx = 4;
    uint32_t borderWidthPx = 1;
    uint32_t borderRadiusPx = 4;
    uint32_t containerPaddingPx = 16;
    uint32_t containerGapPx = 12;
};

/**
 * Theme interface: provides default colors, fonts, spacing for UI components.
 * 
 * Design:
 * - Theme is applied BEFORE layout (modifies component property bags with defaults)
 * - Component-specific properties override theme defaults (CSS cascade model)
 * - Typeless: theme doesn't know about Button vs Text vs Image; only provides
 *   named roles (primary button, secondary button, text body, heading, etc.)
 * - Controls (Phase 17+) map their types to theme roles
 * 
 * Usage pattern:
 *   1. Parser loads .avaui -> ComponentTree
 *   2. Theme::Apply(componentTree) -> fills empty properties with theme defaults
 *   3. LayoutEngine processes ComponentTree (sees now-complete properties)
 *   4. ... rest of pipeline (Render Tree, Scene Graph, Renderer)
 */
class ITheme {
public:
    virtual ~ITheme() = default;

    /**
     * Get a named color from the palette.
     * 
     * Common roles:
     *   primary, primaryLight, primaryDark
     *   secondary
     *   background, surface, surfaceVariant
     *   text, textSecondary, textDisabled
     *   border, borderLight
     *   success, error, warning
     * 
     * @param roleName Color role (e.g., "primary", "textSecondary")
     * @param fallback Color to return if role not found (default: black "000000")
     * @return ThemeColor with hex value
     */
    virtual ThemeColor Color(const std::string& roleName, 
                            const ThemeColor& fallback = ThemeColor("000000")) = 0;

    /**
     * Get a named font from typography.
     * 
     * Common roles:
     *   heading1, heading2, heading3
     *   body, bodySmall
     *   button, label
     * 
     * @param roleName Font role (e.g., "body", "button")
     * @param fallback Font to return if role not found
     * @return ThemeFont with name, size, weight, italic
     */
    virtual ThemeFont Font(const std::string& roleName,
                          const ThemeFont& fallback = ThemeFont("Segoe UI", 12)) = 0;

    /**
     * Get spacing/sizing defaults.
     * 
     * @return ThemeSpacing with padding, margin, border widths
     */
    virtual ThemeSpacing Spacing() const = 0;

    /**
     * Get theme name (for debugging/UI display).
     * 
     * @return Theme identifier (e.g., "Default Light", "Dark Mode")
     */
    virtual std::string Name() const = 0;

    /**
     * Check if a color role exists in this theme.
     * 
     * @param roleName Color role
     * @return true if role is defined
     */
    virtual bool HasColor(const std::string& roleName) const = 0;

    /**
     * Check if a font role exists in this theme.
     * 
     * @param roleName Font role
     * @return true if role is defined
     */
    virtual bool HasFont(const std::string& roleName) const = 0;

    /**
     * Get ABI version for binary compatibility.
     */
    virtual uint32_t AbiVersion() const = 0;
};

/**
 * Theme provider: creates and manages themes.
 * 
 * Can hold multiple themes and switch between them.
 * Typically accessed globally or passed through RenderTheme/LayoutEngine.
 */
class IThemeProvider {
public:
    virtual ~IThemeProvider() = default;

    /**
     * Get current active theme.
     * 
     * @return Pointer to active theme; never nullptr
     */
    virtual ITheme* Current() = 0;

    /**
     * Set active theme by name.
     * 
     * @param themeName Theme identifier (e.g., "Default Light")
     * @return true if theme found and activated
     */
    virtual bool SetTheme(const std::string& themeName) = 0;

    /**
     * Register a theme (add to provider's collection).
     * 
     * @param theme Heap-allocated theme; provider takes ownership
     * @param name Theme identifier
     * @return true if registration succeeded
     */
    virtual bool Register(std::unique_ptr<ITheme> theme, const std::string& name) = 0;

    /**
     * Get ABI version for binary compatibility.
     */
    virtual uint32_t AbiVersion() const = 0;
};

/**
 * Factory function to create a default theme provider.
 * 
 * Returns a provider with a built-in "Default Light" theme applied.
 * 
 * @return Heap-allocated IThemeProvider; caller must delete
 */
AVA_UI_API IThemeProvider* CreateDefaultThemeProvider();

} // namespace avalang::ui
