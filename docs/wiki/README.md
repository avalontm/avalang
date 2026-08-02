# Wiki de AvaUI

Esta wiki documenta los controles disponibles para construir vistas
`.avaui` (el formato de UI de AvaLang, ver
`docs/architecture/17_AVAUI_FILE_FORMAT.md` para el detalle del
formato de archivo completo).

Por cada control hay una ficha con:

- qué propiedades acepta y qué valor toman por defecto,
- qué eventos expone,
- un ejemplo mínimo de sintaxis `.avaui` real.

## Índice de controles

| Control | Descripción |
|---|---|
| [Control Base](./controls/control-base.md) | Propiedades comunes a **todos** los controles (layout, apariencia, eventos). Leer primero. |
| [Button](./controls/button.md) | Botón clickeable, incluye cómo hacerlo redondo/píldora. |
| [TextBox](./controls/textbox.md) | Campo de entrada de texto de una línea. |
| [CheckBox](./controls/checkbox.md) | Casilla de verificación con etiqueta opcional. |
| [RadioButton](./controls/radiobutton.md) | Botón de opción circular, mutuamente excluyente por `group`. |
| [ComboBox](./controls/combobox.md) | Selector desplegable de una opción, con hijos `option`. |
| [Container](./controls/container.md) | Familia `container`/`row`/`column`/`stack` — organiza hijos. |
| [Dialog](./controls/dialog.md) | Ventana modal con overlay y backdrop. |
| [Image](./controls/image.md) | Muestra una imagen, incluye cómo hacerla circular (avatar). |
| [Text](./controls/text.md) | Etiqueta de texto plano (incluye el alias `label`). |

Con esto quedan documentados todos los controles existentes en
`runtime/avaui/src/controls/`. `ScrollView`/`Icon` comparten
render/tema con controles ya documentados pero todavía no tienen ficha
propia — se agregan siguiendo la misma plantilla si hace falta.

## Estructura

```
docs/wiki/
  README.md                 <- este archivo (índice)
  controls/
    control-base.md         <- propiedades comunes a todos los controles
    button.md
    textbox.md
    ...
```

Cada control tiene **un archivo `.md` propio** dentro de
`docs/wiki/controls/`, nombrado en minúsculas igual que el tipo tal
como se escribe en un archivo `.avaui` (ej. `button` -> `button.md`,
`textbox` -> `textbox.md`).

## Convención de sintaxis `.avaui`

Un control se declara dentro del bloque `view` como una palabra clave
en minúsculas, seguida de sus propiedades (`propiedad = valor`) y
cerrado con `end`:

```
view
    column
        gap = 16
        padding = 20

        button
            id = "Guardar"
            text = "Guardar"
            click = OnGuardarClick()
        end
    end
end
```

Reglas generales que aplican a **todos** los controles:

- **`id`**: identificador único del componente dentro de la vista. No
  es obligatorio, pero es necesario para el *auto-bind* de eventos y
  para referenciarlo desde pruebas o desde el Designer.
- **Valores string**: van entre comillas dobles (`text = "Guardar"`).
- **Valores numéricos y booleanos**: sin comillas (`fontSize = 32`,
  `isEnabled = true`).
- **Bindings de estado**: se puede asignar directamente una variable
  declarada en el bloque `state` (`text = textValue`), o una expresión
  simple con concatenación (`text = "Total: " + total`).
- **Eventos**: se asignan como `evento = NombreFuncion()`, donde
  `NombreFuncion` está definida en el bloque `code`. Si el control
  tiene `id` y existe una función `On{Id}{Evento}` (PascalCase), se
  enlaza automáticamente sin necesidad de escribir el evento de forma
  explícita — un evento explícito siempre tiene prioridad sobre el
  auto-bind. Ver `runtime/avaui/docs/17_AVAUI_FILE_FORMAT.md` sección
  "Automatic Event Binding".

Además de las propiedades propias de cada control, cualquier control
dentro de un `view` acepta un conjunto de propiedades genéricas
(layout, apariencia visual, eventos) resueltas por el motor antes de
llegar al código propio del control — documentadas una sola vez en
[Control Base](./controls/control-base.md) en vez de repetirse en cada
ficha.

## Plantilla para documentar un control nuevo

Cada archivo dentro de `controls/` sigue esta estructura:

1. **Título y descripción breve** de qué es y para qué se usa el
   control.
2. **Tabla de propiedades** propias del control (nombre, tipo, valor
   por defecto, descripción).
3. **Tabla de eventos** que expone (si aplica).
4. **Ejemplo** de sintaxis `.avaui` real, tomado o adaptado de un caso
   de uso existente en `samples/`.

Al agregar un control nuevo, también sumarlo a la tabla del
[Índice de controles](#índice-de-controles) arriba.

Fuente de verdad usada para documentar cada control:

- Definición y valores por defecto: `runtime/avaui/src/controls/<Control>.h/.cpp`
- Propiedades rellenadas por el tema visual: `runtime/avaui/src/theme/RenderTheme.cpp`
- Ejemplo de sintaxis real: `samples/web/testproj/routes/controls.avaui`
- Propiedades base (comunes a todos los controles): layout en
  `runtime/avaui/src/layout/LayoutProperties.h`, apariencia/eventos
  genéricos en `runtime/avaui/src/render_tree/RenderTree.cpp`
  (`RenderTree::BuildComponent`, antes del `Decompose*` propio de cada
  tipo).
