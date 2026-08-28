#include "languages/class_index.h"

#include <cctype>
#include <filesystem>

#include "languages/block_scanner.h"

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

std::string InferReturnTypeFromBody(const std::string& body, size_t body_start, size_t body_end) {
    std::unordered_map<std::string, std::string> local_types;

    size_t i = body_start;
    while (i < body_end) {
        char c = body[i];
        if (c == '#') { while (i < body_end && body[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < body_end && body[i] != quote) {
                if (body[i] == '\\' && i + 1 < body_end) i += 2; else ++i;
            }
            if (i < body_end) ++i;
            continue;
        }
        if (IsIdentStart(c)) {
            std::string word = ReadIdent(body, i);

            if (word == "return") {
                SkipInlineWhitespace(body, i);
                if (i < body_end && IsIdentStart(body[i])) {
                    std::string candidate = ReadIdent(body, i);
                    size_t after_ident = i;
                    SkipInlineWhitespace(body, i);
                    if (i < body_end && body[i] == '(') return candidate;
                    auto local_it = local_types.find(candidate);
                    if (local_it != local_types.end()) return local_it->second;
                    i = after_ident;
                }
                continue;
            }

            size_t save = i;
            SkipInlineWhitespace(body, i);
            bool is_plain_assign = i < body_end && body[i] == '=' && (i + 1 >= body_end || body[i + 1] != '=');
            if (!is_plain_assign) { i = save; continue; }

            ++i;
            SkipInlineWhitespace(body, i);
            if (i < body_end && IsIdentStart(body[i])) {
                std::string rhs_name = ReadIdent(body, i);
                size_t after_rhs = i;
                SkipInlineWhitespace(body, i);
                if (i < body_end && body[i] == '(') {
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

}

void ClassIndex::Rebuild(const std::string& text, const std::string& current_file_dir,
                          ImportFileCache* shared_cache) {
    classes_.clear();
    ScanText(text, "");

    ImportFileCache local_cache;
    ImportFileCache& cache = shared_cache ? *shared_cache : local_cache;

    std::unordered_set<std::string> visited;
    ScanImports(text, current_file_dir, visited, cache);
}

namespace {

void RecordAttribute(ClassInfo& info, const std::string& attr_name, bool is_static, bool is_private,
                      const std::string& declared_type = "") {
    auto& attr = info.attributes[attr_name];
    attr.is_static = attr.is_static || is_static;
    attr.is_private = attr.is_private || is_private;
    if (!declared_type.empty()) attr.declared_type = declared_type;
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

void ScanClassBody(const std::string& body, ClassInfo& info) {
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
                    std::string next_word = ReadIdent(body, k);

                    if (next_word == "func") {
                        SkipInlineWhitespace(body, k);
                        if (k < body.size() && IsIdentStart(body[k])) {
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
                            RecordAttribute(info, next_word, is_static, is_private, declared_type);
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

                        if (is_assignment) RecordAttribute(info, attr_name, false, false);

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
                    RecordAttribute(info, word, false, false, declared_type);
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
            ScanClassBody(text.substr(body_start, body_end - body_start), info);

            if (classes_.find(class_name) == classes_.end())
                classes_[class_name] = std::move(info);
            continue;
        }

        ++i;
    }
}

void ClassIndex::ScanImports(const std::string& text, const std::string& current_file_dir,
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

std::string ClassIndex::ResolveImportPath(const std::vector<std::string>& module_path,
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
