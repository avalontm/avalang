#pragma once

#include "theme/ITheme.h"
#include "Export.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace avalang::ui::theme {

// One `font` line from a project's app.ava. See
// LoadProjectFontOverrides for the exact syntax. `filePath` is already
// resolved to an absolute path by the time it comes out of
// LoadProjectFontOverrides -- callers (RenderTheme via
// ProjectTheme::Font, then layout::FontRegistry::RegisterFontFile) open
// it directly, with no further path-joining against projectRoot.
//
// `role` empty means "the app default" -- applies to every role that
// doesn't have its own more specific `font` line (see
// ProjectTheme::Font). `name` is always filled in by
// LoadProjectFontOverrides even when the app.ava line didn't specify
// one explicitly: it's just the internal FontRegistry/CSS lookup key
// for this file, never shown to the user, so a sensible one (see
// ParseFontLine) is generated rather than required on every line.
struct ProjectFontOverride {
    std::string role;
    std::string name;
    std::string filePath;
};

// Reads `projectRoot/app.ava` and parses every `font ...` line, same
// tolerant style as AvaHost's own app.ava reader (config/app_manifest.cpp):
// blank lines, `#` comments, and any line that isn't recognized
// (including plain `import "..."` lines, which are AvaHost's concern,
// not this function's) are silently skipped rather than treated as
// errors -- app.ava is meant to grow other kinds of declarations over
// time without every reader needing to understand all of them.
//
// This lives in AvaUI (not AvaHost's config/app_manifest.cpp) so that
// AvaStudio's design-time preview (live_render_bridge.cpp, which links
// avalang_ui but NOT avahost) and AvaHost's actual runtime render
// pipeline can both call the exact same parser and get the exact same
// answer -- duplicating this logic in two places is how preview/runtime
// drift happens in the first place (see the AvaUI-is-the-only-layout-
// authority rationale in layout/TextMeasure.h).
//
// Syntax -- one to three double-quoted strings after `font`, same
// quoting convention as app_manifest.cpp's `import "..."`:
//
//   font "path/to/file.ttf"
//     Sets the app-wide default font. Every role (body, heading1,
//     button, link, ...) that isn't given its own `font` line below
//     picks this up automatically -- this is normally the only line a
//     project needs.
//
//   font "role" "path/to/file.ttf"
//     Overrides just `role` (must match a roleName ITheme::Font/
//     RenderTheme already use: "body", "heading1", "heading2",
//     "heading3", "label", "button", "link", ...) with this file,
//     regardless of what the default (if any) says. Typical use: the
//     default covers body text, one or two `font "headingN" "..."`
//     lines cover bold headings.
//
//   font "role" "family name" "path/to/file.ttf"
//     Same as the two-argument form but also pins the internal
//     registry/CSS family name instead of letting one be generated --
//     rarely needed; only matters if something downstream inspects
//     that name directly.
//
// Example:
//   font "assets/fonts/Inter-Regular.ttf"
//   font "heading1" "assets/fonts/Inter-Bold.ttf"
//
// `path` is project-relative (forward or backward slashes both work);
// this function resolves it against `projectRoot` before returning, so
// the caller never needs projectRoot again.
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

    // Eagerly registers every app.ava `font` line into
    // layout::FontRegistry, instead of relying on RenderTheme::
    // ApplyTypeDefaults to register a font the first time some
    // component happens to resolve that role. That lazy path only
    // fires when a component has NO explicit `fontName` of its own
    // (see RenderTheme.cpp's ResolveFont) -- a component that sets
    // `fontName = "heading1"` directly (the documented way to opt into
    // a non-body role, see samples/web/testproj/routes/index.avaui)
    // never goes through ResolveFont at all, so "heading1" stayed
    // unregistered and silently fell back to the built-in default
    // font. Call this once, right after constructing a ProjectTheme,
    // before RenderTheme::Apply runs -- cheap no-op when app.ava has
    // no `font` lines.
    void RegisterProjectFonts() const;

private:
    ITheme* base_;
    std::unordered_map<std::string, ProjectFontOverride> overridesByRole_;
    // Set only when app.ava has a bare `font "path"` line (role == "").
    // Font(roleName) falls back to this whenever roleName has no entry
    // of its own in overridesByRole_ -- see the class doc comment above.
    bool hasDefault_ = false;
    ProjectFontOverride defaultOverride_;
};

} // namespace avalang::ui::theme
