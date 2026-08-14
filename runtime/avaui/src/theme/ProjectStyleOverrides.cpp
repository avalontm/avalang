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

// Consumes one double-quoted string starting at `pos` (which must
// point at the opening `"`). Same convention as ProjectFontOverrides.
// cpp's ConsumeQuoted -- app.ava paths never need escape sequences.
bool ConsumeQuoted(const std::string& line, std::size_t& pos, std::string& out) {
    if (pos >= line.size() || line[pos] != '"') return false;
    const std::size_t closing = line.find('"', pos + 1);
    if (closing == std::string::npos) return false;
    out = line.substr(pos + 1, closing - pos - 1);
    pos = closing + 1;
    return true;
}

// Parses one `style "path/to/file.ava"` line from app.ava -- the
// manifest declaration that tells LoadProjectStyleOverrides which
// file(s) to load (see the syntax table in ProjectStyleOverrides.h).
// This is a different grammar from the `style <target> ... end`
// blocks inside the declared file itself: app.ava's `style` line
// takes exactly one quoted path, same shape as app.ava's `font
// "path"` default-font line. Returns false for anything else (blank,
// `#` comment, `import`/`font` lines, malformed `style` line) --
// always a silent skip, never an error, same tolerance
// ProjectFontOverrides.cpp's ParseFontLine documents.
bool ParseStyleDeclarationLine(const std::string& rawLine, std::string& outPath) {
    const std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false;
    if (!StartsWith(line, "style")) return false;
    // Require a word boundary after "style" (not e.g. "styleguide ...").
    if (line.size() > 5 && !std::isspace(static_cast<unsigned char>(line[5]))) return false;

    const std::string rest = Trim(line.substr(5));
    std::size_t pos = 0;
    std::string path;
    if (!ConsumeQuoted(rest, pos, path)) return false;
    if (path.empty()) return false;
    // Nothing but the closing quote should remain (one path per line).
    if (!Trim(rest.substr(pos)).empty()) return false;

    outPath = path;
    return true;
}

// The interactive states a `style <target>:<state>` block can name
// (see the syntax table in ProjectStyleOverrides.h) -- deliberately a
// small, fixed set rather than accepting any CSS pseudo-class name:
// every recognized state must have a caller that actually knows how
// to render it (today, HTMLRenderer::EmitProjectStateCSS), and a
// state nothing renders would silently do nothing, which is worse
// than a typo being caught by "unrecognized state, block skipped".
bool IsRecognizedState(const std::string& stateLower) {
    return stateLower == "hover" || stateLower == "focus" ||
           stateLower == "active" || stateLower == "disabled";
}

// Strips a trailing `# comment` from a line, same convention
// app_manifest.cpp / ProjectFontOverrides.cpp use for whole-line
// comments -- styles.ava additionally allows a comment after a real
// statement on the same line (`fontSize = 14  # matches Figma`),
// which those single-purpose line parsers never needed to.
std::string StripComment(const std::string& line) {
    const std::size_t hash = line.find('#');
    if (hash == std::string::npos) return line;
    return line.substr(0, hash);
}

// Unquotes a "..."-wrapped value if present, otherwise returns the
// (trimmed) value verbatim -- styles.ava allows bare tokens for
// colors/paths (`backgroundColor = 0078D4`) as well as quoted ones
// (`backgroundColor = "0078D4"`), since component properties in
// .avaui itself are always quoted strings but a project author typing
// a hex color by hand will often forget the quotes; being tolerant
// here costs nothing.
std::string Unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

// Parses a numeric value, tolerating a trailing unit suffix like "px"
// (`padding = 12px`) the way a CSS-familiar author might type one --
// styles.ava has no other unit system, so the suffix is simply
// ignored rather than rejected.
bool ParseNumber(const std::string& raw, double* out) {
    std::string value = Trim(raw);
    // Strip a trailing non-numeric unit suffix (e.g. "px", "pt").
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

// Applies one `key = value` line to `out`. Unrecognized keys are
// silently ignored (see the tolerance rationale in the header) --
// returns nothing because a skipped key is not a parse failure.
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
    // Anything else: unrecognized key, skip (see header doc comment).
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

ControlStyleOverride ProjectStyleSheet::Resolve(const std::string& typeLower) const {
    ControlStyleOverride result;
    if (hasGlobal_) {
        global_.MergeOnto(result);
    }
    const auto it = perType_.find(typeLower);
    if (it != perType_.end()) {
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

// Parses one already-resolved style file's `style <target> ... end`
// blocks and folds them into `sheet`. Called once per `style "..."`
// line declared in app.ava, in declaration order -- a target this
// file sets that `sheet` already has (from an earlier declared file)
// is overlaid field-by-field via MergeOnto, so a later file can
// override just the fields it cares about without clobbering the
// rest of an earlier file's block for that same target. Missing file:
// silently a no-op, same tolerant-skip stance as everything else here.
void MergeStyleFileInto(const std::string& styleFilePath, const std::string& projectRoot,
                         ProjectStyleSheet& sheet) {
    std::ifstream file(styleFilePath);
    if (!file) return;

    // A project-relative fontName ("assets/fonts/Foo.ttf") is
    // resolved to an absolute path here, once, same as
    // ProjectFontOverrides does for `font` lines -- everything
    // downstream (ProjectTheme/FontRegistry) just opens the path
    // directly. A bare family name with no path separator or
    // extension (e.g. "Segoe UI") is left as-is: it's a display
    // label, not a file to resolve (see ThemeFont::filePath's doc
    // comment in ITheme.h for the same distinction).
    auto resolveFontPath = [&](std::optional<std::string>& fontName) {
        if (!fontName) return;
        const fs::path asPath(*fontName);
        if (asPath.has_extension()) {
            fs::path resolved = fs::path(projectRoot) / asPath;
            fontName = resolved.lexically_normal().string();
        }
    };

    std::string line;
    std::string currentTarget; // empty => not inside a `style` block
    std::string currentState;  // empty => `style <target>` (normal state)
    bool inBlock = false;
    bool blockValid = true; // false => `:state` suffix unrecognized, drop at `end`
    bool currentIsNamed = false; // true => `style "name"`, see header comment
    bool currentIsClassScoped = false; // true => `style type.class`, see header comment
    std::string currentClass; // set only when currentIsClassScoped
    ControlStyleOverride current;

    while (std::getline(file, line)) {
        const std::string stripped = Trim(StripComment(line));
        if (stripped.empty()) continue;

        if (!inBlock) {
            if (!StartsWith(stripped, "style")) continue;
            // Require a word boundary after "style" (not e.g.
            // "styleguide ...").
            if (stripped.size() > 5 && !std::isspace(static_cast<unsigned char>(stripped[5]))) {
                continue;
            }
            const std::string target = Trim(stripped.substr(5));
            if (target.empty()) continue; // malformed `style` line, skip

            // A quoted target (`style "my_button"`) declares a
            // standalone NAMED style instead of a control-type
            // default -- see the header comment's "style \"name\""
            // section. Never merged with `style *`/`style <type>`,
            // and no `:<state>` suffix is recognized on it.
            currentIsNamed = (target.front() == '"');
            if (currentIsNamed) {
                currentTarget = Lowercase(Unquote(target));
                currentState.clear();
                blockValid = !currentTarget.empty(); // empty name (`style ""`) -- drop at `end`
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
                    // Malformed target or unrecognized state (e.g. a
                    // typo'd "hovar") -- still track the block so its
                    // `end` doesn't get misread as a stray top-level
                    // line, just don't apply anything it sets.
                    blockValid = false;
                }
            }
            // `type.class` (e.g. "button.primary") -- see the header
            // comment's `style <type>.<class>` section. Checked after
            // the state suffix above so `button.primary:hover` splits
            // into state="hover", then type.class="button.primary".
            const std::size_t dot = targetLower.find('.');
            if (dot != std::string::npos) {
                currentClass = targetLower.substr(dot + 1);
                targetLower = targetLower.substr(0, dot);
                if (targetLower.empty() || currentClass.empty()) {
                    blockValid = false;
                } else if (!currentState.empty()) {
                    // A class-scoped state block (`style
                    // type.class:state`) parses without erroring, but
                    // isn't wired to the renderer yet -- see the
                    // header comment -- so it's dropped here rather
                    // than stored somewhere nothing ever reads.
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

        // Inside a block: `end` closes it, anything else is a
        // `key = value` property line (unrecognized keys ignored).
        if (Lowercase(stripped) == "end") {
            if (blockValid) {
                resolveFontPath(current.fontName);
                if (currentIsNamed) {
                    // `style "name"` -- kept in its own map, never
                    // merged with `style *`/`style <type>`; see
                    // ResolveNamed(). Same later-file-wins,
                    // field-by-field overlay as perType_ below.
                    const auto it = sheet.named_.find(currentTarget);
                    if (it != sheet.named_.end()) {
                        current.MergeOnto(it->second);
                    } else {
                        sheet.named_[currentTarget] = current;
                    }
                } else if (currentIsClassScoped) {
                    // `style type.class` -- see ResolveClasses().
                    // Same later-file-wins overlay as perType_.
                    const std::string key = currentTarget + "." + currentClass;
                    const auto it = sheet.classPerType_.find(key);
                    if (it != sheet.classPerType_.end()) {
                        current.MergeOnto(it->second);
                    } else {
                        sheet.classPerType_[key] = current;
                    }
                } else if (!currentState.empty()) {
                    // `style <target>:<state>` -- kept separate from
                    // perType_/global_ above; see ResolveState().
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

    // An unterminated final block (no `end` before EOF) is dropped --
    // same tolerant-skip stance as every other malformed line here,
    // rather than silently applying a half-written style.
}

ProjectStyleSheet LoadProjectStyleOverrides(const std::string& projectRoot) {
    ProjectStyleSheet sheet;
    if (projectRoot.empty()) {
        return sheet;
    }

    // app.ava's `style "path"` lines are the only way a project's
    // style file(s) get loaded -- there is no implicit "styles.ava"
    // fallback (see the header doc comment). Same reader shape as
    // ProjectFontOverrides.cpp's LoadProjectFontOverrides: read
    // app.ava once, collect every recognized declaration in order.
    const fs::path appAvaPath = fs::path(projectRoot) / "app.ava";
    std::ifstream appAva(appAvaPath);
    if (!appAva) {
        return sheet; // No app.ava -- not an error, just no overrides.
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
