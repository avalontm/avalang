#include "avalang_reference.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

// --- locating the CSV files -------------------------------------------
// Mirrors util/data_dir.h's ResolveDataDir()/ReadFileToString() exactly
// (data/ lives next to ava_studio.exe, not the plugin DLL and not the
// process cwd) -- duplicated on purpose instead of linked, since this
// plugin builds as its own standalone SHARED library that otherwise
// doesn't depend on the rest of Ava Studio's source tree (see this
// module's header comment and the plugin's CMakeLists.txt). GetModuleFileNameA(nullptr, ...)
// still resolves to the *main process's* exe path (ava_studio.exe) even
// called from inside a loaded plugin DLL, which is exactly what's
// wanted here.
std::string ResolveDataDir() {
    fs::path base = fs::current_path();
#if defined(_WIN32)
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        base = fs::path(exe_path).parent_path();
    }
#endif
    fs::path data = base / "data";
    std::string result = data.string();
    if (result.empty() || (result.back() != '/' && result.back() != '\\')) result += "/";
    return result;
}

bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

// --- minimal RFC4180 CSV parser ----------------------------------------
// Same two conventions core's util/csv.h documents (kept in sync by
// hand, scoped to this plugin -- see this module's header comment):
// fields quoted with "" escaping an internal quote, commas/real
// newlines allowed inside a quoted field, and a literal "\n"
// (backslash + n, two characters) inside a cell standing in for a real
// line break in the text the model/tooltip actually shows.
std::vector<std::vector<std::string>> ParseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool in_quotes = false;
    bool row_has_content = false;

    auto end_field = [&]() {
        row.push_back(field);
        field.clear();
    };
    auto end_row = [&]() {
        end_field();
        rows.push_back(row);
        row.clear();
        row_has_content = false;
    };

    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    field += '"';
                    i += 2;
                    continue;
                }
                in_quotes = false;
                ++i;
                continue;
            }
            field += c;
            ++i;
            continue;
        }
        if (c == '"') {
            in_quotes = true;
            row_has_content = true;
            ++i;
            continue;
        }
        if (c == ',') {
            end_field();
            row_has_content = true;
            ++i;
            continue;
        }
        if (c == '\r') {
            ++i;
            continue;
        }
        if (c == '\n') {
            end_row();
            ++i;
            continue;
        }
        field += c;
        row_has_content = true;
        ++i;
    }
    if (row_has_content || !field.empty() || !row.empty()) end_row();
    return rows;
}

std::string UnescapeCell(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == 'n') {
            out += '\n';
            ++i;
        } else {
            out += raw[i];
        }
    }
    return out;
}

std::vector<std::string> SplitOn(const std::string& text, const std::string& sep) {
    std::vector<std::string> parts;
    if (sep.empty()) {
        parts.push_back(text);
        return parts;
    }
    size_t start = 0;
    while (true) {
        size_t pos = text.find(sep, start);
        if (pos == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + sep.size();
    }
    return parts;
}

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

// Just the first sentence of a doc string, so the always-injected cheat
// sheet stays to one short line per entry -- the full doc (and the full
// worked example) is one avalang_syntax_lookup call away.
std::string FirstSentence(const std::string& doc) {
    size_t cut = doc.find(". ");
    if (cut != std::string::npos) return doc.substr(0, cut + 1);
    return doc;
}

struct KeywordRow {
    std::string name;
    std::vector<std::string> syntax; // one or more variants, already unescaped
    std::string example;
    std::string doc;
};

struct BuiltinRow {
    std::string name;
    std::string params; // "|"-joined parameter names, or "*values" style, raw from the CSV
    std::string doc;
};

void LoadKeywordDocsFromCsv(const std::string& path, std::unordered_map<std::string, KeywordRow>* out) {
    std::string text;
    if (!ReadFileToString(path, text)) return;
    for (const auto& fields : ParseCsv(text)) {
        if (fields.empty() || fields[0].empty() || fields[0] == "name") continue; // blank/header row
        if (fields.size() < 4) continue;
        KeywordRow entry;
        entry.name = fields[0];
        for (const auto& variant : SplitOn(fields[1], "|||")) entry.syntax.push_back(UnescapeCell(variant));
        entry.example = UnescapeCell(fields[2]);
        entry.doc = UnescapeCell(fields[3]);
        (*out)[ToLower(entry.name)] = std::move(entry);
    }
}

void LoadBuiltinSignaturesFromCsv(const std::string& path, std::unordered_map<std::string, BuiltinRow>* out) {
    std::string text;
    if (!ReadFileToString(path, text)) return;
    for (const auto& fields : ParseCsv(text)) {
        if (fields.empty() || fields[0].empty() || fields[0] == "name") continue; // blank/header row
        if (fields.size() < 3) continue;
        BuiltinRow entry;
        entry.name = fields[0];
        entry.params = fields[1];
        entry.doc = UnescapeCell(fields[2]);
        (*out)[ToLower(entry.name)] = std::move(entry);
    }
}

// Small hand-picked table, used only if data/keyword_docs.csv can't be
// found/parsed AND we've never had a good load of it this session --
// same reasoning as the editor's DefaultKeywordDocs()/
// DefaultBuiltinSignatures(): a missing or briefly-corrupted CSV
// shouldn't leave the agent with zero signal about AvaLang's syntax,
// even if this fallback is far from exhaustive.
const std::unordered_map<std::string, KeywordRow>& FallbackKeywords() {
    static const std::unordered_map<std::string, KeywordRow> table = {
        {"if", {"if", {"if condition then\n    ...\nelif other_condition then\n    ...\nelse\n    ...\nend"}, "",
                "Runs the first block whose condition is true."}},
        {"for", {"for", {"for item in iterable then\n    ...\nend"}, "", "Iterates item over iterable."}},
        {"while", {"while", {"while condition\n    ...\nend"}, "", "Repeats the block while condition stays true."}},
        {"func", {"func", {"func name(params)\n    ...\nend"}, "", "Declares a named function."}},
        {"class", {"class", {"class Name\n    ...\nend"}, "", "Declares a class."}},
        {"try", {"try", {"try\n    ...\ncatch (e)\n    ...\nend"}, "", "Runs the block, routing exceptions to catch."}},
        {"end", {"end", {"if / while / for / func / class / try\n    ...\nend"}, "",
                 "Closes the block started by if, while, for, func, class, or try."}},
    };
    return table;
}

const std::unordered_map<std::string, BuiltinRow>& FallbackBuiltins() {
    static const std::unordered_map<std::string, BuiltinRow> table = {
        {"print", {"print", "*values", "Prints every argument."}},
        {"len", {"len", "value", "Returns the length of a string, list, or dict."}},
        {"range", {"range", "end", "Builds a list of numbers."}},
    };
    return table;
}

struct LoadedTables {
    std::unordered_map<std::string, KeywordRow> keywords;
    std::unordered_map<std::string, BuiltinRow> builtins;
    bool loaded_keywords_from_csv = false;
    bool loaded_builtins_from_csv = false;
    fs::file_time_type keywords_mtime{};
    fs::file_time_type builtins_mtime{};
};

std::mutex g_tables_mutex;
LoadedTables g_tables;
bool g_first_call = true;

// Re-reads whichever CSV changed on disk since the last call (or both,
// on the very first call) -- same "check mtime, cheap" approach as the
// editor's KeywordDocs()/BuiltinSignatures(), so editing either CSV by
// hand while Ava Studio is open is picked up on the agent's next
// message without a restart. Guarded by g_tables_mutex because tool
// calls run one thread per call (see ai_agent_plugin.cpp's SendMessage)
// and could all reach LookupAvalangSyntax at once.
void EnsureLoaded() {
    std::lock_guard<std::mutex> lock(g_tables_mutex);

    const std::string dir = ResolveDataDir();
    const std::string kw_path = dir + "keyword_docs.csv";
    const std::string bi_path = dir + "builtin_signatures.csv";

    std::error_code ec;
    fs::file_time_type kw_time = fs::last_write_time(kw_path, ec);
    bool kw_exists = !ec;
    ec.clear();
    fs::file_time_type bi_time = fs::last_write_time(bi_path, ec);
    bool bi_exists = !ec;

    bool reload_kw = g_first_call || (kw_exists && kw_time != g_tables.keywords_mtime);
    bool reload_bi = g_first_call || (bi_exists && bi_time != g_tables.builtins_mtime);

    if (reload_kw) {
        std::unordered_map<std::string, KeywordRow> loaded;
        if (kw_exists) LoadKeywordDocsFromCsv(kw_path, &loaded);
        if (!loaded.empty()) {
            g_tables.keywords = std::move(loaded);
            g_tables.loaded_keywords_from_csv = true;
            g_tables.keywords_mtime = kw_time;
        } else if (!g_tables.loaded_keywords_from_csv) {
            // Only fall back if we've never had a good CSV load -- a CSV
            // that briefly fails to parse mid-edit shouldn't blow away a
            // working table that's already loaded and in use.
            g_tables.keywords = FallbackKeywords();
        }
    }
    if (reload_bi) {
        std::unordered_map<std::string, BuiltinRow> loaded;
        if (bi_exists) LoadBuiltinSignaturesFromCsv(bi_path, &loaded);
        if (!loaded.empty()) {
            g_tables.builtins = std::move(loaded);
            g_tables.loaded_builtins_from_csv = true;
            g_tables.builtins_mtime = bi_time;
        } else if (!g_tables.loaded_builtins_from_csv) {
            g_tables.builtins = FallbackBuiltins();
        }
    }
    g_first_call = false;
}

std::string JoinParams(const std::string& raw_params) {
    std::string joined;
    auto parts = SplitOn(raw_params, "|");
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) joined += ", ";
        joined += parts[i];
    }
    return joined;
}

} // namespace

std::string BuildAvalangReferenceMessage(size_t max_chars) {
    EnsureLoaded();

    std::vector<KeywordRow> keywords;
    std::vector<BuiltinRow> builtins;
    {
        std::lock_guard<std::mutex> lock(g_tables_mutex);
        keywords.reserve(g_tables.keywords.size());
        for (const auto& entry : g_tables.keywords) keywords.push_back(entry.second);
        builtins.reserve(g_tables.builtins.size());
        for (const auto& entry : g_tables.builtins) builtins.push_back(entry.second);
    }
    std::sort(keywords.begin(), keywords.end(), [](const KeywordRow& a, const KeywordRow& b) { return a.name < b.name; });
    std::sort(builtins.begin(), builtins.end(), [](const BuiltinRow& a, const BuiltinRow& b) { return a.name < b.name; });

    std::ostringstream out;
    out << "[Referencia de sintaxis de AvaLang -- generada por Ava Studio a partir de "
           "data/keyword_docs.csv y data/builtin_signatures.csv. AvaLang NO es Python ni "
           "JavaScript: los bloques se cierran con 'end' (nunca con indentacion), y usa "
           "'func'/'then' en vez de 'def'/':'. Segui esta sintaxis, no la de otro lenguaje. "
           "Para el detalle completo (todas las variantes + un ejemplo) de una palabra clave o "
           "funcion built-in puntual, llama a avalang_syntax_lookup.]\n\n";

    out << "## Palabras clave\n";
    for (const auto& kw : keywords) {
        std::string first_syntax = kw.syntax.empty() ? "" : kw.syntax.front();
        size_t nl = first_syntax.find('\n');
        // Solo la primera linea del patron (que puede ser multi-linea) --
        // el bloque completo, y cualquier otra variante, es lo que
        // avalang_syntax_lookup devuelve entero.
        std::string syntax_head = (nl == std::string::npos) ? first_syntax : (first_syntax.substr(0, nl) + " ... end");

        std::string line = "- " + kw.name + ": `" + syntax_head + "`";
        if (kw.syntax.size() > 1) line += " (tiene otra forma tambien valida)";
        std::string doc = FirstSentence(kw.doc);
        if (!doc.empty()) line += " -- " + doc;
        line += "\n";

        if (static_cast<size_t>(out.tellp()) + line.size() > max_chars) {
            out << "... (referencia truncada)\n";
            return out.str();
        }
        out << line;
    }

    out << "\n## Funciones built-in\n";
    for (const auto& bi : builtins) {
        std::string line = "- " + bi.name + "(" + JoinParams(bi.params) + ")";
        std::string doc = FirstSentence(bi.doc);
        if (!doc.empty()) line += " -- " + doc;
        line += "\n";

        if (static_cast<size_t>(out.tellp()) + line.size() > max_chars) {
            out << "... (referencia truncada)\n";
            return out.str();
        }
        out << line;
    }

    return out.str();
}

std::string LookupAvalangSyntax(const std::string& name) {
    EnsureLoaded();
    const std::string key = ToLower(name);

    std::lock_guard<std::mutex> lock(g_tables_mutex);

    auto kw_it = g_tables.keywords.find(key);
    if (kw_it != g_tables.keywords.end()) {
        const auto& kw = kw_it->second;
        json result;
        result["found"] = true;
        result["kind"] = "keyword";
        result["name"] = kw.name;
        result["syntax"] = kw.syntax;
        result["example"] = kw.example;
        result["doc"] = kw.doc;
        return result.dump();
    }

    auto bi_it = g_tables.builtins.find(key);
    if (bi_it != g_tables.builtins.end()) {
        const auto& bi = bi_it->second;
        json result;
        result["found"] = true;
        result["kind"] = "builtin";
        result["name"] = bi.name;
        result["params"] = bi.params;
        result["doc"] = bi.doc;
        return result.dump();
    }

    json result;
    result["found"] = false;
    result["message"] = "'" + name + "' no es ninguna palabra clave ni funcion built-in conocida de AvaLang.";
    return result.dump();
}
