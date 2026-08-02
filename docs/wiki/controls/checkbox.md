# CheckBox

Casilla de verificación con una etiqueta opcional a la derecha. Se
dibuja como un cuadro pequeño (relleno sólido cuando está marcado) más
el texto de `label`.

Fuente: `runtime/avaui/src/controls/CheckBox.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"checkbox"`),
render en `runtime/avaui/src/render_tree/RenderTree.cpp`
(`DecomposeCheckBox`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador de la casilla. Necesario para el auto-bind de eventos (`On{Id}Change`). |
| `label` | string | `""` | Texto mostrado a la derecha del cuadro. Si está vacío, no se dibuja ningún texto. |
| `isChecked` | bool | `false` | Estado de la casilla. Normalmente se enlaza a una variable de `state`. |
| `isEnabled` | bool | `true` | Si es `false`, la casilla no responde a clicks. |
| `borderColor` | color (hex) | color de tema `border` (`CCCCCC`) | Color del borde del cuadro. |
| `borderWidth` | number | ancho de borde del tema (`1`) | Grosor del borde del cuadro. |

El cuadro se dibuja siempre como cuadrado (no circular — eso es
[RadioButton](./radiobutton.md)), de hasta `16px` de lado (o el alto
disponible del nodo si es menor), y cuando `isChecked = true` se
rellena en azul (`#0078D4`), fijo por ahora — no configurable vía
propiedad. `borderRadius` (ver [Control Base](./control-base.md)) sí
se puede aplicar manualmente sobre el cuadro si se quiere un checkbox
con esquinas redondeadas.

> El resto de las propiedades base (`width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

## Eventos

| Evento | Argumentos | Descripción |
|---|---|---|
| `change` | — | Se dispara cuando el usuario hace click sobre la casilla o su etiqueta. El handler es responsable de invertir el estado (`isChecked = not isChecked`); el control no lo hace solo. |

## Ejemplo

```
state
    checkValue = false
end

view
    checkbox
        id = "TestCheckbox"
        label = "CheckBox"
        isChecked = checkValue
        change = OnCheckChange()
    end
end

code
    func OnCheckChange()
        checkValue = not checkValue
    end
end
```

---

[« Volver al índice](../README.md)
