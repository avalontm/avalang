# Control Base

Todos los controles (`button`, `textbox`, `checkbox`, `radiobutton`,
`combobox`, `image`, `text`, `container`, `dialog`, ...) se resuelven a
partir del mismo componente base (`IComponent`, ver
`runtime/avaui/src/components/IComponent.h`). Antes de que el pipeline
llegue al código específico de cada control, dos pasos leen un mismo
conjunto de propiedades "genéricas" de cualquier nodo del árbol:

- **Motor de layout** (`runtime/avaui/src/layout/LayoutProperties.h`):
  resuelve tamaño y posición.
- **`RenderTree::BuildComponent`** (`runtime/avaui/src/render_tree/RenderTree.cpp`):
  resuelve apariencia visual y wiring de eventos, antes de despachar al
  `Decompose*` propio de cada tipo de control.

Estas son las propiedades que cualquier control puede usar,
independientemente de su tipo, sin que el control tenga que declararlas
él mismo.

## Layout

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del nodo. Habilita el auto-bind de eventos (`On{Id}{Evento}`) y la selección/edición en Ava Studio. |
| `width` | number | auto | Ancho explícito. Si se omite, se ajusta al espacio del padre (si `align`/`align-h` es `"stretch"`) o al tamaño intrínseco del contenido. |
| `height` | number | auto | Alto explícito. Mismo comportamiento que `width` en el eje vertical. |
| `margin` | number | `0` | Espaciado externo uniforme. |
| `margin-left` / `margin-top` / `margin-right` / `margin-bottom` | number | `0` | Override de `margin` por lado. |
| `padding` | number | `0` | Espaciado interno uniforme (aplica a los hijos del control). |
| `padding-left` / `padding-top` / `padding-right` / `padding-bottom` | number | `0` | Override de `padding` por lado. |
| `align` | `"start"` \| `"center"` \| `"end"` \| `"stretch"` | `"stretch"` | Alineación en el eje cruzado cuando el padre es `row` o `column`. |
| `align-h` / `align-v` | `"start"` \| `"center"` \| `"end"` \| `"stretch"` | `"stretch"` | Alineación cuando el padre es `stack`. |
| `gap` (alias de `spacing`) | number | `0` | Separación entre hijos. Se define en el contenedor padre (`row`/`column`), no en el hijo. |

## Apariencia visual

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `backgroundColor` | color (hex, sin `#`) | — | Color de fondo del nodo. Activa el relleno (`shouldFill`). |
| `borderColor` | color (hex, sin `#`) | — | Color del borde. Activa el trazo (`shouldStroke`). |
| `borderWidth` | number | `0` | Grosor del borde en px. |
| `borderRadius` | number | `0` (algunos controles traen un valor de tema por defecto, ver su ficha) | Radio de las esquinas en px. Se dibuja igual en HTML (`border-radius`) y en desktop/GDI (`RoundRect`). **Para un control completamente redondo** (círculo/píldora): usar `width == height` y `borderRadius >= width / 2` — el motor de render clampea el radio al tamaño del propio control, así que no hace falta una propiedad de "forma" separada. |
| `textColor` (alias legacy `color`) | color (hex) | — | Color del texto/contenido del nodo. |
| `class` | string | — | Clase CSS aplicada solo en el renderer web (HTML). Sin efecto en desktop/GDI — ver `docs/AVAUI_NATIVE_RENDERING_FIX_PLAN.md`. |
| `overlay` | bool | `false` | Marca el nodo como overlay (se dibuja por encima del flujo normal, ej. contenido de un `dialog`). |
| `backdrop` | bool | `false` | Dibuja un fondo semitransparente detrás del nodo (usado por `dialog`). |
| `zIndex` | number | `0` | Prioridad de apilado entre overlays. |

## Eventos

| Propiedad | Descripción |
|---|---|
| `click` | Handler genérico de click. Cualquier control puede recibirlo, no solo `button`. |
| `change` | Handler genérico de cambio de valor. Si un nodo define `click` y `change` a la vez, `click` tiene prioridad. |

Ver también la sección "Convención de sintaxis `.avaui`" en
[`docs/wiki/README.md`](../README.md) para cómo se escriben estas
propiedades y cómo funciona el auto-bind de eventos.

## Fichas de control disponibles

- [Button](./button.md)
- [TextBox](./textbox.md)
- [CheckBox](./checkbox.md)
- [RadioButton](./radiobutton.md)
- [ComboBox](./combobox.md)
- [Container](./container.md)
- [Dialog](./dialog.md)
- [Image](./image.md)
- [Text](./text.md)

---

[« Volver al índice](../README.md)
