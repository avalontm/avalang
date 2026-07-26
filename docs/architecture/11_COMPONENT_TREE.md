# Component Tree

## Purpose

El **Component Tree** es la estructura de datos central de AvaUI:
representa la jerarquía visual de una aplicación. Es análogo a React
Virtual DOM, Flutter Widget Tree o Qt Object Tree. Es el documento
canónico de referencia para `Component`/`ComponentTree`/`LayoutType` —
el resto de la documentación enlaza acá en vez de redefinirlos.

## Responsibilities

- Representar un árbol de nodos UI (`Component`) con tipo, id, layout,
  propiedades, eventos e hijos.
- Serializar/deserializar ese árbol a JSON.
- Exponer el árbol a hosts fuera de C++ vía la C API (`avalang.h`).

## Current Implementation

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

Nota de estado: `Grid` y `Flex` existen en el enum pero el layout
engine todavía no tiene un algoritmo propio para ellos — ver
`12_LAYOUT.md`.

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

### Serialización JSON

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

### Integración con Studio

El Preview panel muestra el tree como árbol expandible:
```
▼ page "MainPage"
  ▼ stack (Column)
     ├─ text "Bienvenido"
     └─ button "Guardar"
```

El Properties panel, al seleccionar un nodo, muestra sus propiedades:
| Property | Value |
|----------|-------|
| type | button |
| id | btnGuardar |
| text | "Guardar" |
| enabled | true |

Ver `16_STUDIO.md` para el resto de los paneles y cómo este árbol se
relaciona con `DesignNode` (el modelo paralelo que usa el Designer).

## Public Interfaces

C API completa en `public/include/avalang.h`:

```c
// Gestión del Tree
AvaComponentTree* ava_ui_create_tree(void);
void ava_ui_destroy_tree(AvaComponentTree* tree);
AvaComponent* ava_ui_get_root(AvaComponentTree* tree);
void ava_ui_set_root(AvaComponentTree* tree, AvaComponent* root);

// Gestión de Componentes
AvaComponent* ava_ui_create_component(const char* type);
void ava_ui_destroy_component(AvaComponent* comp);
void ava_ui_set_id(AvaComponent* comp, const char* id);
void ava_ui_set_layout(AvaComponent* comp, int layout);

// Propiedades
void ava_ui_set_property(AvaComponent* comp, const char* key, ava_value_t value);
void ava_ui_get_property(AvaComponent* comp, const char* key, ava_value_t* out_value);

// Jerarquía
void ava_ui_add_child(AvaComponent* parent, AvaComponent* child);
AvaComponent* ava_ui_get_child(AvaComponent* parent, int index);
int ava_ui_get_child_count(AvaComponent* comp);

// Serialización
const char* ava_ui_tree_to_json(AvaComponentTree* tree);
```

## Dependencies

- `core/src/ui/component.*` — implementación de `Component`/`ComponentTree`.
- `public/src/c_api.cpp` — superficie C sobre lo anterior.
- Consumido por: `10_AVAUI.md` (capas del sistema), `12_LAYOUT.md`
  (el layout engine opera sobre `Component`), `16_STUDIO.md` (Preview
  panel construye un árbol de demo vía esta misma C API).

## Future Evolution

- **Property Enumeration API** (`public/src/c_api.cpp`): no existe hoy
  una función para listar todas las propiedades de un componente.
  Firma propuesta:
  ```c
  int ava_ui_list_properties(AvaComponent* comp, const char** out_keys, int max_keys);
  ```
- **Sistema de eventos completo**: conectar componentes con handlers en
  scripts. C API propuesta:
  ```c
  void ava_ui_set_event(AvaComponent* comp, const char* event, ava_value_t handler);
  ```

## Open Questions

Ver `10_AVAUI.md` sección "Preguntas abiertas" — varias (identidad
estable de nodo, ownership del árbol en runtime) son sobre este mismo
tipo.
