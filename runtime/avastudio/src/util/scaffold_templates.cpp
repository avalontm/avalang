#include "util/scaffold_templates.h"

#include <cctype>
#include <unordered_map>

#include "util/csv.h"
#include "util/data_dir.h"

namespace studio::util {

namespace {

const char* KindKey(ScaffoldKind kind) {
    switch (kind) {
        case ScaffoldKind::kClass: return "class";
        case ScaffoldKind::kScreen: return "screen";
    }
    return "class";
}

// Same load-once-from-CSV shape as component_catalog.cpp's
// LoadCatalogMetadata: read data/scaffold/file_templates.csv (columns
// kind,extension,content), key it by `kind`. Cached in a function-local
// static, same "loaded once at first use" reasoning the catalog and i18n
// tables already follow -- this data doesn't change while the app runs.
struct ScaffoldTemplate {
    std::string extension;
    std::string content;
};

const std::unordered_map<std::string, ScaffoldTemplate>& LoadTemplates() {
    static const std::unordered_map<std::string, ScaffoldTemplate> templates = [] {
        std::unordered_map<std::string, ScaffoldTemplate> loaded;
        std::string text;
        if (!ReadFileToString(ResolveDataDir() + "scaffold/file_templates.csv", text)) {
            return loaded;
        }
        auto rows = ParseCsv(text);
        for (size_t r = 1; r < rows.size(); ++r) {
            const auto& row = rows[r];
            if (row.size() < 3) continue;
            if (row[0].empty()) continue;
            loaded[row[0]] = ScaffoldTemplate{row[1], row[2]};
        }
        return loaded;
    }();
    return templates;
}

// Valid AvaLang identifier: [A-Za-z_][A-Za-z0-9_]* (same charset the
// language's own examples use for class names). Anything else in
// `base_name` becomes '_'; a leading digit gets a '_' prefix so the result
// stays a legal identifier instead of merely a legal filename.
std::string SanitizeIdentifier(const std::string& base_name) {
    std::string out;
    out.reserve(base_name.size());
    for (unsigned char c : base_name) {
        if (std::isalnum(c) || c == '_') {
            out += static_cast<char>(c);
        } else {
            out += '_';
        }
    }
    if (!out.empty() && std::isdigit(static_cast<unsigned char>(out[0]))) {
        out = "_" + out;
    }
    if (out.empty()) {
        out = "NewClass";
    }
    return out;
}

// Only '"' and '\' need escaping to sit safely inside a quoted .avaui
// string property -- the format doesn't have any other special escape
// sequences in property values (see AvauiParser.cpp's Unquote/property
// handling), so this deliberately doesn't try to mirror C-style \n/\t.
std::string EscapeAvauiString(const std::string& base_name) {
    std::string out;
    out.reserve(base_name.size());
    for (char c : base_name) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

std::string ReplaceAll(std::string text, const std::string& token, const std::string& value) {
    size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        text.replace(pos, token.size(), value);
        pos += value.size();
    }
    return text;
}

}

std::string ScaffoldExtension(ScaffoldKind kind) {
    const auto& templates = LoadTemplates();
    auto it = templates.find(KindKey(kind));
    if (it == templates.end()) {
        return kind == ScaffoldKind::kScreen ? ".avaui" : ".ava";
    }
    return it->second.extension;
}

std::string BuildScaffoldContent(ScaffoldKind kind, const std::string& base_name) {
    const auto& templates = LoadTemplates();
    auto it = templates.find(KindKey(kind));
    if (it == templates.end()) return "";

    std::string content = it->second.content;
    content = ReplaceAll(std::move(content), "{ClassName}", SanitizeIdentifier(base_name));
    content = ReplaceAll(std::move(content), "{DisplayName}", EscapeAvauiString(base_name));
    return content;
}

}
