# AvaUI Framework - Visión y Arquitectura

## Concepto

**AvaUI** es un framework de UI declarativo construido sobre AvaLang. Permite crear interfaces gráficas usando código similar a este:

```lua
-- Ejemplo de UI con AvaUI
page "Main"
    stack Column
        text "Hola Mundo"
        button "Guardar" on_click=btnGuardar_Click
    end
end

func btnGuardar_Click(sender, e)
    print("Guardado!")
end
```

## Arquitectura General

```
┌─────────────────────────────────────────────────────────────┐
│                     Host Application                        │
│              (Ava Studio, C# App, Game Engine)               │
├─────────────────────────────────────────────────────────────┤
│                    EngineBridge (C++)                        │
│           ava_compile() / ava_run() / ava_ui_*()            │
├─────────────────────────────────────────────────────────────┤
│                      AvaLang Core                            │
│                    (VM + Compiler)                           │
├─────────────────────────────────────────────────────────────┤
│                    AvaUI Framework                          │
│          Component Tree + Layout Engine                      │
├─────────────────────────────────────────────────────────────┤
│                    Rendering Backend                        │
│         (HTML5/CSS, ImGui, native controls, etc.)           │
└─────────────────────────────────────────────────────────────┘
```

## Tipos de Componentes

### Layout Components
| Tipo | Descripción |
|------|-------------|
| `Column` | Apila hijos verticalmente |
| `Row` | Distribuye hijos horizontalmente |
| `Stack` | Apila hijos solapados |
| `Grid` | Layout en cuadrícula |
| `Flex` | Layout flexible/responsive |

### Content Components
| Tipo | Descripción |
|------|-------------|
| `Text` | Texto plano o formateado |
| `Image` | Imagen estática |
| `Spacer` | Espacio vacío expandible |
| `Divider` | Separador horizontal |
| `Link` | Enlace clickeable |

### Interactive Components
| Tipo | Descripción |
|------|-------------|
| `Button` | Botón clickeable |
| `TextBox` | Campo de texto input |
| `CheckBox` | Toggle booleano |
| `RadioButton` | Opción de selección |

### Navigation Components
| Tipo | Descripción |
|------|-------------|
| `Page` | Contenedor de página |
| `Navigator` | Navegación entre páginas |
| `TabControl` | Pestañas |
| `Drawer` | Panel lateral deslizable |

## Estructura de Componente

```cpp
class Component {
    std::string type_;        // "button", "text", etc.
    std::string id_;          // Identificador único
    int layout_;              // LayoutType enum
    
    // Propiedades (clave-valor)
    std::vector<std::pair<std::string, Value>> properties_;
    
    // Eventos (nombre → callback)
    std::vector<std::pair<std::string, Value>> events_;
    
    // Hijos
    std::vector<std::shared_ptr<Component>> children_;
};
```

## Layout Engine

### LayoutType Enum
```cpp
enum class LayoutType {
    None = 0,   // Sin layout automático
    Column,     // Stack vertical
    Row,        // Stack horizontal
    Stack,      // Stack con overlap
    Grid,       // Grid de celdas
    Flex,       // Flexbox
};
```

### Algoritmo de Layout
1. Medir hijos (各自的 min/max size)
2. Distribuir espacio según LayoutType
3. Posicionar cada hijo
4. Retornar tamaño final del contenedor

## C API (avalang.h)

```c
// Crear árbol de componentes
AvaComponentTree* ava_ui_create_tree(void);
void ava_ui_destroy_tree(AvaComponentTree* tree);

// Componentes
AvaComponent* ava_ui_create_component(const char* type);
void ava_ui_destroy_component(AvaComponent* comp);

// Modificar componente
void ava_ui_set_id(AvaComponent* comp, const char* id);
void ava_ui_set_layout(AvaComponent* comp, int layout);
void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value);
void ava_ui_get_property(AvaComponent* comp, const char* key, ava_value_t* out_value);

// Jerarquía
void ava_ui_add_child(AvaComponent* parent, AvaComponent* child);
AvaComponent* ava_ui_get_child(AvaComponent* parent, int index);
AvaComponent* ava_ui_get_root(AvaComponentTree* tree);
void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root);

// Serialización
const char* ava_ui_tree_to_json(AvaComponentTree* tree);
```

## Builtins a Registrar

Para que scripts puedan crear UI, se necesitan estos builtins:

```cpp
// page(name) → crea página
// stack(layout) → crea contenedor
// column() → stack con Column layout
// row() → stack con Row layout
// button(text) → botón
// text(value) → texto
// image(src) → imagen
// on_click = handler → registra evento
// on_change = handler → registra evento
```

## Serialización JSON

El Component Tree se serializa a JSON:

```json
{
  "type": "page",
  "id": "Main",
  "layout": 0,
  "properties": {},
  "children": [
    {
      "type": "stack",
      "layout": 1,
      "children": [
        {"type": "text", "properties": {"value": "Hola"}},
        {"type": "button", "properties": {"text": "OK"}}
      ]
    }
  ]
}
```

## Próximos Pasos

1. Registrar UI builtins en VM
2. API de enumeración de propiedades
3. Integración con Studio Preview
4. Event binding system completo

## Tasks

### [PENDING] Registrar UI Builtins en VM
- **Fecha objetivo**: Por definir
- **Archivo**: `core/src/builtins/`
- **Problema**: Scripts no pueden crear componentes UI directamente
- **Solución**: Agregar funciones nativas `page()`, `stack()`, `button()`, etc.

### [PENDING] Property Enumeration API
- **Fecha objetivo**: Por definir
- **Archivos**: `public/src/c_api.cpp`, `public/include/avalang.h`
- **Problema**: No se pueden listar todas las propiedades de un componente
- **Solución**: `ava_ui_list_properties()` que retorna array de claves