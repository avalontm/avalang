# Component Tree - Arquitectura y Diseño

## Concepto

El **Component Tree** es la estructura de datos central de AvaUI que representa la jerarquía visual de una aplicación. Es análogo a:
- React Virtual DOM
- Flutter Widget Tree
- Qt Object Tree

## Estructura de Datos

### Clase Component
```cpp
class Component : public std::enable_shared_from_this<Component> {
    std::string type_;       // "page", "stack", "button", etc.
    std::string id_;         // Identificador único
    
    int layout_ = 0;         // LayoutType enum
    
    // Propiedades (insertion-ordered)
    std::vector<std::pair<std::string, Value>> properties_;
    
    // Eventos (nombre → callback)
    std::vector<std::pair<std::string, Value>> events_;
    
    // Hijos
    std::vector<std::shared_ptr<Component>> children_;
};
```

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

### Clase ComponentTree
```cpp
class ComponentTree {
    std::shared_ptr<Component> root_;
    
public:
    void SetRoot(std::shared_ptr<Component> root);
    std::shared_ptr<Component> GetRoot() const;
    std::string ToJson() const;
};
```

## C API (avalang.h)

### Gestión del Tree
```c
AvaComponentTree* ava_ui_create_tree(void);
void ava_ui_destroy_tree(AvaComponentTree* tree);
AvaComponent* ava_ui_get_root(AvaComponentTree* tree);
void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root);
```

### Gestión de Componentes
```c
AvaComponent* ava_ui_create_component(const char* type);
void ava_ui_destroy_component(AvaComponent* comp);
void ava_ui_set_id(AvaComponent* comp, const char* id);
void ava_ui_set_layout(AvaComponent* comp, int layout);
```

### Propiedades
```c
void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value);
void ava_ui_get_property(AvaComponent* comp, const char* key, ava_value_t* out_value);
// PENDIENTE: ava_ui_list_properties() - enumerate todas las claves
```

### Jerarquía
```c
void ava_ui_add_child(AvaComponent* parent, AvaComponent* child);
AvaComponent* ava_ui_get_child(AvaComponent* parent, int index);
int ava_ui_get_child_count(AvaComponent* comp);
```

## Serialización JSON

```json
{
  "type": "page",
  "id": "MainPage",
  "layout": 0,
  "properties": {
    "title": "Mi App",
    "theme": "dark"
  },
  "children": [
    {
      "type": "stack",
      "layout": 1,
      "properties": {},
      "children": [
        {
          "type": "text",
          "properties": {
            "value": "Bienvenido"
          }
        },
        {
          "type": "button",
          "properties": {
            "text": "Guardar",
            "enabled": true
          },
          "events": {
            "on_click": "btnGuardar_Click"
          }
        }
      ]
    }
  ]
}
```

## Algoritmo de Layout

### Column Layout
```
┌─────────────────────┐
│     Child 1         │
├─────────────────────┤
│     Child 2         │
├─────────────────────┤
│     Child 3         │
└─────────────────────┘
```

1. Calcular altura total de hijos (suma)
2. Si hay spacer remaining, distribuir proporcionalmente
3. Posicionar cada hijo secuencialmente

### Row Layout
```
┌───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ 4 │
└───┴───┴───┴───┘
```

1. Calcular ancho total de hijos
2. Distribuir espacio restante
3. Posicionar horizontalmente

## Integración con Studio

### Preview Panel
Muestra el tree como árbol expandible:
```
▼ page "MainPage"
  ▼ stack (Column)
     ├─ text "Bienvenido"
     └─ button "Guardar"
```

### Properties Panel
Al seleccionar un nodo, muestra sus propiedades:
| Property | Value |
|----------|-------|
| type | button |
| id | btnGuardar |
| text | "Guardar" |
| enabled | true |

## Tasks

### [PENDING] Property Enumeration API
- **Archivos**: `public/src/c_api.cpp`
- **Descripción**: Implementar función para listar todas las propiedades de un componente
- **Firma propuesta**:
```c
int ava_ui_list_properties(AvaComponent* comp, const char** out_keys, int max_keys);
```

### [PENDING] Event System Completo
- **Descripción**: Sistema de eventos que conecta componentes con handlers en scripts
- **C API propuesta**:
```c
void ava_ui_set_event(AvaComponent* comp, const char* event, ava_value_t handler);
```