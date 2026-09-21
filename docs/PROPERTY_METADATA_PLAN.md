# Plan: Metadatos dinámicos de propiedades por control (Properties panel)

Basado en lectura directa del código fuente actual (no de docs, que pueden estar
desactualizados). Fuentes revisadas:

- `runtime/avaui/src/registry/ComponentTypeRegistry.{h,cpp}` — registro real de tipos.
- `runtime/avaui/src/controls/*.cpp` (Button, TextBox, CheckBox, RadioButton, ComboBox,
  Link, Text, Image, Container, Dialog, Icon, ScrollView) — cada control se
  auto-registra con `RegisterComponentType(...)` en un `static` init.
- `runtime/avaui/src/components/PropertyValue.h` — tipos de runtime de un valor.
- `runtime/avastudio/src/designer/component_metadata.h` — `ComponentMetadata` /
  `PropertyMetadata` (ya bastante completos como *forma*, pero no se llenan bien).
- `runtime/avastudio/src/designer/component_registry.cpp` — construye
  `ComponentMetadata` a partir del registro de `avaui`.
- `runtime/avastudio/src/panels/properties_panel.cpp` — UI del panel, ya sabe
  dibujar editores de Color/Boolean/Number/Enum/String según `PropertyEditorKind`.

## Diagnóstico (por qué "no detecta dinámicamente")

1. **La fuente de verdad es demasiado pobre.** Cada control se registra así
   (ej. `CheckBox.cpp`):
   ```cpp
   RegisterComponentType({
       "CheckBox", "CheckBox", false,
       {
           {"label", PropertyValue("CheckBox")},
           {"isChecked", PropertyValue(false)},
           {"isEnabled", PropertyValue(true)},
       },
   });
   ```
   `PropertyDefault` solo tiene `name` + `PropertyValue` (y `PropertyValue::Type()`
   solo distingue `Nil/Bool/Number/String/List`). No hay forma de declarar que
   `isChecked` es un booleano *de comportamiento* (Behavior) en vez de Appearance,
   que un futuro `iconPosition` es un enum `(top|bottom|left|right)`, que
   `maxLength` es un número con mínimo 0, ni que `source` de `image` debe abrir un
   selector de archivos.

2. **`component_registry.cpp` adivina por el *nombre* de la propiedad**, no por su
   semántica real (`EditorKindFor`, `CategoryFor` en ese archivo hacen
   `lowered.find("color")`, `lowered.find("text")`, etc.). Esto es fragil:
   - Cualquier propiedad nueva que no calce con esos patrones cae en
     `PropertyEditorKind::String` / `PropertyCategory::Advanced` por defecto,
     aunque sea un booleano o un número.
   - Dos controles con la misma palabra en el nombre pero distinto significado
     colisionan (ej. `"group"` siempre es Behavior, aunque en otro control sea
     Identity).

3. **El editor `Enum` existe en la UI pero nadie lo alimenta.** `PropertyMetadata`
   tiene `validation`, y `properties_panel.cpp` ya sabe extraer alternativas de un
   regex tipo `(a|b|c)` (`ExtractRegexAlternatives`) para pintar un combo — pero
   **ningún control setea `validation` ni `designerEditor = Enum` hoy**, así que ese
   camino está muerto en la práctica.

4. **No hay forma de declarar `min/max`, `unit`, `multiline`, `options` con
   value+label (como `ComboBoxItem`), `readOnly`, `styleable`, `animatable`,
   `description` desde el control.** Esos campos existen en `PropertyMetadata`
   pero se quedan con su valor por defecto porque `BuildMetadata()` nunca los
   toca (salvo `category`/`designerEditor` heurísticos).

**Conclusión:** la solución no es parchear más heurísticas de texto — es mover la
declaración del tipo/editor/validación **al punto de registro de cada control**
(la fuente de verdad real), y hacer que `ComponentRegistry` la *consuma* en vez de
adivinarla. Las heurísticas actuales se mantienen solo como *fallback* para no
romper controles que aún no se migren.

## Diseño propuesto

### 1. Nuevo `PropertyDescriptor` en `avaui` (reemplaza/extiende `PropertyDefault`)

Archivo: `runtime/avaui/src/registry/ComponentTypeRegistry.h`

```cpp
enum class PropertyKind {
    Auto,       // infiere desde PropertyValue::Type() (compat con lo actual)
    Text,
    MultilineText,
    Number,
    Boolean,
    Color,
    Enum,       // requiere `options`
    Resource,   // archivo/imagen/ícono -> abre selector
    Binding,    // referencia a estado/expresión avalang
    Event,      // nombre de handler
};

enum class PropertyGroup {
    Identity, Layout, Appearance, Typography, Behavior, Accessibility, Events, Advanced,
};

struct PropertyOption {
    std::string value;
    std::string label;      // para mostrar en el combo (i18n-friendly)
};

struct PropertyDescriptor {
    std::string name;
    PropertyValue defaultValue;

    PropertyKind kind = PropertyKind::Auto;
    PropertyGroup group = PropertyGroup::Advanced;
    std::string description;                 // tooltip
    std::vector<PropertyOption> options;      // solo si kind == Enum
    double minValue = 0.0, maxValue = 0.0;    // solo si kind == Number y hasRange
    bool hasRange = false;
    std::string unit;                         // "px", "%", "" ...
    bool readOnly = false;
    bool styleable = true;
    bool animatable = false;
};
```

`PropertyDefault` (nombre actual) queda como alias/typedef de compatibilidad, o se
agrega un constructor implícito `PropertyDescriptor(std::string name, PropertyValue)`
para que **los controles que no se migren sigan compilando sin cambios** (kind
queda en `Auto` y se comporta igual que hoy, vía heurística de fallback).

### 2. Migrar `ComponentTypeDescriptor::default_properties`

Mismo archivo. `default_properties` pasa de
`std::vector<PropertyDefault>` a `std::vector<PropertyDescriptor>`. Como el punto 1
mantiene compatibilidad binaria/fuente para el caso `Auto`, esto no rompe a los
controles no migrados — solo habilita que los migrados declaren más.

### 3. `component_registry.cpp` (AvaStudio) consume el descriptor en vez de adivinar

```cpp
PropertyMetadata BuildProperty(const registry::PropertyDescriptor& src) {
    PropertyMetadata p;
    p.name = src.name;
    p.type = PropertyTypeName(src.defaultValue.Type());
    p.defaultValue = FormatPropertyValue(src.defaultValue);
    p.description = src.description;
    p.readOnly = src.readOnly;
    p.styleable = src.styleable;
    p.animatable = src.animatable;

    if (src.kind == registry::PropertyKind::Auto) {
        // fallback: heurística actual (EditorKindFor/CategoryFor por nombre)
        p.category = CategoryFor(src.name);
        p.designerEditor = EditorKindFor(src.name, src.defaultValue.Type());
    } else {
        p.category = ToDesignerCategory(src.group);      // mapeo 1:1
        p.designerEditor = ToEditorKind(src.kind);        // mapeo 1:1
        if (src.kind == registry::PropertyKind::Enum) {
            p.validation = BuildOptionsValidation(src.options); // "(a|b|c)"
            // (ver punto 4: mejor aún, reemplazar el regex-hack)
        }
    }
    return p;
}
```

Esto invierte la lógica: **el control dice qué es su propiedad; el registry solo
traduce**, no infiere. Las heurísticas actuales quedan como red de seguridad para
todo lo no migrado (cero regresiones durante la transición).

### 4. Reemplazar el hack de regex por `options` reales

`PropertyMetadata` gana un campo nuevo:
```cpp
std::vector<std::pair<std::string,std::string>> enumOptions; // value,label
```
`DrawEnumRow` en `properties_panel.cpp` deja de llamar
`ExtractRegexAlternatives(row.metadata.validation)` y usa `row.metadata.enumOptions`
directamente. `validation` vuelve a ser solo para reglas de validación reales
(regex de formato, ej. un `href` con esquema `https?://...`), no para listar
opciones — hoy están mezclados y eso es confuso.

### 5. Editor de `Resource` con selector real

`PropertyEditorKind::Resource` ya existe pero hoy cae en `DrawTextRow` (texto
plano). Con el `kind` explícito por control (ej. `image.source`, `link.icon`) se
puede:
- Añadir un botón "…" que abra `asset_browser_panel` filtrado por extensión.
- Detectar automáticamente `image`/`icon` sin heurística de nombre.

### 6. Rango numérico y unidades

`PropertyEditorKind::Number/Dimension` con `hasRange` true → `DrawNumberRow` usa
`ImGui::SliderScalar`/clamp con `minValue/maxValue`, y `unit` se pinta como sufijo
(ej. `"%.0f px"`).

## UI del panel Properties — ajustes fuera de la migración de metadata

Estos dos ítems no son parte de la migración control-por-control, pero surgieron
al revisar el panel ya funcionando con metadata real y se resolvieron en el
mismo lote:

1. **Editor por tipo — ya funcionaba, confirmado.** `DrawGridRowEditor` en
   `properties_panel.cpp` ya despachaba por `PropertyEditorKind`: `Boolean` →
   checkbox, `Number`/`Dimension` → slider si `hasRange` o input numérico si
   no, `Color` → color picker, `Enum` → combo, el resto → texto. No hacía
   falta tocar nada aquí; era exactamente lo que pedías, y ya se ve así en el
   panel (ej. `isFocused`/`isEnabled` como checkbox, `text`/`placeholder`
   como texto).
2. **Combo "Tipo" eliminado.** Se quitó el `BeginCombo("##type_combo", ...)`
   de `properties_panel.cpp` (permitía cambiar el tipo del componente desde
   el panel de Properties). Queda solo el campo `Id`. El `PropertyEditKind::kType`
   y su manejo en `main.cpp` quedan sin uso pero no se tocaron (cero riesgo).
3. **Bug encontrado y corregido: doble sección "Avanzado".** `BuildDesignerPropertyGrid`
   en `designer_canvas.cpp` agregaba *siempre* una sección `Advanced` nueva
   para propiedades del nodo que no están en la metadata declarada del tipo
   (ej. overrides de estilo guardados en el nodo como `backgroundColor`/
   `borderColor`/`borderWidth`), sin revisar si el control ya traía su propia
   sección `Advanced` desde su metadata (como `textbox` con `caretIndex`,
   etc.). Resultado: dos encabezados "Avanzado" separados en el mismo panel.
   Ahora se fusionan en una sola sección si ya existe una `Advanced`.

### 7. Metadata genérica compartida entre controles (`CommonInteractiveProperties`)

Los 4 controles interactivos migrados (`Button`, `TextBox`, `CheckBox`,
`RadioButton`) repetían el mismo `PropertyDescriptor` para `isEnabled`
(mismo `kind`, `group` y casi la misma descripción) en cada `.cpp`. Se
extrajo a un bloque genérico reutilizable en
`avaui/src/registry/ComponentTypeRegistry.{h,cpp}`:

```cpp
// Genérico — una sola definición, sin duplicar en cada control.
AVA_UI_API std::vector<PropertyDescriptor> CommonInteractiveProperties();

// Combina genérico + específico del control ("el plus de cada uno").
AVA_UI_API std::vector<PropertyDescriptor> WithCommonProperties(
    std::vector<PropertyDescriptor> common, std::vector<PropertyDescriptor> specific);
```

Cada control ahora se registra así (control genérico + su plus específico):

```cpp
RegisterComponentType({
    "Button", "Button", false,
    WithCommonProperties(CommonInteractiveProperties(), {
        PropertyDescriptor{.name = "text", ...},
        PropertyDescriptor{.name = "style", ...},
    }),
});
```

`CommonInteractiveProperties()` hoy solo trae `isEnabled` porque es la
**única** propiedad que de verdad se repetía igual en los 4 controles ya
migrados (y en `combobox`, según la tabla, cuando se migre). No se agregó
nada más al bloque genérico por inventar generalidad: `text` no calificó
porque significa algo distinto en cada control (label del botón vs. valor
actual del textbox vs. label de checkbox/radiobutton) y con default distinto
en cada uno, así que queda como parte del "plus" específico de cada control.

Si en el futuro dos o más controles vuelven a repetir la misma propiedad con
el mismo significado, se agrega una función `CommonXxxProperties()` nueva
(no se sobrecarga `CommonInteractiveProperties()` con cosas no relacionadas)
y se combina igual con `WithCommonProperties`.

**Nota de orden:** `WithCommonProperties` antepone el bloque genérico al
específico. Esto solo afecta el orden de filas *dentro* de una misma
categoría en el panel (las categorías igual se agrupan aparte); no cambia
comportamiento ni compatibilidad — `isEnabled` sigue siendo Behavior en
todos los casos.

## Migración control por control

Cada control se migra tocando **solo** su `RegisterComponentType(...)` (un archivo
`.cpp` en `avaui/src/controls/`), sin tocar UI. Se entrega igual que con
`CONTROLS.md`: un control a la vez, con zip descargable, marcando ✅ aquí.

> Nota: la primera versión de esta tabla asumía propiedades que en realidad no
> están registradas hoy (p.ej. `text.fontSize`/`text.color` no existen como
> `default_properties` — el grid de propiedades de un nodo muestra **solo** lo
> que el control registró). La tabla de abajo se corrigió contra el código
> fuente real de cada `controls/*.cpp`.

| # | Control | Archivo fuente | Propiedades registradas realmente | Estado |
|---|---------|----------------|-------------------------------------|:---:|
| 1 | `text` (Label) | `Text.cpp` | `text` | ✅ |
| 2 | `image` | `Image.cpp` | `source`, `alt` | ✅ |
| 3 | `link` | `Link.cpp` | `text`, `href` | ✅ |
| 4 | `button` | `Button.cpp` | `text`, `isEnabled`, `style` | ✅ |
| 5 | `textbox` | `TextBox.cpp` | `text`, `placeholder`, `isFocused`, `isEnabled`, `caretIndex`, `selectionAnchor`, `imeComposition`, `imeCompositionCursor` | ✅ |
| 6 | `checkbox` | `CheckBox.cpp` | `label`, `isChecked`, `isEnabled` | ✅ |
| 7 | `radiobutton` | `RadioButton.cpp` | `label`, `group`, `isSelected`, `isEnabled` | ✅ |
| 8 | `combobox` | `ComboBox.cpp` | `selectedValue`, `isEnabled`, `isOpen` | ⬜ |
| 9 | `grid` (única de la familia Container con props) | `Container.cpp` | `columns`, `rows` | ✅ |
| 10 | `dialog` | `Dialog.cpp` | `title`, `isOpen`, `dismissible` | ⬜ |
| 11 | `icon` | `Icon.cpp` | `source` | ⬜ |
| 12 | `scrollview` / `listview` | `ScrollView.cpp` | `scrollOffsetX`, `scrollOffsetY` (gestionadas en runtime, se marcan `readOnly`) | ⬜ |

`container`/`column`/`row`/`stack`/`page`/`flex` se registran sin
`default_properties` (`RegisterComponentType({"Container", "Container", true, {}})`),
así que hoy no tienen nada que migrar en el panel de Properties — quedan fuera
de esta tabla hasta que se les agregue alguna propiedad real (ej. `gap`,
`align`).

## Fases de entrega

1. ✅ **Fase 1 — Infraestructura** (implementada, sin migrar ningún control todavía):
   - `PropertyKind`/`PropertyGroup`/`PropertyOption`/`PropertyDescriptor` añadidos
     en `avaui/src/registry/ComponentTypeRegistry.h` (`PropertyDefault` queda como
     alias). `PropertyDescriptor` sigue siendo *aggregate* (sin constructores), así
     que los ~12 `RegisterComponentType({...})` existentes compilan sin tocarlos:
     solo llenan `name`/`defaultValue` y el resto usa `kind = Auto` por defecto.
   - `component_registry.cpp`: `BuildProperty()` ahora separa dos caminos —
     `kind == Auto` usa la heurística histórica (`CategoryFor`/`EditorKindFor`,
     sin cambios de comportamiento); `kind != Auto` traduce 1:1 el
     `PropertyGroup`→`PropertyCategory` y `PropertyKind`→`PropertyEditorKind`
     declarados por el control, más `description`/`readOnly`/`styleable`/
     `animatable`/`options`/rango-unidad.
   - `PropertyMetadata` (avastudio) ganó `enumOptions` (pares value/label) y
     `hasRange`/`minValue`/`maxValue`/`unit`.
   - `properties_panel.cpp`: `DrawEnumRow` ahora prioriza `enumOptions`
     explícito y muestra `label` en el combo (con `value` real por debajo);
     el regex-hack sobre `validation` queda solo como fallback para lo no
     migrado. `DrawNumberRow` usa `SliderScalar` con clamp cuando el control
     declaró `hasRange`, con la unidad como sufijo del formato.
   - **Riesgo/compatibilidad:** cero cambios de comportamiento para los 12
     controles existentes (todos siguen en `kind = Auto`); nada del pipeline de
     render se tocó.
2. **Fase 2 — Migración control por control** (tabla arriba, uno a la vez, con zip
   cada vez — mismo flujo que con `CONTROLS.md`).
3. **Fase 3 — Extras**: selector de recursos real para `Resource`. (Se
   descarta el punto "exponer `combobox` en el catálogo del Toolbox": ya
   estaba expuesto — ver corrección en `CONTROLS.md`, el catálogo recorre
   `GetComponentTypeRegistry()` completo, no el CSV.)

## Compatibilidad y riesgo

- Cambio de `PropertyDefault` → `PropertyDescriptor` es la única modificación de
  tipo compartido; se resuelve con un constructor implícito
  `PropertyDescriptor(std::string, PropertyValue)` para no tocar los ~12 sitios de
  `RegisterComponentType` que no se migren en la Fase 1.
- El fallback heurístico (`EditorKindFor`/`CategoryFor`) **no se borra** — sigue
  siendo el comportamiento para todo lo no migrado, así el build nunca queda roto
  a medias.
- Nada de esto toca el pipeline de render (`panels/designer_canvas.cpp`,
  función `DrawRealWidget`) ni el motor `avalang` — es exclusivamente
  AvaStudio (metadata + UI del panel).

---
_Plan basado en inspección directa de `runtime/avaui/src/registry`,
`runtime/avaui/src/controls/*.cpp` y `runtime/avastudio/src/designer` /
`src/panels/properties_panel.cpp` del zip actual — no se asumió nada de docs
existentes._
_Última actualización: Fase 1 (infraestructura) completa. Fase 2: controles
`text` (#1), `image` (#2), `link` (#3), `button` (#4), `textbox` (#5),
`checkbox` (#6), `radiobutton` (#7) y `grid` (#9) migrados a metadata
declarativa. Corrección: el catálogo del Toolbox nunca estuvo limitado a
14 tipos — recorre los 19 registrados en el runtime (ver `CONTROLS.md`).
Restan sin migrar a metadata declarativa: `combobox`, `dialog`, `icon` y
`scrollview`/`listview`, que además ya tienen render real en Design
(`combobox`, `icon`) o son contenedores que ya se dibujaban como tales
(`scrollview`, `listview`), o se muestran fuera del canvas por diseño
(`dialog`, en la bandeja de diálogos)._

### Nota de la migración #9 (`grid`)

`columns`/`rows` se declararon `PropertyKind::Number` / `PropertyGroup::Layout`
con `hasRange` real, en vez de dejarlos en `Auto` (que hoy los mostraría como
campo numérico simple sin límites). Los rangos se sacaron de cómo
`LayoutEngineImpl.cpp` los consume, no de una suposición:
- `columns`: `hasRange(1, 24)` — el motor de layout hace
  `std::max(1, columns)`, así que 0 o negativo no tiene efecto real; el panel
  ahora lo refleja con un slider que no permite bajar de 1.
- `rows`: `hasRange(0, 24)` — a diferencia de `columns`, `0` es un valor
  válido con significado propio ("automático": el motor calcula las filas
  necesarias a partir de la cantidad de hijos y de `columns`), así que el
  mínimo del slider se dejó en 0 en vez de 1, y se documentó ese
  comportamiento en la descripción de la propiedad.
El límite superior (24) es arbitrario — no hay un máximo real en el motor —
elegido solo para que el slider tenga un rango utilizable; se puede ajustar
si en la práctica se necesitan grillas más grandes.

### Notas de las migraciones #4–#7

- **`button.style`**: se dejó como `PropertyKind::Text` (no `Enum`). Revisando
  `RenderTheme::ApplyToComponent`, `style` no es un enum cerrado: es una lista
  de tokens de clase separados por espacio que se resuelven contra
  `ProjectStyleSheet` (nombres definidos por el usuario) y, si no calzan, se
  intentan como clase `tipo.token`. Declararlo como `Enum` con solo
  `primary`/`secondary` habría sido incorrecto — el proyecto puede definir
  cualquier clase propia.
- **`textbox`**: `caretIndex`, `selectionAnchor`, `imeComposition` e
  `imeCompositionCursor` se marcaron `readOnly = true` y `group = Advanced`:
  son estado interno que gestiona `TextBoxEditingController` en tiempo de
  ejecución (foco de teclado real), no algo con sentido de preconfigurar desde
  el panel Design (que es una vista estática, según se documentó al
  implementar `OnDrawInput`). `text`/`placeholder`/`isFocused`/`isEnabled`
  quedan editables como antes.
- **`checkbox`/`radiobutton`**: migración directa, sin decisiones especiales;
  `group` en `radiobutton` se documentó explícitamente como el campo que
  vincula las opciones mutuamente excluyentes.

### Nota sobre `link.href` (migración #3)

Antes de la migración, `href` caía en la heurística `EditorKindFor` genérica
(coincide con `source`/`href`/`icon` → `PropertyEditorKind::Resource`), lo cual
abriría un selector de archivos para una URL — semánticamente incorrecto (un
`href` es texto libre: URL absoluta, ruta relativa o ancla `#id`, no un archivo
local). Con metadata explícita se corrigió a `PropertyKind::Text` /
`PropertyGroup::Behavior`, así el editor es un campo de texto simple y la
categoría sigue siendo Behavior (igual que antes, ahí no había regresión).
Este es exactamente el tipo de corrección que la Fase 1 buscaba habilitar sin
tocar el pipeline de render.

## Popup de "nueva clave" en el panel Properties

El campo "nueva clave" del panel Properties (`properties_panel.cpp`) es un
autocompletado: al hacer clic o escribir abre un popup con las propiedades
que se pueden agregar al control seleccionado, filtradas por lo tipeado
(prefijo primero, luego coincidencia parcial). Navegación con ↑/↓, Enter,
clic y Escape.

Origen de las opciones (`designer/property_catalog.cpp`,
`ListAddableProperties`):

- Filas del grid que el control declara y que todavía no están definidas
  localmente en el documento (excluye `Identity`, `Events`, `readOnly` y
  editores `Event`). Se confirman como `PropertyEditKind::kValue`, porque
  `kAddProperty` se ignora si el nodo ya tiene la propiedad (guarda
  `HasProperty` en `main.cpp`).
- Catálogo de propiedades comunes que el runtime lee para cualquier control
  (`width`, `height`, `margin`, `padding`, `zIndex`, `grow`, `align`,
  `backgroundColor`, `borderColor`, `borderWidth`, `borderRadius`,
  `textColor`, `fontSize`, `fontName`) y, solo para contenedores, `padding`,
  `spacing`, `direction` y `wrap`. Se confirman como `kAddProperty` con el valor inicial
  del catálogo.
- Una entrada final "Agregar personalizada" para claves libres sin espacios
  que no existan ya en el grid.

`gap` no está en el catálogo: `SetPropertyWithAlias` lo trata como alias de
`spacing`. `justify` tampoco, porque el motor de layout no lo lee.

Las propiedades no declaradas por el control que sí están en el catálogo
(por ejemplo `spacing` o `padding` en un `stack`) ya no caen siempre en
Avanzado como texto: `BuildDesignerPropertyGrid` las inserta con
`InsertGridRow` en su categoría y con su editor (color, número, booleano,
combo). Las que no están en el catálogo siguen en Avanzado como texto.

### Tipado de valores al editar (`ResolveTypedPropertyValue`)

El motor de layout solo lee `width`, `height`, `margin`, `padding`,
`spacing` y similares cuando el valor es `PropertyType::Number` (`grow` y
`wrap` cuando es `Bool`). El panel Properties entrega siempre strings, así
que `ExecuteSetProperty` los convierte antes de escribirlos en el nodo:

1. Tipo declarado por el control en el registry (`supportedProperties`).
2. Tipo del catálogo de propiedades comunes (incluye `margin-left`,
   `padding-top`, etc. a través de su propiedad base).
3. Tipo del valor que el nodo ya tiene.
4. String.

Si el texto no se puede parsear al tipo esperado (por ejemplo `abc` en un
número) se guarda como string, igual que antes. `padding` solo lo lee el
layout de contenedores (`Row`, `Column`, `Stack`, `Grid`, ...); en
`Button`, `Text`, `TextBox` etc. el runtime usa un relleno interno fijo, por
eso el catálogo lo ofrece solo para contenedores.
