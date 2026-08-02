#pragma once

#include <string>
#include <unordered_map>

#include "languages/class_index.h"

namespace studio {

// Fase 2 de TODO_autocompletado_miembros.md -- inferencia de tipo de
// variable, best-effort, sin ningún tipo de scoping: un único mapa plano
// `variable -> nombre_de_clase` para todo el buffer.
//
// Solo registra una entrada cuando el lado derecho de un `variable = ...`
// es literalmente `ClaseConocida(...)` Y `ClaseConocida` es una clase que
// `ClassIndex` ya indexó (local o importada) -- así "variable = 5",
// "variable = otraFuncion()" o una clase que no existe simplemente no
// generan ninguna entrada, en vez de una adivinanza. Recorre el texto de
// arriba hacia abajo y CADA `variable = ...` que encuentra vuelve a
// evaluar la entrada de ese nombre (la sobreescribe si es un constructor
// conocido, o la borra si no lo es) -- por lo que "última asignación gana"
// sale solo, igual que pide la Fase 6 ("Variable reasignada a otro tipo
// más adelante en el archivo").
//
// No intenta resolver `this.attr = Clase(...)` (eso es un atributo, no una
// variable -- ver ClassIndex/RecordAttribute) ni parámetros de función ni
// nada que dependa de flujo de control: es deliberadamente ingenuo, igual
// que el resto de estos índices "del lado del editor".
class VariableTypeIndex {
public:
    void Rebuild(const std::string& text, const ClassIndex& class_index);

    // "" si `variable` no tiene un tipo inferido (no se pudo inferir, o
    // nunca se vio) -- el caller debe caer de vuelta al comportamiento
    // actual en ese caso, nunca forzar una clase incorrecta.
    std::string TypeOf(const std::string& variable) const;

    void Clear() { variable_types_.clear(); }

private:
    std::unordered_map<std::string, std::string> variable_types_;
};

// Fase 3 -- a qué se le está pidiendo autocompletado.
//   kInstance  -> `variable.` (variable resuelta vía VariableTypeIndex)
//   kThis      -> `this.` escrito dentro del cuerpo de una clase
//   kClassName -> `NombreDeClase.` (acceso directo al objeto clase)
// Ver MemberAccessKind en class_index.h para el detalle de qué filtra
// cada uno vía ClassIndex::FilterForAccess.
struct MemberAccessContext {
    MemberAccessKind kind = MemberAccessKind::kInstance;
    std::string class_name;    // clase cuyos FlattenedMembers() hay que ofrecer
    std::string viewer_class;  // clase que encierra al cursor, "" si es top-level
};

// Punto de entrada de la Fase 3: dado el texto completo del buffer, la
// línea 0-based donde está el cursor, y el texto de ESA línea desde su
// inicio hasta la posición del cursor (`text_before_cursor_on_line` --
// mismo substring que ya arma DrawParameterHint en editor_panel.cpp vía
// TextEditor::GetLineText/GetCursorPosition), intenta reconocer el patrón
// `identificador.parcial` justo antes del cursor y resolver `identificador`
// contra `class_index` / `var_types`.
//
// Nota (ver punto 1 de la Fase 3 en el TODO): el header vendorizado de
// TextEditor.h (FetchContent, fuera de este zip) no está disponible acá
// para confirmar qué expone `AutoCompleteState` más allá de
// `suggestions`/`searchTerm` (los dos únicos campos que editor_panel.cpp
// ya usa). Por eso esta función no depende de nada nuevo de
// AutoCompleteState: recibe el texto/posición ya obtenidos con los mismos
// métodos de TextEditor que DrawParameterHint usa (GetCursorPosition,
// GetLineText), que sabemos que existen porque ya están en uso.
//
// Devuelve false (y no toca `out`) si no se puede resolver con confianza
// -- identificador desconocido, encadenado (`a.b.`), `this` fuera de toda
// clase, etc. El caller debe caer de vuelta al trie global en ese caso
// (Fase 4), nunca mostrar un popup vacío.
bool ResolveMemberAccess(const std::string& full_text, int cursor_line,
                          const std::string& text_before_cursor_on_line,
                          const ClassIndex& class_index, const VariableTypeIndex& var_types,
                          MemberAccessContext& out);

} // namespace studio
