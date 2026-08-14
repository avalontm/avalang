#include "theme/ProjectFontOverrides.h"

#include "layout/FontRegistry.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace avalang::ui::theme {

namespace {

namespace fs = std::filesystem;

std::string Trim(const std::string& s) {
    const std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Consumes one double-quoted string starting at `pos` (which must
// point at the opening `"`). On success, advances `pos` to just past
// the closing quote and returns the unescaped-nothing contents (app.ava
// font paths/names don't need escape sequences -- same as
// app_manifest.cpp's ParseImportLine, which also just slices between
// quotes verbatim). Returns false if `pos` doesn't point at a `"` or
// there's no closing quote.
bool ConsumeQuoted(const std::string& line, std::size_t& pos, std::string& out) {
    if (pos >= line.size() || line[pos] != '"') return false;
    const std::size_t closing = line.find('"', pos + 1);
    if (closing == std::string::npos) return false;
    out = line.substr(pos + 1, closing - pos - 1);
    pos = closing + 1;
    return true;
}

// Parses one `font ...` line -- one, two, or three double-quoted
// strings after `font` (see the syntax table in ProjectFontOverrides.h):
//   font "path"                      -> app-wide default
//   font "role" "path"               -> override for `role`, auto name
//   font "role" "family" "path"      -> override for `role`, explicit name
// Returns false for anything else (blank, `#` comment, `import ...`,
// malformed `font` line, more than three quoted strings) -- app.ava
// lines this function doesn't recognize are always silently skipped,
// never an error, same tolerance app_manifest.cpp::ParseImportLine
// documents.
bool ParseFontLine(const std::string& rawLine, ProjectFontOverride& out) {
    const std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false;
    if (!StartsWith(line, "font")) return false;

    // Require a word boundary after "font" (not e.g. "fontawesome...").
    if (line.size() > 4 && !std::isspace(static_cast<unsigned char>(line[4]))) return false;

    std::string rest = Trim(line.substr(4));

    // Collect up to four quoted strings -- a fourth means the line is
    // malformed (unsupported arity), not silently truncated.
    std::vector<std::string> parts;
    while (!rest.empty() && parts.size() <= 3) {
        std::size_t pos = 0;
        std::string part;
        if (!ConsumeQuoted(rest, pos, part)) return false;
        parts.push_back(std::move(part));
        rest = Trim(rest.substr(pos));
    }

    std::string role, name, path;
    switch (parts.size()) {
        case 1:
            // font "path" -- app-wide default; no role, no explicit name.
            path = parts[0];
            break;
        case 2:
            // font "role" "path" -- name auto-generated below.
            role = parts[0];
            path = parts[1];
            break;
        case 3:
            // font "role" "family" "path" -- explicit name, as before.
            role = parts[0];
            name = parts[1];
            path = parts[2];
            break;
        default:
            return false; // 0 or 4+ quoted strings: not a valid font line.
    }

    if (path.empty() || (parts.size() >= 2 && role.empty())) return false;

    // Auto-generate the registry/CSS lookup key when the line didn't
    // pin one explicitly: "AppDefaultFont" for the default, or the
    // role name itself (e.g. "heading1") for a role override. Never
    // shown to the user -- see the field doc comment in the header.
    if (name.empty()) {
        name = role.empty() ? "AppDefaultFont" : role;
    }

    out.role = role;
    out.name = name;
    out.filePath = path;
    return true;
}

} // namespace

std::vector<ProjectFontOverride> LoadProjectFontOverrides(const std::string& projectRoot) {
    std::vector<ProjectFontOverride> overrides;
    if (projectRoot.empty()) {
        return overrides;
    }

    const fs::path appAvaPath = fs::path(projectRoot) / "app.ava";
    std::ifstream file(appAvaPath);
    if (!file) {
        return overrides; // No app.ava -- not an error, just no overrides.
    }

    std::string line;
    while (std::getline(file, line)) {
        ProjectFontOverride decl;
        if (!ParseFontLine(line, decl)) {
            continue;
        }
        // Resolve project-relative -> absolute now, once, here --
        // every downstream consumer (ProjectTheme::Font ->
        // layout::FontRegistry::RegisterFontFile -> std::ifstream)
        // just opens decl.filePath directly, with no projectRoot in
        // sight and therefore no chance of resolving it against the
        // wrong working directory.
        fs::path resolved = fs::path(projectRoot) / decl.filePath;
        decl.filePath = resolved.lexically_normal().string();
        overrides.push_back(std::move(decl));
    }
    return overrides;
}

ProjectTheme::ProjectTheme(ITheme* base, std::vector<ProjectFontOverride> overrides)
    : base_(base) {
    for (auto& o : overrides) {
        if (o.role.empty()) {
            // `font "path"` with no role: app-wide default. A later
            // default line replaces an earlier one (last one in
            // app.ava wins), same as role-specific lines below.
            defaultOverride_ = std::move(o);
            hasDefault_ = true;
            continue;
        }
        const std::string role = o.role; // copy before move-into-map
        overridesByRole_.emplace(role, std::move(o));
    }
}

ThemeColor ProjectTheme::Color(const std::string& roleName, const ThemeColor& fallback) {
    return base_->Color(roleName, fallback);
}

ThemeFont ProjectTheme::Font(const std::string& roleName, const ThemeFont& fallback) {
    ThemeFont font = base_->Font(roleName, fallback);
    // Role-specific `font "role" ...` line wins over the app-wide
    // default, which wins over whatever base_ (AvaStudio's built-in
    // theme) already had. Only the identity of the font changes --
    // size/weight/italic stay whatever the base theme already decided
    // for this role (see class doc comment).
    const auto it = overridesByRole_.find(roleName);
    if (it != overridesByRole_.end()) {
        font.name = it->second.name;
        font.filePath = it->second.filePath;
    } else if (hasDefault_) {
        font.name = defaultOverride_.name;
        font.filePath = defaultOverride_.filePath;
    }
    return font;
}

ThemeSpacing ProjectTheme::Spacing() const {
    return base_->Spacing();
}

std::string ProjectTheme::Name() const {
    return base_->Name();
}

bool ProjectTheme::HasColor(const std::string& roleName) const {
    return base_->HasColor(roleName);
}

bool ProjectTheme::HasFont(const std::string& roleName) const {
    return overridesByRole_.count(roleName) > 0 || hasDefault_ || base_->HasFont(roleName);
}

void ProjectTheme::RegisterProjectFonts() const {
    auto& registry = layout::FontRegistry::Instance();
    if (hasDefault_ && !registry.HasFont(defaultOverride_.name)) {
        registry.RegisterFontFile(defaultOverride_.name, defaultOverride_.filePath);
    }
    for (const auto& [role, override_] : overridesByRole_) {
        (void)role;
        if (!registry.HasFont(override_.name)) {
            registry.RegisterFontFile(override_.name, override_.filePath);
        }
    }
}

uint32_t ProjectTheme::AbiVersion() const {
    return base_->AbiVersion();
}

} // namespace avalang::ui::theme
