#include "languages/member_access_resolver.h"

#include <cctype>
#include <vector>

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

// Mismo set que IsBlockKeyword en class_index.cpp (duplicado a propósito:
// ese está en un namespace anónimo de otra unidad de traducción, y este
// escaneo tiene una forma distinta -- avanza hacia adelante construyendo
// una pila de bloques abiertos en vez de buscar el `end` de un bloque ya
// conocido).
bool IsBlockKeyword(const std::string& word) {
    return word == "try" || word == "if" || word == "while" || word == "for" ||
           word == "func" || word == "class";
}

// Un bloque `class`/`func`/`if`/`while`/`for`/`try` todavía abierto en el
// punto del buffer donde se cortó el escaneo. Solo los bloques `class`
// llevan nombre -- es lo único que FindEnclosingClass necesita.
struct BlockFrame {
    bool is_class = false;
    std::string class_name;
};

// Escanea `prefix` (todo el texto del buffer HASTA el cursor, ver
// ResolveMemberAccess) llevando una pila de bloques abiertos, y devuelve
// el nombre de la clase más interna que todavía esté abierta en ese punto
// -- "" si el cursor no está lexicamente dentro de ningún `class ... end`
// (código de nivel de módulo). Best-effort, mismo criterio de
// comentarios/strings que el resto de estos escáneres del lado del editor.
std::string FindEnclosingClass(const std::string& prefix) {
    std::vector<BlockFrame> stack;
    size_t i = 0;
    while (i < prefix.size()) {
        char c = prefix[i];

        if (c == '#') {
            while (i < prefix.size() && prefix[i] != '\n') ++i;
            continue;
        }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < prefix.size() && prefix[i] != quote) {
                if (prefix[i] == '\\' && i + 1 < prefix.size()) i += 2; else ++i;
            }
            if (i < prefix.size()) ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(prefix, i);

            if (word == "class") {
                size_t save = i;
                SkipInlineWhitespace(prefix, i);
                BlockFrame frame;
                frame.is_class = true;
                if (i < prefix.size() && IsIdentStart(prefix[i])) {
                    frame.class_name = ReadIdent(prefix, i);
                } else {
                    i = save;  // sin nombre (línea a medio escribir) -- igual empuja el frame
                }
                stack.push_back(std::move(frame));
                continue;
            }
            if (word == "end") {
                if (!stack.empty()) stack.pop_back();
                continue;
            }
            if (IsBlockKeyword(word)) {
                stack.push_back(BlockFrame{});
                continue;
            }
            continue;
        }

        ++i;
    }

    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if (it->is_class) return it->class_name;
    }
    return "";
}

// Detecta el patrón `identificador.parcial` justo al final de `before`
// (el texto de la línea actual hasta el cursor). `parcial` puede ser
// vacío (cursor recién después del '.'). Devuelve false si no hay '.' en
// esa posición, si no hay identificador antes del '.', o si el caracter
// anterior al identificador es OTRO '.' (encadenado `a.b.` -- fuera de
// alcance, Fase 6).
bool ExtractDotAccess(const std::string& before, std::string& identifier) {
    size_t i = before.size();
    while (i > 0 && IsIdentChar(before[i - 1])) --i;  // parcial (no hace falta guardarlo)

    if (i == 0 || before[i - 1] != '.') return false;
    size_t dot = i - 1;

    size_t j = dot;
    while (j > 0 && IsIdentChar(before[j - 1])) --j;
    if (j == dot) return false;  // '.' sin identificador antes (ej. "5." o inicio de línea)

    if (j > 0 && before[j - 1] == '.') return false;  // encadenado "a.b." -- Fase 6, fuera de alcance

    identifier = before.substr(j, dot - j);
    return true;
}

} // namespace

void VariableTypeIndex::Rebuild(const std::string& text, const ClassIndex& class_index) {
    variable_types_.clear();

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
            std::string var_name = ReadIdent(text, i);
            size_t save = i;
            SkipInlineWhitespace(text, i);

            // Solo `=` simple (Fase 2 dice literalmente "variable =
            // ClaseConocida(...)") -- ni `==`, ni `+=`/`-=`/`*=`/`/=`, y
            // nunca `this.NAME = ...` (eso ya viene excluido porque acá
            // `var_name` es "this" y el siguiente caracter sería '.', no
            // '=', así que este bloque simplemente no matchea).
            bool is_plain_assign = i < text.size() && text[i] == '=' &&
                                    (i + 1 >= text.size() || text[i + 1] != '=');
            if (!is_plain_assign) { i = save; continue; }

            ++i;  // el '='
            SkipInlineWhitespace(text, i);

            std::string resolved_class;
            if (i < text.size() && IsIdentStart(text[i])) {
                size_t ident_start = i;
                std::string rhs_name = ReadIdent(text, i);
                size_t after_ident = i;
                SkipInlineWhitespace(text, i);
                if (i < text.size() && text[i] == '(' && class_index.Find(rhs_name) != nullptr) {
                    resolved_class = rhs_name;
                } else {
                    i = after_ident;  // no era un constructor conocido -- no consumir de más
                }
                (void)ident_start;
            }

            if (!resolved_class.empty()) {
                variable_types_[var_name] = resolved_class;
            } else {
                // Reasignado a algo que no se puede inferir -- invalidar
                // cualquier tipo anterior conocido para este nombre en vez
                // de dejar un mapeo viejo y ahora incorrecto (Fase 6:
                // "variable reasignada a otro tipo más adelante").
                variable_types_.erase(var_name);
            }
            continue;
        }

        ++i;
    }
}

std::string VariableTypeIndex::TypeOf(const std::string& variable) const {
    auto it = variable_types_.find(variable);
    return it == variable_types_.end() ? "" : it->second;
}

bool ResolveMemberAccess(const std::string& full_text, int cursor_line,
                          const std::string& text_before_cursor_on_line,
                          const ClassIndex& class_index, const VariableTypeIndex& var_types,
                          MemberAccessContext& out) {
    std::string identifier;
    if (!ExtractDotAccess(text_before_cursor_on_line, identifier)) return false;

    // Prefijo completo hasta el cursor (para saber en qué clase, si
    // alguna, está el cursor lexicamente) -- ver FindEnclosingClass.
    std::string prefix;
    if (cursor_line > 0) {
        size_t pos = 0;
        int line = 0;
        while (line < cursor_line && pos <= full_text.size()) {
            size_t nl = full_text.find('\n', pos);
            if (nl == std::string::npos) { pos = full_text.size(); break; }
            prefix.append(full_text, pos, nl - pos + 1);
            pos = nl + 1;
            ++line;
        }
    }
    prefix += text_before_cursor_on_line;

    std::string viewer_class = FindEnclosingClass(prefix);

    // Orden de resolución: `this` primero, después una VARIABLE conocida,
    // y solo si ninguna de esas aplica, un nombre de clase directo. Una
    // variable tiene que ganarle a un nombre de clase homónimo -- el caso
    // de referencia del TODO es literalmente `dog = dog()` seguido de
    // `dog.`, donde "dog" es a la vez el nombre de la variable Y el de la
    // clase; ahí se quiere el acceso de INSTANCIA (kInstance, todos los
    // miembros públicos), no kClassName (que solo dejaría pasar los
    // `static`, y "say()" no lo es).
    if (identifier == "this") {
        if (viewer_class.empty()) return false;  // `this` fuera de toda clase -- no resoluble
        out.kind = MemberAccessKind::kThis;
        out.class_name = viewer_class;
        out.viewer_class = viewer_class;
        return true;
    }

    std::string var_class = var_types.TypeOf(identifier);
    if (!var_class.empty()) {
        out.kind = MemberAccessKind::kInstance;
        out.class_name = var_class;
        out.viewer_class = viewer_class;
        return true;
    }

    if (class_index.Find(identifier) != nullptr) {
        out.kind = MemberAccessKind::kClassName;
        out.class_name = identifier;
        out.viewer_class = viewer_class;
        return true;
    }

    return false;  // identificador desconocido -- fallback seguro (Fase 2/4)
}

} // namespace studio
