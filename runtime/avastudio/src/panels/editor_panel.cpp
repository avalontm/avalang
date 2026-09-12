#include "panels/editor_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "branding/logo_texture.h"
#include "fonts/embedded_font.h"
#include "imgui.h"
// For ImGuiWindow/GImGui (popup hit-testing) and ImGuiKeyData (key suppression) --
// already used the same way by src/main.cpp.
#include "imgui_internal.h"
#include "languages/avalang_language.h"
#include "languages/block_scanner.h"
#include "languages/keyword_docs.h"
#include "languages/lexer_utils.h"
#include "languages/member_access_resolver.h"
#include "palette.h"
#include "panels/designer_canvas.h"
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

// Editor text zoom (mouse wheel + keyboard shortcuts, see DrawEditorPanel). The
// vendored editor (TextEditor::render, "Legacy") snapshots ImGui::GetFontSize()
// once at the very top of its own render() -- *before* it opens its internal
// child window -- and feeds that single value into every glyph measurement and
// draw call for the frame (glyphSize, line/column layout, font->RenderChar).
// That means scaling the *panel* window's font via ImGui::SetWindowFontScale()
// immediately before calling tab.editor.Render() is enough to zoom the whole
// editor; no changes to the vendored library are needed.
constexpr float kEditorMinZoom = 0.5f;
constexpr float kEditorMaxZoom = 3.0f;
constexpr float kEditorWheelZoomStep = 0.1f;

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
}

// True if `member_name` is reachable by walking `start`'s own heritage graph
// (start itself, then its heritage_names, recursively) -- i.e. whether
// `start`, as directly listed on a class's header line, is the one actually
// responsible for requiring `member_name`. Used to attribute each abstract
// member MissingInterfaceMembers reports (which only knows the closest
// declared_in, possibly several `interface IB : IA` hops away) back to the
// interface name written in the class's own heritage clause, since that's
// where VS/Roslyn puts the squiggle.
bool HeritageChainDeclares(const ClassIndex& class_index, const std::string& start,
                            const std::string& member_name, std::unordered_set<std::string>& visited) {
    if (!visited.insert(start).second) return false;
    const ClassInfo* info = class_index.Find(start);
    if (!info) return false;
    if (info->methods.count(member_name)) return true;
    for (const auto& parent : info->heritage_names) {
        if (HeritageChainDeclares(class_index, parent, member_name, visited)) return true;
    }
    return false;
}

// Recomputes tab.incomplete_interfaces from the freshly-rebuilt class_index.
// Only classes actually declared in this buffer are considered -- flagging a
// class defined in an imported file would point the squiggle/quick-fix at
// the wrong file (or nowhere, since ImplementMissingInterfaceMembers only
// knows how to edit the currently open tab). One IncompleteInterface is
// produced per directly-listed heritage name that's still missing at least
// one member, not per class, so `class C : IShape, IColor` with only IColor
// incomplete squiggles just IColor.
void RebuildIncompleteInterfaces(EditorTab& tab) {
    tab.incomplete_interfaces.clear();
    for (const auto& [name, info] : tab.class_index.Classes()) {
        if (info.is_interface || !info.source_file.empty()) continue;
        std::vector<ClassMember> missing = tab.class_index.MissingInterfaceMembers(name);
        if (missing.empty()) continue;

        std::vector<std::string> order;
        std::unordered_map<std::string, std::vector<ClassMember>> by_heritage;
        for (auto& member : missing) {
            for (const auto& heritage : info.heritage_names) {
                std::unordered_set<std::string> visited;
                if (!HeritageChainDeclares(tab.class_index, heritage, member.name, visited)) continue;
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

void RebuildIndexAndTrie(EditorState& state, EditorTab& tab) {
    ImportFileCache import_cache;
    const std::string dir = DirOf(tab.file_path);
    tab.function_index.Rebuild(tab.GetText(), dir, &import_cache, tab.modules_path);
    tab.class_index.Rebuild(tab.GetText(), dir, &import_cache, tab.modules_path);
    RebuildIncompleteInterfaces(tab);

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

    std::shared_ptr<const WorkspaceIndex::Snapshot> workspace = state.workspace_index.CurrentSnapshot();
    tab.variable_type_index.Rebuild(tab.GetText(), tab.class_index, tab.function_index, &workspace->classes,
                                     &workspace->functions);

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

void PopulateGeneralSuggestions(EditorTab& tab, TextEditor::AutoCompleteState& ac_state) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> ordered;

    const std::string search_term_lower = ToLowerAscii(ac_state.searchTerm);

    auto add_filtered = [&](const std::string& name) {
        if (ordered.size() >= kAutocompleteLimit) return;
        if (!search_term_lower.empty()) {
            const std::string name_lower = ToLowerAscii(name);
            if (name_lower.compare(0, search_term_lower.size(), search_term_lower) != 0) return;
        }
        if (seen.insert(name).second) ordered.push_back(name);
    };

    auto add_raw = [&](const std::string& name) {
        if (ordered.size() >= kAutocompleteLimit) return;
        if (seen.insert(name).second) ordered.push_back(name);
    };

    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);

    std::vector<std::string> own_variables;
    std::vector<ClassMember> own_members;
    ResolveOwnScopeSuggestions(tab.GetText(), pos.line, before, tab.class_index, tab.variable_type_index,
                                own_variables, own_members);

    for (const auto& name : own_variables) add_filtered(name);
    for (const auto& member : own_members) add_filtered(member.name);

    std::vector<std::string> prefix_matches;
    tab.autocomplete_trie.findSuggestions(prefix_matches, ac_state.searchTerm, kAutocompleteLimit,
                                           /*maxSkippedLetters=*/0);
    for (const auto& word : prefix_matches) add_raw(word);

    if (ordered.size() < kAutocompleteLimit && ac_state.searchTerm.size() >= kAutocompleteFuzzyMinChars) {
        std::vector<std::string> fuzzy_matches;
        tab.autocomplete_trie.findSuggestions(fuzzy_matches, ac_state.searchTerm, kAutocompleteLimit,
                                               /*maxSkippedLetters=*/2);
        for (const auto& word : fuzzy_matches) add_raw(word);
    }

    ac_state.suggestions = std::move(ordered);
}

bool PopulateMemberSuggestions(EditorState& state, EditorTab& tab, TextEditor::AutoCompleteState& ac_state) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);

    MemberAccessContext ctx;
    std::vector<ClassMember> members;
    if (!ResolveVisibleMembers(state, tab, pos.line, before, ctx, members)) return false;

    const std::string search_term_lower = ToLowerAscii(ac_state.searchTerm);

    std::vector<std::string> suggestions;
    for (const auto& member : members) {
        if (!search_term_lower.empty()) {
            const std::string name_lower = ToLowerAscii(member.name);
            if (name_lower.compare(0, search_term_lower.size(), search_term_lower) != 0) continue;
        }
        // NOTE: this feeds the editor widget's own built-in suggestion popup (used once the
        // user types a filter letter after the dot -- see DrawDotCompletionPopup, which only
        // covers the bare "just typed a dot" moment). That popup inserts whatever string is
        // selected verbatim on Tab/Enter/click, so this must be plain insertable text, not the
        // decorated "name : Type" / full signature label used for on-screen display elsewhere.
        std::string insert_text = member.name;
        if (member.is_method) insert_text += "()";
        suggestions.push_back(insert_text);
    }

    if (suggestions.empty()) return false;
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

std::string WordEndingAtCursor(const std::string& line_before_cursor) {
    size_t end = line_before_cursor.size();
    size_t start = end;
    while (start > 0 && IsIdentChar(line_before_cursor[start - 1])) --start;
    return line_before_cursor.substr(start, end - start);
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
    // Use the *full* identifier the cursor sits inside -- scanning both left and right from the
    // column, not just backward from it. A plain left click places the caret wherever the pixel
    // under the mouse lands, which is very often in the *middle* of a word, not at its end.
    // Scanning backward-only from there (the old behavior) grabbed just the prefix up to the
    // click point -- e.g. clicking between the 't' and 'e' of "test" produced the word "t", not
    // "test" -- so whether F12 worked at all depended on exactly which pixel/column inside the
    // word you happened to click. That's the "a veces se traba" behavior: a right-click (whose
    // column gets rounded to the nearest character, see ScreenPosToCursor) or a second click
    // would often land differently and happen to hit the end of the word, while a first plain
    // click landing mid-word would not.
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

bool DrawKeywordHint(EditorTab& tab) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string word = WordEndingAtCursor(TextBeforeCursor(tab, pos));
    if (word.empty() || !IsIdentStart(word[0])) return false;

    const auto& docs = KeywordDocs();

    auto exact = docs.find(word);
    const KeywordDoc* match = exact != docs.end() ? &exact->second : nullptr;

    if (!match) {
        const KeywordDoc* candidate = nullptr;
        for (const auto& [name, doc] : docs) {
            if (name.size() > word.size() && name.compare(0, word.size(), word) == 0) {
                if (candidate) { candidate = nullptr; break; }
                candidate = &doc;
            }
        }
        match = candidate;
    }
    if (!match) return false;

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
    const int first_visible_line = tab.editor.GetFirstVisibleLine();
    const int first_visible_column = tab.editor.GetFirstVisibleColumn();
    const float gutter_width = EstimateGutterWidth(tab, glyph_width);

    const float x = editor_screen_min.x + gutter_width +
                     static_cast<float>(pos.column - first_visible_column) * glyph_width;
    const float y = editor_screen_min.y +
                     static_cast<float>(pos.line - first_visible_line) * line_height;
    return ImVec2(x, y);
}

// Inverse of EstimateCaretScreenPos: which line/column is under a given screen point. Used so
// the right-click context menu ("Ir a la definición" etc.) acts on whatever the user actually
// clicked on, not on the blinking text cursor -- the vendored TextEditor only moves that cursor
// on left clicks (see its handling of ImGuiMouseButton_Right in TextEditor.cpp), so reading
// GetCursorPosition() after a right click reports a stale, unrelated position.
TextEditor::CursorPosition ScreenPosToCursor(EditorTab& tab, const ImVec2& screen_pos,
                                              const ImVec2& editor_screen_min) {
    const float line_height = tab.editor.GetLineHeight();
    const float glyph_width = tab.editor.GetGlyphWidth();
    const int first_visible_line = tab.editor.GetFirstVisibleLine();
    const int first_visible_column = tab.editor.GetFirstVisibleColumn();
    const float gutter_width = EstimateGutterWidth(tab, glyph_width);

    const int line =
        first_visible_line + std::max(0, static_cast<int>((screen_pos.y - editor_screen_min.y) / line_height));
    const int column =
        first_visible_column +
        std::max(0, static_cast<int>((screen_pos.x - editor_screen_min.x - gutter_width + glyph_width * 0.5f) /
                                      glyph_width));
    return TextEditor::CursorPosition(line, column);
}

// Column (0-based, in glyphs) where `interface_name` starts in the
// comma-separated heritage clause after the ':' on `line_text`, e.g. the
// "IShape" in `class Circle : IShape, IColor`. class_index only keeps the
// line, not per-name columns, so this re-finds it the same cheap way
// ScreenPosToCursor's caller already re-derives things from GetLineText
// rather than growing ClassInfo for a couple of rendering-only callers.
// Returns -1 if not found (heritage clause missing, or edited since the
// index was last rebuilt).
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

// Picks which IncompleteInterface (if any) a right-click at `pos` should act
// on. Several interfaces can be incomplete on the same class header line
// (`class C : IShape, IColor`), so a click landing inside one name's own
// squiggle span wins that one specifically; otherwise this falls back to the
// first incomplete interface on the line, same as clicking anywhere else on
// a Roslyn squiggle line does.
const EditorTab::IncompleteInterface* FindIncompleteInterfaceAtLine(const EditorTab& tab,
                                                                     const TextEditor::CursorPosition& pos) {
    const EditorTab::IncompleteInterface* line_match = nullptr;
    const std::string line_text = tab.editor.GetLineText(pos.line);
    for (const auto& entry : tab.incomplete_interfaces) {
        if (entry.line != pos.line) continue;
        if (!line_match) line_match = &entry;
        const int col = HeritageNameColumnOnLine(line_text, entry.interface_name);
        if (col >= 0 && pos.column >= col &&
            pos.column <= col + static_cast<int>(entry.interface_name.size())) {
            return &entry;
        }
    }
    return line_match;
}

// Draws a VS-Code-style wavy amber underline under the name of every
// currently-incomplete interface in a class's heritage clause -- matching
// Roslyn (`class Circle : IShape` underlines IShape, not Circle, and the
// interface's own file is left untouched since the interface itself is
// valid) -- plus a tooltip listing what's missing when the mouse hovers it.
// Purely a rendering overlay -- it never touches the buffer, so it's cheap
// enough to run every frame off the cached tab.incomplete_interfaces rather
// than only when hovering.
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

// Builds the stub body inserted for one missing interface method, e.g.:
//     func Area(x, y) as float
//         # TODO: implementar
//     end
// `indent` is the class body's own indentation (whatever the class already
// uses), so generated stubs match the surrounding code instead of hardcoding
// a width the project might not use.
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

// Inserts a stub for every still-missing member of `entry` right before the
// class's closing `end`, then rebuilds the index so the squiggle clears once
// the newly-inserted stubs make the class complete.
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

// Everything DrawDotCompletionPopup needs to know, computed *before* tab.editor.Render() runs
// for this frame -- see the long comment on kDotCompletionNavKeys below for why the split
// matters (in short: by the time Render() returns, it has already consumed Up/Down/Enter/Tab
// itself if we don't claim them first).
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

// Keys the popup wants for itself instead of the code underneath it: list navigation
// (Up/Down), accepting a suggestion (Enter/Tab), and dismissing the popup (Escape).
constexpr ImGuiKey kDotCompletionNavKeys[] = {
    ImGuiKey_DownArrow, ImGuiKey_UpArrow, ImGuiKey_Enter, ImGuiKey_KeypadEnter,
    ImGuiKey_Tab,       ImGuiKey_Escape,
};

// The popup is drawn *after* tab.editor.Render() (so it can position itself against the
// editor's current scroll/caret and paint on top of the text), but Render() is a black box
// from the vendored TextEditor library -- it has no idea this popup exists and will happily
// treat Down/Up as "move the caret a line", Enter as "insert a newline", Tab as "insert a
// tab", all *before* our code below even runs. Reading IsKeyPressed() after the fact would
// still report "yes, Down was pressed" (ImGui doesn't consume key reads), but the caret would
// already have jumped a line by then. So: while the popup is (or is about to be) showing, zero
// out these keys' state in ImGui's own key table *before* calling Render(), so the editor
// widget sees them as not-pressed, and only *we* act on the edge we captured beforehand.
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

// Unlike DrawParameterHint/DrawKeywordHint (plain tooltips, never interactive), this draws a
// real ImGui window with clickable *and* keyboard-navigable rows -- see keys.* for how Up/Down/
// Enter/Tab/Escape reach here despite the editor widget being rendered first.
bool DrawDotCompletionPopup(EditorTab& tab, const ImVec2& editor_screen_min,
                            const DotCompletionPending& pending, const DotCompletionKeys& keys) {
    const int count = static_cast<int>(pending.members.size());

    // Reset the selection whenever the popup starts covering a different spot (a new '.', or
    // the member list changed because the filter after it grew/shrank) so a stale index never
    // lands on an out-of-range or unrelated row.
    if (tab.dot_popup_line != pending.pos.line || tab.dot_popup_column != pending.pos.column) {
        tab.dot_popup_selected = 0;
        tab.dot_popup_line = pending.pos.line;
        tab.dot_popup_column = pending.pos.column;
    }

    if (keys.dismiss) {
        // Remember this exact caret spot so re-resolving the same members next frame (caret
        // hasn't moved yet) doesn't just reopen the popup the user just closed.
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

    // Only hide the popup while a mouse press is happening *outside* of it -- e.g. a click to
    // move the caret elsewhere, or a drag-to-select in the code above/below. If the press
    // lands inside the popup's own rect (the user clicking a suggestion), keep it open so the
    // click can land on the Selectable below: unconditionally hiding the window mid-press used
    // to eat that exact click, because the row's mouse-down had no window left to register
    // against by the time the mouse went up.
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
    // Color::identifier is the engine's own default color for any plain
    // identifier the colorizer doesn't otherwise recognize (variables,
    // parameters, anything you type) -- keep it the normal editor text
    // color. Known interface names get their own slot below instead of
    // hijacking this one (see patches/imguicolortextedit_interface_name.patch).
    palette[static_cast<size_t>(TextEditor::Color::identifier)] = palette::U32FromHex(palette::kSynIdentifier);
    palette[static_cast<size_t>(TextEditor::Color::interfaceName)] = palette::U32FromHex(palette::kSynInterface);
    palette[static_cast<size_t>(TextEditor::Color::variableName)] = palette::U32FromHex(palette::kSynVariable);

    tab.editor.SetPalette(palette);
    tab.editor.SetShowLineNumbersEnabled(true);
    tab.editor.SetTabSize(4);
    tab.editor.SetAutoIndentEnabled(true);
    tab.editor.SetShowMatchingBrackets(true);
    tab.editor.SetCompletePairedGlyphs(true);

    tab.editor.SetBoldFont(GetCodeFont());
    tab.editor.SetBoldColors({TextEditor::Color::keyword, TextEditor::Color::declaration});

    tab.editor.SetChangeCallback([&tab] {
        tab.dirty = true;
        tab.editor.ClearMarkers();
        tab.index_dirty = true;
        tab.last_edit_time = ImGui::GetTime();
    }, 0);

    tab.autocomplete_config.callback = [&state, &tab](TextEditor::AutoCompleteState& ac_state) {
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
    // Release this tab's contribution to the shared, reference-counted
    // interface-name table (see UpdateKnownInterfaceNames) -- if some other
    // open tab still declares the same interface name, its own entry keeps
    // the refcount above zero and the color survives.
    languages::UpdateKnownInterfaceNames(state.tabs[index]->known_interface_names, {});
    languages::UpdateKnownVariableNames(state.tabs[index]->known_variable_names, {});
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

void SaveTab(EditorState& state, EditorTab& tab) {
    if (tab.file_path.empty()) return;

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

                    const ImVec2 editor_min = ImGui::GetCursorScreenPos();
                    const ImVec2 editor_max = ImVec2(editor_min.x + avail.x, editor_min.y + avail.y);

                    // Must be computed, and the keys it wants must be claimed, *before*
                    // Render() below -- see the comments on ClaimDotCompletionKeys().
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

                    // The known-interface-names table is shared across every open tab and
                    // can change from some *other* tab's index rebuild (a newly opened
                    // interface, a rename, a tab closing) -- but the vendored editor only
                    // colorizes on SetText()/typed edits, so catch up here before drawing
                    // whenever this tab's own last colorize predates the current
                    // generation (see KnownInterfaceNamesGeneration()'s header comment).
                    if (tab.colored_interface_generation != languages::KnownInterfaceNamesGeneration() ||
                        tab.colored_variable_generation != languages::KnownVariableNamesGeneration()) {
                        tab.editor.SetLanguage(languages::AvaLang());
                        tab.colored_interface_generation = languages::KnownInterfaceNamesGeneration();
                        tab.colored_variable_generation = languages::KnownVariableNamesGeneration();
                    }

                    // --- Editor zoom: Ctrl+mouse wheel and keyboard shortcuts ------------
                    // Must run *before* Render() -- see ClampEditorZoom's comment on why
                    // ImGui::SetWindowFontScale() here is enough to scale the vendored
                    // editor's text. Left active through the squiggles/hover-popup code
                    // below (they measure text against the same zoomed font) and reset to
                    // 1.0f right before EndTabItem() so it never leaks into the tab bar or
                    // any tab drawn afterward.
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
                    ImGui::SetWindowFontScale(tab.zoom);

                    tab.editor.Render("##editor", avail, false);
                    state.code_editor_has_focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
                    DrawIncompleteInterfaceSquiggles(tab, editor_min, editor_max);

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

                    // The vendored editor only moves its own text cursor on left clicks (see its
                    // handling of ImGuiMouseButton_Right in TextEditor.cpp) -- record where a
                    // right click actually landed ourselves, on the exact frame it happens, so
                    // the context menu below can resolve "Ir a la definición" against the word
                    // under the cursor the user right-clicked, not wherever the blinking text
                    // cursor was last left (typically wherever they were previously typing).
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
                    const EditorTab::IncompleteInterface* menu_incomplete_interface =
                        FindIncompleteInterfaceAtLine(tab, menu_pos);

                    // Right-click context menu, VS/VS Code style. BeginPopupContextItem
                    // attaches to the last item, i.e. the "##editor" child Render() just drew.
                    if (ImGui::BeginPopupContextItem("##editor_context_menu")) {
                        const std::string goto_def_label = ShortcutRegistry::Instance().Label(ShortcutId::GotoDefinition);
                        if (ImGui::MenuItem(util::Tr("editor.context.goto_definition").c_str(), goto_def_label.c_str(),
                                            false, menu_has_definition)) {
                            JumpToDefinition(state, tab, menu_target);
                        }
                        if (menu_incomplete_interface) {
                            if (ImGui::MenuItem(util::Tr("editor.context.implement_interface").c_str())) {
                                ImplementMissingInterfaceMembers(state, tab, *menu_incomplete_interface);
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
                            if (!dot_popup_drawn) DrawKeywordHint(tab);
                        }
                    }

                    // Reset the zoom applied above before EndTabItem() -- see the comment
                    // by ImGui::SetWindowFontScale(tab.zoom) above.
                    ImGui::SetWindowFontScale(1.0f);
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
