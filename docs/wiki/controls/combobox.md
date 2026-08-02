# ComboBox

Selector desplegable de una sola opción. Sus opciones se declaran como
hijos `option` dentro del bloque `combobox`.

Fuente: `runtime/avaui/src/controls/ComboBox.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"combobox"` — mismos 4 campos que
[TextBox](./textbox.md)), render en
`runtime/avaui/src/render_tree/RenderTree.cpp` (`DecomposeComboBox`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del control. Necesario para el auto-bind de eventos (`On{Id}Change`). |
| `selectedValue` | string | `""` | `value` de la opción actualmente seleccionada. Normalmente se enlaza a una variable de `state`. |
| `isEnabled` | bool | `true` | Si es `false`, el control no responde a selección. |
| `backgroundColor` | color (hex) | color de tema `inputBackground` | Color de fondo del control. |
| `borderColor` | color (hex) | color de tema `inputBorder` | Color del borde del control. |
| `borderWidth` | number | ancho de borde del tema (`1`) | Grosor del borde del control. |
| `fontSize` | number | tamaño de fuente `body` del tema | Tamaño de fuente de las opciones. |

> El resto de las propiedades base (`width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

### Hijos `option`

Cada opción del combo se declara como un bloque `option` dentro del
`combobox`:

| Propiedad | Tipo | Descripción |
|---|---|---|
| `value` | string | Valor interno de la opción (lo que termina en `selectedValue` al elegirla). |
| `label` | string | Texto visible para el usuario. |

Una opción se considera seleccionada cuando su `value` coincide con el
`selectedValue` del `combobox` padre.

## Eventos

| Evento | Argumentos | Descripción |
|---|---|---|
| `change` | — | Se dispara cuando el usuario elige una opción distinta. El nuevo valor queda reflejado en la variable de `state` enlazada a `selectedValue`. |

## Ejemplo

```
state
    comboValue = "b"
    comboChanges = 0
end

view
    combobox
        id = "TestCombo"
        selectedValue = comboValue
        change = OnComboChange()

        option
            value = "a"
            label = "Opción A"
        end

        option
            value = "b"
            label = "Opción B"
        end

        option
            value = "c"
            label = "Opción C"
        end
    end
end

code
    func OnComboChange()
        comboChanges++
    end
end
```

---

[« Volver al índice](../README.md)
