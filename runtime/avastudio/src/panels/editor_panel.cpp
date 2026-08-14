#include "panels/editor_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "branding/logo_texture.h"
#include "fonts/embedded_font.h"
#include "imgui.h"
#include "languages/avalang_language.h"
#include "languages/keyword_docs.h"
#include "languages/member_access_resolver.h"
#include "palette.h"
#include "panels/designer_canvas.h"
#include "parser/AvauiWriter.h"

namespace studio {

namespace {




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
}



void RebuildIndexAndTrie(EditorTab& tab) {
    tab.function_index.Rebuild(tab.GetText(), DirOf(tab.file_path));




    tab.class_index.Rebuild(tab.GetText(), DirOf(tab.file_path));
    tab.variable_type_index.Rebuild(tab.GetText(), tab.class_index);
    RebuildAutocompleteTrie(tab);
}





















std::string TextBeforeCursor(EditorTab& tab, const TextEditor::CursorPosition& pos) {
    return tab.editor.GetSectionText(pos.line, 0, pos.line, pos.column);
}











bool ResolveVisibleMembers(EditorTab& tab, int cursor_line, const std::string& before,
                           MemberAccessContext& out_ctx, std::vector<ClassMember>& out_members) {
    if (!ResolveMemberAccess(tab.GetText(), cursor_line, before, tab.class_index,
                              tab.variable_type_index, out_ctx)) {
        return false;
    }

    out_members = tab.class_index.FlattenedMembers(out_ctx.class_name);
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
    return member.is_method && member.signature ? member.signature->display : member.name;
}


















bool PopulateMemberSuggestions(EditorTab& tab, TextEditor::AutoCompleteState& ac_state) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);

    MemberAccessContext ctx;
    std::vector<ClassMember> members;
    if (!ResolveVisibleMembers(tab, pos.line, before, ctx, members)) return false;






    ac_state.suggestions.clear();
    for (const auto& member : members) {


        if (!ac_state.searchTerm.empty() &&
            member.name.compare(0, ac_state.searchTerm.size(), ac_state.searchTerm) != 0) {
            continue;
        }
        ac_state.suggestions.push_back(MemberSuggestionLabel(member));
    }
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

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }






std::string WordEndingAtCursor(const std::string& line_before_cursor) {
    size_t end = line_before_cursor.size();
    size_t start = end;
    while (start > 0 && IsIdentChar(line_before_cursor[start - 1])) --start;
    return line_before_cursor.substr(start, end - start);
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
    std::string before = TextBeforeCursor(tab, pos);

    CallContext ctx;
    if (!FindEnclosingCall(before, ctx)) return false;

    const FunctionSignature* sig = tab.function_index.Find(ctx.function_name);
    if (!sig) return false;

    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::BeginTooltip();





    if (sig->is_builtin) {
        DrawHintBadge("BUILT-IN", palette::U32FromHex(palette::kBorder), palette::U32FromHex(palette::kTextSecondary));
    } else {
        DrawHintBadge("FUNCION", palette::U32FromHex(palette::kPrimary), palette::U32FromHex(palette::kBackground));
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
        DrawHintSectionLabel("QUE HACE");
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
            ImGui::TextColored(palette::FromHex(palette::kTextDisabled),
                                "Viene con AvaLang. Podes definir tu propia func %s(...) para reemplazarla.",
                                sig->name.c_str());
        }
    } else if (!sig->source_file.empty()) {
        ImGui::TextColored(palette::FromHex(palette::kTextDisabled), "Definida en %s", sig->source_file.c_str());
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





    DrawHintBadge("PALABRA CLAVE", palette::U32FromHex(palette::kPrimary), palette::U32FromHex(palette::kBackground));
    ImGui::SameLine();
    ImGui::TextColored(palette::FromHex(palette::kSynKeyword), "%s", match->name.c_str());







    ImGui::Spacing();
    DrawHintSectionLabel(match->syntax.size() > 1 ? "COMO SE ESCRIBE (varias formas válidas)" : "COMO SE ESCRIBE");
    for (size_t i = 0; i < match->syntax.size(); ++i) {
        if (match->syntax.size() > 1) {
            ImGui::TextColored(palette::FromHex(palette::kTextMuted), "Opcion %zu", i + 1);
        }
        DrawHintCodeBox(match->syntax[i], palette::U32FromHex(palette::kBorder));
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
    }








    if (!match->example.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        DrawHintSectionLabel("EJEMPLO");
        DrawHintCodeBox(match->example, palette::U32FromHex(palette::kPrimary));
    }




    if (!match->doc.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        DrawHintSectionLabel("QUE HACE");
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





































bool DrawDotCompletionPopup(EditorTab& tab, const ImVec2& editor_screen_min) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string before = TextBeforeCursor(tab, pos);
    if (before.empty() || before.back() != '.') return false;

    MemberAccessContext ctx;
    std::vector<ClassMember> members;
    if (!ResolveVisibleMembers(tab, pos.line, before, ctx, members)) return false;

    const float line_height = tab.editor.GetLineHeight();
    const ImVec2 caret_pos = EstimateCaretScreenPos(tab, pos, editor_screen_min);
    ImGui::SetNextWindowPos(ImVec2(caret_pos.x, caret_pos.y + line_height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                         ImGuiWindowFlags_AlwaysAutoResize;





    const std::string window_id = "##dot_completion_popup_" + std::to_string(tab.id);
    if (ImGui::Begin(window_id.c_str(), nullptr, kFlags)) {
        for (const auto& member : members) {
            ImGui::PushID(member.name.c_str());










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

            if (ImGui::Selectable(MemberSuggestionLabel(member).c_str())) {







                std::string insert_text = member.name;
                int caret_offset = static_cast<int>(insert_text.size());
                if (member.is_method) {
                    insert_text += "()";




                    bool has_params = member.signature && !member.signature->params.empty();
                    caret_offset = static_cast<int>(insert_text.size()) - (has_params ? 1 : 0);
                }
                tab.editor.ReplaceSectionText(pos.line, pos.column, pos.line, pos.column, insert_text);
                tab.editor.SetCursor(pos.line, pos.column + caret_offset);
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
    return true;
}






void InitTab(EditorTab& tab) {
    tab.editor.SetLanguage(languages::AvaLang());
    TextEditor::Palette palette = TextEditor::GetDarkPalette();












    palette[static_cast<size_t>(TextEditor::Color::keyword)] = IM_COL32(217, 122, 61, 255);
    palette[static_cast<size_t>(TextEditor::Color::declaration)] = IM_COL32(217, 122, 61, 255);
    palette[static_cast<size_t>(TextEditor::Color::comment)] = IM_COL32(106, 153, 78, 255);
    palette[static_cast<size_t>(TextEditor::Color::docComment)] = IM_COL32(77, 182, 172, 255);
    palette[static_cast<size_t>(TextEditor::Color::docParamTag)] = IM_COL32(199, 146, 234, 255);
    palette[static_cast<size_t>(TextEditor::Color::string)] = IM_COL32(212, 163, 115, 255);
    palette[static_cast<size_t>(TextEditor::Color::interpolation)] = IM_COL32(224, 100, 240, 255);
    palette[static_cast<size_t>(TextEditor::Color::knownIdentifier)] = IM_COL32(224, 200, 132, 255);
    palette[static_cast<size_t>(TextEditor::Color::punctuation)] = IM_COL32(200, 186, 171, 255);




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
        RebuildIndexAndTrie(tab);
    }, 0);

    tab.autocomplete_config.callback = [&tab](TextEditor::AutoCompleteState& ac_state) {
        if (PopulateMemberSuggestions(tab, ac_state)) return;
        tab.autocomplete_trie.findSuggestions(ac_state.suggestions, ac_state.searchTerm);
    };
    tab.editor.SetAutoCompleteConfig(&tab.autocomplete_config);

    RebuildIndexAndTrie(tab);
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
    if (is_welcome) return "Welcome";
    return file_path.empty() ? "Untitled" : BaseNameOf(file_path);
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
    InitTab(*tab);

    if (!path.empty()) {
        std::ifstream file(path, std::ios::binary);
        if (file) {
            std::ostringstream ss;
            ss << file.rdbuf();
            tab->file_path = path;
            tab->SetText(ss.str());
            tab->dirty = false;




            RebuildIndexAndTrie(*tab);
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

void SaveTab(EditorTab& tab) {
    if (tab.file_path.empty()) return;

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

void ToggleTabViewMode(EditorTab& tab) {
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
        RebuildIndexAndTrie(tab);
        tab.avaui_load_error.clear();
        tab.view_mode = TabViewMode::Code;
        return;
    }





    design::DesignDocument parsed_doc;
    std::string parse_error;
    if (design::ParseAvauiText(tab.GetText(), parsed_doc, parse_error)) {
        tab.design = std::move(parsed_doc);
        tab.design.selected_node_id.clear();
        tab.avaui_load_error.clear();
        tab.view_mode = TabViewMode::Design;
    } else {



        tab.avaui_load_error = parse_error;
    }
}

namespace {





int FindLineOf(const std::string& text, const std::string& needle) {
    const size_t pos = text.find(needle);
    if (pos == std::string::npos) return -1;
    return static_cast<int>(std::count(text.begin(), text.begin() + static_cast<long>(pos), '\n'));
}














void JumpToCodeBehindHandler(EditorTab& tab, const std::string& handler_name) {
    if (tab.view_mode == TabViewMode::Design) {
        ToggleTabViewMode(tab);
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







        if (!tab->is_welcome && tab->dirty) SaveTab(*tab);
    }
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
        const char* subtitle = "AvaLang editor & runtime";
        const ImVec2 size = ImGui::CalcTextSize(subtitle);
        ImGui::SetCursorPosX(center_x - size.x * 0.5f);
        ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", subtitle);
    }

    ImGui::Dummy(ImVec2(0.0f, 28.0f));

    const float button_w = 180.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
    ImGui::SetCursorPosX(center_x - (button_w + 90.0f) * 0.5f);
    if (ImGui::Button("New File", ImVec2(button_w, 0.0f))) {
        state.new_tab_requested = true;
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Ctrl+N");

    ImGui::SetCursorPosX(center_x - (button_w + 90.0f) * 0.5f);
    if (ImGui::Button("Open File...", ImVec2(button_w, 0.0f))) {
        state.open_requested = true;
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Ctrl+O");





    ImGui::SetCursorPosX(center_x - (button_w + 90.0f) * 0.5f);
    if (ImGui::Button("Open Folder...", ImVec2(button_w, 0.0f))) {
        state.open_folder_requested = true;
    }
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0.0f, 24.0f));
    {
        const char* hint = "Or double-click a file in Explorer to start editing.";
        const ImVec2 size = ImGui::CalcTextSize(hint);
        ImGui::SetCursorPosX(center_x - size.x * 0.5f);
        ImGui::TextDisabled("%s", hint);
    }
}

}

void SaveActiveTab(EditorState& state) {
    if (EditorTab* tab = state.Active()) SaveTab(*tab);
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






    std::string tooltip = column > 0 ? message : ("Line " + std::to_string(line) + ": " + message);





    tab.editor.AddMarker(line - 1, error_color, error_color, tooltip, tooltip);




    tab.editor.SetCursor(line - 1, column > 0 ? column - 1 : 0);
    tab.editor.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle);
}

void ClearErrorHighlights(EditorState& state) {
    for (auto& tab : state.tabs) {
        if (tab) tab->editor.ClearMarkers();
    }
}

void DrawEditorPanel(EditorState& state) {
    state.designer_selection.reset();

    ImGui::Begin("Code Editor");

    if (state.tabs.empty()) {
        ImGui::TextDisabled("No file open. Use File > Open or double-click a file in Explorer.");
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
            if (tab.dirty) label += " *";
            char id_buf[320];
            std::snprintf(id_buf, sizeof(id_buf), "%s###tab%d", label.c_str(), tab.id);

            bool tab_open = true;






            const ImGuiTabItemFlags item_flags =
                (tab.id == state.focus_tab_id) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            const bool selected = ImGui::BeginTabItem(id_buf, &tab_open, item_flags);
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
                if (tab.is_welcome) {
                    DrawWelcomeTab(state);
                } else if (tab.is_avaui && tab.view_mode == TabViewMode::Design) {
                    if (!tab.avaui_load_error.empty()) {
                        ImGui::TextColored(palette::FromHex(palette::kWarning), "No se pudo leer %s: %s",
                                            tab.DisplayName().c_str(), tab.avaui_load_error.c_str());
                        ImGui::TextDisabled("Mostrando una página en blanco -- guardar sobrescribe el archivo original.");
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
                        JumpToCodeBehindHandler(tab, generated_handler);
                    }
                } else {










                    if (tab.is_avaui && !tab.avaui_load_error.empty()) {
                        ImGui::TextColored(palette::FromHex(palette::kWarning), "No se pudo interpretar %s: %s",
                                            tab.DisplayName().c_str(), tab.avaui_load_error.c_str());
                        ImGui::TextDisabled("Arreglá la sintaxis y probá F7 de nuevo para volver a Design.");
                        ImGui::Separator();
                    }
                    ImVec2 avail = ImGui::GetContentRegionAvail();






                    const ImVec2 editor_min = ImGui::GetCursorScreenPos();
                    const ImVec2 editor_max = ImVec2(editor_min.x + avail.x, editor_min.y + avail.y);
                    tab.editor.Render("##editor", avail, false);





                    if (ImGui::IsMouseHoveringRect(editor_min, editor_max)) {
                        if (!DrawParameterHint(tab)) {
                            if (!DrawDotCompletionPopup(tab, editor_min)) DrawKeywordHint(tab);
                        }
                    }
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


    if (state.pending_close_index >= 0) {
        ImGui::OpenPopup("Unsaved Changes##EditorCloseConfirm");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f));
    if (ImGui::BeginPopupModal("Unsaved Changes##EditorCloseConfirm", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        const int idx = state.pending_close_index;
        if (idx < 0 || idx >= static_cast<int>(state.tabs.size())) {
            state.pending_close_index = -1;
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextWrapped("Do you want to save the changes you made to %s?",
                                state.tabs[idx]->DisplayName().c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("Your changes will be lost if you don't save them.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float button_w = 100.0f;
            const bool is_untitled = state.tabs[idx]->file_path.empty();
            if (ImGui::Button("Save", ImVec2(button_w, 0.0f))) {
                if (is_untitled) {





                    state.pending_close_index = -1;
                } else {
                    SaveTab(*state.tabs[idx]);
                    CloseTabNow(state, idx);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(button_w, 0.0f))) {
                CloseTabNow(state, idx);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(button_w, 0.0f))) {
                state.pending_close_index = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}