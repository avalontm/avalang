#include "design/component_catalog.h"

#include <filesystem>

#include "util/csv.h"
#include "util/data_dir.h"

namespace studio::design {

namespace {

namespace fs = std::filesystem;

// Fallback used when data/component_catalog.csv is missing or fails to
// parse on the very first load -- same content the CSV ships with, kept
// embedded so Ava Studio never loses the Toolbox entirely (see
// LoadComponentCatalogFromCsv/GetComponentCatalog below, same pattern as
// DefaultKeywordDocs()/KeywordDocs() in languages/keyword_docs.cpp).
const std::vector<ComponentTypeInfo>& DefaultComponentCatalog() {
    static const std::vector<ComponentTypeInfo> catalog = {
        // --- Layout (containers) --------------------------------------
        {"page", "Page", {{"title", "Untitled"}}, /*is_container=*/true},
        {"column", "Column", {}, /*is_container=*/true},
        {"row", "Row", {}, /*is_container=*/true},
        {"stack", "Stack", {}, /*is_container=*/true},
        {"grid", "Grid", {{"columns", "2"}, {"rows", "2"}}, /*is_container=*/true},
        {"flex", "Flex", {}, /*is_container=*/true},

        // --- Content -----------------------------------------------------
        {"text", "Text", {{"value", "Text"}}, /*is_container=*/false},
        {"image", "Image", {{"src", ""}}, /*is_container=*/false},
        {"spacer", "Spacer", {}, /*is_container=*/false},
        {"divider", "Divider", {}, /*is_container=*/false},
        {"link", "Link", {{"value", "Link"}, {"href", ""}}, /*is_container=*/false},

        // --- Interactive ---------------------------------------------------
        // `value` is the one property name every essential/content-bearing
        // component uses for "the text it shows" (see GetDisplayPropertyKey
        // in state_eval.cpp).
        {"button", "Button", {{"value", "Button"}, {"enabled", "true"}}, /*is_container=*/false},
        {"textbox", "TextBox", {{"placeholder", ""}, {"value", ""}}, /*is_container=*/false},
        {"checkbox", "CheckBox", {{"value", "CheckBox"}, {"checked", "false"}}, /*is_container=*/false},
        {"radiobutton", "RadioButton", {{"value", "RadioButton"}, {"checked", "false"}}, /*is_container=*/false},
    };
    return catalog;
}

// data/component_catalog.csv columns: type,display_name,is_container,properties
//  - type: matches AvaComponent's type string, e.g. "button".
//  - display_name: Toolbox label, e.g. "Button".
//  - is_container: "true" or "false".
//  - properties: zero or more "key=default" pairs joined by "|" (a single
//    pipe -- same convention builtin_signatures.csv uses for a param
//    list), e.g. "value=Button|enabled=true". An empty cell means no
//    default properties (containers like column/row/stack/flex). Default
//    values for today's catalog are plain words/booleans/empty strings,
//    so no escaping beyond CSV's own quoting is implemented for this
//    column -- a default value containing "|" or "=" isn't supported.
bool LoadComponentCatalogFromCsv(const std::string& path, std::vector<ComponentTypeInfo>& out) {
    std::string text;
    if (!util::ReadFileToString(path, text)) return false;

    auto rows = util::ParseCsv(text);
    if (rows.empty()) return false;

    std::vector<ComponentTypeInfo> parsed;
    // rows[0] is the header (type,display_name,is_container,properties) -- skip it.
    for (size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (row.size() == 1 && row[0].empty()) continue; // blank line
        if (row.size() < 4) continue; // malformed row -- skip rather than crash on it

        ComponentTypeInfo info;
        info.type = row[0];
        if (info.type.empty()) continue;
        info.display_name = row[1];
        info.is_container = (row[2] == "true");

        for (const std::string& pair : util::SplitOn(row[3], "|")) {
            if (pair.empty()) continue;
            const size_t eq = pair.find('=');
            if (eq == std::string::npos) continue; // malformed pair -- skip it, not the whole row
            info.default_properties.push_back({pair.substr(0, eq), pair.substr(eq + 1)});
        }
        parsed.push_back(std::move(info));
    }

    if (parsed.empty()) return false;
    out = std::move(parsed);
    return true;
}

} // namespace

const std::vector<ComponentTypeInfo>& GetComponentCatalog() {
    static std::vector<ComponentTypeInfo> catalog;
    static fs::file_time_type last_loaded_time{};
    static bool loaded_from_csv = false;
    static bool first_call = true;

    const std::string csv_path = util::ResolveDataDir() + "component_catalog.csv";
    std::error_code ec;
    fs::file_time_type current_time = fs::last_write_time(csv_path, ec);
    bool csv_exists = !ec;

    // Reload when: first call ever, or the CSV exists and its mtime moved
    // forward since the last successful load (someone edited it while Ava
    // Studio was open) -- same "hot reload, cheap mtime check" pattern as
    // KeywordDocs()/BuiltinSignatures().
    bool should_reload = first_call || (csv_exists && current_time != last_loaded_time);

    if (should_reload) {
        first_call = false;
        if (csv_exists && LoadComponentCatalogFromCsv(csv_path, catalog)) {
            loaded_from_csv = true;
            last_loaded_time = current_time;
        } else if (!loaded_from_csv) {
            // Only fall back if we've never had a good CSV load -- a CSV
            // that briefly fails to parse mid-edit shouldn't blow away a
            // working catalog that's already loaded and in use.
            catalog = DefaultComponentCatalog();
        }
    }

    return catalog;
}

const ComponentTypeInfo* FindComponentType(const std::string& type) {
    for (const auto& info : GetComponentCatalog()) {
        if (info.type == type) return &info;
    }
    return nullptr;
}

} // namespace studio::design
