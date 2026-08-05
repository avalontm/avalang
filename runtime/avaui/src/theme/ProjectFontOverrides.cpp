#include "theme/ProjectFontOverrides.h"

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

// Parses one `font "role" "name" "path"` line. Returns false for
// anything else (blank, `#` comment, `import ...`, malformed `font`
// line) -- app.ava lines this function doesn't recognize are always
// silently skipped, never an error, same tolerance
// app_manifest.cpp::ParseImportLine documents.
bool ParseFontLine(const std::string& rawLine, ProjectFontOverride& out) {
    const std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false;
    if (!StartsWith(line, "font")) return false;

    // Require a word boundary after "font" (not e.g. "fontawesome...").
    if (line.size() > 4 && !std::isspace(static_cast<unsigned char>(line[4]))) return false;

    std::string rest = Trim(line.substr(4));
    std::size_t pos = 0;

    std::string role, name, path;
    if (!ConsumeQuoted(rest, pos, role)) return false;
    rest = Trim(rest.substr(pos));
    pos = 0;
    if (!ConsumeQuoted(rest, pos, name)) return false;
    rest = Trim(rest.substr(pos));
    pos = 0;
    if (!ConsumeQuoted(rest, pos, path)) return false;

    if (role.empty() || name.empty() || path.empty()) return false;

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
        const std::string role = o.role; // copy before move-into-map
        overridesByRole_.emplace(role, std::move(o));
    }
}

ThemeColor ProjectTheme::Color(const std::string& roleName, const ThemeColor& fallback) {
    return base_->Color(roleName, fallback);
}

ThemeFont ProjectTheme::Font(const std::string& roleName, const ThemeFont& fallback) {
    ThemeFont font = base_->Font(roleName, fallback);
    const auto it = overridesByRole_.find(roleName);
    if (it != overridesByRole_.end()) {
        // Only the identity of the font changes -- size/weight/italic
        // stay whatever the base theme already decided for this role
        // (see class doc comment).
        font.name = it->second.name;
        font.filePath = it->second.filePath;
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
    return overridesByRole_.count(roleName) > 0 || base_->HasFont(roleName);
}

uint32_t ProjectTheme::AbiVersion() const {
    return base_->AbiVersion();
}

} // namespace avalang::ui::theme
