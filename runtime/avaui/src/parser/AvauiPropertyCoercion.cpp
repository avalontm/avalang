#include "parser/AvauiPropertyCoercion.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

#include "registry/ComponentTypeRegistry.h"

namespace avalang {
namespace ui {
namespace parser {

std::string Unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool LooksLikeNumber(const std::string& s, double* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return false;
    *out = v;
    return true;
}

PropertyValue InferValue(const std::string& raw) {
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        return PropertyValue(Unquote(raw));
    }
    if (raw == "true") return PropertyValue(true);
    if (raw == "false") return PropertyValue(false);
    double num;
    if (LooksLikeNumber(raw, &num)) return PropertyValue(num);
    // Cualquier otra cosa (identificador suelto, concatenacion,
    // expresion sin resolver) se deja como texto opaco -- mismo "soft
    // fallback" que ya documentaba AvauiParser.h ("Semantic gaps").
    return PropertyValue(raw);
}

namespace {

// Ver AvauiPropertyCoercion.h -- estos dos mapas eran privados a
// AvauiParser.cpp; ahora viven aca porque SetPropertyWithAlias y
// CanonicalTypeName son de uso publico.
const std::unordered_map<std::string, std::string>& PropertyAliases() {
    static const std::unordered_map<std::string, std::string> kPropertyAliases = {
        {"gap", "spacing"},
        {"value", "text"},
    };
    return kPropertyAliases;
}

const std::unordered_map<std::string, std::string>& TypeNameAliases() {
    // Fase B.5 (plan unificado avastudio/avaui): a diferencia del resto
    // de TypeNames() (ver abajo), estos tres no son una variante de
    // may/minuscula del TypeName real -- son atajos de sintaxis del
    // lenguaje .avaui para un tipo que ya existe con otro nombre
    // (p.ej. "input" en vez de "textbox"). No se pueden derivar del
    // registro de avaui igual que el resto, asi que se mantienen a
    // mano aca, mismo motivo que PropertyAliases(): alias de sintaxis,
    // no de catalogo.
    static const std::unordered_map<std::string, std::string> kTypeNameAliases = {
        {"input", "TextBox"},
        {"radio", "RadioButton"},
        {"scroll", "ScrollView"},
    };
    return kTypeNameAliases;
}

const std::unordered_map<std::string, std::string>& TypeNames() {
    // Fase B.5: antes esta tabla repetia a mano cada TypeName real
    // (con su propia oportunidad de desincronizarse, igual que el bug
    // de enabled/isEnabled en B.0) -- ahora se deriva de
    // registry::GetComponentTypeRegistry(), la misma fuente que ya usa
    // avastudio (B.4). Un tipo nuevo que se auto-registre en avaui
    // (controls/Xxx.cpp) queda reconocible por el parser via su propio
    // TypeName en minuscula sin tocar este archivo. Los pocos alias
    // que no son solo mayus/minuscula (input/radio/scroll) siguen
    // arriba, en TypeNameAliases().
    static const std::unordered_map<std::string, std::string> kTypeNames = [] {
        std::unordered_map<std::string, std::string> types;
        for (const avalang::ui::registry::ComponentTypeDescriptor& descriptor :
             avalang::ui::registry::GetComponentTypeRegistry()) {
            std::string lower = descriptor.type;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            types[lower] = descriptor.type;
        }
        for (const auto& [alias, canonical] : TypeNameAliases()) {
            types[alias] = canonical;
        }
        return types;
    }();
    return kTypeNames;
}

} // namespace

std::string CanonicalTypeName(const std::string& asWritten) {
    const auto& types = TypeNames();
    auto it = types.find(asWritten);
    if (it != types.end()) return it->second;
    if (asWritten.empty()) return asWritten;
    std::string result = asWritten;
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    return result;
}

void SetPropertyWithAlias(IComponent* component, const std::string& name,
                           const PropertyValue& value) {
    component->SetProperty(name, value);
    const auto& aliases = PropertyAliases();
    auto alias = aliases.find(name);
    if (alias != aliases.end() && alias->second != name) {
        component->SetProperty(alias->second, value);
    }
}

} // namespace parser
} // namespace ui
} // namespace avalang
