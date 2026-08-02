# TextBox

Campo de entrada de texto de una sola línea. Es el primer control del
set base con input real de usuario (a diferencia de controles que solo
muestran datos).

Fuente: `runtime/avaui/src/controls/TextBox.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"textbox"`, también aplica a los alias
`"textinput"` / `"input"`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del campo. Necesario para el auto-bind de eventos (`On{Id}Change`). |
| `text` (alias `value`) | string | `""` | Valor actual del campo. Normalmente se enlaza a una variable de `state`. |
| `placeholder` | string | `""` | Texto mostrado cuando `text` está vacío. |
| `isFocused` | bool | `false` | Estado de foco del campo. Reservado — todavía no tiene efecto visual propio. |
| `isEnabled` | bool | `true` | Si es `false`, el campo no acepta edición. |
| `backgroundColor` | color (hex) | color de tema `inputBackground` | Color de fondo del campo. |
| `borderColor` | color (hex) | color de tema `inputBorder` | Color del borde del campo. |
| `borderWidth` | number | ancho de borde del tema | Grosor del borde del campo. |
| `fontSize` | number | tamaño de fuente `body` del tema | Tamaño de fuente del texto ingresado. |

> El resto de las propiedades base (`id`, `width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

## Eventos

| Evento | Argumentos | Descripción |
|---|---|---|
| `change` | — | Se dispara cuando el valor del campo cambia. El nuevo valor queda reflejado en la variable de `state` enlazada a `text`. |

## Ejemplo

```
state
    textValue = ""
    inputChanges = 0
end

view
    column
        gap = 12
        padding = 20

        textbox
            id = "TestTextBox"
            text = textValue
            placeholder = "TextBox"
            change = OnTextChange()
        end

        text
            text = "inputChanges: " + inputChanges
        end
    end
end

code
    func OnTextChange()
        inputChanges++
    end
end
```

---

[« Volver al índice](../README.md)
