#include "panels/editor_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "branding/logo_texture.h"
#include "design/avaui_text.h"
#include "fonts/embedded_font.h"
#include "imgui.h"
#include "languages/avalang_language.h"
#include "languages/keyword_docs.h"
#include "palette.h"
#include "panels/designer_canvas.h"

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

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Extrae el identificador que termina justo en el cursor -- la palabra que
// se está escribiendo en este momento, hasta donde está el caret. Devuelve
// "" si justo antes del cursor no hay un carácter de identificador (recién
// se escribió un espacio, un paréntesis, etc.). `line_before_cursor` es el
// texto de la línea actual desde el inicio hasta la columna del cursor.
std::string WordEndingAtCursor(const std::string& line_before_cursor) {
    size_t end = line_before_cursor.size();
    size_t start = end;
    while (start > 0 && IsIdentChar(line_before_cursor[start - 1])) --start;
    return line_before_cursor.substr(start, end - start);
}

// --- Tooltip visual helpers ------------------------------------------
//
// Pensados para que alguien que NO programa pueda entender el tooltip a
// simple vista, sin tener que leer con atención: el "tipo" de cosa que
// se está mirando (palabra clave vs función) se ve antes de leer una
// sola palabra (una etiqueta de color), y el código (patrón abstracto o
// ejemplo concreto) se ve visualmente distinto del texto explicativo
// -- una caja con borde, no un párrafo más. Esto reemplaza los
// ImGui::TextUnformatted/TextColored sueltos que había antes, que
// mezclaban sintaxis y explicación con el mismo look plano.

// Ancho de contenido fijo para todos los tooltips del editor -- que la
// caja de sintaxis, la de ejemplo y el texto de explicación midan todos
// lo mismo hace que el tooltip se sienta como una sola tarjeta prolija
// en vez de bloques sueltos de distinto ancho.
constexpr float kHintContentWidth = 380.0f;

// Etiqueta tipo "pill" (ej. "PALABRA CLAVE", "FUNCIÓN", "BUILT-IN") --
// lo primero que se lee, antes que el nombre. Da contexto instantáneo
// sobre qué tipo de ayuda es esta, sin necesitar vocabulario técnico.
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

// Encabezado de sección discreto (ej. "Cómo se escribe", "Ejemplo",
// "Qué hace") -- en mayúsculas chicas y color apagado, para que el ojo
// lo salte rápido y vaya directo al contenido, pero siga estando ahí
// como referencia para alguien que recién está aprendiendo la interfaz
// del editor.
void DrawHintSectionLabel(const char* text) {
    ImGui::TextColored(palette::FromHex(palette::kTextMuted), "%s", text);
}

// Bloque de código dentro de una caja con borde y fondo propio -- así
// se distingue de un vistazo del texto explicativo de abajo, que es
// prosa normal. `border_color` diferencia una caja de "patrón" (borde
// neutro, con nombres de relleno tipo `condition`) de una de "ejemplo"
// (borde naranja de marca, código real y copiable tal cual). El ancho
// es siempre kHintContentWidth para que todo el tooltip quede alineado.
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

    // TextUnformatted above only registers as wide as the text actually
    // ended up being (for a short one-liner like "break", far narrower
    // than the box we just drew) -- without this, a tooltip window that
    // auto-sizes to its content could end up narrower than the box,
    // clipping its right/bottom edge. Re-placing a full-box-sized Dummy
    // at the box's origin makes ImGui's layout account for the whole
    // box regardless of how short the text inside it was, and leaves the
    // cursor positioned right below it for whatever comes next.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(box_size);
}

// Tooltip tipo "signature help" -- ImGuiColorTextEdit no trae uno (ver
// AvaStudio.md), así que se dibuja a mano sobre FunctionIndex. Devuelve
// true si dibujó algo, para que el caller sepa que ya hay un tooltip en
// pantalla y no intente superponer otro (ver DrawKeywordHint más abajo).
bool DrawParameterHint(EditorTab& tab) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string line = tab.editor.GetLineText(pos.line);
    if (pos.column < 0 || static_cast<size_t>(pos.column) > line.size()) return false;
    std::string before = line.substr(0, static_cast<size_t>(pos.column));

    CallContext ctx;
    if (!FindEnclosingCall(before, ctx)) return false;

    const FunctionSignature* sig = tab.function_index.Find(ctx.function_name);
    if (!sig) return false; // builtin sin firma indexada, o función desconocida -- sin hint

    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::BeginTooltip();

    // Badge primero: "FUNCIÓN" para algo que el usuario escribió (o
    // importó), "BUILT-IN" para algo que ya viene con AvaLang -- esta
    // distinción importa para alguien nuevo (¿esto lo escribí yo o
    // viene del lenguaje?) y se ve antes de leer una sola palabra.
    if (sig->is_builtin) {
        DrawHintBadge("BUILT-IN", palette::U32FromHex(palette::kBorder), palette::U32FromHex(palette::kTextSecondary));
    } else {
        DrawHintBadge("FUNCION", palette::U32FromHex(palette::kPrimary), palette::U32FromHex(palette::kBackground));
    }

    // Firma completa en una sola línea de "código", con el parámetro
    // activo resaltado en el color de marca -- mismo tratamiento visual
    // que el resto del editor le da a las keywords, así que se lee como
    // "esto es código real", no como texto de ayuda genérico.
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

    // sig->doc: for builtins it comes from BuiltinSignatures(); for user
    // functions it comes from the non-"@param" lines of a "##" doc-comment
    // block written right above the `func` (see FunctionIndex::ScanText /
    // ApplyDocBlock) -- either way it renders the same here, so writing
    // "## Descripción" above your own functions gets you the same tooltip
    // experience as built-ins.
    if (!sig->doc.empty()) {
        ImGui::Spacing();
        DrawHintSectionLabel("QUE HACE");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kHintContentWidth);
        ImGui::TextColored(palette::FromHex(palette::kTextPrimary), "%s", sig->doc.c_str());
        ImGui::PopTextWrapPos();
    }

    // Per-parameter doc for whichever argument the cursor is currently on
    // -- "## @param name: ..." lines, matched by ParamBaseName() so it
    // still works with defaults/*rest. Shown in its own small highlighted
    // box tied visually to the active parameter above (same accent
    // color), the way IDE signature-help panels tie the two together --
    // instead of a plain highlighted line, this reads clearly as "this
    // box describes that specific argument you're typing right now".
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
        // Builtin: no source_file to show (it isn't user code), instead
        // show that it can be shadowed -- see FunctionSignature::overridable
        // and languages/builtin_signatures.h.
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

// Tooltip de sintaxis para keywords (if/while/for/try/...), pensado para
// programadores nuevos: se dibuja mientras se está escribiendo la palabra,
// incluso a medio terminar -- "fo" ya alcanza para mostrar el tooltip de
// "for" si es la única keyword que empieza así. Igual que
// DrawParameterHint, es dibujado a mano porque el popup de autocompletado
// de ImGuiColorTextEdit solo acepta una lista plana de strings (ver
// AutoCompleteConfig::callback en TextEditor.h) y no tiene un panel de
// documentación por sugerencia como los IDEs con language server.
bool DrawKeywordHint(EditorTab& tab) {
    TextEditor::CursorPosition pos = tab.editor.GetCursorPosition(0);
    std::string line = tab.editor.GetLineText(pos.line);
    if (pos.column < 0 || static_cast<size_t>(pos.column) > line.size()) return false;

    std::string word = WordEndingAtCursor(line.substr(0, static_cast<size_t>(pos.column)));
    if (word.empty() || !IsIdentStart(word[0])) return false;

    const auto& docs = KeywordDocs();

    // Coincidencia exacta: ya terminó de escribir la keyword completa.
    auto exact = docs.find(word);
    const KeywordDoc* match = exact != docs.end() ? &exact->second : nullptr;

    // Si no, buscar por prefijo (todavía escribiendo, ej. "fo"): solo se
    // muestra el tooltip si el prefijo ya identifica una única keyword --
    // con dos o más candidatas ("i" -> if/in/import) no hay nada útil que
    // mostrar todavía, así que se espera a que el usuario siga escribiendo.
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

    // Badge + nombre: lo primero que se ve es "PALABRA CLAVE", antes de
    // leer nada más -- así alguien que nunca programó entiende de
    // entrada que esto es parte del vocabulario fijo de AvaLang, no algo
    // que él mismo tenga que definir (a diferencia de una función).
    DrawHintBadge("PALABRA CLAVE", palette::U32FromHex(palette::kPrimary), palette::U32FromHex(palette::kBackground));
    ImGui::SameLine();
    ImGui::TextColored(palette::FromHex(palette::kSynKeyword), "%s", match->name.c_str());

    // "Cómo se escribe": el patrón abstracto (con nombres de relleno
    // como `condition`), en su propia caja con borde neutro -- se lee
    // como plantilla, no como código para copiar tal cual. Si el
    // lenguaje acepta más de una forma (ej. paréntesis opcionales en
    // while), cada una se numera para que quede claro que son
    // alternativas y no dos pasos seguidos.
    ImGui::Spacing();
    DrawHintSectionLabel(match->syntax.size() > 1 ? "COMO SE ESCRIBE (varias formas válidas)" : "COMO SE ESCRIBE");
    for (size_t i = 0; i < match->syntax.size(); ++i) {
        if (match->syntax.size() > 1) {
            ImGui::TextColored(palette::FromHex(palette::kTextMuted), "Opcion %zu", i + 1);
        }
        DrawHintCodeBox(match->syntax[i], palette::U32FromHex(palette::kBorder));
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
    }

    // "Ejemplo": código real y copiable, sin nombres de relleno -- para
    // alguien que no programa, ver "if age >= 18 then ..." suele aclarar
    // mucho más rápido que el patrón abstracto de arriba. Caja con borde
    // naranja de marca para diferenciarla claramente de la de sintaxis
    // (esta SÍ se puede pegar tal cual en el editor). Ausente para
    // keywords que solo tienen sentido junto a otra (then/in/as/catch),
    // ya cubiertas por el ejemplo de la keyword principal.
    if (!match->example.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        DrawHintSectionLabel("EJEMPLO");
        DrawHintCodeBox(match->example, palette::U32FromHex(palette::kPrimary));
    }

    // "Qué hace": la explicación en prosa normal, al final -- para quien
    // ya entendió con el ejemplo de arriba, esto es opcional; para quien
    // necesita más contexto, está ahí.
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

// True if `child` is `parent` itself or lives somewhere underneath it.
// Compared path-component-by-path-component (via std::filesystem) rather
// than as raw strings, so it's not tripped up by "/" vs "\\" or by a
// trailing separator on one side but not the other. When it returns true
// and `remainder` is non-null, `remainder` is set to the part of `child`
// below `parent` (empty if child == parent) -- used to re-root a tab's
// path after its containing folder was renamed or moved.
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

        // .avaui detection -- see 08_DESIGNER_VIEW_PLAN.md section 4.
        // Checked by extension regardless of whether the file above
        // could actually be opened/read (a brand-new empty .avaui
        // created from Explorer still needs to land in Design view,
        // not show up as a blank Code tab). Content still lives in
        // `tab->editor` too (loaded above, or empty if the file
        // couldn't be read) so a future F7 toggle to Code view has
        // something real to show.
        if (std::filesystem::path(path).extension().string() == ".avaui") {
            tab->is_avaui = true;
            tab->view_mode = TabViewMode::Design;

            std::string load_error;
            if (!design::LoadAvauiFile(path, tab->design, load_error)) {
                // ParseAvauiText is forgiving by design (see
                // design/avaui_text.h) so this branch mostly exists for
                // the "file couldn't even be read" case now, not a
                // parse failure -- either way, opening still succeeds
                // with a blank page instead of refusing the tab, same
                // as VS6 opening a corrupt .frm. Only surface the
                // message if there was actually something in the file
                // to fail on; a 0-byte file failing to parse isn't an
                // error worth showing the user.
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
    // No InitTab() here -- the Welcome tab never touches the TextEditor
    // widget (DrawEditorPanel renders a static screen for it instead), so
    // there's no language/palette/autocomplete to set up.
    state.tabs.push_back(std::move(tab));
    state.active_tab = static_cast<int>(state.tabs.size()) - 1;
    return *state.tabs.back();
}

void SaveTab(EditorTab& tab) {
    if (tab.file_path.empty()) return;

    if (tab.is_avaui) {
        if (tab.view_mode == TabViewMode::Code) {
            // Code view IS the TextEditor buffer here (see
            // DrawEditorPanel's dispatch below) -- save it verbatim,
            // same as any other tab, so a hand-edit to `methods` (or a
            // mid-edit syntax error) isn't silently overwritten by
            // whatever `tab.design` held from before the last F7
            // toggle. Best-effort fold back into tab.design too, so a
            // later F7 to Design view reflects what was just saved
            // instead of stale state -- same forgiving policy as
            // ToggleTabViewMode: on a parse error, leave the last valid
            // tab.design untouched and still write the raw text to
            // disk (a syntax error is allowed to reach disk, exactly
            // like saving invalid code in a plain .ava file today).
            design::DesignNode parsed_root;
            std::string parsed_code_behind;
            std::vector<PropertyRow> parsed_initial_state;
            std::vector<std::string> parsed_imports;
            std::string parse_error;
            if (design::ParseAvauiText(tab.GetText(), parsed_root, parsed_code_behind, parsed_initial_state,
                                        parsed_imports, parse_error)) {
                tab.design.root = std::move(parsed_root);
                tab.design.code_behind = std::move(parsed_code_behind);
                tab.design.initial_state = std::move(parsed_initial_state);
                tab.design.imports = std::move(parsed_imports);
                tab.avaui_load_error.clear();
            }
            std::ofstream file(tab.file_path, std::ios::binary);
            if (!file) return;
            file << tab.GetText();
            tab.design.dirty = false;
            tab.dirty = false;
            return;
        }

        // Design view: the Design canvas edits `tab.design` directly --
        // it never touches `tab.editor`, so tab.design is the source of
        // truth here.
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
        // Design -> Code: always succeeds -- WriteAvauiText can
        // serialize any in-memory tab.design, there's no "invalid
        // design tree" state to fail on. Programmatic SetText(), so
        // rebuild the function index/autocomplete trie by hand exactly
        // like OpenFileInTab does after its own SetText().
        tab.SetText(design::WriteAvauiText(tab.design.root, tab.design.code_behind, tab.design.initial_state,
                                            tab.design.imports));
        RebuildIndexAndTrie(tab);
        tab.avaui_load_error.clear();
        tab.view_mode = TabViewMode::Code;
        return;
    }

    // Code -> Design: re-parse the buffer so hand-edits made in Code
    // view aren't lost. Selection is cleared either way -- ParseAvauiText
    // assigns fresh node_uids (same as LoadAvauiFile), so the previous
    // selected_uid can't possibly match anything in the reparsed tree.
    design::DesignNode parsed_root;
    std::string parsed_code_behind;
    std::vector<PropertyRow> parsed_initial_state;
    std::vector<std::string> parsed_imports;
    std::string parse_error;
    if (design::ParseAvauiText(tab.GetText(), parsed_root, parsed_code_behind, parsed_initial_state,
                                parsed_imports, parse_error)) {
        tab.design.root = std::move(parsed_root);
        tab.design.code_behind = std::move(parsed_code_behind);
        tab.design.initial_state = std::move(parsed_initial_state);
        tab.design.imports = std::move(parsed_imports);
        tab.design.selected_uid.clear();
        tab.avaui_load_error.clear();
        tab.view_mode = TabViewMode::Design;
    } else {
        // Abort the toggle rather than discard either the last valid
        // Design tree or the user's unparseable edit -- see the banner
        // this drives in DrawEditorPanel's Code-view dispatch below.
        tab.avaui_load_error = parse_error;
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
        // Untitled tabs (empty file_path) are skipped here on purpose --
        // SaveTab() itself is a no-op for them (nothing to write into
        // without a Save As dialog, which needs the GLFW window handle
        // main.cpp has and this function doesn't). The exit-confirmation
        // flow in main.cpp handles those separately, one Save As dialog
        // at a time, the same way it already does for Ctrl+S on an
        // untitled buffer.
        if (!tab->is_welcome && tab->dirty) SaveTab(*tab);
    }
}

namespace {

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
    // Exact match: `path` was itself an open file.
    int index = FindTabForPath(state, path);
    if (index >= 0) {
        CloseTabNow(state, index);
    }
    // Prefix match: `path` was a folder -- every open tab whose file
    // lived somewhere inside it just lost its backing file too, since
    // deleting a folder takes its whole contents with it. Walk backwards
    // so each CloseTabNow's index shift doesn't skip the next candidate.
    const std::filesystem::path dir(path);
    for (int i = static_cast<int>(state.tabs.size()) - 1; i >= 0; --i) {
        if (PathContains(dir, std::filesystem::path(state.tabs[i]->file_path))) {
            CloseTabNow(state, i);
        }
    }
}

void RenameTabPath(EditorState& state, const std::string& old_path, const std::string& new_path) {
    if (old_path.empty()) return;
    // Exact match: renaming/moving a single open file.
    int index = FindTabForPath(state, old_path);
    if (index >= 0) {
        state.tabs[index]->file_path = new_path;
        return;
    }
    // Prefix match: renaming/moving a folder -- every open tab whose file
    // lived inside it keeps tracking the same file by swapping the old
    // folder prefix for the new one, instead of pointing at a path that
    // no longer exists on disk.
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
    std::string tooltip = column > 0 ? message : ("Line " + std::to_string(line) + ": " + message);

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
    state.designer_selection.reset();

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
                } else if (tab.is_avaui && tab.view_mode == TabViewMode::Design) {
                    if (!tab.avaui_load_error.empty()) {
                        ImGui::TextColored(palette::FromHex(palette::kWarning), "No se pudo leer %s: %s",
                                            tab.DisplayName().c_str(), tab.avaui_load_error.c_str());
                        ImGui::TextDisabled("Mostrando una página en blanco -- guardar sobrescribe el archivo original.");
                        ImGui::Separator();
                    }
                    const ImVec2 avail = ImGui::GetContentRegionAvail();
                    if (auto selected_node = DrawDesignerCanvas(tab.design, avail, state.project_root)) {
                        state.designer_selection = std::move(selected_node);
                    }
                    if (tab.design.dirty) tab.dirty = true;
                } else {
                    // Reached for every plain .ava/.md/etc. tab, AND for
                    // a .avaui tab in Code view (view_mode == Code) --
                    // Code view for a .avaui IS this same TextEditor
                    // widget, see ToggleTabViewMode/SaveTab above. Only
                    // difference here: a .avaui with a pending
                    // avaui_load_error (F7 back to Design was blocked by
                    // a parse error, see ToggleTabViewMode) shows the
                    // same banner Design view shows for a load error, so
                    // the reason F7 isn't switching is visible right
                    // where the user is fixing it.
                    if (tab.is_avaui && !tab.avaui_load_error.empty()) {
                        ImGui::TextColored(palette::FromHex(palette::kWarning), "No se pudo interpretar %s: %s",
                                            tab.DisplayName().c_str(), tab.avaui_load_error.c_str());
                        ImGui::TextDisabled("Arreglá la sintaxis y probá F7 de nuevo para volver a Design.");
                        ImGui::Separator();
                    }
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
                        if (!DrawParameterHint(tab)) DrawKeywordHint(tab);
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