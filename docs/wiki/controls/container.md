# Container (Container / Row / Column / Stack)

Familia de controles "caja" sin comportamiento propio más allá de
organizar a sus hijos. Cuatro palabras clave `.avaui` mapean a esta
misma familia:

| Palabra clave `.avaui` | Comportamiento de layout |
|---|---|
| `row` | Organiza a sus hijos en fila (eje principal horizontal). |
| `column` | Organiza a sus hijos en columna (eje principal vertical). |
| `stack` | Apila a sus hijos superpuestos (todos ocupan el mismo rect, útil para overlays simples). |
| `container` | Caja genérica sin arreglo propio — el motor de layout la trata igual que `stack` (fallback, ver `runtime/avaui/src/layout/LayoutEngineImpl.cpp`). |

Fuente: `runtime/avaui/src/controls/Container.h/.cpp` (factories
tipadas para `row`/`column`/`stack`), valores de tema en
`runtime/avaui/src/theme/RenderTheme.cpp` (`ApplyTypeDefaults`, tipos
`"container"`, `"row"`, `"column"`, `"stack"`, `"hstack"`, `"vstack"`,
`"scrollview"`, `"scroll"`), render en
`runtime/avaui/src/render_tree/RenderTree.cpp` (`DecomposeContainer`).

> `scrollview`/`scroll` comparte el mismo bloque de tema que esta
> familia (fondo `surface` por defecto), pero tiene su propio
> comportamiento de layout (scroll) y su propio `Decompose*`
> (`DecomposeScrollView`) — no está cubierto en esta ficha todavía.

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del contenedor. |
| `spacing` (alias `gap`) | number | `0` | Separación entre hijos en el eje principal. Solo tiene efecto en `row`/`column` — `stack`/`container` la ignoran (sus hijos se superponen). |
| `padding` | number | `0` | Espaciado interno uniforme aplicado a los hijos. Ver [Control Base](./control-base.md) para las variantes por lado (`padding-left`, etc.). |
| `backgroundColor` | color (hex) | color de tema `surface` (`F3F3F3`) | Color de fondo del contenedor. |
| `fill` | `"true"` \| `"false"` (string) | — | Usado en la práctica para que el contenedor ocupe el espacio disponible del padre (ver ejemplo). No tiene una tabla de valores propia más allá de layout estándar (`width`/`height`/`align`). |

> El resto de las propiedades base (`width`, `height`, `margin`,
> `align`, `borderColor`, `borderWidth`, `borderRadius`, `class`,
> `zIndex`, etc.) también aplican — ver [Control Base](./control-base.md).

## Eventos

Esta familia no expone eventos propios. `click`/`change` se pueden
asignar igual que a cualquier control (ver [Control Base](./control-base.md#eventos)),
pero no tienen manejo especial aquí.

## Ejemplo

```
view
    column
        fill = "true"
        padding = 20
        gap = 20

        row
            gap = 12
            padding = 12
            borderWidth = 1
            borderColor = "E5E7EB"

            text
                text = "Row"
            end
        end

        container
            padding = 12
            borderWidth = 1
            borderColor = "E5E7EB"

            text
                text = "Container"
            end
        end
    end
end
```

---

[« Volver al índice](../README.md)
