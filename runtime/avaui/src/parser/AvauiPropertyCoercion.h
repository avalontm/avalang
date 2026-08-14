#ifndef AVA_UI_PARSER_AVAUI_PROPERTY_COERCION_H
#define AVA_UI_PARSER_AVAUI_PROPERTY_COERCION_H

#include <string>

#include "Export.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace parser {












AVA_UI_API std::string Unquote(const std::string& s);


AVA_UI_API bool LooksLikeNumber(const std::string& s, double* out);





AVA_UI_API PropertyValue InferValue(const std::string& raw);





AVA_UI_API std::string CanonicalTypeName(const std::string& asWritten);





AVA_UI_API void SetPropertyWithAlias(IComponent* component, const std::string& name,
                                      const PropertyValue& value);

AVA_UI_API std::string NumberToDisplayString(double n);

AVA_UI_API bool LooksLikeCall(const std::string& s);

}
}
}

#endif