# Dialog

Ventana modal con overlay y fondo semitransparente (backdrop). Cuando
`isOpen = false`, el diálogo no dibuja nada ni decompone a sus hijos
— desaparece por completo del árbol renderizado, no solo visualmente.

Fuente: `runtime/avaui/src/controls/Dialog.h/.cpp`,
valores de tema en `runtime/avaui/src/theme/RenderTheme.cpp`
(`ApplyTypeDefaults`, tipo `"dialog"`), render en
`runtime/avaui/src/render_tree/RenderTree.cpp` (`DecomposeDialog`).

## Propiedades

| Propiedad | Tipo | Por defecto | Descripción |
|---|---|---|---|
| `id` | string | — | Identificador del diálogo. |
| `title` | string | `""` | Título del diálogo. |
| `isOpen` | bool | `false` | Controla si el diálogo se dibuja. Con `false`, ni el overlay/backdrop ni los hijos se renderizan. Normalmente se enlaza a una variable de `state`. |
| `dismissible` | bool | `true` | Reservado para cerrar haciendo click fuera del diálogo — ese wiring de interactividad todavía no está implementado; hoy no tiene efecto visual/funcional propio. |
| `overlay` | bool | `true` (relleno por el tema para `dialog`) | Ver [Control Base](./control-base.md#apariencia-visual) — marca el nodo para dibujarse por encima del flujo normal. |
| `backdrop` | bool | `true` (relleno por el tema para `dialog`) | Ver [Control Base](./control-base.md#apariencia-visual) — dibuja un fondo semitransparente detrás del diálogo. |
| `backgroundColor` | color (hex) | color de tema `surface` (`F3F3F3`); si sigue vacío al abrir, cae a blanco (`#ffffff`) | Color de fondo del diálogo. |
| `borderColor` | color (hex) | color de tema `border` (`CCCCCC`) | Color del borde del diálogo. |
| `borderWidth` | number | ancho de borde del tema (`1`) | Grosor del borde del diálogo. |
| `borderRadius` | number | radio de borde del tema (`4`); si el tema no llegó a aplicarse, cae a `8` como resguardo interno | Radio de las esquinas del diálogo. |

> El resto de las propiedades base (`width`, `height`, `margin`,
> `padding`, `align`, `class`, `zIndex`, etc.) también aplican — ver
> [Control Base](./control-base.md).

**Cerrar el diálogo no es un evento propio**: el patrón estándar es un
botón "Cerrar"/"Cancelar" dentro de los hijos del diálogo, con su
propio `click` que pone `isOpen = false` en el estado de la app, igual
que cualquier otro botón.

## Eventos

Dialog no expone eventos propios (`click`/`change` funcionan igual que
en cualquier control, ver [Control Base](./control-base.md#eventos),
pero no tienen manejo especial aquí — el control de apertura/cierre se
hace enteramente vía `isOpen`).

## Ejemplo

```
state
    showDialog = false
    confirmResult = "(sin acción todavía)"
end

view
    button
        id = "OpenDialog"
        text = "Eliminar elemento"
        click = OnOpenDialog()
    end

    dialog
        id = "ConfirmDialog"
        title = "Confirmar acción"
        isOpen = showDialog
        dismissible = false

        column
            gap = 16
            padding = 24

            text
                text = "Esta acción no se puede deshacer. ¿Deseas continuar?"
            end

            row
                gap = 12

                button
                    id = "CancelConfirm"
                    text = "Cancelar"
                    click = OnCancelConfirm()
                end

                button
                    id = "AcceptConfirm"
                    text = "Aceptar"
                    backgroundColor = "DC2626"
                    click = OnAcceptConfirm()
                end
            end
        end
    end
end

code
    func OnOpenDialog()
        showDialog = true
    end

    func OnAcceptConfirm()
        confirmResult = "Elemento eliminado"
        showDialog = false
    end

    func OnCancelConfirm()
        showDialog = false
    end
end
```

---

[« Volver al índice](../README.md)
