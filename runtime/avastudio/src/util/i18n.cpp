#include "util/i18n.h"

#include <filesystem>
#include <unordered_map>
#include <utility>

#include "util/csv.h"
#include "util/data_dir.h"

namespace studio::util {

namespace {
namespace fs = std::filesystem;

Locale g_active_locale = Locale::English;

struct LocaleTable {
    std::unordered_map<std::string, std::string> entries;
    fs::file_time_type last_loaded_time{};
    bool loaded_from_csv = false;
    bool first_call = true;
};

bool LoadLocaleCsv(const std::string& path, std::unordered_map<std::string, std::string>& out) {
    std::string text;
    if (!ReadFileToString(path, text)) return false;

    auto rows = ParseCsv(text);
    if (rows.empty()) return false;

    std::unordered_map<std::string, std::string> parsed;

    for (size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (row.size() == 1 && row[0].empty()) continue;
        if (row.size() < 2) continue;

        const std::string& key = row[0];
        if (key.empty()) continue;
        parsed[key] = UnescapeCell(row[1]);
    }

    if (parsed.empty()) return false;
    out = std::move(parsed);
    return true;
}

const char* CsvFileNameFor(Locale locale) {
    switch (locale) {
        case Locale::English: return "langs/en.csv";
        case Locale::Spanish: return "langs/es.csv";
    }
    return "langs/en.csv";
}

LocaleTable& TableFor(Locale locale) {
    static LocaleTable english;
    static LocaleTable spanish;
    return locale == Locale::English ? english : spanish;
}

void EnsureLoaded(Locale locale) {
    LocaleTable& table = TableFor(locale);
    const std::string csv_path = ResolveDataDir() + CsvFileNameFor(locale);

    std::error_code ec;
    fs::file_time_type current_time = fs::last_write_time(csv_path, ec);
    bool csv_exists = !ec;

    bool should_reload = table.first_call || (csv_exists && current_time != table.last_loaded_time);
    if (!should_reload) return;

    table.first_call = false;
    if (csv_exists && LoadLocaleCsv(csv_path, table.entries)) {
        table.loaded_from_csv = true;
        table.last_loaded_time = current_time;
    } else if (!table.loaded_from_csv) {

        table.entries.clear();
    }
}

}

Locale LocaleFromString(const std::string& value, Locale fallback_locale) {
    if (value == "en") return Locale::English;
    if (value == "es") return Locale::Spanish;
    return fallback_locale;
}

std::string LocaleToString(Locale locale) {
    switch (locale) {
        case Locale::English: return "en";
        case Locale::Spanish: return "es";
    }
    return "en";
}

void SetLocale(Locale locale) { g_active_locale = locale; }
Locale GetLocale() { return g_active_locale; }

const std::string& Tr(const std::string& key) {
    EnsureLoaded(g_active_locale);
    const LocaleTable& active = TableFor(g_active_locale);
    if (auto it = active.entries.find(key); it != active.entries.end()) {
        return it->second;
    }

    if (g_active_locale != Locale::English) {
        EnsureLoaded(Locale::English);
        const LocaleTable& english = TableFor(Locale::English);
        if (auto it = english.entries.find(key); it != english.entries.end()) {
            return it->second;
        }
    }

    static std::unordered_map<std::string, std::string> missing_key_cache;
    auto [it, inserted] = missing_key_cache.try_emplace(key, "[" + key + "]");
    return it->second;
}

}
