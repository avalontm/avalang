#include "theme/ProjectFontOverrides.h"

#include "layout/FontRegistry.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace avalang {
namespace ui {
namespace theme {

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

bool ConsumeQuoted(const std::string& line, std::size_t& pos, std::string& out) {
    if (pos >= line.size() || line[pos] != '"') return false;
    const std::size_t closing = line.find('"', pos + 1);
    if (closing == std::string::npos) return false;
    out = line.substr(pos + 1, closing - pos - 1);
    pos = closing + 1;
    return true;
}

bool ParseFontLine(const std::string& rawLine, ProjectFontOverride& out) {
    const std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false;
    if (!StartsWith(line, "font")) return false;

    if (line.size() > 4 && !std::isspace(static_cast<unsigned char>(line[4]))) return false;

    std::string rest = Trim(line.substr(4));

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
            path = parts[0];
            break;
        case 2:
            role = parts[0];
            path = parts[1];
            break;
        case 3:
            role = parts[0];
            name = parts[1];
            path = parts[2];
            break;
        default:
            return false;
    }

    if (path.empty() || (parts.size() >= 2 && role.empty())) return false;

    if (name.empty()) {
        name = role.empty() ? "AppDefaultFont" : role;
    }

    out.role = role;
    out.name = name;
    out.filePath = path;
    return true;
}

}

std::vector<ProjectFontOverride> LoadProjectFontOverrides(const std::string& projectRoot) {
    std::vector<ProjectFontOverride> overrides;
    if (projectRoot.empty()) {
        return overrides;
    }

    const fs::path appAvaPath = fs::path(projectRoot) / "app.ava";
    std::ifstream file(appAvaPath);
    if (!file) {
        return overrides;
    }

    std::string line;
    while (std::getline(file, line)) {
        ProjectFontOverride decl;
        if (!ParseFontLine(line, decl)) {
            continue;
        }
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
            defaultOverride_ = std::move(o);
            hasDefault_ = true;
            continue;
        }
        const std::string role = o.role;
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

}
}
}