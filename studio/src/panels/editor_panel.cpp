#include "panels/editor_panel.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "branding/logo_texture.h"
#include "fonts/embedded_font.h"
#include "imgui.h"
#include "languages/avalang_language.h"
#include "palette.h"

namespace studio {

namespace {

// Directorio de un archivo ("a/b/c.ava" -> "a/b"), usado para resolver los
// imports del FunctionIndex relativos al archivo abierto. "" si el buffer
// no está guardado (sin archivo, no hay dónde resolver imports en disco).
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

// Rebuilds the fuzzy-match word list used by autocomplete: AvaLang
// keywords/literals/built-ins, every identifier currently used in the
// document, y además los nombres de función que FunctionIndex encontró
// (locales + importados) -- así el autocompletado conoce funciones
// importadas aunque su nombre nunca se haya escrito en el buffer actual.
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

// Reconstruye tanto el trie de autocompletado como el índice de funciones.
// Se llama en cada cambio del buffer y al cargar un archivo.
void RebuildIndexAndTrie(EditorTab& tab) {
    tab.function_index.Rebuild(tab.GetText(), DirOf(tab.file_path));
    RebuildAutocompleteTrie(tab);
}

// Escanea `line_before_cursor` hacia atrás buscando el '(' de la llamada
// que envuelve al cursor (respetando paréntesis anidados), y cuenta en qué
// argumento (separado por comas a profundidad 0) está parado el cursor.
// Best-effort: solo mira la línea actual, no cruza saltos de línea -- la
// librería expone el texto línea por línea (GetLineText), no un iterador
// de todo el buffer hacia atrás desde el cursor.
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
            if (start >= end) return false; // '(' sin identificador antes -> agrupación, no llamada
            out.function_name = line_before_cursor.substr(start, end - start);
            out.active_param = comma_count;
            return true;
        }
        if (c == ',' && depth == 0) ++comma_count;
    }
    return false;
}

// Tooltip tipo "signature help" -- ImGuiColorTextEdit no trae uno (ver
// AvaStudio.md), así que se dibuja a mano sobre FunctionIndex.
void DrawParameterHint(EditorTab& tab) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string line = tab.editor.GetLineText(pos.line);
    if (pos.column < 0 || static_cast<size_t>(pos.column) > line.size()) return;
    std::string before = line.substr(0, static_cast<size_t>(pos.column));

    CallContext ctx;
    if (!FindEnclosingCall(before, ctx)) return;

    const FunctionSignature* sig = tab.function_index.Find(ctx.function_name);
    if (!sig) return; // builtin sin firma indexada, o función desconocida -- sin hint

    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(sig->name.c_str());
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted("(");
    for (size_t i = 0; i < sig->params.size(); ++i) {
        if (i > 0) { ImGui::SameLine(0, 0); ImGui::TextUnformatted(", "); }
        ImGui::SameLine(0, 0);
        if (static_cast<int>(i) == ctx.active_param) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", sig->params[i].c_str());
        } else {
            ImGui::TextUnformatted(sig->params[i].c_str());
        }
    }
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(")");
    // sig->doc: for builtins it comes from BuiltinSignatures(); for user
    // functions it comes from the non-"@param" lines of a "##" doc-comment
    // block written right above the `func` (see FunctionIndex::ScanText /
    // ApplyDocBlock) -- either way it renders the same here, so writing
    // "## Descripción" above your own functions gets you the same tooltip
    // experience as built-ins.
    if (!sig->doc.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 380.0f);
        ImGui::TextColored(palette::FromHex(palette::kTextSecondary), "%s", sig->doc.c_str());
        ImGui::PopTextWrapPos();
    }
    // Per-parameter doc for whichever argument the cursor is currently on
    // -- "## @param name: ..." lines, matched by ParamBaseName() so it
    // still works with defaults/*rest. Shown highlighted (same accent as
    // the active param above) so it visually reads as "this describes
    // that one", the way IDE signature-help panels tie the two together.
    if (!sig->param_docs.empty() && ctx.active_param >= 0 &&
        static_cast<size_t>(ctx.active_param) < sig->params.size()) {
        const std::string param_name = ParamBaseName(sig->params[static_cast<size_t>(ctx.active_param)]);
        auto it = sig->param_docs.find(param_name);
        if (it != sig->param_docs.end() && !it->second.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 380.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s: %s", param_name.c_str(), it->second.c_str());
            ImGui::PopTextWrapPos();
        }
    }
    if (sig->is_builtin) {
        // Builtin: no source_file to show (it isn't user code), instead
        // show that it can be shadowed -- see FunctionSignature::overridable
        // and languages/builtin_signatures.h.
        if (sig->overridable) {
            ImGui::TextDisabled(
                "Built-in. Define your own top-level func %s(...) in this script to override it.",
                sig->name.c_str());
        }
    } else if (!sig->source_file.empty()) {
        ImGui::TextDisabled("%s", sig->source_file.c_str());
    }
    ImGui::EndTooltip();
}

// One-time setup shared by every tab, regardless of whether it starts
// empty, from a file, or with initial demo text -- language, palette,
// line numbers, autocomplete callback. Mirrors the old (single-buffer)
// InitEditorPanel, just scoped to one EditorTab instead of the whole
// panel.
void InitTab(EditorTab& tab) {
    tab.editor.SetLanguage(languages::AvaLang());
    TextEditor::Palette palette = TextEditor::GetDarkPalette();

    // Antes: string e identifier (variables) quedaban con EXACTAMENTE el
    // mismo azul (156,220,254), y knownIdentifier (funciones/builtins como
    // print) era otro azul (79,193,255) casi indistinguible a simple
    // vista -- todo el editor se leía "azul". Ahora cada categoría tiene
    // un color propio:
    // Paleta "Ava amber" (ver theme.cpp): el naranja de acento (#d97a3d) es
    // el mismo que usan los botones/tabs/bordes de foco, así que domina
    // también dentro del editor vía keywords/declarations -- pero el resto
    // de categorías se mantiene en colores distintos entre sí a propósito;
    // pintar todo de naranja destruiría el contraste que hace legible el
    // código de un vistazo (keyword vs string vs identifier vs comment).
    palette[static_cast<size_t>(TextEditor::Color::keyword)] = IM_COL32(217, 122, 61, 255);      // #d97a3d -- if/while/func/... (== acento del tema)
    palette[static_cast<size_t>(TextEditor::Color::declaration)] = IM_COL32(217, 122, 61, 255);  // #d97a3d -- true/false/nil
    palette[static_cast<size_t>(TextEditor::Color::comment)] = IM_COL32(106, 153, 78, 255);      // #6a994e -- verde, para distinguir comentarios del resto del código
    palette[static_cast<size_t>(TextEditor::Color::docComment)] = IM_COL32(77, 182, 172, 255);   // #4db6ac -- turquesa: el azul pastel anterior se confundía con el celeste de las variables (identifier); este tono no se parece a ningún otro color de la paleta (naranja, verde, ámbar, dorado, morado, celeste)
    palette[static_cast<size_t>(TextEditor::Color::docParamTag)] = IM_COL32(199, 146, 234, 255); // #c792ea -- morado, solo para el token "@param" (no el nombre ni la descripción que siguen, para que no se "propague")
    palette[static_cast<size_t>(TextEditor::Color::string)] = IM_COL32(212, 163, 115, 255);      // #d4a373 -- ámbar claro, hermano más suave del naranja de acento
    palette[static_cast<size_t>(TextEditor::Color::interpolation)] = IM_COL32(224, 100, 240, 255); // #e064f0 -- fuchsia/purple, deliberately NOT in the amber family: {expr} inside f-strings needs to pop against keyword/string/declaration, which are all warm oranges in this theme
    palette[static_cast<size_t>(TextEditor::Color::knownIdentifier)] = IM_COL32(224, 200, 132, 255); // #e0c884 -- dorado apagado -- print(), len(), etc.
    palette[static_cast<size_t>(TextEditor::Color::punctuation)] = IM_COL32(200, 186, 171, 255);   // #c8baab -- gris cálido claro, neutro
    // Color::identifier (variables comunes): celeste frío original de la
    // librería, sin tocar -- es el único color "frío" que queda adentro
    // del editor, y es intencional: da el contraste que necesita el ojo
    // para separar variables del resto de categorías, todas cálidas.
    tab.editor.SetPalette(palette);
    tab.editor.SetShowLineNumbersEnabled(true);
    tab.editor.SetTabSize(4);
    tab.editor.SetAutoIndentEnabled(true);
    tab.editor.SetShowMatchingBrackets(true);
    tab.editor.SetCompletePairedGlyphs(true);

    // Bold only for keywords/declarations (if/while/func/... and
    // true/false/nil) -- needs studio/patches/imguicolortextedit_bold_keywords.patch,
    // which adds per-glyph font selection to the library (upstream only
    // supports one font for the whole Render() call, see the patch's
    // header comment in TextEditor.h). GetCodeFont() is the JetBrains
    // Mono Bold instance loaded in main.cpp; if it's ever null (e.g. font
    // loading failed) SetBoldFont(nullptr) is a no-op and everything just
    // renders in the regular font, same as before this patch existed.
    tab.editor.SetBoldFont(GetCodeFont());
    tab.editor.SetBoldColors({TextEditor::Color::keyword, TextEditor::Color::declaration});

    // Small AvaLang scripts, not megabyte source files -- rebuilding the
    // autocomplete word list (y el FunctionIndex) on every keystroke is
    // cheap, so no delay. Si esto llegara a pesar con imports grandes,
    // agregar debounce acá es el primer lugar a tocar.
    tab.editor.SetChangeCallback([&tab] {
        tab.dirty = true;
        tab.editor.ClearMarkers();
        RebuildIndexAndTrie(tab);
    }, 0);

    tab.autocomplete_config.callback = [&tab](TextEditor::AutoCompleteState& ac_state) {
        tab.autocomplete_trie.findSuggestions(ac_state.suggestions, ac_state.searchTerm);
    };
    tab.editor.SetAutoCompleteConfig(&tab.autocomplete_config);

    RebuildIndexAndTrie(tab);
}

// Finds an already-open tab for `path`, if any -- so opening a file that's
// already open focuses it instead of creating a duplicate tab, same as
// VSCode.
int FindTabForPath(EditorState& state, const std::string& path) {
    if (path.empty()) return -1;
    for (int i = 0; i < static_cast<int>(state.tabs.size()); ++i) {
        if (state.tabs[i]->file_path == path) return i;
    }
    return -1;
}

void CloseTabNow(EditorState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.tabs.size())) return;
    state.tabs.erase(state.tabs.begin() + index);

    if (state.tabs.empty()) {
        state.active_tab = -1;
    } else if (state.active_tab >= static_cast<int>(state.tabs.size())) {
        state.active_tab = static_cast<int>(state.tabs.size()) - 1;
    } else if (index <= state.active_tab && state.active_tab > 0) {
        // Keep focus on the same logical neighbor when closing a tab
        // before or at the active one (VSCode falls back to the tab that
        // slid into the closed one's place).
        --state.active_tab;
    }

    if (state.pending_close_index == index) {
        state.pending_close_index = -1;
    } else if (state.pending_close_index > index) {
        --state.pending_close_index;
    }
}

} // namespace

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

void InitEditorPanel(EditorState& /*state*/) {
    // Nothing to set up at the panel level yet -- each tab initializes
    // itself in InitTab() when it's created. Kept as a function (rather
    // than removed) so main.cpp's setup sequence reads the same as before
    // and so future panel-wide state has an obvious home.
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
            // SetText() es carga programática, no dispara SetChangeCallback --
            // hay que reconstruir el índice/trie a mano, y recién ahora que
            // file_path está seteado, para que la resolución de imports
            // tenga directorio base.
            RebuildIndexAndTrie(*tab);
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
    // No InitTab() here -- the Welcome tab never touches the TextEditor
    // widget (DrawEditorPanel renders a static screen for it instead), so
    // there's no language/palette/autocomplete to set up.
    state.tabs.push_back(std::move(tab));
    state.active_tab = static_cast<int>(state.tabs.size()) - 1;
    return *state.tabs.back();
}

namespace {

void SaveTab(EditorTab& tab) {
    if (tab.file_path.empty()) return;
    std::ofstream file(tab.file_path, std::ios::binary);
    if (!file) return;
    file << tab.GetText();
    tab.dirty = false;
}

// Startup landing page for the Welcome tab -- centered brand mark plus
// the two actions someone opening Ava Studio for the first time actually
// needs (start a new script, or open an existing one), instead of
// dropping them straight into a pre-filled demo script they didn't write.
void DrawWelcomeTab(EditorState& state) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(0.0f, avail.y * 0.16f));
    const float center_x = ImGui::GetCursorPosX() + avail.x * 0.5f;

    // Real Ava Studio logo (avastudio.png, baked into the exe -- see
    // src/branding/logo_texture.h), not a flat orange dot placeholder.
    // Bigger than the titlebar's copy since this is the hero mark on the
    // landing page, not a small caption icon; still drawn 1:1 square, no
    // stretching.
    constexpr float kIconSize = 56.0f;
    {
        const ImVec2 icon_pos = ImGui::GetCursorScreenPos();
        const unsigned int logo_texture = branding::GetLogoTextureId();
        if (logo_texture != 0) {
            ImGui::SetCursorScreenPos(ImVec2(center_x - kIconSize * 0.5f, icon_pos.y));
            ImGui::Image(static_cast<ImTextureID>(logo_texture), ImVec2(kIconSize, kIconSize));
        } else {
            // Fallback in the unlikely event the embedded logo failed to
            // decode/upload -- keeps the Welcome tab from showing a hole.
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

    // "Open Folder..." switches the Explorer panel's root to a different
    // directory -- AvaLang's equivalent of VSCode's "open a project" --
    // rather than opening a single buffer, so it's kept separate from
    // "Open File..." above instead of folded into the same dialog.
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

} // namespace

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
}

void HighlightError(EditorState& state, const std::string& file_path, int line, int column,
                     const std::string& message) {
    if (line <= 0 || file_path.empty()) return;
    int index = FindTabForPath(state, file_path);
    if (index < 0) return;
    EditorTab& tab = *state.tabs[index];

    // One error at a time -- a stale marker from a previous run (or one
    // on a different line) would be confusing sitting next to the new one.
    tab.editor.ClearMarkers();

    // Alpha < 1 so the highlighted line's own text/syntax coloring stays
    // readable underneath the red wash, instead of a solid opaque bar.
    const ImU32 error_color = palette::U32FromHex(palette::kError, 0.35f);

    // Compile errors' `message` already embeds "error at file:line:col: "
    // (see frontend_antlr.cpp's formatError), but runtime errors' don't
    // -- their text is just the raw C++ exception message, with no
    // position of its own (column is always 0 for those, see
    // core/src/vm/vm.cpp) -- so prefix the line number here for parity.
    std::string tooltip = column > 0 ? message : ("Línea " + std::to_string(line) + ": " + message);

    // AddMarker's line is zero-based (see TextEditor.h's "access markers
    // (line numbers are zero-based)" comment); everything else in this
    // codebase (SourceError, AvaError, RunResult) is 1-based, so -1 here
    // and nowhere else.
    tab.editor.AddMarker(line - 1, error_color, error_color, tooltip, tooltip);

    // Jump the caret to the exact spot (column falls back to the start
    // of the line when unknown) and scroll it into view even if the
    // error is off-screen.
    tab.editor.SetCursor(line - 1, column > 0 ? column - 1 : 0);
    tab.editor.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle);
}

void ClearErrorHighlights(EditorState& state) {
    for (auto& tab : state.tabs) {
        if (tab) tab->editor.ClearMarkers();
    }
}

void DrawEditorPanel(EditorState& state) {
    ImGui::Begin("Code Editor");

    if (state.tabs.empty()) {
        ImGui::TextDisabled("No file open. Use File > Open or double-click a file in Explorer.");
        ImGui::End();
        return;
    }

    // --- VSCode-style tab strip -----------------------------------------
    // Reorderable (drag to reorder, matching VSCode), scrollable once too
    // many tabs fit, with a dropdown listing every open tab (the small
    // arrow VSCode shows on the right of a crowded tab bar).
    const ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable |
                                            ImGuiTabBarFlags_AutoSelectNewTabs |
                                            ImGuiTabBarFlags_TabListPopupButton |
                                            ImGuiTabBarFlags_FittingPolicyScroll;

    int tab_to_close = -1;

    // Active tab gets a subtle orange (primary) tint plus a solid accent
    // line along its top edge, drawn manually below -- otherwise the
    // active tab only differs from the others by a barely-there
    // background shade, easy to lose track of once several tabs are open.
    ImGui::PushStyleColor(ImGuiCol_TabActive, palette::FromHex(palette::kPrimary, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, palette::FromHex(palette::kPrimary, 0.12f));

    if (ImGui::BeginTabBar("##EditorTabs", tab_bar_flags)) {
        for (int i = 0; i < static_cast<int>(state.tabs.size()); ++i) {
            EditorTab& tab = *state.tabs[i];

            // "##tab%d" (hidden after ##) keeps this tab's ImGui identity
            // stable across renames (Save As) and reordering -- only the
            // visible part changes when the file name or dirty-dot does.
            std::string label = tab.DisplayName();
            if (tab.dirty) label += " *";
            char id_buf[320];
            std::snprintf(id_buf, sizeof(id_buf), "%s###tab%d", label.c_str(), tab.id);

            bool tab_open = true;
            // Force ImGui to switch to this tab this frame if something
            // (e.g. clicking an already-open file in Explorer, or
            // OpenFileInTab in general) asked for it -- BeginTabItem's
            // return value alone only reflects the user's own last click
            // inside the tab bar, it doesn't let outside code drive
            // selection without this flag.
            const ImGuiTabItemFlags item_flags =
                (tab.id == state.focus_tab_id) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            const bool selected = ImGui::BeginTabItem(id_buf, &tab_open, item_flags);
            if (selected) {
                // Solid accent line on top of the active tab -- the same
                // orange used for buttons/focus borders elsewhere, so the
                // active tab reads as clearly "selected" at a glance, the
                // same role VSCode's colored top border plays.
                const ImVec2 p0 = ImGui::GetItemRectMin();
                const ImVec2 p1 = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p0, ImVec2(p1.x, p0.y + 2.0f), palette::U32FromHex(palette::kPrimary));
                state.active_tab = i;
            }
            // Middle-click a tab to close it, same as browser/VSCode tabs.
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                tab_open = false;
            }
            if (selected) {
                if (tab.is_welcome) {
                    DrawWelcomeTab(state);
                } else {
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    // Captured before Render() (rather than relying on
                    // GetItemRectMin/Max afterwards) so this doesn't depend
                    // on what ImGui item TextEditor::Render() happens to
                    // leave behind internally -- the editor always occupies
                    // exactly the rect from its starting screen pos to
                    // that pos + avail, regardless of library internals.
                    const ImVec2 editor_min = ImGui::GetCursorScreenPos();
                    const ImVec2 editor_max = ImVec2(editor_min.x + avail.x, editor_min.y + avail.y);
                    tab.editor.Render("##editor", avail, false);
                    // Parameter hint follows the mouse, not just the caret:
                    // only draw it while the pointer is actually over the
                    // editor, so it disappears as soon as it leaves (e.g.
                    // moving to inspect another panel) instead of lingering
                    // on screen tied purely to where the cursor sits in text.
                    if (ImGui::IsMouseHoveringRect(editor_min, editor_max)) {
                        DrawParameterHint(tab);
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

    // --- Unsaved-changes confirmation (Save / Don't Save / Cancel) -----
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
                    // Untitled buffer: nothing to save into without a
                    // Save As dialog, which lives in main.cpp (needs the
                    // GLFW window handle). Leave the tab open and let the
                    // user Save As from the File menu instead of silently
                    // discarding their work.
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

} // namespace studio