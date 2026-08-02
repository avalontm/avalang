# Text

El control más simple del set base: una etiqueta de texto plano. La
palabra clave `label` comparte exactamente la misma función de render
(`DecomposeText`) que `text` — la única diferencia real es el rol de
tema que recibe cada una por defecto (ver tabla de abajo).

Fuente: `runtime/avaui/src/controls/Text.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipos `"text"` y `"label"` — bloques separados),
render en `runtime/avaui/src/render_tree/RenderTree.cpp`
(`DecomposeText`, usada tanto por `typeName == "Text"` como por
`typeName == "Label"`).

## Propiedades

| Propiedad | Tipo | Por defecto (`text`) | Por defecto (`label`) | Descripción |
|---|---|---|---|---|
| `id` | string | — | — | Identificador del nodo. |
| `text` | string | `""` | `""` | Contenido a mostrar. Admite concatenación simple con `state` (`"Total: " + total`). |
| `fontSize` | number | `12` (fuente de tema `body`) | `12` (fuente de tema `label`) | Tamaño de fuente en px. |
| `fontName` | string | `"Segoe UI"` (fuente de tema `body`) | `"Segoe UI"` (fuente de tema `label`) | Familia tipográfica. |
| `textColor` | color (hex) | color de tema `text` (`333333`) | color de tema `text` (`333333`) | Color del texto. |

La diferencia real entre `text` y `label` es el **peso** de la fuente
que trae el tema por defecto: `body` (usada por `text`) es peso `400`
(normal), mientras que `label` es peso `600` (semibold) — útil para
etiquetas de formularios sin tener que fijar `fontSize`/`fontName` a
mano. El peso de fuente en sí no es una propiedad expuesta en
`.avaui` — viene resuelto ya por el tema.

> El resto de las propiedades base (`width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

## Eventos

Text no expone eventos propios. `click` se puede asignar igual que a
cualquier control (ver [Control Base](./control-base.md#eventos)) para
usar un texto como elemento clickeable simple.

## Ejemplo

```
state
    total = 42
end

view
    column
        gap = 8
        padding = 20

        text
            text = "Controles de prueba (avaui)"
            fontSize = 32
        end

        label
            text = "Total"
        end

        text
            text = "Total: " + total
        end
    end
end
```

---

[« Volver al índice](../README.md)
