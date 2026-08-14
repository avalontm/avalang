#include "theme/ProjectAnimationOverrides.h"

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

// Same convention as ProjectStyleOverrides.cpp's ConsumeQuoted /
// ProjectFontOverrides.cpp's -- app.ava paths never need escape
// sequences.
bool ConsumeQuoted(const std::string& line, std::size_t& pos, std::string& out) {
    if (pos >= line.size() || line[pos] != '"') return false;
    const std::size_t closing = line.find('"', pos + 1);
    if (closing == std::string::npos) return false;
    out = line.substr(pos + 1, closing - pos - 1);
    pos = closing + 1;
    return true;
}

// Parses one `animation "path/to/file.ava"` line from app.ava -- the
// manifest declaration that tells LoadProjectAnimationOverrides which
// file(s) to load (see the syntax table in ProjectAnimationOverrides.h).
// Same shape as ProjectStyleOverrides.cpp's ParseStyleDeclarationLine,
// just matching the `animation` keyword instead of `style`. Silent
// skip (never an error) for anything else (blank, `#` comment,
// `import`/`font`/`style` lines, malformed `animation` line).
bool ParseAnimationDeclarationLine(const std::string& rawLine, std::string& outPath) {
    const std::string line = Trim(rawLine);
    if (line.empty() || StartsWith(line, "#")) return false;
    if (!StartsWith(line, "animation")) return false;
    // Require a word boundary after "animation" (not e.g.
    // "animationSpeed ...").
    if (line.size() > 9 && !std::isspace(static_cast<unsigned char>(line[9]))) return false;

    const std::string rest = Trim(line.substr(9));
    std::size_t pos = 0;
    std::string path;
    if (!ConsumeQuoted(rest, pos, path)) return false;
    if (path.empty()) return false;
    // Nothing but the closing quote should remain (one path per line).
    if (!Trim(rest.substr(pos)).empty()) return false;

    outPath = path;
    return true;
}

// Same whole-line/trailing `# comment` stripping ProjectStyleOverrides.
// cpp's StripComment documents.
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

// Parses `from`/`to`'s opacity value: a bare 0-1 number ("0", "1",
// "0.5") or a percentage ("0%", "100%") -- both normalize to 0-1, the
// unit CSS `opacity` actually takes. Tolerates the same trailing-unit
// pattern ProjectStyleOverrides.cpp's ParseNumber does for "px", just
// dividing by 100 when the value was written as a percentage so a
// project author who thinks in percentages (closer to how most CSS
// animation tutorials phrase opacity keyframes) doesn't have to
// convert by hand.
bool ParseOpacity(const std::string& raw, double* out) {
    std::string value = Trim(raw);
    if (value.empty()) return false;
    const bool isPercent = value.back() == '%';
    if (isPercent) value.pop_back();
    value = Trim(value);
    if (value.empty()) return false;
    try {
        std::size_t consumed = 0;
        double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) return false;
        if (isPercent) parsed /= 100.0;
        *out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

// Applies one `key = value` line to `out`. Unrecognized keys are
// silently ignored, same tolerance ApplyStyleProperty documents.
void ApplyAnimationProperty(const std::string& key, const std::string& rawValue,
                             AnimationOverride& out) {
    const std::string k = Lowercase(key);
    const std::string value = Unquote(Trim(rawValue));

    if (k == "from") {
        double num = 0.0;
        if (ParseOpacity(value, &num)) out.from = num;
    } else if (k == "to") {
        double num = 0.0;
        if (ParseOpacity(value, &num)) out.to = num;
    } else if (k == "duration") {
        if (!value.empty()) out.duration = value;
    } else if (k == "easing") {
        if (!value.empty()) out.easing = value;
    }
    // `property`, `trigger`, `mode`: recognized by the in-.avaui
    // `animate ... end` block (AvauiParser.cpp's AnimationSpec) but
    // not by this file format -- see ProjectAnimationOverrides.h for
    // why `property` is implied by the target instead of a free-form
    // field here. Silently ignored if present, same as any other
    // unrecognized key.
}

} // namespace

void AnimationOverride::MergeOnto(AnimationOverride& base) const {
    if (from) base.from = from;
    if (to) base.to = to;
    if (duration) base.duration = duration;
    if (easing) base.easing = easing;
}

AnimationOverride ProjectAnimationSheet::Resolve(const std::string& typeLower,
                                                  const std::string& trigger) const {
    const auto it = targets_.find(typeLower + ":" + trigger);
    if (it != targets_.end()) {
        return it->second;
    }
    return AnimationOverride{};
}

// Parses one already-resolved animation file's
// `animation <target>:<trigger> ... end` blocks and folds them into
// `sheet`. Called once per `animation "..."` line declared in app.ava,
// in declaration order -- a target:trigger this file sets that
// `sheet` already has (from an earlier declared file) is overlaid
// field-by-field via MergeOnto, same later-file-wins rule
// ProjectStyleOverrides.cpp's MergeStyleFileInto documents. Missing
// file: silently a no-op.
void MergeAnimationFileInto(const std::string& animationFilePath, ProjectAnimationSheet& sheet) {
    std::ifstream file(animationFilePath);
    if (!file) return;

    std::string line;
    std::string currentKey; // "<target>:<trigger>", empty => not inside a block
    bool inBlock = false;
    bool blockValid = true; // false => malformed target/trigger, drop at `end`
    AnimationOverride current;

    while (std::getline(file, line)) {
        const std::string stripped = Trim(StripComment(line));
        if (stripped.empty()) continue;

        if (!inBlock) {
            if (!StartsWith(stripped, "animation")) continue;
            // Require a word boundary after "animation" (not e.g.
            // "animationSpeed ...").
            if (stripped.size() > 9 &&
                !std::isspace(static_cast<unsigned char>(stripped[9]))) {
                continue;
            }
            const std::string target = Trim(stripped.substr(9));
            if (target.empty()) continue; // malformed `animation` line, skip

            std::string targetLower = Lowercase(target);
            const std::size_t colon = targetLower.find(':');
            if (colon == std::string::npos) {
                // No `:<trigger>` suffix -- an animation is meaningless
                // without one (see the header comment), so this block
                // parses (its `end` is still consumed, same as an
                // unrecognized `style ...:<state>` block) but nothing
                // it sets is ever stored.
                currentKey.clear();
                blockValid = false;
            } else {
                const std::string typePart = targetLower.substr(0, colon);
                const std::string triggerPart = targetLower.substr(colon + 1);
                if (typePart.empty() || triggerPart.empty()) {
                    currentKey.clear();
                    blockValid = false;
                } else {
                    currentKey = typePart + ":" + triggerPart;
                    blockValid = true;
                }
            }
            current = AnimationOverride{};
            inBlock = true;
            continue;
        }

        // Inside a block: `end` closes it, anything else is a
        // `key = value` property line (unrecognized keys ignored).
        if (Lowercase(stripped) == "end") {
            if (blockValid) {
                const auto it = sheet.targets_.find(currentKey);
                if (it != sheet.targets_.end()) {
                    current.MergeOnto(it->second);
                } else {
                    sheet.targets_[currentKey] = current;
                }
            }
            inBlock = false;
            currentKey.clear();
            continue;
        }

        const std::size_t eq = stripped.find('=');
        if (eq == std::string::npos) continue; // not a property line, skip
        const std::string key = Trim(stripped.substr(0, eq));
        const std::string value = Trim(stripped.substr(eq + 1));
        if (key.empty() || value.empty()) continue;
        ApplyAnimationProperty(key, value, current);
    }

    // An unterminated final block (no `end` before EOF) is dropped --
    // same tolerant-skip stance MergeStyleFileInto documents.
}

ProjectAnimationSheet LoadProjectAnimationOverrides(const std::string& projectRoot) {
    ProjectAnimationSheet sheet;
    if (projectRoot.empty()) {
        return sheet;
    }

    // app.ava's `animation "path"` lines are the only way a project's
    // animation file(s) get loaded -- there is no implicit
    // "animations.ava" fallback, same stance LoadProjectStyleOverrides
    // takes for `style "..."`.
    const fs::path appAvaPath = fs::path(projectRoot) / "app.ava";
    std::ifstream appAva(appAvaPath);
    if (!appAva) {
        return sheet; // No app.ava -- not an error, just no overrides.
    }

    std::string line;
    while (std::getline(appAva, line)) {
        std::string relativePath;
        if (!ParseAnimationDeclarationLine(line, relativePath)) continue;
        fs::path resolved = fs::path(projectRoot) / relativePath;
        MergeAnimationFileInto(resolved.lexically_normal().string(), sheet);
    }

    return sheet;
}

} // namespace avalang::ui::theme
