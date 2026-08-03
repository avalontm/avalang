#ifndef AVA_UI_PARSER_AVAUI_PROPERTY_COERCION_H
#define AVA_UI_PARSER_AVAUI_PROPERTY_COERCION_H

#include <string>

#include "Export.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace parser {

// Extraido de AvauiParser.cpp (estaba en su namespace anonimo) para
// que CUALQUIER builder que construya un ComponentTree a partir de
// datos que NO vienen de texto .avaui (p.ej. Ava Studio's
// live_render_bridge, que arma el arbol directo desde DesignNode)
// use exactamente las mismas reglas de inferencia de tipo y de alias
// de nombre que el parser real -- sin esto, el panel Design y el
// pipeline real (avahost/ui_pipeline_*) podrian silenciosamente
// interpretar el mismo valor de forma distinta (p.ej. "4" como String
// en un lado y Number en el otro) y desviarse con el tiempo.

// "texto" -> "texto" (quita comillas dobles envolventes si estan).
AVA_UI_API std::string Unquote(const std::string& s);

// True si `s` parsea completo como double (sin basura al final).
AVA_UI_API bool LooksLikeNumber(const std::string& s, double* out);

// Decide el PropertyType a partir del texto ya recortado (sin
// comillas -> String citado explicito; "true"/"false" -> Bool;
// numerico -> Number; cualquier otra cosa -> String opaco, sin
// evaluar expresiones/bindings de estado).
AVA_UI_API PropertyValue InferValue(const std::string& raw);

// spec keyword en minuscula (p.ej. "textbox") -> TypeName PascalCase
// que LayoutEngine/RenderTree reconocen (p.ej. "TextBox"). Nombres no
// reconocidos pasan con solo la primera letra en mayuscula (soft
// fallback, no error -- ver comentario original en AvauiParser.cpp).
AVA_UI_API std::string CanonicalTypeName(const std::string& asWritten);

// SetProperty(name, value) + si `name` tiene un alias conocido
// (kPropertyAliases: "gap"->"spacing", "value"->"text"), tambien lo
// setea bajo ese nombre -- para que un builder que no pasa por el
// parser no tenga que redescubrir esos alias por su cuenta.
AVA_UI_API void SetPropertyWithAlias(IComponent* component, const std::string& name,
                                      const PropertyValue& value);

} // namespace parser
} // namespace ui
} // namespace avalang

#endif // AVA_UI_PARSER_AVAUI_PROPERTY_COERCION_H
