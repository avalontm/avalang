#include "languages/function_index.h"

#include <cctype>
#include <filesystem>

#include "languages/block_scanner.h"
#include "languages/builtin_signatures.h"
#include "languages/import_file_cache.h"

namespace studio {

namespace {

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string ReadIdent(const std::string& text, size_t& i) {
    size_t start = i;
    while (i < text.size() && IsIdentChar(text[i])) ++i;
    return text.substr(start, i - start);
}

void SkipInlineWhitespace(const std::string& text, size_t& i) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
}

std::vector<std::string> SplitParams(const std::string& raw) {
    std::vector<std::string> params;
    int depth = 0;
    char in_string = '\0';
    std::string current;

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (in_string) {
            current += c;
            if (c == '\\' && i + 1 < raw.size()) { current += raw[++i]; continue; }
            if (c == in_string) in_string = '\0';
            continue;
        }
        if (c == '\'' || c == '"') { in_string = c; current += c; continue; }
        if (c == '(' || c == '[' || c == '{') { ++depth; current += c; continue; }
        if (c == ')' || c == ']' || c == '}') { --depth; current += c; continue; }
        if (c == ',' && depth == 0) { params.push_back(current); current.clear(); continue; }
        current += c;
    }
    if (!current.empty() || !params.empty()) params.push_back(current);

    std::vector<std::string> trimmed;
    for (auto& p : params) {
        size_t b = p.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = p.find_last_not_of(" \t\r\n");
        trimmed.push_back(p.substr(b, e - b + 1));
    }
    return trimmed;
}

std::string BuildDisplay(const std::string& name, const std::vector<std::string>& params) {
    std::string display = name + "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i) display += ", ";
        display += params[i];
    }
    display += ")";
    return display;
}

std::string TrimTrailing(const std::string& s) {
    size_t e = s.find_last_not_of(" \t\r");
    return e == std::string::npos ? "" : s.substr(0, e + 1);
}

bool ParseParamLine(const std::string& line, std::string& name, std::string& desc) {
    size_t i = 6;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    size_t start = i;
    while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) ++i;
    if (i == start) return false;
    name = line.substr(start, i - start);
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i < line.size() && (line[i] == ':' || line[i] == '-')) {
        ++i;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    }
    desc = line.substr(i);
    return true;
}

void ApplyDocBlock(FunctionSignature& sig, const std::vector<std::string>& pending_doc) {
    std::vector<std::string> summary_lines;
    for (const auto& raw_line : pending_doc) {
        std::string line = TrimTrailing(raw_line);
        std::string pname, pdesc;
        if (line.compare(0, 6, "@param") == 0 && ParseParamLine(line, pname, pdesc)) {
            sig.param_docs[pname] = pdesc;
            continue;
        }
        summary_lines.push_back(line);
    }
    std::string doc;
    for (const auto& line : summary_lines) {
        if (line.empty()) continue;
        if (!doc.empty()) doc += " ";
        doc += line;
    }
    sig.doc = doc;
}

}

std::string ParamBaseName(const std::string& raw_param) {
    std::string p = raw_param;
    size_t b = p.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    p = p.substr(b);
    if (!p.empty() && p[0] == '*') p = p.substr(1);
    size_t eq = p.find('=');
    if (eq != std::string::npos) p = p.substr(0, eq);
    size_t as_pos = p.find(" as ");
    if (as_pos != std::string::npos) p = p.substr(0, as_pos);
    size_t e = p.find_last_not_of(" \t");
    return e == std::string::npos ? "" : p.substr(0, e + 1);
}

std::string ParamBaseType(const std::string& raw_param) {
    std::string p = raw_param;
    size_t b = p.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    p = p.substr(b);
    if (!p.empty() && p[0] == '*') p = p.substr(1);
    size_t eq = p.find('=');
    std::string core = eq == std::string::npos ? p : p.substr(0, eq);
    size_t as_pos = core.find(" as ");
    if (as_pos == std::string::npos) return "";
    std::string type = core.substr(as_pos + 4);
    size_t tb = type.find_first_not_of(" \t");
    if (tb == std::string::npos) return "";
    size_t te = type.find_last_not_of(" \t");
    return type.substr(tb, te - tb + 1);
}

std::string ParseReturnTypeAnnotation(const std::string& text, size_t& i, size_t end) {
    size_t save = i;
    SkipInlineWhitespace(text, i);
    if (i >= end || !IsIdentStart(text[i])) { i = save; return ""; }
    size_t as_save = i;
    std::string maybe_as = ReadIdent(text, i);
    if (maybe_as != "as") { i = as_save; return ""; }
    SkipInlineWhitespace(text, i);
    if (i >= end || !IsIdentStart(text[i])) { i = as_save; return ""; }
    return ReadIdent(text, i);
}

std::string InferReturnTypeFromBody(const std::string& text, size_t body_start) {
    size_t scan_pos = body_start;
    size_t body_end = 0;
    if (!FindMatchingEnd(text, scan_pos, body_end)) return "";

    std::unordered_map<std::string, std::string> local_types;

    size_t i = body_start;
    while (i < body_end) {
        char c = text[i];
        if (c == '#') { while (i < body_end && text[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < body_end && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < body_end) i += 2; else ++i;
            }
            if (i < body_end) ++i;
            continue;
        }
        if (IsIdentStart(c)) {
            std::string word = ReadIdent(text, i);

            if (word == "return") {
                SkipInlineWhitespace(text, i);
                if (i < body_end && IsIdentStart(text[i])) {
                    std::string candidate = ReadIdent(text, i);
                    size_t after_ident = i;
                    SkipInlineWhitespace(text, i);
                    if (i < body_end && text[i] == '(') return candidate;
                    auto local_it = local_types.find(candidate);
                    if (local_it != local_types.end()) return local_it->second;
                    i = after_ident;
                }
                continue;
            }

            size_t save = i;
            SkipInlineWhitespace(text, i);
            bool is_plain_assign = i < body_end && text[i] == '=' && (i + 1 >= body_end || text[i + 1] != '=');
            if (!is_plain_assign) { i = save; continue; }

            ++i;
            SkipInlineWhitespace(text, i);
            if (i < body_end && IsIdentStart(text[i])) {
                std::string rhs_name = ReadIdent(text, i);
                size_t after_rhs = i;
                SkipInlineWhitespace(text, i);
                if (i < body_end && text[i] == '(') {
                    local_types[word] = rhs_name;
                } else {
                    local_types.erase(word);
                    i = after_rhs;
                }
            } else {
                local_types.erase(word);
            }
            continue;
        }
        ++i;
    }
    return "";
}

void FunctionIndex::ScanText(const std::string& text, const std::string& source_file) {
    size_t i = 0;

    std::vector<std::string> pending_doc;

    while (i < text.size()) {
        char c = text[i];

        if (c == '#') {
            size_t start = i;
            while (i < text.size() && text[i] != '\n') ++i;
            std::string comment = text.substr(start, i - start);
            if (comment.size() >= 2 && comment[1] == '#') {
                size_t b = comment.find_first_not_of(" \t", 2);
                pending_doc.push_back(b == std::string::npos ? "" : comment.substr(b));
            } else {
                pending_doc.clear();
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            pending_doc.clear();
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(text, i);
            if (word != "func") { pending_doc.clear(); continue; }

            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= text.size() || !IsIdentStart(text[i])) { i = save; pending_doc.clear(); continue; }

            std::string name = ReadIdent(text, i);
            SkipInlineWhitespace(text, i);
            if (i >= text.size() || text[i] != '(') { i = save; pending_doc.clear(); continue; }

            size_t open = i;
            int depth = 0;
            size_t j = open;
            for (; j < text.size(); ++j) {
                if (text[j] == '(') ++depth;
                else if (text[j] == ')') { --depth; if (depth == 0) break; }
            }
            if (j >= text.size()) { i = save; pending_doc.clear(); continue; }

            FunctionSignature sig;
            sig.name = name;
            sig.params = SplitParams(text.substr(open + 1, j - open - 1));
            sig.source_file = source_file;
            for (const auto& p : sig.params) {
                if (!p.empty() && p[0] == '*') { sig.has_var_args = true; continue; }
                if (p.find('=') == std::string::npos) sig.min_args++;
            }
            sig.display = BuildDisplay(name, sig.params);
            if (!pending_doc.empty()) ApplyDocBlock(sig, pending_doc);
            pending_doc.clear();

            size_t after_params = j + 1;
            sig.declared_return_type = ParseReturnTypeAnnotation(text, after_params, text.size());
            if (sig.declared_return_type.empty()) {
                sig.inferred_return_type = InferReturnTypeFromBody(text, after_params);
            }

            if (signatures_.find(name) == signatures_.end()) {
                signatures_[name] = std::move(sig);
            }
            i = j + 1;
            continue;
        }

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') pending_doc.clear();
        ++i;
    }
}

void FunctionIndex::ScanImports(const std::string& text, const std::string& current_file_dir,
                                 std::unordered_set<std::string>& visited, ImportFileCache& cache) {
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];

        if (c == '#') { while (i < text.size() && text[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(text, i);
            if (word == "import") {
                std::vector<std::string> module_path;
                for (;;) {
                    SkipInlineWhitespace(text, i);
                    if (i >= text.size() || !IsIdentStart(text[i])) break;
                    module_path.push_back(ReadIdent(text, i));
                    if (i < text.size() && text[i] == '.') { ++i; continue; }
                    break;
                }
                if (!module_path.empty()) {
                    std::string path = ResolveImportPath(module_path, current_file_dir);
                    if (!path.empty() && visited.insert(path).second) {
                        if (const std::string* imported_text = cache.Load(path)) {
                            ScanText(*imported_text, path);

                            namespace fs = std::filesystem;
                            std::string imported_dir = fs::path(path).parent_path().string();
                            ScanImports(*imported_text, imported_dir, visited, cache);
                        }
                    }
                }
            }
            continue;
        }

        ++i;
    }
}

std::string FunctionIndex::ResolveImportPath(const std::vector<std::string>& module_path,
                                              const std::string& current_file_dir) {
    if (module_path.empty()) return "";

    std::string rel;
    for (size_t k = 0; k < module_path.size(); ++k) {
        if (k) rel += "/";
        rel += module_path[k];
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path base = current_file_dir.empty() ? fs::current_path(ec) : fs::path(current_file_dir);

    fs::path candidate = base / (rel + ".ava");
    if (fs::exists(candidate, ec)) return candidate.string();

    candidate = base / rel / "index.ava";
    if (fs::exists(candidate, ec)) return candidate.string();

    return "";
}

void FunctionIndex::Rebuild(const std::string& text, const std::string& current_file_dir,
                             ImportFileCache* shared_cache) {
    signatures_.clear();
    ScanText(text, "");

    ImportFileCache local_cache;
    ImportFileCache& cache = shared_cache ? *shared_cache : local_cache;

    std::unordered_set<std::string> visited;
    ScanImports(text, current_file_dir, visited, cache);

    for (const auto& [name, sig] : BuiltinSignatures()) {
        signatures_.emplace(name, sig);
    }
}

}
