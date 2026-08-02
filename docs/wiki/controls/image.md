# Image

Muestra una imagen a partir de una ruta lógica (`source`), resuelta
por el proveedor de recursos activo (prefijos `@icons/...`,
`@local/...`, o una ruta de archivo directa).

Fuente: `runtime/avaui/src/controls/Image.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"image"`), render en
`runtime/avaui/src/render_tree/RenderTree.cpp` (`DecomposeImage`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador de la imagen. |
| `source` | string | — | Ruta lógica de la imagen. Se copia tal cual al render node — la resolución real (`@icons/...`, `@local/...`, ruta de archivo) la hace el renderer activo, no este control. |
| `alt` | string | — | Texto de accesibilidad. Reservado — todavía no lo consume ningún renderer. |
| `borderRadius` | number | radio de borde del tema (`4`) | Radio de las esquinas de la imagen. Ver [Control Base](./control-base.md#apariencia-visual) para cómo hacer una imagen circular (`width == height`, `borderRadius >= width / 2`). |

> El resto de las propiedades base (`width`, `height`, `margin`,
> `padding`, `align`, `borderColor`, `borderWidth`, `class`, `zIndex`,
> etc.) también aplican — ver [Control Base](./control-base.md).

## Eventos

Image no expone eventos propios. `click` se puede asignar igual que a
cualquier control (ver [Control Base](./control-base.md#eventos)) para,
por ejemplo, usar una imagen como botón.

## Ejemplo

```
image
    id = "Logo"
    source = "@icons/star.svg"
    alt = "Logo de la app"
end
```

Imagen circular (avatar):

```
image
    id = "Avatar"
    source = "@local/avatar.png"
    width = 48
    height = 48
    borderRadius = 24
end
```

---

[« Volver al índice](../README.md)
