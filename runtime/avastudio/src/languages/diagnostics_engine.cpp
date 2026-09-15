#include "languages/diagnostics_engine.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include "languages/avalang_language.h"
#include "languages/block_scanner.h"
#include "languages/lexer_utils.h"

namespace studio::diagnostics {

using namespace lexer;

namespace {

struct Occurrence {
    int line = 0;
    int column_start = 0;
    int column_end = 0;
    size_t offset = 0;
};

struct ImportStatement {
    int line = 0;
    int column_start = 0;
    int column_end = 0;
    std::vector<std::string> segments;
    std::string bound_name;
    bool has_alias = false;
};

struct SymbolUse {
    std::string name;
    int line = 0;
    int column_start = 0;
    int column_end = 0;
};

struct ScanResult {
    std::vector<ImportStatement> imports;
    std::vector<SymbolUse> calls;
    std::vector<SymbolUse> new_exprs;
    std::vector<SymbolUse> as_types;
    std::unordered_map<std::string, std::vector<Occurrence>> occurrences;
    std::unordered_map<std::string, std::vector<Occurrence>> member_occurrences;
    std::vector<std::pair<int, int>> import_line_spans;
};

int ColumnAt(const std::string& text, size_t offset) {
    if (offset == 0) return 0;
    const size_t newline = text.rfind('\n', offset - 1);
    return newline == std::string::npos ? static_cast<int>(offset) : static_cast<int>(offset - newline - 1);
}

bool IsInSpan(int line, const std::vector<std::pair<int, int>>& spans) {
    for (const auto& [from, to] : spans) {
        if (line >= from && line <= to) return true;
    }
    return false;
}

const std::unordered_set<std::string>& KeywordAndBuiltinNames() {
    static const std::unordered_set<std::string> names = [] {
        std::unordered_set<std::string> set;
        if (const TextEditor::Language* lang = languages::AvaLang()) {
            for (const auto& w : lang->keywords) set.insert(w);
            for (const auto& w : lang->identifiers) set.insert(w);
        }
        return set;
    }();
    return names;
}

ScanResult ScanText(const std::string& text) {
    ScanResult result;
    size_t i = 0;
    std::string prev_word;
    bool prev_was_dot = false;

    // Records a plain identifier occurrence/call-site/member-access. Shared
    // between the top-level loop and the {expr} interpolation scanner below
    // so a symbol referenced only inside an f-string -- e.g. the 'resta' in
    // $"resta: {resta(4, 2)}" -- is tracked the same as one referenced in
    // plain code, instead of being swallowed as inert string text. Does not
    // handle 'import'/'new'/'as' statement forms, which can't appear inside
    // an interpolation expression.
    auto record_identifier = [&](const std::string& word, int line, int col_start, int col_end,
                                  size_t word_start) {
        if (!prev_was_dot) {
            size_t after_ws = word_start + word.size();
            SkipInlineWhitespace(text, after_ws);
            const bool is_call = after_ws < text.size() && text[after_ws] == '(' &&
                                  !KeywordAndBuiltinNames().count(word);

            if (prev_word == "new") {
                result.new_exprs.push_back({word, line, col_start, col_end});
            } else if (prev_word == "as") {
                result.as_types.push_back({word, line, col_start, col_end});
            } else if (is_call && word != "func" && prev_word != "func") {
                result.calls.push_back({word, line, col_start, col_end});
            }

            if (!KeywordAndBuiltinNames().count(word)) {
                result.occurrences[word].push_back({line, col_start, col_end, word_start});
            }
        } else {
            result.member_occurrences[word].push_back({line, col_start, col_end, word_start});
        }
        prev_was_dot = false;
        prev_word = word;
    };

    while (i < text.size()) {
        char c = text[i];

        if (c == '#') {
            while (i < text.size() && text[i] != '\n') ++i;
            continue;
        }
        // f-string ($"...", languages::AvaLang()'s otherStringStart/otherStringEnd,
        // see avalang_language.cpp). Handled before the generic quote branch below
        // because its body isn't inert text: a {expr} interpolation inside it is
        // live AvaLang code and has to be scanned for identifier usage, the same
        // way Compiler::ParsePrimary (compiler.cpp) parses it at compile time.
        // '{{'/'}}' are literal braces (no interpolation), matching the compiler.
        if (c == '$' && i + 1 < text.size() && text[i + 1] == '"') {
            i += 2;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    i += 2;
                    continue;
                }
                if (text[i] == '{') {
                    if (i + 1 < text.size() && text[i + 1] == '{') {
                        i += 2;
                        continue;
                    }
                    ++i;
                    int depth = 1;
                    while (i < text.size() && depth > 0) {
                        const char ic = text[i];
                        if (ic == '{') {
                            ++depth;
                            ++i;
                        } else if (ic == '}') {
                            --depth;
                            ++i;
                        } else if (ic == '\'' || ic == '"') {
                            const char iq = ic;
                            ++i;
                            while (i < text.size() && text[i] != iq) {
                                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
                            }
                            if (i < text.size()) ++i;
                        } else if (IsIdentStart(ic)) {
                            const size_t word_start = i;
                            const int line = LineAt(text, word_start);
                            const int col_start = ColumnAt(text, word_start);
                            std::string word = ReadIdent(text, i);
                            const int col_end = col_start + static_cast<int>(word.size());
                            record_identifier(word, line, col_start, col_end, word_start);
                        } else if (ic == '.') {
                            prev_was_dot = true;
                            ++i;
                        } else {
                            prev_was_dot = false;
                            ++i;
                        }
                    }
                    continue;
                }
                if (text[i] == '}' && i + 1 < text.size() && text[i + 1] == '}') {
                    i += 2;
                    continue;
                }
                ++i;
            }
            if (i < text.size()) ++i;
            prev_was_dot = false;
            continue;
        }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            prev_was_dot = false;
            continue;
        }
        if (c == '.') {
            prev_was_dot = true;
            ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            const size_t word_start = i;
            const int line = LineAt(text, word_start);
            const int col_start = ColumnAt(text, word_start);
            std::string word = ReadIdent(text, i);
            const int col_end = col_start + static_cast<int>(word.size());

            if (word == "import") {
                ImportStatement stmt;
                stmt.line = line;
                stmt.column_start = col_start;
                size_t save = i;
                for (;;) {
                    SkipInlineWhitespace(text, i);
                    if (i >= text.size() || !IsIdentStart(text[i])) break;
                    stmt.segments.push_back(ReadIdent(text, i));
                    stmt.column_end = ColumnAt(text, i);
                    size_t after_segment = i;
                    SkipInlineWhitespace(text, i);
                    if (i < text.size() && text[i] == '.') { ++i; continue; }
                    i = after_segment;
                    break;
                }
                if (stmt.segments.empty()) { i = save; prev_was_dot = false; prev_word = word; continue; }

                stmt.bound_name = stmt.segments.back();
                size_t after_path = i;
                SkipInlineWhitespace(text, i);
                size_t as_save = i;
                if (i < text.size() && IsIdentStart(text[i])) {
                    std::string maybe_as = ReadIdent(text, i);
                    if (maybe_as == "as") {
                        SkipInlineWhitespace(text, i);
                        if (i < text.size() && IsIdentStart(text[i])) {
                            stmt.bound_name = ReadIdent(text, i);
                            stmt.has_alias = true;
                        } else {
                            i = as_save;
                        }
                    } else {
                        i = as_save;
                    }
                }
                if (i == as_save) i = after_path;

                const int end_line = LineAt(text, i);
                result.import_line_spans.push_back({stmt.line, end_line});
                result.imports.push_back(std::move(stmt));
                prev_was_dot = false;
                prev_word = "import";
                continue;
            }

            record_identifier(word, line, col_start, col_end, word_start);
            continue;
        }

        prev_was_dot = false;
        ++i;
    }

    return result;
}

std::vector<std::string> ImportPathFor(const std::string& target_file, const std::string& current_file_dir,
                                        const std::string& stdlib_dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (target_file.empty()) return {};

    auto try_base = [&](const std::string& base_dir) -> std::vector<std::string> {
        if (base_dir.empty()) return {};
        fs::path rel = fs::relative(fs::path(target_file), fs::path(base_dir), ec);
        if (ec || rel.empty() || rel.generic_string().substr(0, 2) == "..") return {};

        fs::path without_ext = rel;
        if (without_ext.filename() == "index.ava") {
            without_ext = without_ext.parent_path();
        } else if (without_ext.extension() == ".ava") {
            without_ext.replace_extension();
        }

        std::vector<std::string> segments;
        for (const auto& part : without_ext) {
            const std::string s = part.string();
            if (!s.empty()) segments.push_back(s);
        }
        return segments;
    };

    std::vector<std::string> segments = try_base(current_file_dir);
    if (!segments.empty()) return segments;
    return try_base(stdlib_dir);
}

bool AlreadyImported(const std::string& bound_name, const std::vector<ImportStatement>& imports) {
    for (const auto& stmt : imports) {
        if (stmt.bound_name == bound_name) return true;
    }
    return false;
}

void ResolveUnresolvedAndMissingImports(const std::vector<SymbolUse>& uses, bool is_class_target,
                                         const std::string& current_file_dir, const std::string& stdlib_dir,
                                         const ClassIndex& class_index, const FunctionIndex& function_index,
                                         const ClassIndex* workspace_classes, const FunctionIndex* workspace_functions,
                                         const std::vector<ImportStatement>& imports,
                                         const std::unordered_set<std::string>& local_variable_names,
                                         std::vector<Diagnostic>& out) {
    std::unordered_set<std::string> already_reported;

    for (const auto& use : uses) {
        if (local_variable_names.count(use.name)) continue;
        if (KeywordAndBuiltinNames().count(use.name)) continue;

        const bool known_locally =
            is_class_target ? class_index.Find(use.name) != nullptr : function_index.Find(use.name) != nullptr;
        if (known_locally) continue;

        const std::string dedup_key = use.name + (is_class_target ? "#class" : "#func");
        if (!already_reported.insert(dedup_key).second) continue;

        std::string workspace_source_file;
        if (is_class_target && workspace_classes) {
            if (const ClassInfo* info = workspace_classes->Find(use.name)) workspace_source_file = info->source_file;
        } else if (!is_class_target && workspace_functions) {
            if (const FunctionSignature* sig = workspace_functions->Find(use.name)) {
                workspace_source_file = sig->source_file;
            }
        }

        if (!workspace_source_file.empty()) {
            if (AlreadyImported(use.name, imports)) continue;

            std::vector<std::string> module_path = ImportPathFor(workspace_source_file, current_file_dir, stdlib_dir);
            if (module_path.empty()) continue;

            std::string joined;
            for (size_t k = 0; k < module_path.size(); ++k) {
                if (k) joined += ".";
                joined += module_path[k];
            }

            Diagnostic diag;
            diag.severity = Severity::Info;
            diag.kind = Kind::MissingImport;
            diag.line = use.line;
            diag.column_start = use.column_start;
            diag.column_end = use.column_end;
            diag.symbol = use.name;
            diag.import_segments = module_path;
            diag.message = "'" + use.name + "' se define en '" + joined + "', pero ese módulo no está importado.";
            out.push_back(std::move(diag));
            continue;
        }

        Diagnostic diag;
        diag.severity = Severity::Error;
        diag.kind = Kind::UnresolvedSymbol;
        diag.line = use.line;
        diag.column_start = use.column_start;
        diag.column_end = use.column_end;
        diag.symbol = use.name;
        diag.message = is_class_target ? "No se encontró la clase '" + use.name + "'."
                                        : "No se encontró la función '" + use.name + "'.";
        out.push_back(std::move(diag));
    }
}

void DetectUnusedImports(const ScanResult& scan, std::vector<Diagnostic>& out) {
    for (const auto& stmt : scan.imports) {
        if (!stmt.has_alias && stmt.segments.size() == 1) continue;
        auto occ_it = scan.occurrences.find(stmt.bound_name);
        const bool used = occ_it != scan.occurrences.end() &&
                           std::any_of(occ_it->second.begin(), occ_it->second.end(), [&](const Occurrence& o) {
                               return !IsInSpan(o.line, scan.import_line_spans);
                           });
        auto member_it = scan.member_occurrences.find(stmt.bound_name);
        const bool used_as_member = member_it != scan.member_occurrences.end() && !member_it->second.empty();
        if (used || used_as_member) continue;

        Diagnostic diag;
        diag.severity = Severity::Warning;
        diag.kind = Kind::UnusedImport;
        diag.line = stmt.line;
        diag.column_start = stmt.column_start;
        diag.column_end = stmt.column_end;
        diag.symbol = stmt.bound_name;
        diag.message = "El import '" + stmt.bound_name + "' no se usa en este archivo.";
        out.push_back(std::move(diag));
    }
}

void DetectUnusedVariables(const std::string& text, const ScanResult& scan, std::vector<Diagnostic>& out) {
    std::unordered_set<std::string> variable_names = languages::ScanKnownVariableNames(text);
    for (const auto& name : variable_names) {
        auto it = scan.occurrences.find(name);
        if (it == scan.occurrences.end() || it->second.empty()) continue;
        if (it->second.size() > 1) continue;

        const Occurrence& only = it->second.front();
        Diagnostic diag;
        diag.severity = Severity::Hint;
        diag.kind = Kind::UnusedVariable;
        diag.line = only.line;
        diag.column_start = only.column_start;
        diag.column_end = only.column_end;
        diag.symbol = name;
        diag.message = "La variable '" + name + "' se asigna pero nunca se lee.";
        out.push_back(std::move(diag));
    }
}

void DetectUnusedPrivateMembers(const ClassIndex& class_index, const ScanResult& scan,
                                 std::vector<Diagnostic>& out) {
    for (const auto& [class_name, info] : class_index.Classes()) {
        if (!info.source_file.empty()) continue;

        auto check_member = [&](const std::string& name, int line) {
            auto member_it = scan.member_occurrences.find(name);
            const bool used_as_member = member_it != scan.member_occurrences.end() && !member_it->second.empty();
            if (used_as_member) return;

            auto occ_it = scan.occurrences.find(name);
            const bool used_bare =
                occ_it != scan.occurrences.end() &&
                std::any_of(occ_it->second.begin(), occ_it->second.end(),
                             [&](const Occurrence& o) { return o.line != line; });
            if (used_bare) return;

            Diagnostic diag;
            diag.severity = Severity::Warning;
            diag.kind = Kind::UnusedPrivateMember;
            diag.line = line;
            diag.column_start = 0;
            diag.column_end = 0;
            diag.symbol = name;
            diag.message = "El miembro privado '" + class_name + "." + name + "' nunca se usa.";
            out.push_back(std::move(diag));
        };

        for (const auto& [name, method] : info.methods) {
            if (method.is_private && !method.is_abstract) check_member(name, method.signature.line);
        }
        for (const auto& [name, attr] : info.attributes) {
            if (attr.is_private) check_member(name, attr.line);
        }
    }
}

std::string LeadingWord(const std::string& line_text) {
    size_t i = 0;
    SkipInlineWhitespace(line_text, i);
    if (i >= line_text.size() || !IsIdentStart(line_text[i])) return "";
    return ReadIdent(line_text, i);
}

bool IsBlankOrComment(const std::string& line_text) {
    size_t i = 0;
    SkipInlineWhitespace(line_text, i);
    return i >= line_text.size() || line_text[i] == '#';
}

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

const std::unordered_set<std::string> kBranchContinuationWords = {"else", "elif", "catch", "finally"};
const std::unordered_set<std::string> kTerminatorWords = {"return", "break", "continue", "raise"};

void DetectUnreachableCode(const std::string& text, std::vector<Diagnostic>& out) {
    std::vector<std::string> lines = SplitLines(text);
    std::vector<bool> terminated_at_depth = {false};
    int depth = 0;

    for (int line_no = 0; line_no < static_cast<int>(lines.size()); ++line_no) {
        const std::string& line_text = lines[static_cast<size_t>(line_no)];
        if (IsBlankOrComment(line_text)) continue;

        const std::string leading = LeadingWord(line_text);

        if (leading == "end") {
            if (depth > 0) {
                --depth;
                terminated_at_depth.resize(static_cast<size_t>(depth) + 1);
            }
            continue;
        }

        if (kBranchContinuationWords.count(leading)) {
            terminated_at_depth[static_cast<size_t>(depth)] = false;
            continue;
        }

        if (terminated_at_depth[static_cast<size_t>(depth)]) {
            Diagnostic diag;
            diag.severity = Severity::Warning;
            diag.kind = Kind::UnreachableCode;
            diag.line = line_no;
            diag.column_start = 0;
            diag.column_end = static_cast<int>(TrimTrailing(line_text).size());
            diag.message = "Código inalcanzable: nunca se llega a ejecutar.";
            out.push_back(std::move(diag));
            terminated_at_depth[static_cast<size_t>(depth)] = false;
        }

        if (kTerminatorWords.count(leading)) {
            terminated_at_depth[static_cast<size_t>(depth)] = true;
        } else if (IsBlockKeyword(leading)) {
            ++depth;
            if (static_cast<size_t>(depth) >= terminated_at_depth.size()) terminated_at_depth.push_back(false);
            else terminated_at_depth[static_cast<size_t>(depth)] = false;
        }
    }
}

}

std::vector<Diagnostic> ComputeDiagnostics(const std::string& text, const std::string& current_file_dir,
                                            const std::string& stdlib_dir, const ClassIndex& class_index,
                                            const FunctionIndex& function_index,
                                            const ClassIndex* workspace_classes,
                                            const FunctionIndex* workspace_functions) {
    std::vector<Diagnostic> out;
    ScanResult scan = ScanText(text);
    std::unordered_set<std::string> local_variable_names = languages::ScanKnownVariableNames(text);

    ResolveUnresolvedAndMissingImports(scan.calls, false, current_file_dir, stdlib_dir, class_index, function_index,
                                        workspace_classes, workspace_functions, scan.imports, local_variable_names,
                                        out);
    ResolveUnresolvedAndMissingImports(scan.new_exprs, true, current_file_dir, stdlib_dir, class_index,
                                        function_index, workspace_classes, workspace_functions, scan.imports,
                                        local_variable_names, out);
    ResolveUnresolvedAndMissingImports(scan.as_types, true, current_file_dir, stdlib_dir, class_index,
                                        function_index, workspace_classes, workspace_functions, scan.imports,
                                        local_variable_names, out);

    DetectUnusedImports(scan, out);
    DetectUnusedVariables(text, scan, out);
    DetectUnusedPrivateMembers(class_index, scan, out);
    DetectUnreachableCode(text, out);

    return out;
}

std::vector<InlayHint> ComputeInlayHints(const std::string& text, const VariableTypeIndex& variable_types,
                                          const FunctionIndex& function_index, const ClassIndex* workspace_classes,
                                          const FunctionIndex* workspace_functions) {
    (void)workspace_classes;
    std::vector<InlayHint> hints;
    std::unordered_set<std::string> seen;

    ScanResult scan = ScanText(text);
    std::unordered_set<std::string> variable_names = languages::ScanKnownVariableNames(text);
    std::vector<std::string> lines = SplitLines(text);

    for (const auto& name : variable_names) {
        auto occ_it = scan.occurrences.find(name);
        if (occ_it == scan.occurrences.end()) continue;

        for (const Occurrence& occ : occ_it->second) {
            if (variable_types.DeclarationLine(name, occ.offset) != occ.line) continue;

            const std::string type = variable_types.TypeOf(name, occ.offset);
            if (type.empty()) continue;

            const std::string key = name + "@" + std::to_string(occ.line);
            if (!seen.insert(key).second) continue;

            if (occ.line >= static_cast<int>(lines.size())) continue;
            const std::string& line_text = lines[static_cast<size_t>(occ.line)];
            if (line_text.find(" as ") != std::string::npos) continue;

            InlayHint hint;
            hint.line = occ.line;
            hint.column = occ.column_end;
            hint.text = ": " + type;
            hints.push_back(std::move(hint));
            break;
        }
    }

    for (int line_no = 0; line_no < static_cast<int>(lines.size()); ++line_no) {
        const std::string& line_text = lines[static_cast<size_t>(line_no)];
        size_t i = 0;
        while (i < line_text.size()) {
            if (!IsIdentStart(line_text[i])) { ++i; continue; }
            const size_t word_start = i;
            std::string word = ReadIdent(line_text, i);
            size_t after = i;
            SkipInlineWhitespace(line_text, after);
            if (after >= line_text.size() || line_text[after] != '(') continue;
            if (word_start > 0 && line_text[word_start - 1] == '.') continue;

            size_t before_word = word_start;
            while (before_word > 0 && (line_text[before_word - 1] == ' ' || line_text[before_word - 1] == '\t')) --before_word;
            if (before_word >= 4 && line_text.compare(before_word - 4, 4, "func") == 0 &&
                (before_word == 4 || !IsIdentChar(line_text[before_word - 5]))) {
                continue;
            }

            const FunctionSignature* sig = function_index.Find(word);
            if (!sig && workspace_functions) sig = workspace_functions->Find(word);
            if (!sig || sig->params.empty()) continue;

            size_t arg_pos = after + 1;
            std::string raw_args;
            int depth = 1;
            size_t k = arg_pos;
            while (k < line_text.size() && depth > 0) {
                if (line_text[k] == '(') ++depth;
                else if (line_text[k] == ')') { --depth; if (depth == 0) break; }
                raw_args += line_text[k];
                ++k;
            }
            std::vector<std::string> args = SplitParams(raw_args);
            size_t cursor = arg_pos;
            for (size_t a = 0; a < args.size() && a < sig->params.size(); ++a) {
                while (cursor < line_text.size() && (line_text[cursor] == ' ' || line_text[cursor] == '\t')) ++cursor;
                const std::string param_name = ParamBaseName(sig->params[a]);
                if (!param_name.empty()) {
                    InlayHint hint;
                    hint.line = line_no;
                    hint.column = static_cast<int>(cursor);
                    hint.text = param_name + ":";
                    hints.push_back(std::move(hint));
                }
                cursor += args[a].size() + 1;
            }
        }
    }

    return hints;
}

}