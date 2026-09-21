#pragma once

#include <string>
#include <vector>

#include "Export.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {
namespace registry {

// Fase 1 del plan de metadatos dinámicos de propiedades (ver PROPERTY_METADATA_PLAN.md).
//
// `PropertyKind::Auto` preserva el comportamiento histórico: AvaStudio infiere el
// editor/categoría por el nombre de la propiedad (heurística en
// avastudio/src/designer/component_registry.cpp). Un control puede declarar
// explícitamente el resto de valores de `PropertyKind` para que el panel de
// Properties muestre el editor correcto sin adivinar nada.
enum class PropertyKind {
    Auto,
    Text,
    MultilineText,
    Number,
    Boolean,
    Color,
    Enum,
    Resource,
    Binding,
    Event,
};

// Agrupación semántica de la propiedad dentro del panel de Properties. Espeja
// `studio::designer::PropertyCategory`, pero vive en avaui para que los
// controles (capa de runtime) no dependan de AvaStudio.
enum class PropertyGroup {
    Identity,
    Layout,
    Appearance,
    Typography,
    Behavior,
    Accessibility,
    Events,
    Advanced,
};

// Una opción de un editor Enum (p.ej. alineación: "start"/"center"/"end").
struct PropertyOption {
    std::string value;
    std::string label;
};

// Metadatos declarativos de una propiedad de un tipo de componente.
//
// Compatibilidad: esta struct sigue siendo un aggregate (sin constructores
// declarados), así que los sitios existentes `RegisterComponentType({..., {
//     {"label", PropertyValue("CheckBox")},
//     {"isChecked", PropertyValue(false)},
// }})` siguen compilando igual: solo llenan `name` y `defaultValue`, y el resto
// de campos toma su valor por defecto (`kind = Auto`), que reproduce la
// heurística previa sin cambios.
struct PropertyDescriptor {
    std::string name;
    PropertyValue defaultValue;

    PropertyKind kind = PropertyKind::Auto;
    PropertyGroup group = PropertyGroup::Advanced;
    std::string description;
    std::vector<PropertyOption> options;   // solo relevante si kind == Enum

    bool hasRange = false;                 // solo relevante si kind == Number
    double minValue = 0.0;
    double maxValue = 0.0;
    std::string unit;                      // "px", "%", "" ...

    bool readOnly = false;
    bool styleable = true;
    bool animatable = false;
};

// Alias de compatibilidad: el nombre histórico seguía usándose por claridad en
// código antiguo o herramientas externas que referencien el tipo por nombre.
using PropertyDefault = PropertyDescriptor;

struct ComponentTypeDescriptor {
    std::string type;
    std::string display_name;
    bool is_container = false;
    std::vector<PropertyDescriptor> default_properties;
};

// Bloque de metadata *genérica*, compartido por controles que la necesiten,
// para no repetir el mismo `PropertyDescriptor` (nombre, kind, group,
// descripción) en cada `Register*Type()`. Hoy solo existe el bloque de
// "control interactivo" (`isEnabled`), que es la única propiedad realmente
// duplicada entre los controles ya migrados (Button, TextBox, CheckBox,
// RadioButton, y ComboBox cuando se migre). Si en el futuro aparece otra
// propiedad genuinamente común a varios controles, se agrega aquí como una
// función nueva en vez de copiarla en cada `controls/*.cpp`.
//
// Uso en un control (ver Button.cpp/TextBox.cpp/CheckBox.cpp/RadioButton.cpp):
//   RegisterComponentType({
//       "Button", "Button", false,
//       WithCommonProperties(CommonInteractiveProperties(), {
//           PropertyDescriptor{.name = "text", ...},
//           PropertyDescriptor{.name = "style", ...},
//       }),
//   });
AVA_UI_API std::vector<PropertyDescriptor> CommonInteractiveProperties();

// Concatena un bloque de metadata genérica con la metadata específica del
// control (el "plus" que menciona cada control concreto), en ese orden.
AVA_UI_API std::vector<PropertyDescriptor> WithCommonProperties(
    std::vector<PropertyDescriptor> common, std::vector<PropertyDescriptor> specific);

AVA_UI_API void RegisterComponentType(ComponentTypeDescriptor desc);

AVA_UI_API const std::vector<ComponentTypeDescriptor>& GetComponentTypeRegistry();

AVA_UI_API const ComponentTypeDescriptor* FindComponentType(const std::string& type);

}
}
}