#include "theme/ProjectStyleOverrides.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace avalang::ui::theme {

namespace {

namespace fs = std::filesystem;

std::string Trim(const std::string& s) {
    const std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string Lowercase(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return result;
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

bool ParseStyleDeclarationLine(const std::string& rawLine, std::string& outPath) {
    const std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false;
    if (!StartsWith(line, "style")) return false;

    if (line.size() > 5 && !std::isspace(static_cast<unsigned char>(line[5]))) return false;

    const std::string rest = Trim(line.substr(5));
    std::size_t pos = 0;
    std::string path;
    if (!ConsumeQuoted(rest, pos, path)) return false;
    if (path.empty()) return false;

    if (!Trim(rest.substr(pos)).empty()) return false;

    outPath = path;
    return true;
}

bool IsRecognizedState(const std::string& stateLower) {
    return stateLower == "hover" || stateLower == "focus" ||
           stateLower == "active" || stateLower == "disabled";
}


std::string StripComment(const std::string& line) {
    const std::size_t hash = line.find('#');
    if (hash == std::string::npos) return line;
    return line.substr(0, hash);
}

std::string Unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool ParseNumber(const std::string& raw, double* out) {
    std::string value = Trim(raw);
    std::size_t end = value.size();
    while (end > 0) {
        const char c = value[end - 1];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+') {
            break;
        }
        --end;
    }
    value = value.substr(0, end);
    if (value.empty()) return false;
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) return false;
        *out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

void ApplyStyleProperty(const std::string& key, const std::string& rawValue,
                         ControlStyleOverride& out) {
    const std::string k = Lowercase(key);
    const std::string value = Unquote(Trim(rawValue));

    if (k == "backgroundcolor" || k == "background") {
        out.backgroundColor = value;
    } else if (k == "textcolor" || k == "color") {
        out.textColor = value;
    } else if (k == "bordercolor") {
        out.borderColor = value;
    } else if (k == "fontname" || k == "font") {
        out.fontName = value;
    } else if (k == "fontsize") {
        double num = 0.0;
        if (ParseNumber(value, &num)) out.fontSize = num;
    } else if (k == "borderwidth") {
        double num = 0.0;
        if (ParseNumber(value, &num)) out.borderWidth = num;
    } else if (k == "borderradius" || k == "radius") {
        double num = 0.0;
        if (ParseNumber(value, &num)) out.borderRadius = num;
    } else if (k == "padding") {
        double num = 0.0;
        if (ParseNumber(value, &num)) out.padding = num;
    } else if (k == "margin") {
        double num = 0.0;
        if (ParseNumber(value, &num)) out.margin = num;
    } else if (k == "spacing" || k == "gap") {
        double num = 0.0;
        if (ParseNumber(value, &num)) out.spacing = num;
    }
}

} // namespace

void ControlStyleOverride::MergeOnto(ControlStyleOverride& base) const {
    if (backgroundColor) base.backgroundColor = backgroundColor;
    if (textColor) base.textColor = textColor;
    if (borderColor) base.borderColor = borderColor;
    if (fontName) base.fontName = fontName;
    if (fontSize) base.fontSize = fontSize;
    if (borderWidth) base.borderWidth = borderWidth;
    if (borderRadius) base.borderRadius = borderRadius;
    if (padding) base.padding = padding;
    if (margin) base.margin = margin;
    if (spacing) base.spacing = spacing;
}

ControlStyleOverride ProjectStyleSheet::Resolve(const std::string& typeLower,
                                                 bool isPureLayoutContainer) const {
    ControlStyleOverride result;
    if (hasGlobal_) {
        if (isPureLayoutContainer) {
            // Layout-only wrappers don't have a visual box of their own, so a
            // global reset shouldn't hand them a margin or a background --
            // that's what produces a grey box that doesn't match the content
            // it wraps. Keep the rest of the global reset (fonts, text color,
            // padding, etc.), which is harmless on an invisible wrapper.
            ControlStyleOverride globalForLayout = global_;
            globalForLayout.margin.reset();
            globalForLayout.backgroundColor.reset();
            globalForLayout.MergeOnto(result);
        } else {
            global_.MergeOnto(result);
        }
    }
    const auto it = perType_.find(typeLower);
    if (it != perType_.end()) {
        // An explicit `style row` / `style column` / etc. block is the
        // author opting a specific type back in, so it always wins.
        it->second.MergeOnto(result);
    }
    return result;
}

ControlStyleOverride ProjectStyleSheet::ResolveState(const std::string& typeLower,
                                                      const std::string& state) const {
    ControlStyleOverride result;
    const auto globalIt = stateGlobal_.find(state);
    if (globalIt != stateGlobal_.end()) {
        globalIt->second.MergeOnto(result);
    }
    const auto it = statePerType_.find(typeLower + ":" + state);
    if (it != statePerType_.end()) {
        it->second.MergeOnto(result);
    }
    return result;
}

ControlStyleOverride ProjectStyleSheet::ResolveNamed(const std::string& name) const {
    const auto it = named_.find(Lowercase(name));
    if (it != named_.end()) {
        return it->second;
    }
    return ControlStyleOverride{};
}

bool ProjectStyleSheet::HasNamedStyle(const std::string& name) const {
    return named_.find(Lowercase(name)) != named_.end();
}

ControlStyleOverride ProjectStyleSheet::ResolveClasses(
    const std::string& typeLower, const std::vector<std::string>& classesLower) const {
    ControlStyleOverride result;
    for (const std::string& cls : classesLower) {
        const auto it = classPerType_.find(typeLower + "." + cls);
        if (it != classPerType_.end()) {
            it->second.MergeOnto(result);
        }
    }
    return result;
}

void MergeStyleFileInto(const std::string& styleFilePath, const std::string& projectRoot,
                         ProjectStyleSheet& sheet) {
    std::ifstream file(styleFilePath);
    if (!file) return;

    auto resolveFontPath = [&](std::optional<std::string>& fontName) {
        if (!fontName) return;
        const fs::path asPath(*fontName);
        if (asPath.has_extension()) {
            fs::path resolved = fs::path(projectRoot) / asPath;
            fontName = resolved.lexically_normal().string();
        }
    };

    std::string line;
    std::string currentTarget; 
    std::string currentState;  
    bool inBlock = false;
    bool blockValid = true; 
    bool currentIsNamed = false; 
    bool currentIsClassScoped = false; 
    std::string currentClass;
    ControlStyleOverride current;

    while (std::getline(file, line)) {
        const std::string stripped = Trim(StripComment(line));
        if (stripped.empty()) continue;

        if (!inBlock) {
            if (!StartsWith(stripped, "style")) continue;
            if (stripped.size() > 5 && !std::isspace(static_cast<unsigned char>(stripped[5]))) {
                continue;
            }
            const std::string target = Trim(stripped.substr(5));
            if (target.empty()) continue; 

            currentIsNamed = (target.front() == '"');
            if (currentIsNamed) {
                currentTarget = Lowercase(Unquote(target));
                currentState.clear();
                blockValid = !currentTarget.empty();
                current = ControlStyleOverride{};
                inBlock = true;
                continue;
            }

            std::string targetLower = Lowercase(target);
            currentState.clear();
            currentClass.clear();
            currentIsClassScoped = false;
            blockValid = true;
            const std::size_t colon = targetLower.find(':');
            if (colon != std::string::npos) {
                currentState = targetLower.substr(colon + 1);
                targetLower = targetLower.substr(0, colon);
                if (targetLower.empty() || !IsRecognizedState(currentState)) {
                    blockValid = false;
                }
            }

            const std::size_t dot = targetLower.find('.');
            if (dot != std::string::npos) {
                currentClass = targetLower.substr(dot + 1);
                targetLower = targetLower.substr(0, dot);
                if (targetLower.empty() || currentClass.empty()) {
                    blockValid = false;
                } else if (!currentState.empty()) {
                    blockValid = false;
                } else {
                    currentIsClassScoped = true;
                }
            }
            currentTarget = targetLower;
            current = ControlStyleOverride{};
            inBlock = true;
            continue;
        }

        if (Lowercase(stripped) == "end") {
            if (blockValid) {
                resolveFontPath(current.fontName);
                if (currentIsNamed) {
                    const auto it = sheet.named_.find(currentTarget);
                    if (it != sheet.named_.end()) {
                        current.MergeOnto(it->second);
                    } else {
                        sheet.named_[currentTarget] = current;
                    }
                } else if (currentIsClassScoped) {
                    const std::string key = currentTarget + "." + currentClass;
                    const auto it = sheet.classPerType_.find(key);
                    if (it != sheet.classPerType_.end()) {
                        current.MergeOnto(it->second);
                    } else {
                        sheet.classPerType_[key] = current;
                    }
                } else if (!currentState.empty()) {
                    auto& bucket = (currentTarget == "*")
                        ? sheet.stateGlobal_[currentState]
                        : sheet.statePerType_[currentTarget + ":" + currentState];
                    current.MergeOnto(bucket);
                } else if (currentTarget == "*") {
                    if (sheet.hasGlobal_) {
                        current.MergeOnto(sheet.global_);
                    } else {
                        sheet.global_ = current;
                    }
                    sheet.hasGlobal_ = true;
                } else {
                    const auto it = sheet.perType_.find(currentTarget);
                    if (it != sheet.perType_.end()) {
                        current.MergeOnto(it->second);
                    } else {
                        sheet.perType_[currentTarget] = current;
                    }
                }
            }
            inBlock = false;
            currentTarget.clear();
            currentState.clear();
            currentIsNamed = false;
            currentIsClassScoped = false;
            currentClass.clear();
            continue;
        }

        const std::size_t eq = stripped.find('=');
        if (eq == std::string::npos) continue; // not a property line, skip
        const std::string key = Trim(stripped.substr(0, eq));
        const std::string value = Trim(stripped.substr(eq + 1));
        if (key.empty() || value.empty()) continue;
        ApplyStyleProperty(key, value, current);
    }

}

ProjectStyleSheet LoadProjectStyleOverrides(const std::string& projectRoot) {
    ProjectStyleSheet sheet;
    if (projectRoot.empty()) {
        return sheet;
    }

    const fs::path appAvaPath = fs::path(projectRoot) / "app.ava";
    std::ifstream appAva(appAvaPath);
    if (!appAva) {
        return sheet;
    }

    std::string line;
    while (std::getline(appAva, line)) {
        std::string relativePath;
        if (!ParseStyleDeclarationLine(line, relativePath)) continue;
        fs::path resolved = fs::path(projectRoot) / relativePath;
        MergeStyleFileInto(resolved.lexically_normal().string(), projectRoot, sheet);
    }

    return sheet;
}

} // namespace avalang::ui::theme
