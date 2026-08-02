#include "languages/class_index.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace studio {

namespace {

// The set of AvaLang block-opening keywords that require a matching `end`
// (see grammar/AvaLang.g4: tryStatement, ifStatement, whileStatement,
// forStatement, funcDeclaration, classDeclaration). `then`/`elif`/`else`
// are NOT here on purpose -- they belong to the same `if` block and don't
// introduce their own `end`.
bool IsBlockKeyword(const std::string& word) {
    return word == "try" || word == "if" || word == "while" || word == "for" ||
           word == "func" || word == "class";
}

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Requires that IsIdentStart(text[i]) was already checked by the caller.
std::string ReadIdent(const std::string& text, size_t& i) {
    size_t start = i;
    while (i < text.size() && IsIdentChar(text[i])) ++i;
    return text.substr(start, i - start);
}

void SkipInlineWhitespace(const std::string& text, size_t& i) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
}

// Same splitting rule as FunctionIndex's SplitParams (function_index.cpp):
// respects nested (), [], {} and quoted strings so a default value like
// `b=(1, 2)` or `c="a,b"` isn't split on its internal comma.
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

// Parses a "@param name: desc" line (already stripped of "## " and
// trimmed). Accepts ':' or '-' as an optional separator, or none at all.
bool ParseParamLine(const std::string& line, std::string& name, std::string& desc) {
    size_t i = 6;  // strlen("@param")
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

// Same "##" doc-block convention as FunctionIndex (see ApplyDocBlock in
// function_index.cpp): everything that isn't an "@param" line becomes the
// summary in `sig.doc`; "@param name: ..." lines go into `sig.param_docs`.
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

// Starting with `i` positioned right after a block-opening keyword's own
// header (past its NAME/params/condition/etc, at the first character of
// its body), scans forward -- skipping strings and comments -- counting
// nested try/if/while/for/func/class blocks until it finds the `end` that
// matches the block we started inside (depth starts at 1 for that block).
// On success, sets `body_end` to the index where that matching `end`
// keyword begins and advances `i` to just past it, returning true. On
// failure (no matching `end`, e.g. the file is mid-edit and unbalanced),
// restores `i` to its original value and returns false -- the caller
// abandons that one class/block rather than misparsing the rest of the
// file.
bool FindMatchingEnd(const std::string& text, size_t& i, size_t& body_end) {
    size_t start = i;
    int depth = 1;
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
            size_t word_start = i;
            std::string word = ReadIdent(text, i);
            if (word == "end") {
                --depth;
                if (depth == 0) { body_end = word_start; return true; }
                continue;
            }
            if (IsBlockKeyword(word)) ++depth;
            continue;
        }
        ++i;
    }
    i = start;
    return false;
}

} // namespace

void ClassIndex::Rebuild(const std::string& text, const std::string& current_file_dir) {
    classes_.clear();
    ScanText(text, "");  // local buffer first -- "local wins" (see header comment)
    std::unordered_set<std::string> visited;
    ScanImports(text, current_file_dir, visited);
}

// Parses `body` -- the text strictly between a class's header and its
// closing `end` (already extracted by ScanText via FindMatchingEnd) --
// looking for `func NAME(params) ... end` method declarations (same "##"
// doc-comment convention as FunctionIndex) and `this.NAME = ...` /
// `self.NAME = ...` attribute assignments. Unlike FindMatchingEnd, this
// does NOT need to track block nesting itself: it only cares about
// locating `func` headers and `this`/`self` attribute writes wherever they
// occur in the body, the same best-effort, non-block-aware philosophy
// FunctionIndex::ScanText already uses for finding `func` anywhere in a
// buffer.
namespace {

// Records `attr_name` as an attribute if it isn't already known, or merges
// in `is_static`/`is_private` if it is -- but NEVER downgrades: a bare
// `this.x = ...` assignment inside a constructor must not erase the
// `static`/`private` flags that a class-body-level `static x = ...` /
// `private x = ...` declaration already set for the same name (see
// DISENO_visibilidad_clases_avalang.md §3.3 -- a `this.NAME =` write is
// just a normal use of an already-declared attribute, not a redeclaration).
void RecordAttribute(ClassInfo& info, const std::string& attr_name, bool is_static, bool is_private) {
    auto& attr = info.attributes[attr_name];  // default-constructs (false, false) if new
    attr.is_static = attr.is_static || is_static;
    attr.is_private = attr.is_private || is_private;
}

// At `i` (already advanced past whatever word was just read), tries to
// consume zero or more `static`/`private` keywords separated by inline
// whitespace, in any order (mirrors the grammar's `memberModifier*` from
// Fase A of DISENO_visibilidad_clases_avalang.md -- both orders are valid,
// e.g. `static private` or `private static`). Sets `is_static`/`is_private`
// accordingly. On the first word that ISN'T a modifier, rewinds `i` back to
// right before that word so the caller can reparse it normally (as `func`,
// an attribute name, or anything else) -- this function only ever consumes
// modifier keywords, never the thing that follows them.
void ConsumeModifiers(const std::string& body, size_t& i, bool& is_static, bool& is_private) {
    for (;;) {
        size_t before = i;
        SkipInlineWhitespace(body, i);
        if (i >= body.size() || !IsIdentStart(body[i])) { i = before; return; }

        size_t word_start = i;
        std::string word = ReadIdent(body, i);
        if (word == "static") { is_static = true; continue; }
        if (word == "private") { is_private = true; continue; }

        i = word_start;  // not a modifier -- let the caller reparse this word
        return;
    }
}

void ScanClassBody(const std::string& body, ClassInfo& info) {
    size_t i = 0;
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

            // `static`/`private` prefix (Fase E): consume any further
            // modifiers, then expect either `func NAME(...)` (a modified
            // method) or `NAME = expr` (a modified attribute declaration,
            // the `attrDeclaration` from the design doc -- a bare
            // class-body-level assignment, NOT `this.NAME =`).
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

                                    if (info.methods.find(name) == info.methods.end())
                                        info.methods[name] = std::move(method_info);

                                    i = j + 1;
                                    continue;
                                }
                            }
                        }
                        // Malformed `static`/`private func` header (no
                        // matching parens, etc.) -- fall through and
                        // reparse from the modifier keywords as plain
                        // identifiers, same "abandon and resync" policy as
                        // the rest of this best-effort scanner.
                    } else {
                        // `NAME = expr` / `NAME += expr` / etc -- the
                        // `attrDeclaration` from the design doc (a bare
                        // class-body-level assignment, distinct from a
                        // `this.NAME = ...` write inside a method).
                        size_t m = k;
                        SkipInlineWhitespace(body, m);
                        bool is_assignment = false;
                        if (m < body.size() && body[m] == '=') {
                            is_assignment = (m + 1 >= body.size() || body[m + 1] != '=');
                        } else if (m + 1 < body.size() &&
                                   (body[m] == '+' || body[m] == '-' || body[m] == '*' || body[m] == '/') &&
                                   body[m + 1] == '=') {
                            is_assignment = true;
                        }

                        if (is_assignment) {
                            RecordAttribute(info, next_word, is_static, is_private);
                            i = k;
                            pending_doc.clear();
                            continue;
                        }
                    }
                    // Neither `func NAME(...)` nor `NAME = expr` -- fall
                    // through: rewind to the first modifier keyword and let
                    // normal scanning continue (e.g. `static`/`private` used
                    // as a plain identifier somewhere unrelated, or a
                    // malformed declaration).
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

                // Constructor methods share the class's own name (see
                // scripts/dog.ava: `class dog` / `dog()`) -- still indexed
                // as a plain method like any other; the popup can filter
                // it out later if that turns out to be noisy.
                if (info.methods.find(name) == info.methods.end())
                    info.methods[name] = std::move(method_info);

                i = j + 1;
                continue;
            }

            // AvaLang solo tiene `this` -- no existe `self` en el lenguaje
            // (confirmado: no aparece en grammar/AvaLang.g4 ni en el
            // frontend). Si algún archivo tiene `self`, es resto de otro
            // lenguaje/plantilla y no debe tratarse como acceso a atributo.
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

                        // A plain `this.NAME = ...` write, with no
                        // modifier prefix, never upgrades an attribute to
                        // static/private on its own -- see RecordAttribute
                        // (it only ever ORs flags in, defaulting to false
                        // here, so a name already marked static/private via
                        // a class-body-level declaration stays that way).
                        if (is_assignment) RecordAttribute(info, attr_name, /*is_static=*/false, /*is_private=*/false);

                        i = after_attr;
                        pending_doc.clear();
                        continue;
                    }
                }
            }

            pending_doc.clear();
            continue;
        }

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') pending_doc.clear();
        ++i;
    }
}

} // namespace

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
                    i = colon;  // not actually a classHeritage colon -- leave it for normal scanning
                }
            }

            size_t body_start = i;
            size_t body_end = 0;
            if (!FindMatchingEnd(text, i, body_end)) {
                // Unmatched `end` (file mid-edit, or something we can't
                // parse with confidence) -- abandon just this class and
                // let the outer loop resume scanning from body_start.
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
                              std::unordered_set<std::string>& visited) {
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
                        std::ifstream file(path, std::ios::binary);
                        if (file) {
                            std::ostringstream ss;
                            ss << file.rdbuf();
                            std::string imported_text = ss.str();

                            // Local buffer already scanned first in
                            // Rebuild(); for imports there's no "local
                            // wins" tie-break to preserve, so just index
                            // whatever this import defines.
                            ScanText(imported_text, path);

                            // TRANSITIVE, unlike FunctionIndex::ScanImports
                            // (which only follows one level): also walk
                            // THIS file's own imports, resolved relative to
                            // its own directory, so a class several
                            // imports away still surfaces in the popup.
                            namespace fs = std::filesystem;
                            std::string imported_dir = fs::path(path).parent_path().string();
                            ScanImports(imported_text, imported_dir, visited);
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
    std::unordered_set<std::string> visited_classes;  // guards against inheritance cycles

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
        // "Own private member" -- visible only where the modifier's whole
        // point is to still work: from inside the very class that declared
        // it (see DISENO_visibilidad_clases_avalang.md §3.3 -- a private
        // member inherited from a base class is NOT "own" for the child).
        bool is_own_private_context = !viewer_class.empty() && member.declared_in == viewer_class;

        switch (kind) {
            case MemberAccessKind::kInstance:
                // `variable.` from outside the class -- §5, first bullet:
                // only public members, static or not.
                if (!member.is_private) result.push_back(member);
                break;

            case MemberAccessKind::kThis:
                // `this.` inside a method of `viewer_class` -- §5/§9: every
                // public member, plus this class's own private members
                // (never a private member inherited from a base class).
                if (!member.is_private || is_own_private_context) result.push_back(member);
                break;

            case MemberAccessKind::kClassName:
                // `NombreDeClase.` -- §5, third bullet: only `static`
                // members. A private static is only offered from inside
                // its own declaring class (e.g. Contador.validarLimite);
                // a public static is always offered.
                if (member.is_static && (!member.is_private || is_own_private_context)) result.push_back(member);
                break;
        }
    }

    return result;
}

} // namespace studio
