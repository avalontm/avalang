#include "languages/function_index.h"

#include <cctype>
#include <filesystem>

#include "languages/block_scanner.h"
#include "languages/builtin_signatures.h"
#include "languages/import_file_cache.h"
#include "languages/lexer_utils.h"

namespace studio {

using namespace lexer;

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

std::string InferReturnTypeFromBody(const std::string& text, size_t body_start) {
    size_t scan_pos = body_start;
    size_t body_end = 0;
    if (!FindMatchingEnd(text, scan_pos, body_end)) return "";
    return lexer::InferReturnTypeFromBody(text, body_start, body_end);
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
            if (word == "class" || word == "interface") {
                size_t body_end = 0;
                FindMatchingEnd(text, i, body_end, word == "interface");
                pending_doc.clear();
                continue;
            }
            if (word != "func") { pending_doc.clear(); continue; }

            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= text.size() || !IsIdentStart(text[i])) { i = save; pending_doc.clear(); continue; }

            const size_t name_start = i;
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
            sig.line = LineAt(text, name_start);
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
                                 std::unordered_set<std::string>& visited, ImportFileCache& cache,
                                 const std::string& stdlib_dir) {
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
                    std::string path = ResolveImportPath(module_path, current_file_dir, stdlib_dir);
                    if (!path.empty() && visited.insert(path).second) {
                        if (const std::string* imported_text = cache.Load(path)) {
                            ScanText(*imported_text, path);

                            namespace fs = std::filesystem;
                            std::string imported_dir = fs::path(path).parent_path().string();
                            ScanImports(*imported_text, imported_dir, visited, cache, stdlib_dir);
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
                                              const std::string& current_file_dir,
                                              const std::string& stdlib_dir) {
    return lexer::ResolveImportPath(module_path, current_file_dir, stdlib_dir);
}

void FunctionIndex::Rebuild(const std::string& text, const std::string& current_file_dir,
                             ImportFileCache* shared_cache, const std::string& stdlib_dir) {
    signatures_.clear();
    ScanText(text, "");

    ImportFileCache local_cache;
    ImportFileCache& cache = shared_cache ? *shared_cache : local_cache;

    std::unordered_set<std::string> visited;
    ScanImports(text, current_file_dir, visited, cache, stdlib_dir);

    for (const auto& [name, sig] : BuiltinSignatures()) {
        signatures_.emplace(name, sig);
    }
}

}
