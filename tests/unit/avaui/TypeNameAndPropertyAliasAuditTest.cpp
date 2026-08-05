#include "parser/AvauiPropertyCoercion.h"
#include "registry/ComponentTypeRegistry.h"

#include <cassert>
#include <cstdio>
#include <string>

using avalang::ui::PropertyValue;
using avalang::ui::registry::ComponentTypeDescriptor;
using avalang::ui::registry::FindComponentType;
using avalang::ui::registry::PropertyDefault;
using avalang::ui::registry::RegisterComponentType;

namespace {

bool HasDefaultProperty(const std::string& type, const std::string& propertyName) {
    const ComponentTypeDescriptor* desc = FindComponentType(type);
    if (!desc) return false;
    for (const PropertyDefault& prop : desc->default_properties) {
        if (prop.name == propertyName) return true;
    }
    return false;
}

} // namespace

int main() {
    // Registrado antes de la primera llamada a CanonicalTypeName: la
    // tabla TypeNames() de AvauiPropertyCoercion.cpp cachea en la
    // primera llamada, igual que el catalogo de avastudio (B.4). Un
    // tipo nuevo que exista SOLO en el registro de avaui debe quedar
    // reconocible por el parser via su propio TypeName en minuscula,
    // sin ninguna entrada a mano en este archivo.
    RegisterComponentType(ComponentTypeDescriptor{
        "TempWidgetPhaseB5", "Temp Widget", /*is_container=*/false, {}});

    // Derivados del registro (antes eran pares hardcodeados en
    // TypeNames() -- ahora vienen de minusculizar ComponentTypeDescriptor::type).
    assert(avalang::ui::parser::CanonicalTypeName("button") == "Button");
    assert(avalang::ui::parser::CanonicalTypeName("checkbox") == "CheckBox");
    assert(avalang::ui::parser::CanonicalTypeName("textbox") == "TextBox");
    assert(avalang::ui::parser::CanonicalTypeName("combobox") == "ComboBox");
    assert(avalang::ui::parser::CanonicalTypeName("radiobutton") == "RadioButton");
    assert(avalang::ui::parser::CanonicalTypeName("scrollview") == "ScrollView");
    assert(avalang::ui::parser::CanonicalTypeName("grid") == "Grid");
    assert(avalang::ui::parser::CanonicalTypeName("flex") == "Flex");
    assert(avalang::ui::parser::CanonicalTypeName("row") == "Row");
    assert(avalang::ui::parser::CanonicalTypeName("column") == "Column");
    assert(avalang::ui::parser::CanonicalTypeName("stack") == "Stack");
    assert(avalang::ui::parser::CanonicalTypeName("page") == "Page");
    assert(avalang::ui::parser::CanonicalTypeName("text") == "Text");
    assert(avalang::ui::parser::CanonicalTypeName("image") == "Image");
    assert(avalang::ui::parser::CanonicalTypeName("link") == "Link");
    assert(avalang::ui::parser::CanonicalTypeName("dialog") == "Dialog");

    // No registrados en avaui (Container/Label/Icon) -- resueltos por
    // el fallback de "primera letra en mayuscula", no por la tabla, y
    // coinciden con el TypeName real que LayoutEngine/RenderTree ya
    // reconocen (ver LayoutEngineImpl.cpp/RenderTree.cpp).
    assert(avalang::ui::parser::CanonicalTypeName("container") == "Container");
    assert(avalang::ui::parser::CanonicalTypeName("label") == "Label");
    assert(avalang::ui::parser::CanonicalTypeName("icon") == "Icon");

    // Alias de sintaxis que no son solo mayus/minuscula -- no se pueden
    // derivar del registro, se mantienen a mano (TypeNameAliases()).
    assert(avalang::ui::parser::CanonicalTypeName("input") == "TextBox");
    assert(avalang::ui::parser::CanonicalTypeName("radio") == "RadioButton");
    assert(avalang::ui::parser::CanonicalTypeName("scroll") == "ScrollView");

    // Un tipo agregado solo en el registro de avaui es reconocible por
    // el parser sin tocar este archivo -- mismo criterio que B.4 probo
    // para el catalogo de avastudio.
    assert(avalang::ui::parser::CanonicalTypeName("tempwidgetphaseb5") == "TempWidgetPhaseB5");

    // Auditoria de PropertyAliases() contra el registro (B.5): el bug
    // de B.0 fue que el catalogo ponia "enabled" como default de
    // Button mientras el control real esperaba "isEnabled" -- un
    // alias de sintaxis (source->target) tiene el mismo riesgo si
    // `source` reaparece como nombre de propiedad real en el registro
    // en vez de `target`. Los dos alias vigentes son "gap"->"spacing"
    // y "value"->"text": ningun tipo registrado debe tener "gap" o
    // "value" como nombre de propiedad -- si lo tuviera, el alias
    // estaria pisando silenciosamente un valor que el control real
    // nunca lee bajo ese nombre.
    for (const ComponentTypeDescriptor& desc : avalang::ui::registry::GetComponentTypeRegistry()) {
        for (const PropertyDefault& prop : desc.default_properties) {
            assert(prop.name != "gap");
            assert(prop.name != "value");
            // Regresion directa del bug de B.0: "enabled" no debe
            // volver a aparecer como nombre de propiedad real en
            // ningun control -- el nombre real siempre es "isEnabled".
            assert(prop.name != "enabled");
        }
    }

    // Los targets de los alias si son nombres de propiedad reales que
    // al menos un control usa -- si no, el alias apuntaria al vacio.
    assert(HasDefaultProperty("Button", "text"));
    assert(HasDefaultProperty("Button", "isEnabled"));
    assert(HasDefaultProperty("Grid", "columns"));

    std::printf("TypeNameAndPropertyAliasAuditTest: OK\n");
    return 0;
}
