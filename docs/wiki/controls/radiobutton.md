# RadioButton

Botón de opción circular con etiqueta opcional. A diferencia de
[CheckBox](./checkbox.md), varios `radiobutton` con el mismo `group`
son mutuamente excluyentes.

Fuente: `runtime/avaui/src/controls/RadioButton.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"radiobutton"` / alias `"radio"`),
render en `runtime/avaui/src/render_tree/RenderTree.cpp`
(`DecomposeRadioButton`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del control. Necesario para el auto-bind de eventos (`On{Id}Change`). |
| `label` | string | `""` | Texto mostrado a la derecha del círculo. Si está vacío, no se dibuja ningún texto. |
| `group` | string | `""` | Nombre del grupo. Seleccionar un `radiobutton` deselecciona a todos los demás del mismo `group`. |
| `isSelected` | bool | `false` | Estado del control. Normalmente se enlaza a una variable de `state`. |
| `isEnabled` | bool | `true` | Si es `false`, el control no responde a clicks. |
| `borderColor` | color (hex) | color de tema `border` (`CCCCCC`) | Color del borde del círculo. |
| `borderWidth` | number | ancho de borde del tema (`1`) | Grosor del borde del círculo. |

El indicador se dibuja como un círculo (elipse) de hasta `16px` de
diámetro (o el alto disponible del nodo si es menor), y cuando
`isSelected = true` se rellena en azul (`#0078D4`), fijo por ahora —
no configurable vía propiedad.

> El resto de las propiedades base (`width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

## Eventos

| Evento | Argumentos | Descripción |
|---|---|---|
| `change` | — | Se dispara cuando el usuario hace click sobre el control o su etiqueta. El handler es responsable de actualizar el estado seleccionado; el control no lo hace solo (la exclusión mutua real entre miembros del `group` la resuelve el runtime a nivel de componente, no el binding de `state`). |

## Ejemplo

```
state
    radioValue = true
end

view
    radiobutton
        id = "TestRadio"
        label = "RadioButton"
        group = "opciones"
        isSelected = radioValue
        change = OnRadioChange()
    end
end

code
    func OnRadioChange()
        radioValue = not radioValue
    end
end
```

---

[« Volver al índice](../README.md)
