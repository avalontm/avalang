#include "languages/class_index.h"

#include <cctype>
#include <filesystem>

#include "languages/block_scanner.h"
#include "languages/lexer_utils.h"

namespace studio {

using namespace lexer;

void ClassIndex::Rebuild(const std::string& text, const std::string& current_file_dir,
                          ImportFileCache* shared_cache, const std::string& stdlib_dir) {
    classes_.clear();
    ScanText(text, "");

    ImportFileCache local_cache;
    ImportFileCache& cache = shared_cache ? *shared_cache : local_cache;

    std::unordered_set<std::string> visited;
    ScanImports(text, current_file_dir, visited, cache, stdlib_dir);
}

namespace {

void RecordAttribute(ClassInfo& info, const std::string& attr_name, bool is_static, bool is_private,
                      int line, const std::string& declared_type = "") {
    auto& attr = info.attributes[attr_name];
    attr.is_static = attr.is_static || is_static;
    attr.is_private = attr.is_private || is_private;
    if (!declared_type.empty()) attr.declared_type = declared_type;
    // First sighting wins (matches methods/classes: "if not already present,
    // insert") -- a class-body declaration should win the jump target over a
    // later `this.x = ...` assignment inside a constructor for the same name.
    if (attr.line == 0 && line > 0) attr.line = line;
}

void ConsumeModifiers(const std::string& body, size_t& i, bool& is_static, bool& is_private) {
    for (;;) {
        size_t before = i;
        SkipInlineWhitespace(body, i);
        if (i >= body.size() || !IsIdentStart(body[i])) { i = before; return; }

        size_t word_start = i;
        std::string word = ReadIdent(body, i);
        if (word == "static") { is_static = true; continue; }
        if (word == "private") { is_private = true; continue; }

        i = word_start;
        return;
    }
}

void ScanClassBody(const std::string& body, ClassInfo& info, int base_line) {
    size_t i = 0;
    int body_depth = 0;
    std::vector<std::string> pending_doc;

    while (i < body.size()) {
        char c = body[i];

        if (c == '#') {
            size_t start = i;
            while (i < body.size() && body[i] != '\n') ++i;
            std::string comment = body.substr(start, i - start);
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
            while (i < body.size() && body[i] != quote) {
                if (body[i] == '\\' && i + 1 < body.size()) i += 2; else ++i;
            }
            if (i < body.size()) ++i;
            pending_doc.clear();
            continue;
        }

        if (IsIdentStart(c)) {
            const size_t word_start = i;
            std::string word = ReadIdent(body, i);

            if (word == "end") {
                if (body_depth > 0) --body_depth;
                pending_doc.clear();
                continue;
            }

            if (word != "func" && word != "this" && IsBlockKeyword(word)) {
                ++body_depth;
                pending_doc.clear();
                continue;
            }

            if (word == "static" || word == "private") {
                bool is_static = (word == "static");
                bool is_private = (word == "private");
                size_t after_modifiers = i;
                ConsumeModifiers(body, after_modifiers, is_static, is_private);

                size_t save = after_modifiers;
                size_t k = after_modifiers;
                SkipInlineWhitespace(body, k);

                if (k < body.size() && IsIdentStart(body[k])) {
                    const size_t next_word_start = k;
                    std::string next_word = ReadIdent(body, k);

                    if (next_word == "func") {
                        SkipInlineWhitespace(body, k);
                        if (k < body.size() && IsIdentStart(body[k])) {
                            const size_t name_start = k;
                            std::string name = ReadIdent(body, k);
                            SkipInlineWhitespace(body, k);
                            if (k < body.size() && body[k] == '(') {
                                size_t open = k;
                                int depth = 0;
                                size_t j = open;
                                for (; j < body.size(); ++j) {
                                    if (body[j] == '(') ++depth;
                                    else if (body[j] == ')') { --depth; if (depth == 0) break; }
                                }
                                if (j < body.size()) {
                                    ClassMethodInfo method_info;
                                    method_info.is_static = is_static;
                                    method_info.is_private = is_private;
                                    method_info.signature.name = name;
                                    method_info.signature.params = SplitParams(body.substr(open + 1, j - open - 1));
                                    method_info.signature.source_file = info.source_file;
                                    method_info.signature.line = base_line + LineAt(body, name_start);
                                    for (const auto& p : method_info.signature.params) {
                                        if (!p.empty() && p[0] == '*') { method_info.signature.has_var_args = true; continue; }
                                        if (p.find('=') == std::string::npos) method_info.signature.min_args++;
                                    }
                                    method_info.signature.display = BuildDisplay(name, method_info.signature.params);
                                    if (!pending_doc.empty()) ApplyDocBlock(method_info.signature, pending_doc);
                                    pending_doc.clear();

                                    size_t after_params = j + 1;
                                    method_info.signature.declared_return_type =
                                        ParseReturnTypeAnnotation(body, after_params, body.size());

                                    size_t scan_pos = after_params;
                                    size_t method_body_end = 0;
                                    bool has_body = FindMatchingEnd(body, scan_pos, method_body_end);
                                    if (has_body && method_info.signature.declared_return_type.empty()) {
                                        method_info.signature.inferred_return_type =
                                            InferReturnTypeFromBody(body, after_params, method_body_end);
                                    }

                                    if (info.methods.find(name) == info.methods.end())
                                        info.methods[name] = std::move(method_info);

                                    if (has_body) ++body_depth;
                                    i = after_params;
                                    continue;
                                }
                            }
                        }

                    } else {

                        size_t type_check = k;
                        std::string declared_type;
                        if (type_check < body.size() && IsIdentStart(body[type_check])) {
                            size_t as_save = type_check;
                            std::string maybe_as = ReadIdent(body, type_check);
                            if (maybe_as == "as") {
                                SkipInlineWhitespace(body, type_check);
                                if (type_check < body.size() && IsIdentStart(body[type_check])) {
                                    declared_type = ReadIdent(body, type_check);
                                } else {
                                    type_check = as_save;
                                }
                            } else {
                                type_check = as_save;
                            }
                        }

                        size_t m = type_check;
                        SkipInlineWhitespace(body, m);
                        bool is_assignment = false;
                        if (m < body.size() && body[m] == '=') {
                            is_assignment = (m + 1 >= body.size() || body[m + 1] != '=');
                        } else if (m + 1 < body.size() &&
                                   (body[m] == '+' || body[m] == '-' || body[m] == '*' || body[m] == '/') &&
                                   body[m + 1] == '=') {
                            is_assignment = true;
                        }

                        if (is_assignment || !declared_type.empty()) {
                            RecordAttribute(info, next_word, is_static, is_private,
                                             base_line + LineAt(body, next_word_start), declared_type);
                            i = is_assignment ? m : type_check;
                            pending_doc.clear();
                            continue;
                        }
                    }

                }

                i = save;
                pending_doc.clear();
                continue;
            }

            if (word == "func") {
                size_t save = i;
                SkipInlineWhitespace(body, i);
                if (i >= body.size() || !IsIdentStart(body[i])) { i = save; pending_doc.clear(); continue; }

                const size_t name_start = i;
                std::string name = ReadIdent(body, i);
                SkipInlineWhitespace(body, i);
                if (i >= body.size() || body[i] != '(') { i = save; pending_doc.clear(); continue; }

                size_t open = i;
                int depth = 0;
                size_t j = open;
                for (; j < body.size(); ++j) {
                    if (body[j] == '(') ++depth;
                    else if (body[j] == ')') { --depth; if (depth == 0) break; }
                }
                if (j >= body.size()) { i = save; pending_doc.clear(); continue; }

                ClassMethodInfo method_info;
                method_info.signature.name = name;
                method_info.signature.params = SplitParams(body.substr(open + 1, j - open - 1));
                method_info.signature.source_file = info.source_file;
                method_info.signature.line = base_line + LineAt(body, name_start);
                for (const auto& p : method_info.signature.params) {
                    if (!p.empty() && p[0] == '*') { method_info.signature.has_var_args = true; continue; }
                    if (p.find('=') == std::string::npos) method_info.signature.min_args++;
                }
                method_info.signature.display = BuildDisplay(name, method_info.signature.params);
                if (!pending_doc.empty()) ApplyDocBlock(method_info.signature, pending_doc);
                pending_doc.clear();

                size_t after_params = j + 1;
                method_info.signature.declared_return_type =
                    ParseReturnTypeAnnotation(body, after_params, body.size());

                size_t scan_pos = after_params;
                size_t method_body_end = 0;
                bool has_body = FindMatchingEnd(body, scan_pos, method_body_end);
                if (has_body && method_info.signature.declared_return_type.empty()) {
                    method_info.signature.inferred_return_type =
                        InferReturnTypeFromBody(body, after_params, method_body_end);
                }

                if (info.methods.find(name) == info.methods.end())
                    info.methods[name] = std::move(method_info);

                if (has_body) ++body_depth;
                i = after_params;
                continue;
            }

            if (word == "this") {
                size_t k = i;
                SkipInlineWhitespace(body, k);
                if (k < body.size() && body[k] == '.') {
                    ++k;
                    SkipInlineWhitespace(body, k);
                    if (k < body.size() && IsIdentStart(body[k])) {
                        const size_t attr_name_start = k;
                        std::string attr_name = ReadIdent(body, k);
                        size_t after_attr = k;
                        SkipInlineWhitespace(body, k);

                        bool is_assignment = false;
                        if (k < body.size() && body[k] == '=') {
                            if (k + 1 >= body.size() || body[k + 1] != '=') is_assignment = true;
                        } else if (k + 1 < body.size() &&
                                   (body[k] == '+' || body[k] == '-' || body[k] == '*' || body[k] == '/') &&
                                   body[k + 1] == '=') {
                            is_assignment = true;
                        }

                        if (is_assignment) {
                            RecordAttribute(info, attr_name, false, false, base_line + LineAt(body, attr_name_start));
                        }

                        i = after_attr;
                        pending_doc.clear();
                        continue;
                    }
                }
            }

            if (body_depth == 0) {
                size_t type_check = i;
                SkipInlineWhitespace(body, type_check);
                std::string declared_type;
                if (type_check < body.size() && IsIdentStart(body[type_check])) {
                    size_t as_save = type_check;
                    std::string maybe_as = ReadIdent(body, type_check);
                    if (maybe_as == "as") {
                        SkipInlineWhitespace(body, type_check);
                        if (type_check < body.size() && IsIdentStart(body[type_check])) {
                            declared_type = ReadIdent(body, type_check);
                        } else {
                            type_check = as_save;
                        }
                    } else {
                        type_check = as_save;
                    }
                }

                size_t m = type_check;
                SkipInlineWhitespace(body, m);
                bool is_assignment = false;
                if (m < body.size() && body[m] == '=') {
                    is_assignment = (m + 1 >= body.size() || body[m + 1] != '=');
                } else if (m + 1 < body.size() &&
                           (body[m] == '+' || body[m] == '-' || body[m] == '*' || body[m] == '/') &&
                           body[m + 1] == '=') {
                    is_assignment = true;
                }

                if (is_assignment || !declared_type.empty()) {
                    RecordAttribute(info, word, false, false, base_line + LineAt(body, word_start), declared_type);
                    i = is_assignment ? m : type_check;
                    pending_doc.clear();
                    continue;
                }
            }

            pending_doc.clear();
            continue;
        }

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') pending_doc.clear();
        ++i;
    }
}

}

void ClassIndex::ScanText(const std::string& text, const std::string& source_file) {
    size_t i = 0;

    while (i < text.size()) {
        char c = text[i];

        if (c == '#') {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }
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
            if (word != "class") continue;

            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= text.size() || !IsIdentStart(text[i])) { i = save; continue; }

            const size_t class_name_start = i;
            std::string class_name = ReadIdent(text, i);
            SkipInlineWhitespace(text, i);

            std::string base_name;
            if (i < text.size() && text[i] == ':') {
                size_t colon = i;
                ++i;
                SkipInlineWhitespace(text, i);
                if (i < text.size() && IsIdentStart(text[i])) {
                    base_name = ReadIdent(text, i);
                } else {
                    i = colon;
                }
            }

            size_t body_start = i;
            size_t body_end = 0;
            if (!FindMatchingEnd(text, i, body_end)) {

                continue;
            }

            ClassInfo info;
            info.name = class_name;
            info.base_class_name = base_name;
            info.source_file = source_file;
            info.line = LineAt(text, class_name_start);
            ScanClassBody(text.substr(body_start, body_end - body_start), info, LineAt(text, body_start));

            if (classes_.find(class_name) == classes_.end())
                classes_[class_name] = std::move(info);
            continue;
        }

        ++i;
    }
}

void ClassIndex::ScanImports(const std::string& text, const std::string& current_file_dir,
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

std::string ClassIndex::ResolveImportPath(const std::vector<std::string>& module_path,
                                           const std::string& current_file_dir,
                                           const std::string& stdlib_dir) {
    return lexer::ResolveImportPath(module_path, current_file_dir, stdlib_dir);
}

std::vector<ClassMember> ClassIndex::FlattenedMembers(const std::string& class_name) const {
    std::vector<ClassMember> result;
    std::unordered_set<std::string> seen;
    std::unordered_set<std::string> visited_classes;

    std::string current = class_name;
    while (!current.empty() && visited_classes.insert(current).second) {
        const ClassInfo* info = Find(current);
        if (!info) break;

        for (const auto& [name, method_info] : info->methods) {
            if (seen.insert(name).second) {
                ClassMember member;
                member.name = name;
                member.is_method = true;
                member.is_static = method_info.is_static;
                member.is_private = method_info.is_private;
                member.signature = &method_info.signature;
                member.declared_in = info->name;
                member.line = method_info.signature.line;
                result.push_back(std::move(member));
            }
        }
        for (const auto& [name, attr_info] : info->attributes) {
            if (seen.insert(name).second) {
                ClassMember member;
                member.name = name;
                member.is_method = false;
                member.is_static = attr_info.is_static;
                member.is_private = attr_info.is_private;
                member.signature = nullptr;
                member.declared_in = info->name;
                member.declared_type = attr_info.declared_type;
                member.line = attr_info.line;
                result.push_back(std::move(member));
            }
        }

        current = info->base_class_name;
    }

    return result;
}

std::vector<ClassMember> ClassIndex::FilterForAccess(const std::vector<ClassMember>& members,
                                                      MemberAccessKind kind,
                                                      const std::string& viewer_class) {
    std::vector<ClassMember> result;
    result.reserve(members.size());

    for (const auto& member : members) {

        bool is_own_private_context = !viewer_class.empty() && member.declared_in == viewer_class;

        switch (kind) {
            case MemberAccessKind::kInstance:

                if (!member.is_private) result.push_back(member);
                break;

            case MemberAccessKind::kThis:

                if (!member.is_private || is_own_private_context) result.push_back(member);
                break;

            case MemberAccessKind::kClassName:

                if (member.is_static && (!member.is_private || is_own_private_context)) result.push_back(member);
                break;
        }
    }

    return result;
}

}
