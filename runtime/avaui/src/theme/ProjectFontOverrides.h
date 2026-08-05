#pragma once

#include "theme/ITheme.h"
#include "Export.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang::ui::theme {

// One `font "role" "Family Name" "path/to/file.ttf"` line from a
// project's app.ava. See LoadProjectFontOverrides for the exact
// syntax. `filePath` is already resolved to an absolute path by the
// time it comes out of LoadProjectFontOverrides -- callers (RenderTheme
// via ProjectTheme::Font, then layout::FontRegistry::RegisterFontFile)
// open it directly, with no further path-joining against projectRoot.
struct ProjectFontOverride {
    std::string role;
    std::string name;
    std::string filePath;
};

// Reads `projectRoot/app.ava` and parses every `font "role" "name"
// "path"` line, same tolerant style as AvaHost's own app.ava reader
// (config/app_manifest.cpp): blank lines, `#` comments, and any line
// that isn't recognized (including plain `import "..."` lines, which
// are AvaHost's concern, not this function's) are silently skipped
// rather than treated as errors -- app.ava is meant to grow other
// kinds of declarations over time without every reader needing to
// understand all of them.
//
// This lives in AvaUI (not AvaHost's config/app_manifest.cpp) so that
// AvaStudio's design-time preview (live_render_bridge.cpp, which links
// avalang_ui but NOT avahost) and AvaHost's actual runtime render
// pipeline can both call the exact same parser and get the exact same
// answer -- duplicating this logic in two places is how preview/runtime
// drift happens in the first place (see the AvaUI-is-the-only-layout-
// authority rationale in layout/TextMeasure.h).
//
// Syntax (three double-quoted strings after `font`, same quoting
// convention as app_manifest.cpp's `import "..."`):
//   font "body" "Inter" "assets/fonts/Inter-Regular.ttf"
//   font "heading1" "Inter Bold" "assets/fonts/Inter-Bold.ttf"
//
// `role` matches the roleName strings ITheme::Font/RenderTheme already
// use ("body", "button", "link", "label", "heading1", ...). `path` is
// project-relative (forward or backward slashes both work); this
// function resolves it against `projectRoot` before returning, so the
// caller never needs projectRoot again.
//
// Returns an empty vector (never an error) if projectRoot is empty, if
// app.ava doesn't exist, or if it has no `font` lines -- a project with
// no custom fonts is the common case, not a failure.
AVA_UI_API std::vector<ProjectFontOverride> LoadProjectFontOverrides(const std::string& projectRoot);

// ITheme decorator: wraps `base` and overrides just the `name` +
// `filePath` of whichever roles appear in `overrides`, leaving
// sizePoints/weight/italic (and everything else -- Color, Spacing) to
// `base`. That split is deliberate: a project customizing its heading
// font shouldn't also have to redeclare the size AvaStudio's default
// theme already picked for "heading1", only which glyphs draw it.
//
// `base` is NOT owned by ProjectTheme -- same non-owning-pointer
// convention IThemeProvider::Current() already uses (the provider owns
// the theme; RenderTheme::Apply just borrows a raw ITheme* for the
// duration of one Apply() call). Construct with an empty `overrides`
// list to get a plain passthrough (e.g. when LoadProjectFontOverrides
// found nothing) -- cheaper to always wrap than to branch on
// overrides.empty() at every call site.
class AVA_UI_API ProjectTheme : public ITheme {
public:
    ProjectTheme(ITheme* base, std::vector<ProjectFontOverride> overrides);

    ThemeColor Color(const std::string& roleName,
                      const ThemeColor& fallback = ThemeColor("000000")) override;
    ThemeFont Font(const std::string& roleName,
                   const ThemeFont& fallback = ThemeFont("Segoe UI", 12)) override;
    ThemeSpacing Spacing() const override;
    std::string Name() const override;
    bool HasColor(const std::string& roleName) const override;
    bool HasFont(const std::string& roleName) const override;
    uint32_t AbiVersion() const override;

private:
    ITheme* base_;
    std::unordered_map<std::string, ProjectFontOverride> overridesByRole_;
};

} // namespace avalang::ui::theme
