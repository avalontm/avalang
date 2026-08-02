# Button

Botón clickeable con una etiqueta de texto. Es el control base para
disparar acciones (`click`) en una vista `.avaui`.

Fuente: `runtime/avaui/src/controls/Button.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"button"`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del botón. Necesario para el auto-bind de eventos (`On{Id}Click`). |
| `text` | string | — | Texto mostrado dentro del botón. |
| `style` | `"primary"` \| `"secondary"` | `"primary"` | Variante visual del botón. |
| `isEnabled` | bool | `true` | Si es `false`, el botón se muestra deshabilitado (opacidad reducida) y no dispara `click`. |
| `backgroundColor` | color (hex) | color de tema `buttonPrimary` | Color de fondo del botón. Se puede sobreescribir manualmente. |
| `textColor` | color (hex) | `"FFFFFF"` | Color del texto de la etiqueta. |
| `fontSize` | number | tamaño de tema `button` | Tamaño de fuente del texto. |
| `fontName` | string | fuente de tema `button` | Familia tipográfica del texto. |
| `borderWidth` | number | ancho de borde del tema | Grosor del borde del botón. |
| `borderRadius` | number | radio de borde del tema (`4`) | Radio de las esquinas del botón. Ver [Control Base](./control-base.md#apariencia-visual) para cómo hacer un botón completamente redondo. |

> El resto de las propiedades base (`id`, `width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

`borderRadius` se dibuja igual en la exportación web (HTML) y en
desktop (GDI), y también se refleja en el lienzo de Ava Studio.

## Botón redondo / píldora

Para un botón completamente redondo (círculo), usar `width` igual a
`height` y un `borderRadius` igual o mayor a la mitad del lado — el
motor clampea el radio al tamaño del botón, así que no hace falta
ninguna propiedad de "forma" adicional:

```
button
    id = "RoundButton"
    text = "+"
    width = 48
    height = 48
    borderRadius = 24
end
```

Para un botón tipo "píldora" (rectangular con extremos redondeados),
alcanza con que `borderRadius` sea igual o mayor a la mitad de
`height`:

```
button
    id = "PillButton"
    text = "Suscribirse"
    width = 160
    height = 40
    borderRadius = 20
end
```

## Eventos

| Evento | Argumentos | Descripción |
|---|---|---|
| `click` | — | Se dispara cuando el usuario hace click sobre el botón (si `isEnabled = true`). |

## Ejemplo

```
view
    column
        gap = 12
        padding = 20

        button
            id = "TestButton"
            text = "Button"
            click = OnButtonClick()
        end

        button
            id = "AcceptConfirm"
            text = "Aceptar"
            backgroundColor = "DC2626"
            click = OnAcceptConfirm()
        end
    end
end

code
    func OnButtonClick()
        -- lógica al hacer click
    end

    func OnAcceptConfirm()
        -- lógica al confirmar
    end
end
```

---

[« Volver al índice](../README.md)
