#include "panels/editor_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "branding/logo_texture.h"
#include "fonts/embedded_font.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "languages/avalang_language.h"
#include "languages/block_scanner.h"
#include "languages/code_formatter.h"
#include "languages/diagnostics_engine.h"
#include "languages/keyword_docs.h"
#include "languages/lexer_utils.h"
#include "languages/member_access_resolver.h"
#include "palette.h"
#include "panels/designer_canvas.h"
#include "panels/syntax_highlight.h"
#include "parser/AvauiWriter.h"
#include "shortcuts/shortcut_registry.h"
#include "util/i18n.h"

namespace studio {

using namespace lexer;

namespace {

std::string TrFormat(const std::string& key, std::initializer_list<std::string> args) {
    std::string result = util::Tr(key);
    for (const std::string& arg : args) {
        const size_t pos = result.find("%s");
        if (pos == std::string::npos) break;
        result = result.substr(0, pos) + arg + result.substr(pos + 2);
    }
    return result;
}

std::string TrFormat(const std::string& key, const std::string& arg) { return TrFormat(key, {arg}); }

constexpr float kEditorMinZoom = 0.5f;
constexpr float kEditorMaxZoom = 3.0f;
constexpr float kEditorWheelZoomStep = 0.1f;

constexpr float kEditorBaseFontSizePx = 16.0f;

float ClampEditorZoom(float zoom) { return std::clamp(zoom, kEditorMinZoom, kEditorMaxZoom); }

std::string DirOf(const std::string& file_path) {
    if (file_path.empty()) return "";
    auto pos = file_path.find_last_of("/\\");
    return pos == std::string::npos ? "" : file_path.substr(0, pos);
}

std::string BaseNameOf(const std::string& file_path) {
    if (file_path.empty()) return "";
    auto pos = file_path.find_last_of("/\\");
    return pos == std::string::npos ? file_path : file_path.substr(pos + 1);
}

void RebuildAutocompleteTrie(EditorTab& tab) {
    tab.autocomplete_trie.clear();

    if (const TextEditor::Language* language = tab.editor.GetLanguage()) {
        for (const auto& word : language->keywords) tab.autocomplete_trie.insert(word);
        for (const auto& word : language->declarations) tab.autocomplete_trie.insert(word);
        for (const auto& word : language->identifiers) tab.autocomplete_trie.insert(word);
    }

    tab.editor.IterateIdentifiers([&tab](const std::string& identifier) {
        tab.autocomplete_trie.insert(identifier);
    });

    for (const auto& [name, sig] : tab.function_index.Signatures()) {
        tab.autocomplete_trie.insert(name);
    }

    for (const auto& [name, info] : tab.class_index.Classes()) {
        (void)info;
        tab.autocomplete_trie.insert(name);
    }

    // Clases nativas del runtime (Application, etc.) -- nunca aparecen
    // como `class X` en el codigo, asi que tab.class_index (que escanea el
    // texto) jamas las ve. Ver comentario en languages::NativeClassNames().
    for (const auto& name : languages::NativeClassNames()) {
        tab.autocomplete_trie.insert(name);
    }
}

bool HeritageChainDeclares(const ClassIndex& class_index, const ClassIndex* fallback, const std::string& start,
                            const std::string& member_name, std::unordered_set<std::string>& visited) {
    if (!visited.insert(start).second) return false;
    const ClassInfo* info = class_index.Find(start);
    if (!info && fallback) info = fallback->Find(start);
    if (!info) return false;
    if (info->methods.count(member_name)) return true;
    for (const auto& parent : info->heritage_names) {
        if (HeritageChainDeclares(class_index, fallback, parent, member_name, visited)) return true;
    }
    return false;
}

void RebuildIncompleteInterfaces(EditorTab& tab, const ClassIndex* fallback) {
    tab.incomplete_interfaces.clear();
    for (const auto& [name, info] : tab.class_index.Classes()) {
        if (info.is_interface || !info.source_file.empty()) continue;
        std::vector<ClassMember> missing = tab.class_index.MissingInterfaceMembers(name, fallback);
        if (missing.empty()) continue;

        std::vector<std::string> order;
        std::unordered_map<std::string, std::vector<ClassMember>> by_heritage;
        for (auto& member : missing) {
            for (const auto& heritage : info.heritage_names) {
                std::unordered_set<std::string> visited;
                if (!HeritageChainDeclares(tab.class_index, fallback, heritage, member.name, visited)) continue;
                if (by_heritage.find(heritage) == by_heritage.end()) order.push_back(heritage);
                by_heritage[heritage].push_back(member);
                break;
            }
        }

        for (const auto& heritage : order) {
            tab.incomplete_interfaces.push_back({name, heritage, info.line, std::move(by_heritage[heritage])});
        }
    }
}

ImU32 DiagnosticSeverityColor(diagnostics::Severity severity) {
    switch (severity) {
        case diagnostics::Severity::Error: return palette::U32FromHex(palette::kError);
        case diagnostics::Severity::Warning: return palette::U32FromHex(palette::kWarning);
        case diagnostics::Severity::Info: return palette::U32FromHex(palette::kInfo);
        case diagnostics::Severity::Hint: return palette::U32FromHex(palette::kTextDisabled);
    }
    return palette::U32FromHex(palette::kTextDisabled);
}

void RebuildIndexAndTrie(EditorState& state, EditorTab& tab) {
    ImportFileCache import_cache;
    const std::string dir = DirOf(tab.file_path);
    tab.function_index.Rebuild(tab.GetText(), dir, &import_cache, tab.modules_path);
    tab.class_index.Rebuild(tab.GetText(), dir, &import_cache, tab.modules_path);
    if (state.log_bridge) {
        std::string names;
        for (const auto& [name, info] : tab.class_index.Classes()) {
            (void)info;
            names += name + " ";
        }
    }
    std::shared_ptr<const WorkspaceIndex::Snapshot> workspace = state.workspace_index.CurrentSnapshot();
    RebuildIncompleteInterfaces(tab, &workspace->classes);

    std::unordered_set<std::string> interface_names;
    for (const auto& [name, info] : tab.class_index.Classes()) {
        if (info.is_interface) interface_names.insert(name);
    }
    std::unordered_set<std::string> removed_interface_names;
    for (const auto& name : tab.known_interface_names) {
        if (!interface_names.count(name)) removed_interface_names.insert(name);
    }
    std::unordered_set<std::string> added_interface_names;
    for (const auto& name : interface_names) {
        if (!tab.known_interface_names.count(name)) added_interface_names.insert(name);
    }
    languages::UpdateKnownInterfaceNames(removed_interface_names, added_interface_names);
    tab.known_interface_names = std::move(interface_names);

    std::unordered_set<std::string> class_names;
    for (const auto& [name, info] : tab.class_index.Classes()) {
        if (!info.is_interface) class_names.insert(name);
    }
    std::unordered_set<std::string> removed_class_names;
    for (const auto& name : tab.known_class_names) {
        if (!class_names.count(name)) removed_class_names.insert(name);
    }
    std::unordered_set<std::string> added_class_names;
    for (const auto& name : class_names) {
        if (!tab.known_class_names.count(name)) added_class_names.insert(name);
    }
    languages::UpdateKnownClassNames(removed_class_names, added_class_names);
    tab.known_class_names = std::move(class_names);

    tab.variable_type_index.Rebuild(tab.GetText(), tab.class_index, tab.function_index, &workspace->classes,
                                     &workspace->functions);

    tab.diagnostics = diagnostics::ComputeDiagnostics(tab.GetText(), dir, tab.modules_path, state.project_root,
                                                       tab.class_index, tab.function_index, &workspace->classes,
                                                       &workspace->functions);

    tab.inlay_hints = diagnostics::ComputeInlayHints(tab.GetText(), tab.variable_type_index, tab.function_index,
                                                      &workspace->classes, &workspace->functions);

    std::unordered_set<std::string> variable_names = languages::ScanKnownVariableNames(tab.GetText());
    std::unordered_set<std::string> removed_variable_names;
    for (const auto& name : tab.known_variable_names) {
        if (!variable_names.count(name)) removed_variable_names.insert(name);
    }
    std::unordered_set<std::string> added_variable_names;
    for (const auto& name : variable_names) {
        if (!tab.known_variable_names.count(name)) added_variable_names.insert(name);
    }
    languages::UpdateKnownVariableNames(removed_variable_names, added_variable_names);
    tab.known_variable_names = std::move(variable_names);

    tab.fold_index.Rebuild(tab.GetText());
    for (auto it = tab.folded_lines.begin(); it != tab.folded_lines.end();) {
        it = tab.fold_index.RangeStartingAt(*it) ? std::next(it) : tab.folded_lines.erase(it);
    }
    RebuildAutocompleteTrie(tab);
    tab.index_dirty = false;
}

constexpr double kIndexRebuildDebounceSeconds = 0.2;

void MaybeRebuildIndex(EditorState& state, EditorTab& tab) {
    if (!tab.index_dirty) return;
    if (ImGui::GetTime() - tab.last_edit_time < kIndexRebuildDebounceSeconds) return;
    RebuildIndexAndTrie(state, tab);
}

void MaybeAutoFormatOnType(EditorState& state, EditorTab& tab) {
    if (!tab.format_pending) return;
    if (ImGui::GetTime() - tab.last_edit_time < kIndexRebuildDebounceSeconds) return;
    tab.format_pending = false;
    FormatTab(state, tab);
}

std::string TextBeforeCursor(EditorTab& tab, const TextEditor::CursorPosition& pos) {
    return tab.editor.GetSectionText(pos.line, 0, pos.line, pos.column);
}

constexpr int kParamHintMaxLookbackLines = 30;

std::string ParamHintTextBeforeCursor(EditorTab& tab, const TextEditor::CursorPosition& pos) {
    const int start_line = std::max(0, pos.line - kParamHintMaxLookbackLines);
    return tab.editor.GetSectionText(start_line, 0, pos.line, pos.column);
}

bool ResolveVisibleMembers(EditorState& state, EditorTab& tab, int cursor_line, const std::string& before,
                           MemberAccessContext& out_ctx, std::vector<ClassMember>& out_members) {
    std::shared_ptr<const WorkspaceIndex::Snapshot> workspace = state.workspace_index.CurrentSnapshot();
    if (!ResolveMemberAccess(tab.GetText(), cursor_line, before, tab.class_index, tab.variable_type_index,
                              out_ctx, &workspace->classes)) {
        return false;
    }

    out_members = tab.class_index.FlattenedMembers(out_ctx.class_name, &workspace->classes);
    if (out_members.empty()) return false;
    out_members = ClassIndex::FilterForAccess(out_members, out_ctx.kind, out_ctx.viewer_class);
    if (out_members.empty()) return false;

    out_members.erase(
        std::remove_if(out_members.begin(), out_members.end(),
                        [](const ClassMember& m) { return m.is_method && m.name == m.declared_in; }),
        out_members.end());
    return !out_members.empty();
}

std::string MemberSuggestionLabel(const ClassMember& member) {
    if (member.is_method && member.signature) return member.signature->display;
    if (!member.declared_type.empty()) return member.name + " : " + member.declared_type;
    return member.name;
}

std::string ToLowerAscii(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

constexpr size_t kAutocompleteLimit = 20;
constexpr size_t kAutocompleteFuzzyMinChars = 3;

// prepends a one-char kind tag ('M'ethod/'F'ield/'C'lass/'K'eyword/'V'ariable) understood by
// patches/imguicolortextedit_suggestion_icons.patch, which draws a matching icon and strips the tag
std::string TagSuggestion(char kind, const std::string& text) {
    return std::string(1, '\x01') + kind + text;
}

char ClassifyTrieWord(EditorTab& tab, const std::string& word) {
    if (tab.function_index.Find(word) != nullptr) return 'M';
    if (tab.class_index.Classes().count(word)) return 'C';
    if (languages::AvaLang()->keywords.count(word)) return 'K';
    return 'V';
}

void PopulateGeneralSuggestions(EditorTab& tab, TextEditor::AutoCompleteState& ac_state) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> ordered;

    const std::string search_term_lower = ToLowerAscii(ac_state.searchTerm);

    auto add_filtered = [&](char kind, const std::string& name) {
        if (ordered.size() >= kAutocompleteLimit) return;
        if (!search_term_lower.empty()) {
            const std::string name_lower = ToLowerAscii(name);
            if (name_lower.compare(0, search_term_lower.size(), search_term_lower) != 0) return;
        }
        if (seen.insert(name).second) ordered.push_back(TagSuggestion(kind, name));
    };

    auto add_raw = [&](char kind, const std::string& name) {
        if (ordered.size() >= kAutocompleteLimit) return;
        if (seen.insert(name).second) ordered.push_back(TagSuggestion(kind, name));
    };

    auto callable_label = [&](const std::string& name, bool is_callable) {
        return is_callable ? name + "()" : name;
    };

    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);

    std::vector<std::string> own_variables;
    std::vector<ClassMember> own_members;
    ResolveOwnScopeSuggestions(tab.GetText(), pos.line, before, tab.class_index, tab.variable_type_index,
                                own_variables, own_members);

    for (const auto& name : own_variables) add_filtered('V', name);
    for (const auto& member : own_members) {
        add_filtered(member.is_method ? 'M' : 'F', callable_label(member.name, member.is_method));
    }

    std::vector<std::string> prefix_matches;
    tab.autocomplete_trie.findSuggestions(prefix_matches, ac_state.searchTerm, kAutocompleteLimit,
                                           /*maxSkippedLetters=*/0);
    for (const auto& word : prefix_matches) {
        char kind = ClassifyTrieWord(tab, word);
        add_raw(kind, callable_label(word, kind == 'M'));
    }

    if (ordered.size() < kAutocompleteLimit && ac_state.searchTerm.size() >= kAutocompleteFuzzyMinChars) {
        std::vector<std::string> fuzzy_matches;
        tab.autocomplete_trie.findSuggestions(fuzzy_matches, ac_state.searchTerm, kAutocompleteLimit,
                                               /*maxSkippedLetters=*/2);
        for (const auto& word : fuzzy_matches) {
            char kind = ClassifyTrieWord(tab, word);
            add_raw(kind, callable_label(word, kind == 'M'));
        }
    }

    ac_state.suggestions = std::move(ordered);
}

bool PopulateMemberSuggestions(EditorState& state, EditorTab& tab, TextEditor::AutoCompleteState& ac_state) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);

    MemberAccessContext ctx;
    std::vector<ClassMember> members;
    if (!ResolveVisibleMembers(state, tab, pos.line, before, ctx, members)) return false;

    // right after the dot, with nothing typed yet, DrawDotCompletionPopup (the hand-drawn
    // popup right below the caret) already shows this exact same member list -- if we also let
    // this library-driven popup open here (it does, since the dot itself now triggers activation,
    // see patches/imguicolortextedit_dot_trigger.patch) the two popups would stack on top of each
    // other. So we suppress ourselves for this one activation; the moment the user types the
    // first letter, DrawDotCompletionPopup deactivates (its own "before.back() == '.'" check
    // fails) and this popup takes over normally for filtering.
    if (!before.empty() && before.back() == '.') {
        ac_state.suppressPopup = true;
        return true;
    }

    const std::string search_term_lower = ToLowerAscii(ac_state.searchTerm);

    std::vector<std::string> suggestions;
    for (const auto& member : members) {
        if (!search_term_lower.empty()) {
            const std::string name_lower = ToLowerAscii(member.name);
            if (name_lower.compare(0, search_term_lower.size(), search_term_lower) != 0) continue;
        }

        std::string insert_text = member.name;
        if (member.is_method) insert_text += "()";
        suggestions.push_back(TagSuggestion(member.is_method ? 'M' : 'F', insert_text));
    }

    if (suggestions.empty()) return false;
    ac_state.suggestions = std::move(suggestions);
    return true;
}

bool ExtractImportPathSegments(const std::string& before, std::vector<std::string>& out_segments) {
    size_t i = before.size();
    while (i > 0 && IsIdentChar(before[i - 1])) --i;

    size_t p = 0;
    while (p < i && (before[p] == ' ' || before[p] == '\t')) ++p;

    constexpr size_t kKeywordLen = 6;
    if (i - p < kKeywordLen || before.compare(p, kKeywordLen, "import") != 0) return false;
    p += kKeywordLen;
    if (p >= i || (before[p] != ' ' && before[p] != '\t')) return false;
    while (p < i && (before[p] == ' ' || before[p] == '\t')) ++p;

    out_segments.clear();
    while (p < i) {
        size_t start = p;
        while (p < i && IsIdentChar(before[p])) ++p;
        if (p == start) return false;
        out_segments.push_back(before.substr(start, p - start));
        if (p < i && before[p] == '.') { ++p; continue; }
        if (p == i) break;
        return false;
    }
    return true;
}

void CollectImportEntries(const std::filesystem::path& dir, std::unordered_set<std::string>& out) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const std::string name = entry.path().filename().string();

        if (entry.is_directory(ec)) {
            if (!name.empty() && IsIdentStart(name[0]) && std::all_of(name.begin(), name.end(), IsIdentChar)) {
                out.insert(name);
            }
            continue;
        }

        constexpr size_t kExtLen = 4;
        if (name.size() <= kExtLen || name.compare(name.size() - kExtLen, kExtLen, ".ava") != 0) continue;

        std::string stem = name.substr(0, name.size() - kExtLen);
        if (!stem.empty() && IsIdentStart(stem[0]) && std::all_of(stem.begin(), stem.end(), IsIdentChar)) {
            out.insert(stem);
        }
    }
}

bool PopulateImportPathSuggestions(EditorState& state, EditorTab& tab, TextEditor::AutoCompleteState& ac_state) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);

    std::vector<std::string> segments;
    if (!ExtractImportPathSegments(before, segments)) return false;

    namespace fs = std::filesystem;
    std::string relative;
    for (const auto& segment : segments) {
        if (!relative.empty()) relative += "/";
        relative += segment;
    }

    auto collect_base = [&](const std::string& base, std::unordered_set<std::string>& out) {
        if (base.empty()) return;
        CollectImportEntries(relative.empty() ? fs::path(base) : fs::path(base) / relative, out);
    };

    std::unordered_set<std::string> entries;
    const std::string dir = DirOf(tab.file_path);
    collect_base(dir, entries);
    if (state.project_root != dir) collect_base(state.project_root, entries);
    collect_base(tab.modules_path, entries);

    const std::string search_term_lower = ToLowerAscii(ac_state.searchTerm);
    std::vector<std::string> suggestions;
    for (const auto& name : entries) {
        if (!search_term_lower.empty()) {
            const std::string name_lower = ToLowerAscii(name);
            if (name_lower.compare(0, search_term_lower.size(), search_term_lower) != 0) continue;
        }
        suggestions.push_back(name);
    }

    if (suggestions.empty()) return false;
    std::sort(suggestions.begin(), suggestions.end());
    if (suggestions.size() > kAutocompleteLimit) suggestions.resize(kAutocompleteLimit);
    ac_state.suggestions = std::move(suggestions);
    return true;
}

struct CallContext {
    std::string function_name;
    int active_param = 0;
};

bool FindEnclosingCall(const std::string& line_before_cursor, CallContext& out) {
    int depth = 0;
    int comma_count = 0;
    for (int i = static_cast<int>(line_before_cursor.size()) - 1; i >= 0; --i) {
        char c = line_before_cursor[i];
        if (c == ')') { ++depth; continue; }
        if (c == '(') {
            if (depth > 0) { --depth; continue; }
            int j = i - 1;
            while (j >= 0 && (line_before_cursor[j] == ' ' || line_before_cursor[j] == '\t')) --j;
            int end = j + 1;
            while (j >= 0 && (std::isalnum(static_cast<unsigned char>(line_before_cursor[j])) ||
                               line_before_cursor[j] == '_')) {
                --j;
            }
            int start = j + 1;
            if (start >= end) return false;
            out.function_name = line_before_cursor.substr(start, end - start);
            out.active_param = comma_count;
            return true;
        }
        if (c == ',' && depth == 0) ++comma_count;
    }
    return false;
}

std::string WordAtColumn(const std::string& line_text, int column) {
    const int len = static_cast<int>(line_text.size());
    int start = std::clamp(column, 0, len);
    int end = start;
    while (start > 0 && IsIdentChar(line_text[start - 1])) --start;
    while (end < len && IsIdentChar(line_text[end])) ++end;
    return line_text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
}

bool FindExternalAttributeAssignment(EditorTab& tab, const std::string& class_name, const std::string& attr_name,
                                      int& out_line) {
    const std::string text = tab.GetText();
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
        if (!IsIdentStart(c)) { ++i; continue; }

        const size_t ident_start = i;
        std::string ident = ReadIdent(text, i);
        if (ident == "this") continue;

        size_t k = i;
        SkipInlineWhitespace(text, k);
        if (k >= text.size() || text[k] != '.') continue;
        ++k;
        SkipInlineWhitespace(text, k);
        if (k >= text.size() || !IsIdentStart(text[k])) continue;

        std::string member = ReadIdent(text, k);
        if (member != attr_name) continue;

        size_t m = k;
        SkipInlineWhitespace(text, m);
        const bool is_assignment = m < text.size() && text[m] == '=' && (m + 1 >= text.size() || text[m + 1] != '=');
        if (!is_assignment) continue;

        if (tab.variable_type_index.TypeOf(ident, ident_start) != class_name) continue;

        out_line = LineAt(text, ident_start);
        return true;
    }
    return false;
}

struct DefinitionTarget {
    bool same_file = true;
    std::string file_path;
    int line = 0;
    int column = 0;
};

size_t TextOffsetForPosition(const std::string& text, int line, int column) {
    size_t offset = 0;
    int current_line = 0;
    while (current_line < line) {
        size_t newline = text.find('\n', offset);
        if (newline == std::string::npos) return text.size();
        offset = newline + 1;
        ++current_line;
    }
    return std::min(offset + static_cast<size_t>(std::max(0, column)), text.size());
}

bool ResolveDefinitionTarget(EditorState& state, EditorTab& tab, const TextEditor::CursorPosition& pos,
                              DefinitionTarget& out) {

    const std::string line = tab.editor.GetLineText(pos.line);
    const int column = std::clamp(pos.column, 0, static_cast<int>(line.size()));
    int word_start = column;
    int word_end = column;
    while (word_start > 0 && IsIdentChar(line[word_start - 1])) --word_start;
    while (word_end < static_cast<int>(line.size()) && IsIdentChar(line[word_end])) ++word_end;
    const std::string word = line.substr(word_start, word_end - word_start);
    if (word.empty() || !IsIdentStart(word[0])) return false;

    std::shared_ptr<const WorkspaceIndex::Snapshot> workspace = state.workspace_index.CurrentSnapshot();

    const std::string before_word = line.substr(0, word_start);
    if (!before_word.empty() && before_word.back() == '.') {
        MemberAccessContext ctx;
        if (ResolveMemberAccess(tab.GetText(), pos.line, before_word, tab.class_index, tab.variable_type_index,
                                 ctx, &workspace->classes)) {
            std::vector<ClassMember> members =
                ClassIndex::FilterForAccess(tab.class_index.FlattenedMembers(ctx.class_name, &workspace->classes),
                                             ctx.kind, ctx.viewer_class);
            for (const auto& member : members) {
                if (member.name != word || member.line <= 0) continue;
                const ClassInfo* owner = tab.class_index.Find(member.declared_in);
                if (!owner) owner = workspace->classes.Find(member.declared_in);
                if (!owner) return false;
                out.file_path = member.is_method && member.signature ? member.signature->source_file
                                                                      : owner->source_file;
                out.same_file = out.file_path.empty();
                out.line = member.line;
                out.column = 0;
                return true;
            }

            int external_line = 0;
            if (FindExternalAttributeAssignment(tab, ctx.class_name, word, external_line)) {
                out.file_path.clear();
                out.same_file = true;
                out.line = external_line;
                out.column = 0;
                return true;
            }
        }
    }

    if (const int var_line = tab.variable_type_index.DeclarationLine(
            word, TextOffsetForPosition(tab.GetText(), pos.line, pos.column));
        var_line >= 0) {
        out.file_path.clear();
        out.same_file = true;
        out.line = var_line;
        out.column = 0;
        return true;
    }

    if (const ClassInfo* cls = tab.class_index.Find(word)) {
        out.file_path = cls->source_file;
        out.same_file = out.file_path.empty();
        out.line = cls->line;
        out.column = 0;
        return true;
    }

    if (const FunctionSignature* sig = tab.function_index.Find(word)) {
        if (sig->is_builtin) return false;
        out.file_path = sig->source_file;
        out.same_file = out.file_path.empty();
        out.line = sig->line;
        out.column = 0;
        return true;
    }

    if (const ClassInfo* cls = workspace->classes.Find(word)) {
        if (cls->source_file.empty()) return false;
        out.file_path = cls->source_file;
        out.same_file = false;
        out.line = cls->line;
        out.column = 0;
        return true;
    }

    if (const FunctionSignature* sig = workspace->functions.Find(word)) {
        if (sig->is_builtin || sig->source_file.empty()) return false;
        out.file_path = sig->source_file;
        out.same_file = false;
        out.line = sig->line;
        out.column = 0;
        return true;
    }

    return false;
}

void JumpToDefinition(EditorState& state, EditorTab& tab, const DefinitionTarget& target) {
    if (target.same_file) {
        tab.editor.SetCursor(target.line, target.column);
        tab.editor.ScrollToLine(target.line, TextEditor::Scroll::alignMiddle);
        return;
    }
    state.goto_definition_requested = true;
    state.goto_definition_file = target.file_path;
    state.goto_definition_line = target.line + 1;
    state.goto_definition_column = target.column + 1;
}

constexpr float kHintContentWidth = 380.0f;

void DrawHintBadge(const char* label, ImU32 bg_color, ImU32 text_color) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const ImVec2 padding(7.0f, 3.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 box_max(origin.x + text_size.x + padding.x * 2.0f, origin.y + text_size.y + padding.y * 2.0f);
    draw_list->AddRectFilled(origin, box_max, bg_color, 3.0f);
    draw_list->AddText(ImVec2(origin.x + padding.x, origin.y + padding.y), text_color, label);
    ImGui::Dummy(ImVec2(box_max.x - origin.x, box_max.y - origin.y));
}

void DrawHintSectionLabel(const char* text) {
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", text);
}

void DrawHintCodeBox(const std::string& text, ImU32 border_color) {
    const ImVec2 padding(10.0f, 8.0f);
    const ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), nullptr, false, kHintContentWidth);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 box_size(kHintContentWidth + padding.x * 2.0f, text_size.y + padding.y * 2.0f);
    const ImVec2 box_max(origin.x + box_size.x, origin.y + box_size.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, box_max, palette::U32FromHex(palette::kCard), 5.0f);
    draw_list->AddRect(origin, box_max, border_color, 5.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(origin.x + padding.x, origin.y + padding.y));
    ImGui::PushTextWrapPos(origin.x + padding.x + kHintContentWidth);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(box_size);
}

bool DrawParameterHint(EditorTab& tab) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = ParamHintTextBeforeCursor(tab, pos);

    CallContext ctx;
    if (!FindEnclosingCall(before, ctx)) return false;

    const FunctionSignature* sig = tab.function_index.Find(ctx.function_name);
    if (!sig) return false;

    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::BeginTooltip();

    if (sig->is_builtin) {
        DrawHintBadge(util::Tr("editor.hint.builtin").c_str(), palette::U32FromHex(palette::kBorder),
                      palette::U32FromHex(palette::kTextSecondary));
    } else {
        DrawHintBadge(util::Tr("editor.hint.function").c_str(), palette::U32FromHex(palette::kPrimary),
                      palette::U32FromHex(palette::kBackground));
    }

    ImGui::TextColored(palette::FromHex(palette::kSynFunction), "%s", sig->name.c_str());
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted("(");
    for (size_t i = 0; i < sig->params.size(); ++i) {
        if (i > 0) { ImGui::SameLine(0, 0); ImGui::TextUnformatted(", "); }
        ImGui::SameLine(0, 0);
        if (static_cast<int>(i) == ctx.active_param) {
            ImGui::TextColored(palette::FromHex(palette::kPrimaryLight), "%s", sig->params[i].c_str());
        } else {
            ImGui::TextColored(palette::FromHex(palette::kTextSecondary), "%s", sig->params[i].c_str());
        }
    }
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(")");

    if (!sig->doc.empty()) {
        ImGui::Spacing();
        DrawHintSectionLabel(util::Tr("editor.hint.what_it_does").c_str());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kHintContentWidth);
        ImGui::TextColored(palette::FromHex(palette::kTextPrimary), "%s", sig->doc.c_str());
        ImGui::PopTextWrapPos();
    }

    if (!sig->param_docs.empty() && ctx.active_param >= 0 &&
        static_cast<size_t>(ctx.active_param) < sig->params.size()) {
        const std::string param_name = ParamBaseName(sig->params[static_cast<size_t>(ctx.active_param)]);
        auto it = sig->param_docs.find(param_name);
        if (it != sig->param_docs.end() && !it->second.empty()) {
            ImGui::Spacing();
            std::string label = param_name + ":";
            ImGui::TextColored(palette::FromHex(palette::kPrimaryLight), "%s", label.c_str());
            ImGui::SameLine();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kHintContentWidth - ImGui::CalcTextSize(label.c_str()).x - 8.0f);
            ImGui::TextColored(palette::FromHex(palette::kTextSecondary), "%s", it->second.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::Spacing();
    if (sig->is_builtin) {

        if (sig->overridable) {
            ImGui::TextColored(palette::FromHex(palette::kTextDisabled), "%s",
                                TrFormat("editor.hint.builtin_overridable_note", sig->name).c_str());
        }
    } else if (!sig->source_file.empty()) {
        ImGui::TextColored(palette::FromHex(palette::kTextDisabled), "%s",
                            TrFormat("editor.hint.defined_in", sig->source_file).c_str());
    }
    ImGui::EndTooltip();
    return true;
}

TextEditor::CursorPosition ScreenPosToCursor(EditorTab& tab, const ImVec2& screen_pos,
                                              const ImVec2& editor_screen_min);

bool DrawFunctionNameHint(EditorState& state, EditorTab& tab, const std::string& word) {
    const FunctionSignature* sig = tab.function_index.Find(word);
    if (!sig) sig = state.workspace_index.CurrentSnapshot()->functions.Find(word);
    if (!sig) return false;

    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::BeginTooltip();

    if (sig->is_builtin) {
        DrawHintBadge(util::Tr("editor.hint.builtin").c_str(), palette::U32FromHex(palette::kBorder),
                      palette::U32FromHex(palette::kTextSecondary));
    } else {
        DrawHintBadge(util::Tr("editor.hint.function").c_str(), palette::U32FromHex(palette::kPrimary),
                      palette::U32FromHex(palette::kBackground));
    }

    ImGui::TextColored(palette::FromHex(palette::kSynFunction), "%s", sig->name.c_str());
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted("(");
    for (size_t i = 0; i < sig->params.size(); ++i) {
        if (i > 0) { ImGui::SameLine(0, 0); ImGui::TextUnformatted(", "); }
        ImGui::SameLine(0, 0);
        ImGui::TextColored(palette::FromHex(palette::kTextSecondary), "%s", sig->params[i].c_str());
    }
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(")");

    if (!sig->doc.empty()) {
        ImGui::Spacing();
        DrawHintSectionLabel(util::Tr("editor.hint.what_it_does").c_str());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kHintContentWidth);
        ImGui::TextColored(palette::FromHex(palette::kTextPrimary), "%s", sig->doc.c_str());
        ImGui::PopTextWrapPos();
    }

    if (!sig->is_builtin && !sig->source_file.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(palette::FromHex(palette::kTextDisabled), "%s",
                            TrFormat("editor.hint.defined_in", sig->source_file).c_str());
    }

    ImGui::EndTooltip();
    return true;
}

bool DrawClassNameHint(EditorState& state, EditorTab& tab, const std::string& word) {
    const ClassInfo* info = tab.class_index.Find(word);
    if (!info) info = state.workspace_index.CurrentSnapshot()->classes.Find(word);
    if (!info) return false;

    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::BeginTooltip();

    DrawHintBadge(util::Tr(info->is_interface ? "editor.hint.interface" : "editor.hint.class").c_str(),
                  palette::U32FromHex(palette::kSynClass), palette::U32FromHex(palette::kBackground));
    ImGui::SameLine();
    ImGui::TextColored(palette::FromHex(palette::kSynClass), "%s", info->name.c_str());

    if (!info->heritage_names.empty()) {
        std::string joined;
        for (size_t i = 0; i < info->heritage_names.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += info->heritage_names[i];
        }
        ImGui::SameLine();
        ImGui::TextColored(palette::FromHex(palette::kTextSecondary), ": %s", joined.c_str());
    }

    if (!info->source_file.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(palette::FromHex(palette::kTextDisabled), "%s",
                            TrFormat("editor.hint.defined_in", info->source_file).c_str());
    }

    ImGui::EndTooltip();
    return true;
}

bool DrawKeywordHint(EditorState& state, EditorTab& tab, const ImVec2& editor_min) {
    const TextEditor::CursorPosition pos = ScreenPosToCursor(tab, ImGui::GetMousePos(), editor_min);
    if (pos.line < 0 || pos.line >= static_cast<int>(tab.editor.GetLineCount())) return false;
    const std::string line_text = tab.editor.GetLineText(pos.line);
    const std::string word = WordAtColumn(line_text, pos.column);
    if (word.empty() || !IsIdentStart(word[0])) return false;

    const auto& docs = KeywordDocs();
    auto exact = docs.find(word);
    const KeywordDoc* match = exact != docs.end() ? &exact->second : nullptr;
    if (!match) {
        if (DrawClassNameHint(state, tab, word)) return true;
        return DrawFunctionNameHint(state, tab, word);
    }

    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::BeginTooltip();

    DrawHintBadge(util::Tr("editor.hint.keyword").c_str(), palette::U32FromHex(palette::kPrimary),
                  palette::U32FromHex(palette::kBackground));
    ImGui::SameLine();
    ImGui::TextColored(palette::FromHex(palette::kSynKeyword), "%s", match->name.c_str());

    ImGui::Spacing();
    DrawHintSectionLabel(util::Tr(match->syntax.size() > 1 ? "editor.hint.syntax_multi" : "editor.hint.syntax").c_str());
    for (size_t i = 0; i < match->syntax.size(); ++i) {
        if (match->syntax.size() > 1) {
            ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s",
                                TrFormat("editor.hint.option_n", std::to_string(i + 1)).c_str());
        }
        DrawHintCodeBox(match->syntax[i], palette::U32FromHex(palette::kBorder));
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
    }

    if (!match->example.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        DrawHintSectionLabel(util::Tr("editor.hint.example").c_str());
        DrawHintCodeBox(match->example, palette::U32FromHex(palette::kPrimary));
    }

    if (!match->doc.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        DrawHintSectionLabel(util::Tr("editor.hint.what_it_does").c_str());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kHintContentWidth);
        ImGui::TextColored(palette::FromHex(palette::kTextPrimary), "%s", match->doc.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::EndTooltip();
    return true;
}

float EstimateGutterWidth(EditorTab& tab, float glyph_width) {

    const std::string text = tab.GetText();
    int total_lines = 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n'));
    int digits = 1;
    for (int n = total_lines; n >= 10; n /= 10) ++digits;
    return glyph_width * (static_cast<float>(digits) + 3.0f);
}

ImVec2 EstimateCaretScreenPos(EditorTab& tab, const TextEditor::CursorPosition& pos,
                               const ImVec2& editor_screen_min) {
    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    // Pixel-precise scroll offset (imguicolortextedit_scroll_offset.patch), not the
    // whole-line/whole-column quantized GetFirstVisibleLine()/GetFirstVisibleColumn():
    // those floor() to an integer line/column, so overlays positioned from them only
    // move in whole-line jumps instead of tracking the scroll continuously.
    const float scroll_x = tab.editor.GetCurrentScrollX();
    const float scroll_y = tab.editor.GetCurrentScrollY();
    const float gutter_width = EstimateGutterWidth(tab, glyph_width);

    const float x = editor_screen_min.x + gutter_width +
                     static_cast<float>(pos.column) * glyph_width - scroll_x;
    const float y = editor_screen_min.y +
                     static_cast<float>(pos.line) * line_height - scroll_y;
    return ImVec2(x, y);
}

TextEditor::CursorPosition ScreenPosToCursor(EditorTab& tab, const ImVec2& screen_pos,
                                              const ImVec2& editor_screen_min) {
    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    const float scroll_x = tab.editor.GetCurrentScrollX();
    const float scroll_y = tab.editor.GetCurrentScrollY();
    const float gutter_width = EstimateGutterWidth(tab, glyph_width);

    const int line = std::max(
        0, static_cast<int>((screen_pos.y - editor_screen_min.y + scroll_y) / line_height));
    const int column = std::max(
        0, static_cast<int>((screen_pos.x - editor_screen_min.x - gutter_width + scroll_x + glyph_width * 0.5f) /
                             glyph_width));
    return TextEditor::CursorPosition(line, column);
}

int HeritageNameColumnOnLine(const std::string& line_text, const std::string& interface_name) {
    const size_t colon = line_text.find(':');
    if (colon == std::string::npos) return -1;
    auto is_ident_start = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; };
    auto is_ident_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    size_t i = colon + 1;
    while (i < line_text.size()) {
        while (i < line_text.size() && (line_text[i] == ' ' || line_text[i] == '\t' || line_text[i] == ',')) ++i;
        if (i >= line_text.size() || !is_ident_start(line_text[i])) break;
        const size_t word_start = i;
        while (i < line_text.size() && is_ident_char(line_text[i])) ++i;
        if (line_text.compare(word_start, i - word_start, interface_name) == 0) {
            return static_cast<int>(word_start);
        }
    }
    return -1;
}

void DrawIncompleteInterfaceSquiggles(EditorTab& tab, const ImVec2& editor_min, const ImVec2& editor_max) {
    if (tab.incomplete_interfaces.empty()) return;

    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    const int first_visible_line = tab.editor.GetFirstVisibleLine();
    const int last_visible_line = first_visible_line + static_cast<int>((editor_max.y - editor_min.y) / line_height) + 1;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 color = palette::U32FromHex(palette::kWarning);
    const ImVec2 mouse = ImGui::GetMousePos();

    for (const auto& entry : tab.incomplete_interfaces) {
        if (entry.line < first_visible_line || entry.line > last_visible_line) continue;

        const std::string line_text = tab.editor.GetLineText(entry.line);
        const int name_col = HeritageNameColumnOnLine(line_text, entry.interface_name);
        const int name_len = static_cast<int>(entry.interface_name.size());
        if (name_col < 0 || name_len == 0) continue;

        const ImVec2 start = EstimateCaretScreenPos(tab, TextEditor::CursorPosition(entry.line, name_col), editor_min);
        const float width = glyph_width * static_cast<float>(name_len);
        if (start.x + width < editor_min.x || start.x > editor_max.x) continue;

        const float y = start.y + line_height - 3.0f;
        draw_list->PushClipRect(editor_min, editor_max, true);
        const float amplitude = 1.6f;
        const float step = 3.0f;
        ImVec2 prev(start.x, y);
        bool up = false;
        for (float x = step; x <= width + step; x += step) {
            up = !up;
            ImVec2 next(start.x + std::min(x, width), y - (up ? amplitude : 0.0f));
            draw_list->AddLine(prev, next, color, 1.3f);
            prev = next;
        }
        draw_list->PopClipRect();

        const ImVec2 hover_min(start.x, start.y);
        const ImVec2 hover_max(start.x + width, start.y + line_height);
        if (mouse.x >= hover_min.x && mouse.x <= hover_max.x && mouse.y >= hover_min.y && mouse.y <= hover_max.y &&
            ImGui::IsMouseHoveringRect(editor_min, editor_max)) {
            ImGui::BeginTooltip();
            ImGui::TextColored(palette::FromHex(palette::kWarning), "%s",
                                TrFormat("editor.hint.missing_interface_members",
                                         {entry.class_name, entry.interface_name})
                                    .c_str());
            for (const auto& member : entry.missing_members) {
                const std::string sig = member.signature ? member.signature->display : member.name;
                ImGui::BulletText("%s", sig.c_str());
            }
            ImGui::EndTooltip();
        }
    }
}

void DrawDiagnosticSquiggles(EditorTab& tab, const ImVec2& editor_min, const ImVec2& editor_max) {
    if (tab.diagnostics.empty()) return;

    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    const int first_visible_line = tab.editor.GetFirstVisibleLine();
    const int last_visible_line = first_visible_line + static_cast<int>((editor_max.y - editor_min.y) / line_height) + 1;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 mouse = ImGui::GetMousePos();

    for (const auto& diag : tab.diagnostics) {
        if (diag.line < first_visible_line || diag.line > last_visible_line) continue;
        if (diag.column_end <= diag.column_start) continue;

        const ImU32 color = DiagnosticSeverityColor(diag.severity);
        const ImVec2 start = EstimateCaretScreenPos(tab, TextEditor::CursorPosition(diag.line, diag.column_start), editor_min);
        const float width = glyph_width * static_cast<float>(diag.column_end - diag.column_start);
        if (start.x + width < editor_min.x || start.x > editor_max.x) continue;

        const float y = start.y + line_height - 3.0f;
        draw_list->PushClipRect(editor_min, editor_max, true);
        const float amplitude = 1.6f;
        const float step = 3.0f;
        ImVec2 prev(start.x, y);
        bool up = false;
        for (float x = step; x <= width + step; x += step) {
            up = !up;
            ImVec2 next(start.x + std::min(x, width), y - (up ? amplitude : 0.0f));
            draw_list->AddLine(prev, next, color, 1.3f);
            prev = next;
        }
        draw_list->PopClipRect();

        const ImVec2 hover_min(start.x, start.y);
        const ImVec2 hover_max(start.x + width, start.y + line_height);
        if (!diag.message.empty() && mouse.x >= hover_min.x && mouse.x <= hover_max.x &&
            mouse.y >= hover_min.y && mouse.y <= hover_max.y &&
            ImGui::IsMouseHoveringRect(editor_min, editor_max)) {
            ImGui::BeginTooltip();
            ImGui::TextColored(palette::FromHex(
                                    diag.severity == diagnostics::Severity::Error ? palette::kError
                                    : diag.severity == diagnostics::Severity::Warning ? palette::kWarning
                                    : diag.severity == diagnostics::Severity::Info ? palette::kInfo
                                                                                    : palette::kTextDisabled),
                                "%s", diag.message.c_str());
            ImGui::EndTooltip();
        }
    }
}

void DrawInlayHints(EditorTab& tab, const ImVec2& editor_min, const ImVec2& editor_max) {
    if (tab.inlay_hints.empty()) return;

    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    const int first_visible_line = tab.editor.GetFirstVisibleLine();
    const int last_visible_line = first_visible_line + static_cast<int>((editor_max.y - editor_min.y) / line_height) + 1;
    const int cursor_line = tab.editor.GetCursorPosition(0).line;

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    const ImU32 hint_color = palette::U32FromHex(palette::kTextDisabled);
    const ImU32 bg_color = tab.editor.GetPalette().get(TextEditor::Color::background);

    std::unordered_map<int, std::vector<const diagnostics::InlayHint*>> hints_by_line;
    for (const auto& hint : tab.inlay_hints) {
        if (hint.line < first_visible_line || hint.line > last_visible_line) continue;
        hints_by_line[hint.line].push_back(&hint);
    }

    auto color_at = [](const std::vector<syntax::Token>& tokens, int idx) {
        for (const auto& t : tokens) {
            if (idx >= t.start && idx < t.end) return syntax::ColorForToken(t.kind);
        }
        return syntax::ColorForToken(syntax::TokenKind::Default);
    };

    for (auto& [line, hints] : hints_by_line) {
        std::sort(hints.begin(), hints.end(), [](const diagnostics::InlayHint* a, const diagnostics::InlayHint* b) {
            return a->column < b->column;
        });
        const std::string line_text = tab.editor.GetLineText(line);

        if (line == cursor_line) {
            for (const diagnostics::InlayHint* hint : hints) {
                const size_t col = static_cast<size_t>(hint->column);
                const size_t hint_len = hint->text.size();
                const size_t available = col < line_text.size() ? line_text.size() - col : 0;
                bool fits = true;
                for (size_t k = 0; k < hint_len && k < available; ++k) {
                    if (line_text[col + k] != ' ' && line_text[col + k] != '\t') { fits = false; break; }
                }
                if (!fits) continue;
                const ImVec2 pos = EstimateCaretScreenPos(tab, TextEditor::CursorPosition(line, hint->column), editor_min);
                if (pos.x > editor_max.x || pos.x < editor_min.x) continue;
                draw_list->PushClipRect(editor_min, editor_max, true);
                draw_list->AddText(pos, hint_color, hint->text.c_str());
                draw_list->PopClipRect();
            }
            continue;
        }

        const ImVec2 line_start = EstimateCaretScreenPos(tab, TextEditor::CursorPosition(line, 0), editor_min);
        if (line_start.y + line_height < editor_min.y || line_start.y > editor_max.y) continue;
        if (line_start.x > editor_max.x) continue;

        const std::vector<syntax::Token> tokens = syntax::Tokenize(line_text);

        draw_list->PushClipRect(editor_min, editor_max, true);
        draw_list->AddRectFilled(ImVec2(line_start.x, line_start.y), ImVec2(editor_max.x, line_start.y + line_height),
                                  bg_color);

        float x = line_start.x;
        int drawn_up_to = 0;
        const int line_len = static_cast<int>(line_text.size());
        auto draw_real_range = [&](int from, int to) {
            int i = from;
            while (i < to) {
                const int run_start = i;
                const ImU32 c = color_at(tokens, i);
                while (i < to && color_at(tokens, i) == c) ++i;
                const std::string piece = line_text.substr(static_cast<size_t>(run_start), static_cast<size_t>(i - run_start));
                if (x <= editor_max.x) draw_list->AddText(ImVec2(x, line_start.y), c, piece.c_str());
                x += glyph_width * static_cast<float>(piece.size());
            }
        };

        for (const diagnostics::InlayHint* hint : hints) {
            const int col = std::clamp(hint->column, 0, line_len);
            draw_real_range(drawn_up_to, col);
            drawn_up_to = col;
            if (x <= editor_max.x) draw_list->AddText(ImVec2(x, line_start.y), hint_color, hint->text.c_str());
            x += glyph_width * static_cast<float>(hint->text.size());
        }
        draw_real_range(drawn_up_to, line_len);

        draw_list->PopClipRect();
    }
}

void AddMissingImport(EditorState& state, EditorTab& tab, const diagnostics::Diagnostic& diag) {
    if (diag.import_segments.empty()) return;

    std::string joined;
    for (size_t k = 0; k < diag.import_segments.size(); ++k) {
        if (k) joined += ".";
        joined += diag.import_segments[k];
    }

    int insert_line = 0;
    const int line_count = static_cast<int>(tab.editor.GetLineCount());
    while (insert_line < line_count) {
        const std::string trimmed = TrimTrailing(tab.editor.GetLineText(insert_line));
        size_t first_non_space = trimmed.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) break;
        std::string word;
        size_t j = first_non_space;
        if (IsIdentStart(trimmed[j])) word = ReadIdent(trimmed, j);
        if (word != "import") break;
        ++insert_line;
    }

    tab.editor.ReplaceSectionText(insert_line, 0, insert_line, 0, "import " + joined + "\n");
    RebuildIndexAndTrie(state, tab);
}

std::string BuildMethodStub(const ClassMember& member, const std::string& indent) {
    std::string params;
    if (member.signature) {
        for (size_t i = 0; i < member.signature->params.size(); ++i) {
            if (i > 0) params += ", ";
            params += member.signature->params[i];
        }
    }
    std::string header = indent + "func " + member.name + "(" + params + ")";
    if (member.signature && !member.signature->declared_return_type.empty()) {
        header += " as " + member.signature->declared_return_type;
    }
    return header + "\n" + indent + "    # TODO: implementar\n" + indent + "end\n";
}

void ImplementMissingInterfaceMembers(EditorState& state, EditorTab& tab,
                                       const EditorTab::IncompleteInterface& entry) {
    const FoldRange* range = tab.fold_index.RangeStartingAt(entry.line);
    if (!range) return;

    const std::string header_text = tab.editor.GetLineText(entry.line);
    std::string indent;
    for (char c : header_text) {
        if (c != ' ' && c != '\t') break;
        indent += c;
    }
    const std::string body_indent = indent + "    ";

    std::string insertion;
    for (const auto& member : entry.missing_members) insertion += BuildMethodStub(member, body_indent);

    tab.editor.ReplaceSectionText(range->end_line, 0, range->end_line, 0, insertion);
    RebuildIndexAndTrie(state, tab);
}

struct QuickFix {
    std::string label;
    std::function<void()> apply;
};

void DeleteLineRange(EditorTab& tab, int start_line, int end_line) {
    const int line_count = static_cast<int>(tab.editor.GetLineCount());
    if (end_line + 1 < line_count) {
        tab.editor.ReplaceSectionText(start_line, 0, end_line + 1, 0, "");
        return;
    }
    if (start_line > 0) {
        const int prev_len = static_cast<int>(tab.editor.GetLineText(start_line - 1).size());
        const int last_len = static_cast<int>(tab.editor.GetLineText(end_line).size());
        tab.editor.ReplaceSectionText(start_line - 1, prev_len, end_line, last_len, "");
        return;
    }
    const int last_len = static_cast<int>(tab.editor.GetLineText(end_line).size());
    tab.editor.ReplaceSectionText(start_line, 0, end_line, last_len, "");
}

void RemoveUnusedImport(EditorState& state, EditorTab& tab, const diagnostics::Diagnostic& diag) {
    DeleteLineRange(tab, diag.line, diag.line);
    RebuildIndexAndTrie(state, tab);
}

void RenameUnusedVariable(EditorState& state, EditorTab& tab, const diagnostics::Diagnostic& diag) {
    tab.editor.ReplaceSectionText(diag.line, diag.column_start, diag.line, diag.column_end, "_" + diag.symbol);
    RebuildIndexAndTrie(state, tab);
}

void RemoveUnusedVariableDeclaration(EditorState& state, EditorTab& tab, const diagnostics::Diagnostic& diag) {
    DeleteLineRange(tab, diag.line, diag.line);
    RebuildIndexAndTrie(state, tab);
}

void RemoveUnusedMember(EditorState& state, EditorTab& tab, const diagnostics::Diagnostic& diag) {
    const FoldRange* range = tab.fold_index.RangeStartingAt(diag.line);
    if (range) DeleteLineRange(tab, range->start_line, range->end_line);
    else DeleteLineRange(tab, diag.line, diag.line);
    RebuildIndexAndTrie(state, tab);
}

void RemoveUnreachableLine(EditorState& state, EditorTab& tab, const diagnostics::Diagnostic& diag) {
    DeleteLineRange(tab, diag.line, diag.line);
    RebuildIndexAndTrie(state, tab);
}

std::vector<QuickFix> CollectQuickFixesAt(EditorState& state, EditorTab& tab, const TextEditor::CursorPosition& pos) {
    std::vector<QuickFix> fixes;

    for (const auto& entry : tab.incomplete_interfaces) {
        if (entry.line != pos.line) continue;
        fixes.push_back({TrFormat("editor.context.implement_interface", entry.interface_name),
                          [&state, &tab, entry]() { ImplementMissingInterfaceMembers(state, tab, entry); }});
    }

    for (const auto& diag : tab.diagnostics) {
        if (diag.line != pos.line) continue;

        switch (diag.kind) {
            case diagnostics::Kind::MissingImport: {
                std::string joined;
                for (size_t k = 0; k < diag.import_segments.size(); ++k) {
                    if (k) joined += ".";
                    joined += diag.import_segments[k];
                }
                fixes.push_back({TrFormat("editor.context.add_import", joined),
                                  [&state, &tab, diag]() { AddMissingImport(state, tab, diag); }});
                break;
            }
            case diagnostics::Kind::UnusedImport:
                fixes.push_back({TrFormat("editor.context.remove_unused_import", diag.symbol),
                                  [&state, &tab, diag]() { RemoveUnusedImport(state, tab, diag); }});
                break;
            case diagnostics::Kind::UnusedVariable:
                fixes.push_back({TrFormat("editor.context.rename_unused_variable", {diag.symbol, diag.symbol}),
                                  [&state, &tab, diag]() { RenameUnusedVariable(state, tab, diag); }});
                fixes.push_back({TrFormat("editor.context.remove_unused_variable", diag.symbol),
                                  [&state, &tab, diag]() { RemoveUnusedVariableDeclaration(state, tab, diag); }});
                break;
            case diagnostics::Kind::UnusedPrivateMember:
                fixes.push_back({TrFormat("editor.context.remove_unused_member", diag.symbol),
                                  [&state, &tab, diag]() { RemoveUnusedMember(state, tab, diag); }});
                break;
            case diagnostics::Kind::UnreachableCode:
                fixes.push_back({util::Tr("editor.context.remove_unreachable"),
                                  [&state, &tab, diag]() { RemoveUnreachableLine(state, tab, diag); }});
                break;
            default:
                break;
        }
    }

    return fixes;
}

void DrawQuickFixGutterIcon(EditorState& state, EditorTab& tab, const ImVec2& editor_min, const ImVec2& editor_max) {
    const TextEditor::CursorPosition cursor = tab.editor.GetCursorPosition(0);
    const std::vector<QuickFix> fixes = CollectQuickFixesAt(state, tab, cursor);

    const std::string popup_id = "##quick_fix_popup_" + std::to_string(tab.id);

    const bool shortcut_pressed =
        ShortcutRegistry::Instance().Pressed(ShortcutId::QuickFix, state.code_editor_has_focus);
    if (shortcut_pressed && !fixes.empty()) {
        ImGui::OpenPopup(popup_id.c_str());
    }

    if (fixes.empty()) return;

    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    const int first_visible_line = tab.editor.GetFirstVisibleLine();
    if (cursor.line < first_visible_line) return;

    const ImVec2 line_pos = EstimateCaretScreenPos(tab, TextEditor::CursorPosition(cursor.line, 0), editor_min);
    if (line_pos.y + line_height < editor_min.y || line_pos.y > editor_max.y) return;

    const float gutter_width = EstimateGutterWidth(tab, glyph_width);
    const ImVec2 bulb_center(editor_min.x + gutter_width - glyph_width * 1.5f, line_pos.y + line_height * 0.5f);
    const float radius = std::min(line_height, glyph_width * 2.0f) * 0.32f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 color = palette::U32FromHex(palette::kWarning);
    draw_list->PushClipRect(editor_min, ImVec2(editor_min.x + gutter_width, editor_max.y), true);
    draw_list->AddCircleFilled(bulb_center, radius, color);
    draw_list->AddRectFilled(ImVec2(bulb_center.x - radius * 0.35f, bulb_center.y + radius * 0.6f),
                              ImVec2(bulb_center.x + radius * 0.35f, bulb_center.y + radius * 1.05f), color);
    draw_list->PopClipRect();

    const ImVec2 hit_min(bulb_center.x - radius - 2.0f, bulb_center.y - radius - 2.0f);
    const ImVec2 hit_max(bulb_center.x + radius + 2.0f, bulb_center.y + radius + 2.0f);
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool hovered = mouse.x >= hit_min.x && mouse.x <= hit_max.x && mouse.y >= hit_min.y && mouse.y <= hit_max.y &&
                         ImGui::IsMouseHoveringRect(editor_min, editor_max, false);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGui::OpenPopup(popup_id.c_str());
    }
    if (hovered) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(TrFormat("editor.quickfix.tooltip",
                                        ShortcutRegistry::Instance().Label(ShortcutId::QuickFix)).c_str());
        ImGui::EndTooltip();
    }

    ImGui::SetNextWindowPos(ImVec2(bulb_center.x, line_pos.y + line_height), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popup_id.c_str())) {
        for (const QuickFix& fix : fixes) {
            if (ImGui::MenuItem(fix.label.c_str())) fix.apply();
        }
        ImGui::EndPopup();
    }
}

struct DotCompletionPending {
    bool active = false;
    TextEditor::CursorPosition pos{};
    std::vector<ClassMember> members;
};

DotCompletionPending PrepareDotCompletion(EditorState& state, EditorTab& tab) {
    DotCompletionPending pending;

    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);
    if (before.empty() || before.back() != '.') return pending;

    MemberAccessContext ctx;
    if (!ResolveVisibleMembers(state, tab, pos.line, before, ctx, pending.members)) return pending;
    if (pending.members.empty()) return pending;

    pending.active = true;
    pending.pos = pos;
    return pending;
}

constexpr ImGuiKey kDotCompletionNavKeys[] = {
    ImGuiKey_DownArrow, ImGuiKey_UpArrow, ImGuiKey_Enter, ImGuiKey_KeypadEnter,
    ImGuiKey_Tab,       ImGuiKey_Escape,
};

void ClaimDotCompletionKeys() {
    for (ImGuiKey key : kDotCompletionNavKeys) {
        if (ImGuiKeyData* data = ImGui::GetKeyData(key)) {
            data->Down = false;
            data->DownDuration = -1.0f;
            data->DownDurationPrev = -1.0f;
        }
    }
}

struct DotCompletionKeys {
    bool nav_down = false;
    bool nav_up = false;
    bool accept = false;
    bool dismiss = false;
};

bool DrawDotCompletionPopup(EditorTab& tab, const ImVec2& editor_screen_min,
                            const DotCompletionPending& pending, const DotCompletionKeys& keys) {
    const int count = static_cast<int>(pending.members.size());

    if (tab.dot_popup_line != pending.pos.line || tab.dot_popup_column != pending.pos.column) {
        tab.dot_popup_selected = 0;
        tab.dot_popup_line = pending.pos.line;
        tab.dot_popup_column = pending.pos.column;
    }

    if (keys.dismiss) {
        tab.dot_popup_line = -1;
        tab.dot_popup_column = -1;
        return false;
    }

    tab.dot_popup_selected = std::clamp(tab.dot_popup_selected, 0, count - 1);
    if (keys.nav_down) tab.dot_popup_selected = (tab.dot_popup_selected + 1) % count;
    if (keys.nav_up) tab.dot_popup_selected = (tab.dot_popup_selected - 1 + count) % count;

    auto accept_member = [&](const ClassMember& member) {
        std::string insert_text = member.name;
        int caret_offset = static_cast<int>(insert_text.size());
        if (member.is_method) {
            insert_text += "()";

            bool has_params = member.signature && !member.signature->params.empty();
            caret_offset = static_cast<int>(insert_text.size()) - (has_params ? 1 : 0);
        }
        tab.editor.ReplaceSectionText(pending.pos.line, pending.pos.column, pending.pos.line,
                                        pending.pos.column, insert_text);
        tab.editor.SetCursor(pending.pos.line, pending.pos.column + caret_offset);
    };

    if (keys.accept) {
        accept_member(pending.members[tab.dot_popup_selected]);
        return true;
    }

    const std::string window_id = "##dot_completion_popup_" + std::to_string(tab.id);

    const bool any_mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                                 ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                 ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (any_mouse_down) {
        ImGuiWindow* existing = ImGui::FindWindowByName(window_id.c_str());
        const bool press_inside_popup =
            existing && existing->WasActive &&
            ImGui::IsMouseHoveringRect(existing->Rect().Min, existing->Rect().Max, false);
        if (!press_inside_popup) return false;
    }

    const float line_height = tab.editor.GetLineHeight();
    const ImVec2 caret_pos = EstimateCaretScreenPos(tab, pending.pos, editor_screen_min);
    ImGui::SetNextWindowPos(ImVec2(caret_pos.x, caret_pos.y + line_height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                         ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin(window_id.c_str(), nullptr, kFlags)) {
        for (int i = 0; i < count; ++i) {
            const ClassMember& member = pending.members[i];
            ImGui::PushID(member.name.c_str());

            const bool row_selected = (i == tab.dot_popup_selected);
            const ImU32 dot_color = member.is_method
                                         ? palette::U32FromHex(palette::kSynFunction)
                                         : palette::U32FromHex(palette::kInfo);
            constexpr float kDotRadius = 3.5f;
            const float row_height = ImGui::GetTextLineHeightWithSpacing();
            const ImVec2 row_start = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(row_start.x + kDotRadius + 2.0f, row_start.y + row_height * 0.5f),
                kDotRadius, dot_color);
            ImGui::Dummy(ImVec2(kDotRadius * 2.0f + 8.0f, 0.0f));
            ImGui::SameLine(0.0f, 0.0f);

            if (row_selected) ImGui::SetScrollHereY();

            if (ImGui::Selectable(MemberSuggestionLabel(member).c_str(), row_selected)) {
                accept_member(member);
            }
            if (ImGui::IsItemHovered()) tab.dot_popup_selected = i;

            ImGui::PopID();
        }
    }
    ImGui::End();
    return true;
}

void InitTab(EditorState& state, EditorTab& tab) {
    tab.editor.SetLanguage(languages::AvaLang());
    TextEditor::Palette palette = TextEditor::GetDarkPalette();

    palette[static_cast<size_t>(TextEditor::Color::keyword)] = palette::U32FromHex(palette::kSynKeyword);
    palette[static_cast<size_t>(TextEditor::Color::declaration)] = palette::U32FromHex(palette::kSynFunction);
    palette[static_cast<size_t>(TextEditor::Color::comment)] = palette::U32FromHex(palette::kSynComment);
    palette[static_cast<size_t>(TextEditor::Color::number)] = palette::U32FromHex(palette::kSynNumber);
    palette[static_cast<size_t>(TextEditor::Color::docComment)] = palette::U32FromHex(palette::kSynDocComment);
    palette[static_cast<size_t>(TextEditor::Color::docParamTag)] = palette::U32FromHex(palette::kSynDocParamTag);
    palette[static_cast<size_t>(TextEditor::Color::string)] = palette::U32FromHex(palette::kSynString);
    palette[static_cast<size_t>(TextEditor::Color::interpolation)] = palette::U32FromHex(palette::kSynInterpolation);
    palette[static_cast<size_t>(TextEditor::Color::importPath)] = palette::U32FromHex(palette::kSynImportPath);
    palette[static_cast<size_t>(TextEditor::Color::knownIdentifier)] = palette::U32FromHex(palette::kSynKnownIdentifier);
    palette[static_cast<size_t>(TextEditor::Color::punctuation)] = palette::U32FromHex(palette::kSynPunctuation);
    palette[static_cast<size_t>(TextEditor::Color::preprocessor)] = palette::U32FromHex(palette::kSynClass);
    palette[static_cast<size_t>(TextEditor::Color::identifier)] = palette::U32FromHex(palette::kSynIdentifier);
    palette[static_cast<size_t>(TextEditor::Color::interfaceName)] = palette::U32FromHex(palette::kSynInterface);
    palette[static_cast<size_t>(TextEditor::Color::variableName)] = palette::U32FromHex(palette::kSynVariable);

    tab.editor.SetPalette(palette);
    tab.editor.SetShowLineNumbersEnabled(true);
    tab.editor.SetTabSize(4);
    tab.editor.SetAutoIndentEnabled(true);
    tab.editor.SetShowMatchingBrackets(true);
    tab.editor.SetCompletePairedGlyphs(true);
    tab.editor.SetShowScrollbarMiniMapEnabled(!state.show_minimap);

    tab.editor.SetBoldFont(GetCodeFont());
    tab.editor.SetBoldColors({TextEditor::Color::keyword, TextEditor::Color::declaration});

    tab.editor.SetChangeCallback([&tab] {
        tab.dirty = true;
        tab.editor.ClearMarkers();
        tab.index_dirty = true;
        tab.format_pending = true;
        tab.last_edit_time = ImGui::GetTime();
    }, 0);

    tab.autocomplete_config.callback = [&state, &tab](TextEditor::AutoCompleteState& ac_state) {
        if (ac_state.inNumber) { ac_state.suggestions.clear(); return; }
        if (PopulateImportPathSuggestions(state, tab, ac_state)) return;
        if (PopulateMemberSuggestions(state, tab, ac_state)) return;
        PopulateGeneralSuggestions(tab, ac_state);
    };
    tab.editor.SetAutoCompleteConfig(&tab.autocomplete_config);

    RebuildIndexAndTrie(state, tab);
}

int FindTabForPath(EditorState& state, const std::string& path) {
    if (path.empty()) return -1;
    for (int i = 0; i < static_cast<int>(state.tabs.size()); ++i) {
        if (state.tabs[i]->file_path == path) return i;
    }
    return -1;
}

bool PathContains(const std::filesystem::path& parent, const std::filesystem::path& child,
                   std::filesystem::path* remainder = nullptr) {
    auto pit = parent.begin();
    auto cit = child.begin();
    for (; pit != parent.end(); ++pit, ++cit) {
        if (cit == child.end() || *pit != *cit) return false;
    }
    if (remainder) {
        std::filesystem::path rel;
        for (; cit != child.end(); ++cit) rel /= *cit;
        *remainder = rel;
    }
    return true;
}

void CloseTabNow(EditorState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.tabs.size())) return;

    InvalidateDesignerVmCache(state.tabs[index]->id);
    languages::UpdateKnownInterfaceNames(state.tabs[index]->known_interface_names, {});
    languages::UpdateKnownVariableNames(state.tabs[index]->known_variable_names, {});
    languages::UpdateKnownClassNames(state.tabs[index]->known_class_names, {});
    state.tabs.erase(state.tabs.begin() + index);

    if (state.tabs.empty()) {
        state.active_tab = -1;
    } else if (state.active_tab >= static_cast<int>(state.tabs.size())) {
        state.active_tab = static_cast<int>(state.tabs.size()) - 1;
    } else if (index <= state.active_tab && state.active_tab > 0) {

        --state.active_tab;
    }

    if (state.pending_close_index == index) {
        state.pending_close_index = -1;
    } else if (state.pending_close_index > index) {
        --state.pending_close_index;
    }
}

}

std::string EditorTab::DisplayName() const {
    if (is_welcome) return util::Tr("editor.tab.welcome");
    return file_path.empty() ? util::Tr("editor.tab.untitled") : BaseNameOf(file_path);
}

EditorTab* EditorState::Active() {
    return active_tab >= 0 && active_tab < static_cast<int>(tabs.size()) ? tabs[active_tab].get() : nullptr;
}

const EditorTab* EditorState::Active() const {
    return active_tab >= 0 && active_tab < static_cast<int>(tabs.size()) ? tabs[active_tab].get() : nullptr;
}

void InitEditorPanel(EditorState& ) {

}

EditorTab& OpenFileInTab(EditorState& state, const std::string& path) {
    int existing = FindTabForPath(state, path);
    if (existing >= 0) {
        state.active_tab = existing;
        state.focus_tab_id = state.tabs[existing]->id;
        return *state.tabs[existing];
    }

    auto tab = std::make_unique<EditorTab>();
    tab->id = state.next_tab_id++;
    tab->modules_path = state.modules_path;
    InitTab(state, *tab);

    if (!path.empty()) {
        std::ifstream file(path, std::ios::binary);
        if (file) {
            std::ostringstream ss;
            ss << file.rdbuf();
            tab->file_path = path;
            tab->SetText(ss.str());
            tab->dirty = false;

            RebuildIndexAndTrie(state, *tab);
        }

        if (std::filesystem::path(path).extension().string() == ".avaui") {
            tab->is_avaui = true;
            tab->view_mode = TabViewMode::Design;

            std::string load_error;
            if (!design::LoadAvauiFile(path, tab->design, load_error)) {

                tab->design = design::NewBlankAvauiDocument();
                if (!tab->GetText().empty()) {
                    tab->avaui_load_error = load_error;
                }
            }
        }
    }

    state.focus_tab_id = tab->id;
    state.tabs.push_back(std::move(tab));
    state.active_tab = static_cast<int>(state.tabs.size()) - 1;
    return *state.tabs.back();
}

EditorTab& NewUntitledTab(EditorState& state) {
    return OpenFileInTab(state, "");
}

EditorTab& OpenWelcomeTab(EditorState& state) {
    auto tab = std::make_unique<EditorTab>();
    tab->id = state.next_tab_id++;
    tab->is_welcome = true;

    state.tabs.push_back(std::move(tab));
    state.active_tab = static_cast<int>(state.tabs.size()) - 1;
    return *state.tabs.back();
}

namespace {

int LeadingWhitespaceLength(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    return static_cast<int>(i);
}

void ApplyFormattedText(EditorTab& tab, const std::string& formatted) {
    const TextEditor::CursorPosition cursor = tab.editor.GetCursorPosition(0);
    const int original_indent_len = LeadingWhitespaceLength(tab.editor.GetLineText(cursor.line));

    tab.SetText(formatted);

    const int new_line_count = tab.editor.GetLineCount();
    const int target_line = std::clamp(cursor.line, 0, std::max(0, new_line_count - 1));
    const std::string new_line = tab.editor.GetLineText(target_line);
    const int new_indent_len = LeadingWhitespaceLength(new_line);

    const int adjusted_column = target_line == cursor.line
                                     ? cursor.column - original_indent_len + new_indent_len
                                     : new_indent_len;
    const int clamped_column = std::clamp(adjusted_column, 0, static_cast<int>(new_line.size()));
    tab.editor.SetCursor(target_line, clamped_column);
}

}

void FormatTab(EditorState& state, EditorTab& tab) {
    if (tab.is_welcome || tab.is_avaui) return;

    const std::string formatted = languages::FormatAvalangSource(tab.GetText());
    if (formatted == tab.GetText()) return;

    ApplyFormattedText(tab, formatted);
    tab.dirty = true;
    RebuildIndexAndTrie(state, tab);
    tab.format_pending = false;
}

void SaveTab(EditorState& state, EditorTab& tab) {
    if (tab.file_path.empty()) return;

    if (state.format_on_save) FormatTab(state, tab);

    if (tab.index_dirty) RebuildIndexAndTrie(state, tab);

    if (tab.is_avaui) {
        if (tab.view_mode == TabViewMode::Code) {

            design::DesignDocument parsed_doc;
            std::string parse_error;
            if (design::LoadAvauiFile(tab.file_path, parsed_doc, parse_error)) {
                tab.design = std::move(parsed_doc);
                tab.avaui_load_error.clear();
            }
            std::ofstream file(tab.file_path, std::ios::binary);
            if (!file) return;
            file << tab.GetText();
            tab.design.dirty = false;
            tab.dirty = false;
            return;
        }

        if (design::SaveAvauiFile(tab.design, tab.file_path)) {
            tab.design.dirty = false;
            tab.dirty = false;
        }
        return;
    }

    std::ofstream file(tab.file_path, std::ios::binary);
    if (!file) return;
    file << tab.GetText();
    tab.dirty = false;
}

void ToggleTabViewMode(EditorState& state, EditorTab& tab) {
    if (!tab.is_avaui) return;

    if (tab.view_mode == TabViewMode::Design) {

        tab.SetText([&] {
            avalang::ui::parser::AvauiWriteOptions opts;
            opts.code_behind = tab.design.code_behind;
            opts.imports = tab.design.imports;
            opts.initial_state.reserve(tab.design.initial_state.size());
            for (const auto& row : tab.design.initial_state) {
                opts.initial_state.push_back({row.key, row.value});
            }
            return avalang::ui::parser::WriteAvaui(tab.design.Root(), opts);
        }());
        RebuildIndexAndTrie(state, tab);
        tab.avaui_load_error.clear();
        tab.view_mode = TabViewMode::Code;
        return;
    }

    design::DesignDocument parsed_doc;
    std::string parse_error;
    avalang::ui::parser::ParseErrorInfo parse_error_info;
    if (design::ParseAvauiText(tab.GetText(), parsed_doc, parse_error, tab.file_path, &parse_error_info)) {
        tab.design = std::move(parsed_doc);
        tab.design.selected_node_id.clear();
        tab.avaui_load_error.clear();
        tab.view_mode = TabViewMode::Design;
    } else {
        if (parse_error_info.line > 0) {
            tab.editor.ClearMarkers();

            const ImU32 error_color = palette::U32FromHex(palette::kError, 0.35f);
            const std::string tooltip = parse_error_info.column > 0
                                             ? parse_error_info.message
                                             : (TrFormat("editor.error_at_line", std::to_string(parse_error_info.line)) +
                                                parse_error_info.message);

            tab.editor.AddMarker(parse_error_info.line - 1, error_color, error_color, tooltip, tooltip);
            tab.editor.SetCursor(parse_error_info.line - 1, parse_error_info.column > 0 ? parse_error_info.column - 1 : 0);
            tab.editor.ScrollToLine(parse_error_info.line - 1, TextEditor::Scroll::alignMiddle);
        }
        tab.avaui_load_error = parse_error;
    }
}

void ToggleFold(EditorTab& tab, int start_line) {
    if (!tab.fold_index.RangeStartingAt(start_line)) return;
    if (!tab.folded_lines.insert(start_line).second) tab.folded_lines.erase(start_line);
}

bool IsLineFolded(const EditorTab& tab, int start_line) {
    return tab.folded_lines.count(start_line) != 0;
}

void FoldAll(EditorTab& tab) {
    tab.folded_lines.clear();
    for (const auto& range : tab.fold_index.Ranges()) tab.folded_lines.insert(range.start_line);
}

void UnfoldAll(EditorTab& tab) {
    tab.folded_lines.clear();
}

namespace {

int FindLineOf(const std::string& text, const std::string& needle) {
    const size_t pos = text.find(needle);
    if (pos == std::string::npos) return -1;
    return static_cast<int>(std::count(text.begin(), text.begin() + static_cast<long>(pos), '\n'));
}

void JumpToCodeBehindHandler(EditorState& state, EditorTab& tab, const std::string& handler_name) {
    if (tab.view_mode == TabViewMode::Design) {
        ToggleTabViewMode(state, tab);
    }
    const int func_line = FindLineOf(tab.GetText(), "func " + handler_name + "(");
    if (func_line < 0) return;

    tab.editor.SetCursor(func_line + 1, 4);
    tab.editor.ScrollToLine(func_line, TextEditor::Scroll::alignMiddle);
}

}

bool HasUnsavedChanges(const EditorState& state) {
    for (const auto& tab : state.tabs) {
        if (!tab->is_welcome && tab->dirty) return true;
    }
    return false;
}

void SaveAllTabs(EditorState& state) {
    for (auto& tab : state.tabs) {

        if (!tab->is_welcome && tab->dirty) SaveTab(state, *tab);
    }
    state.workspace_index.MarkDirty();
}

namespace {

void DrawWelcomeTab(EditorState& state) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(0.0f, avail.y * 0.16f));
    const float center_x = ImGui::GetCursorPosX() + avail.x * 0.5f;

    constexpr float kIconSize = 56.0f;
    {
        const ImVec2 icon_pos = ImGui::GetCursorScreenPos();
        const unsigned int logo_texture = branding::GetLogoTextureId();
        if (logo_texture != 0) {
            ImGui::SetCursorScreenPos(ImVec2(center_x - kIconSize * 0.5f, icon_pos.y));
            ImGui::Image(static_cast<ImTextureID>(logo_texture), ImVec2(kIconSize, kIconSize));
        } else {

            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(center_x, icon_pos.y + kIconSize * 0.5f), kIconSize * 0.5f,
                palette::U32FromHex(palette::kPrimary));
        }
        ImGui::Dummy(ImVec2(0.0f, kIconSize + 14.0f));
    }
    {
        const char* title = "Ava Studio";
        const ImVec2 size = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX(center_x - size.x * 0.5f);
        ImGui::TextColored(palette::FromHex(palette::kTextPrimary), "%s", title);
    }
    {
        const std::string subtitle = util::Tr("about.tagline");
        const ImVec2 size = ImGui::CalcTextSize(subtitle.c_str());
        ImGui::SetCursorPosX(center_x - size.x * 0.5f);
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", subtitle.c_str());
    }

    ImGui::Dummy(ImVec2(0.0f, 28.0f));

    const float button_w = 200.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
    ImGui::SetCursorPosX(center_x - button_w * 0.5f);
    if (ImGui::Button(util::Tr("menu.file.new_project").c_str(), ImVec2(button_w, 0.0f))) {
        state.new_project_requested = true;
    }

    ImGui::SetCursorPosX(center_x - button_w * 0.5f);
    if (ImGui::Button(util::Tr("editor.welcome.open_project").c_str(), ImVec2(button_w, 0.0f))) {
        state.open_folder_requested = true;
    }
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0.0f, 24.0f));
    {
        const std::string hint = util::Tr("editor.welcome.hint");
        const ImVec2 size = ImGui::CalcTextSize(hint.c_str());
        ImGui::SetCursorPosX(center_x - size.x * 0.5f);
        ImGui::TextDisabled("%s", hint.c_str());
    }
}

}

void SaveActiveTab(EditorState& state) {
    if (EditorTab* tab = state.Active()) {
        SaveTab(state, *tab);
        state.workspace_index.MarkDirty();
    }
}

void RequestCloseTab(EditorState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.tabs.size())) return;
    if (state.tabs[index]->dirty) {
        state.pending_close_index = index;
    } else {
        CloseTabNow(state, index);
    }
}

void CloseTabForPath(EditorState& state, const std::string& path) {
    if (path.empty()) return;

    int index = FindTabForPath(state, path);
    if (index >= 0) {
        CloseTabNow(state, index);
    }

    const std::filesystem::path dir(path);
    for (int i = static_cast<int>(state.tabs.size()) - 1; i >= 0; --i) {
        if (PathContains(dir, std::filesystem::path(state.tabs[i]->file_path))) {
            CloseTabNow(state, i);
        }
    }
}

void RenameTabPath(EditorState& state, const std::string& old_path, const std::string& new_path) {
    if (old_path.empty()) return;

    int index = FindTabForPath(state, old_path);
    if (index >= 0) {
        state.tabs[index]->file_path = new_path;
        return;
    }

    const std::filesystem::path old_dir(old_path);
    const std::filesystem::path new_dir(new_path);
    for (auto& tab : state.tabs) {
        std::filesystem::path rel;
        if (PathContains(old_dir, std::filesystem::path(tab->file_path), &rel) && !rel.empty()) {
            tab->file_path = (new_dir / rel).string();
        }
    }
}

void HighlightError(EditorState& state, const std::string& file_path, int line, int column,
                     const std::string& message) {
    if (line <= 0 || file_path.empty()) return;
    int index = FindTabForPath(state, file_path);
    if (index < 0) return;
    EditorTab& tab = *state.tabs[index];

    tab.editor.ClearMarkers();

    const ImU32 error_color = palette::U32FromHex(palette::kError, 0.35f);

    std::string tooltip =
        column > 0 ? message : (TrFormat("editor.error_at_line", std::to_string(line)) + message);

    tab.editor.AddMarker(line - 1, error_color, error_color, tooltip, tooltip);

    tab.editor.SetCursor(line - 1, column > 0 ? column - 1 : 0);
    tab.editor.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle);
}

void ClearErrorHighlights(EditorState& state) {
    for (auto& tab : state.tabs) {
        if (tab) tab->editor.ClearMarkers();
    }
}

void SelectMatchInEditor(EditorState& state, const std::string& file_path, int line, int column_start,
                          int column_end) {
    if (line <= 0 || file_path.empty()) return;
    int index = FindTabForPath(state, file_path);
    if (index < 0) return;
    EditorTab& tab = *state.tabs[index];

    const int start_col = column_start > 0 ? column_start - 1 : 0;
    const int end_col = column_end > column_start ? column_end - 1 : start_col + 1;

    tab.editor.SelectRegion(line - 1, start_col, line - 1, end_col);
    tab.editor.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle);
}

namespace {

constexpr float kMinimapWidth = 90.0f;
constexpr float kMinimapGutter = 4.0f;
constexpr float kMinimapCharWidth = 1.2f;
constexpr float kMinimapMaxRowHeight = 3.0f;
constexpr float kMinimapLeftMargin = 3.0f;

bool MinimapTokenIsBlank(const std::string& text, int start, int end) {
    for (int i = start; i < end; ++i) {
        if (!std::isspace(static_cast<unsigned char>(text[i]))) return false;
    }
    return true;
}

void ScrollMinimapToLocalY(EditorTab& tab, float local_y, float panel_height, float row_height,
                            int total_lines) {
    if (total_lines <= 0) return;
    local_y = std::clamp(local_y, 0.0f, panel_height);
    const int target_line = std::clamp(static_cast<int>(local_y / row_height), 0, total_lines - 1);
    tab.editor.ScrollToLine(target_line, TextEditor::Scroll::alignMiddle);
}

void DrawEditorMinimap(EditorTab& tab, const ImVec2& size) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##editor_minimap", size, false,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(p0, p1, palette::U32FromHex(palette::kSurface));
    draw->AddLine(p0, ImVec2(p0.x, p1.y), palette::U32FromHex(palette::kBorder));

    const int total_lines = tab.editor.GetLineCount();
    if (total_lines > 0 && size.y > 0.0f) {
        float row_height = kMinimapMaxRowHeight;
        if (static_cast<float>(total_lines) * row_height > size.y) {
            row_height = size.y / static_cast<float>(total_lines);
        }
        row_height = std::max(row_height, 0.05f);

        const int first_visible = tab.editor.GetFirstVisibleLine();
        const int last_visible = tab.editor.GetLastVisibleLine();
        const float viewport_y0 = p0.y + static_cast<float>(first_visible) * row_height;
        const float viewport_y1 = p0.y + static_cast<float>(last_visible + 1) * row_height;
        draw->AddRectFilled(ImVec2(p0.x, viewport_y0), ImVec2(p1.x, viewport_y1),
                             palette::U32FromHex(palette::kTextPrimary, 0.10f));
        draw->AddRect(ImVec2(p0.x, viewport_y0), ImVec2(p1.x, viewport_y1),
                      palette::U32FromHex(palette::kTextPrimary, 0.30f));

        float last_drawn_y = -1.0f;
        for (int line = 0; line < total_lines; ++line) {
            const float y = p0.y + static_cast<float>(line) * row_height;
            if (y > p1.y) break;
            if (y - last_drawn_y < 1.0f) continue;  
            last_drawn_y = y;

            const std::string text = tab.editor.GetLineText(line);
            if (text.empty()) continue;

            const float block_h = std::max(1.0f, row_height - 0.4f);
            for (const syntax::Token& token : syntax::Tokenize(text)) {
                if (MinimapTokenIsBlank(text, token.start, token.end)) continue;

                const float x0 = p0.x + kMinimapLeftMargin + static_cast<float>(token.start) * kMinimapCharWidth;
                if (x0 >= p1.x) break;
                const float x1 = std::min(
                    p0.x + kMinimapLeftMargin + static_cast<float>(token.end) * kMinimapCharWidth, p1.x - 1.0f);
                if (x1 <= x0) continue;

                ImVec4 color = ImGui::ColorConvertU32ToFloat4(syntax::ColorForToken(token.kind));
                color.w = 0.8f;  
                draw->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + block_h),
                                     ImGui::ColorConvertFloat4ToU32(color));
            }
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            tab.minimap_dragging = true;
        }
        if (tab.minimap_dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ScrollMinimapToLocalY(tab, ImGui::GetMousePos().y - p0.y, size.y, row_height, total_lines);
            } else {
                tab.minimap_dragging = false;
            }
        }
    }

    ImGui::EndChild();
}

}

std::vector<ProblemEntry> CollectDiagnosticProblems(const EditorState& state) {
    std::vector<ProblemEntry> entries;
    for (const auto& tab : state.tabs) {
        if (!tab || tab->is_welcome || tab->file_path.empty()) continue;
        for (const auto& diag : tab->diagnostics) {
            ProblemEntry entry;
            entry.file = tab->file_path;
            entry.line = diag.line + 1;
            entry.column = diag.column_start + 1;
            entry.message = diag.message;
            entry.severity = diag.severity;
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

void DrawEditorPanel(EditorState& state) {
    state.workspace_index.Poll();

    state.designer_selection.reset();
    state.code_editor_has_focus = false;

    const std::string panel_title = util::Tr("panel.editor.title") + "###code_editor";
    ImGui::Begin(panel_title.c_str());

    if (state.tabs.empty()) {
        ImGui::TextDisabled("%s", util::Tr("editor.no_file_open").c_str());
        ImGui::End();
        return;
    }

    const ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable |
                                            ImGuiTabBarFlags_AutoSelectNewTabs |
                                            ImGuiTabBarFlags_TabListPopupButton |
                                            ImGuiTabBarFlags_FittingPolicyScroll;

    int tab_to_close = -1;

    ImGui::PushStyleColor(ImGuiCol_TabActive, palette::FromHex(palette::kPrimary, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, palette::FromHex(palette::kPrimary, 0.12f));

    if (ImGui::BeginTabBar("##EditorTabs", tab_bar_flags)) {
        for (int i = 0; i < static_cast<int>(state.tabs.size()); ++i) {
            EditorTab& tab = *state.tabs[i];

            std::string label = tab.DisplayName();
            char id_buf[320];
            std::snprintf(id_buf, sizeof(id_buf), "%s###tab%d", label.c_str(), tab.id);

            bool tab_open = true;

            const ImGuiTabItemFlags item_flags =
                (tab.id == state.focus_tab_id) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            const bool selected = ImGui::BeginTabItem(id_buf, &tab_open, item_flags);

            if (tab.dirty) {
                const ImVec2 dot_p0 = ImGui::GetItemRectMin();
                const ImVec2 dot_p1 = ImGui::GetItemRectMax();
                const float radius = 3.0f;
                const ImVec2 center(dot_p1.x - radius - 8.0f, (dot_p0.y + dot_p1.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, palette::U32FromHex(palette::kTextMuted));
            }

            if (selected) {

                const ImVec2 p0 = ImGui::GetItemRectMin();
                const ImVec2 p1 = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p0, ImVec2(p1.x, p0.y + 2.0f), palette::U32FromHex(palette::kPrimary));
                state.active_tab = i;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                tab_open = false;
            }
            if (selected) {
                MaybeRebuildIndex(state, tab);
                if (state.format_on_type) MaybeAutoFormatOnType(state, tab);

                if (tab.is_welcome) {
                    DrawWelcomeTab(state);
                } else if (tab.is_avaui && tab.view_mode == TabViewMode::Design) {
                    if (!tab.avaui_load_error.empty()) {
                        ImGui::TextColored(
                            palette::FromHex(palette::kWarning), "%s",
                            TrFormat("editor.avaui.read_error", {tab.DisplayName(), tab.avaui_load_error}).c_str());
                        ImGui::TextDisabled("%s", util::Tr("editor.avaui.blank_page_note").c_str());
                        ImGui::Separator();
                    }

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    std::string generated_handler;
                    if (auto selected_node = DrawDesignerCanvas(tab.design, avail, state.project_root, tab.id,
                                                                 &generated_handler, state.log_bridge)) {
                        state.designer_selection = std::move(selected_node);
                    }
                    if (tab.design.dirty) tab.dirty = true;

                    if (!generated_handler.empty()) {
                        JumpToCodeBehindHandler(state, tab, generated_handler);
                    }
                } else {

                    if (tab.is_avaui && !tab.avaui_load_error.empty()) {
                        ImGui::TextColored(
                            palette::FromHex(palette::kWarning), "%s",
                            TrFormat("editor.avaui.parse_error", {tab.DisplayName(), tab.avaui_load_error}).c_str());
                        ImGui::TextDisabled("%s", util::Tr("editor.avaui.fix_syntax_hint").c_str());
                        ImGui::Separator();
                    }
                    ImVec2 avail = ImGui::GetContentRegionAvail();

                    const bool show_minimap = state.show_minimap;
                    const float minimap_reserved = show_minimap ? (kMinimapWidth + kMinimapGutter) : 0.0f;
                    avail.x = std::max(avail.x - minimap_reserved, 0.0f);

                    const ImVec2 editor_min = ImGui::GetCursorScreenPos();
                    const ImVec2 editor_max = ImVec2(editor_min.x + avail.x, editor_min.y + avail.y);

                    const DotCompletionPending dot_pending = PrepareDotCompletion(state, tab);
                    DotCompletionKeys dot_keys;
                    if (dot_pending.active) {
                        dot_keys.nav_down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);
                        dot_keys.nav_up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true);
                        dot_keys.accept = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                          ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
                                          ImGui::IsKeyPressed(ImGuiKey_Tab, false);
                        dot_keys.dismiss = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
                        ClaimDotCompletionKeys();
                    }

                    if (tab.colored_interface_generation != languages::KnownInterfaceNamesGeneration() ||
                        tab.colored_variable_generation != languages::KnownVariableNamesGeneration() ||
                        tab.colored_class_generation != languages::KnownClassNamesGeneration()) {
                        tab.editor.SetLanguage(languages::AvaLang());
                        tab.colored_interface_generation = languages::KnownInterfaceNamesGeneration();
                        tab.colored_variable_generation = languages::KnownVariableNamesGeneration();
                        tab.colored_class_generation = languages::KnownClassNamesGeneration();
                    }

                    const bool zoom_hover = ImGui::IsMouseHoveringRect(editor_min, editor_max);
                    const bool zoom_key_scope = state.code_editor_has_focus || zoom_hover;
                    const ImGuiIO& zoom_io = ImGui::GetIO();
                    if (zoom_hover && zoom_io.KeyCtrl && zoom_io.MouseWheel != 0.0f) {
                        tab.zoom = ClampEditorZoom(tab.zoom + zoom_io.MouseWheel * kEditorWheelZoomStep);
                    }
                    if (ShortcutRegistry::Instance().Pressed(ShortcutId::ZoomIn, zoom_key_scope) ||
                        (zoom_key_scope && zoom_io.KeyCtrl &&
                         ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false))) {
                        tab.zoom = ClampEditorZoom(tab.zoom + kEditorWheelZoomStep);
                    }
                    if (ShortcutRegistry::Instance().Pressed(ShortcutId::ZoomOut, zoom_key_scope) ||
                        (zoom_key_scope && zoom_io.KeyCtrl &&
                         ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false))) {
                        tab.zoom = ClampEditorZoom(tab.zoom - kEditorWheelZoomStep);
                    }
                    if (ShortcutRegistry::Instance().Pressed(ShortcutId::ZoomReset, zoom_key_scope)) {
                        tab.zoom = 1.0f;
                    }
                    // NOTE: previously this used ImGui::SetWindowFontScale(tab.zoom).
                    // That call only stretches glyphs when *rendering* the current
                    // window; it does not change the font size that ImGui/TextEditor
                    // use to *compute* layout (GetLineHeight()/GetGlyphWidth(), which
                    // our own gutter overlays below rely on), and it does not apply to
                    // the child window that TextEditor::Render() creates internally
                    // (FontWindowScale is per-window and is not inherited by children).
                    // The result: as tab.zoom grows, the on-screen glyphs get bigger
                    // but the line-height/gutter math the widget (and our own overlay
                    // code) uses to position the line-number gutter, carets, squiggles
                    // etc. does not grow the same way, so the left-hand gutter drifts
                    // away from the actual text rows -- worse the further you zoom in.
                    //
                    // ImGui 1.92+ (which this project's TextEditor fork targets, see
                    // README "Supports dynamic font sizes") replaces SetWindowFontScale
                    // with real dynamic font sizing: PushFont(font, size) re-rasterizes
                    // at the requested pixel size and updates the actual font metrics
                    // used everywhere (including inside nested child windows), so every
                    // consumer of GetLineHeight()/GetGlyphWidth() stays in sync with
                    // what's drawn on screen regardless of the zoom level.
                    ImGui::PushFont(nullptr, kEditorBaseFontSizePx * tab.zoom);

                    tab.editor.Render("##editor", avail, false);
                    state.code_editor_has_focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

                    if (show_minimap) {
                        ImGui::SameLine(0.0f, kMinimapGutter);
                        DrawEditorMinimap(tab, ImVec2(kMinimapWidth, avail.y));
                    }

                    if (state.show_inlay_hints) {
                        DrawInlayHints(tab, editor_min, editor_max);
                    }
                    DrawIncompleteInterfaceSquiggles(tab, editor_min, editor_max);
                    DrawDiagnosticSquiggles(tab, editor_min, editor_max);
                    DrawQuickFixGutterIcon(state, tab, editor_min, editor_max);

                    const bool goto_def_hover = ImGui::IsMouseHoveringRect(editor_min, editor_max);
                    const bool goto_def_key =
                        ShortcutRegistry::Instance().Pressed(ShortcutId::GotoDefinition, state.code_editor_has_focus);
                    const bool goto_def_click = goto_def_hover &&
                                                 ImGui::GetIO().KeyCtrl &&
                                                 ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                    const TextEditor::CursorPosition hover_pos =
                        goto_def_click ? ScreenPosToCursor(tab, ImGui::GetMousePos(), editor_min)
                                        : tab.editor.GetCursorPosition(0);
                    if (goto_def_click) {
                        tab.editor.SetCursor(hover_pos.line, hover_pos.column);
                    }
                    DefinitionTarget hover_target;
                    const bool has_definition_here = ResolveDefinitionTarget(state, tab, hover_pos, hover_target);
                    if ((goto_def_key || goto_def_click) && has_definition_here) {
                        JumpToDefinition(state, tab, hover_target);
                    }

                    if (ImGui::IsMouseHoveringRect(editor_min, editor_max) &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        tab.context_menu_click_valid = true;
                        tab.context_menu_click_pos = ScreenPosToCursor(tab, ImGui::GetMousePos(), editor_min);
                    }
                    const TextEditor::CursorPosition menu_pos = tab.context_menu_click_valid
                                                                      ? tab.context_menu_click_pos
                                                                      : tab.editor.GetCursorPosition(0);
                    DefinitionTarget menu_target;
                    const bool menu_has_definition = ResolveDefinitionTarget(state, tab, menu_pos, menu_target);
                    const std::vector<QuickFix> menu_quick_fixes = CollectQuickFixesAt(state, tab, menu_pos);

                    if (ImGui::BeginPopupContextItem("##editor_context_menu")) {
                        const std::string goto_def_label = ShortcutRegistry::Instance().Label(ShortcutId::GotoDefinition);
                        if (ImGui::MenuItem(util::Tr("editor.context.goto_definition").c_str(), goto_def_label.c_str(),
                                            false, menu_has_definition)) {
                            JumpToDefinition(state, tab, menu_target);
                        }
                        if (!menu_quick_fixes.empty()) {
                            ImGui::Separator();
                            for (const QuickFix& fix : menu_quick_fixes) {
                                if (ImGui::MenuItem(fix.label.c_str())) fix.apply();
                            }
                        }
                        ImGui::Separator();
                        const bool has_selection = tab.editor.AnyCursorHasSelection();
                        if (ImGui::MenuItem(util::Tr("editor.context.cut").c_str(), "Ctrl+X", false,
                                            has_selection)) {
                            tab.editor.Cut();
                        }
                        if (ImGui::MenuItem(util::Tr("editor.context.copy").c_str(), "Ctrl+C", false,
                                            has_selection)) {
                            tab.editor.Copy();
                        }
                        if (ImGui::MenuItem(util::Tr("editor.context.paste").c_str(), "Ctrl+V")) {
                            tab.editor.Paste();
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem(util::Tr("editor.context.select_all").c_str(), "Ctrl+A")) {
                            tab.editor.SelectAll();
                        }
                        ImGui::EndPopup();
                    }

                    if (ImGui::IsMouseHoveringRect(editor_min, editor_max)) {
                        if (!DrawParameterHint(tab)) {
                            const bool dot_popup_drawn =
                                dot_pending.active &&
                                DrawDotCompletionPopup(tab, editor_min, dot_pending, dot_keys);
                            if (!dot_popup_drawn) DrawKeywordHint(state, tab, editor_min);
                        }
                    }

                    ImGui::PopFont();
                }
                ImGui::EndTabItem();
            }
            if (!tab_open) {
                tab_to_close = i;
            }
        }
        ImGui::EndTabBar();
    }
    state.focus_tab_id = -1;
    ImGui::PopStyleColor(2);

    if (tab_to_close >= 0) {
        RequestCloseTab(state, tab_to_close);
    }

    const std::string unsaved_title = util::Tr("editor.unsaved.title") + "##EditorCloseConfirm";
    if (state.pending_close_index >= 0) {
        ImGui::OpenPopup(unsaved_title.c_str());
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f));
    if (ImGui::BeginPopupModal(unsaved_title.c_str(), nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        const int idx = state.pending_close_index;
        if (idx < 0 || idx >= static_cast<int>(state.tabs.size())) {
            state.pending_close_index = -1;
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextWrapped("%s", TrFormat("editor.unsaved.message", state.tabs[idx]->DisplayName()).c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("%s", util::Tr("editor.unsaved.warning").c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float button_w = 100.0f;
            const bool is_untitled = state.tabs[idx]->file_path.empty();
            if (ImGui::Button(util::Tr("menu.file.save").c_str(), ImVec2(button_w, 0.0f))) {
                if (is_untitled) {

                    state.pending_close_index = -1;
                } else {
                    SaveTab(state, *state.tabs[idx]);
                    CloseTabNow(state, idx);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(util::Tr("editor.unsaved.dont_save").c_str(), ImVec2(button_w, 0.0f))) {
                CloseTabNow(state, idx);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(util::Tr("common.cancel").c_str(), ImVec2(button_w, 0.0f))) {
                state.pending_close_index = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}