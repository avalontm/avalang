# Cómo agregar propiedades bindeables a estado en avaui

Guía para futuras implementaciones en `runtime/avaui/src/render_tree/RenderTree.cpp`
(y archivos relacionados). Basada en el bug real de `Dialog.isOpen`, que nunca
abría cuando estaba ligado a una variable de estado (`isOpen = showDialog`).

## El problema de fondo

`AvauiParser.cpp` (`InferValue`) solo reconoce como tipo concreto:

- `"texto entre comillas"` → `PropertyType::String`
- `true` / `false` → `PropertyType::Bool`
- un número → `PropertyType::Number`

**Cualquier otra cosa** — incluido un identificador suelto como `showDialog`,
`miVariable`, `contador` — se guarda como `PropertyType::String` con el texto
literal tal cual, sin evaluar. El parser no conoce el estado en tiempo de
parseo, así que no puede resolverlo ahí.

Si el código que lee esa propiedad exige un tipo estricto:

```cpp
// ❌ MAL — solo funciona con isOpen = true/false literal
if (const auto* open = comp->GetProperty("isOpen")) {
    if (open->Type() == PropertyType::Bool) {
        isOpen = open->AsBool();
    }
}
```

...entonces `isOpen = showDialog` **nunca** entra al `if`, porque el tipo es
`String`, no `Bool`. El valor se queda en su default para siempre, sin
importar cómo cambie el estado. Esto es fácil de introducir sin darse cuenta
porque compila bien y no tira ningún error — simplemente no reacciona.

## La regla

Toda propiedad **booleana** o **numérica** que un componente lea en
`RenderTree.cpp` debe resolverse con los helpers `EvalBool` / `EvalNumber`
(definidos en `RenderTree.h` / `RenderTree.cpp`), **nunca** comparando
`Type() == PropertyType::Bool` o `Type() == PropertyType::Number` a mano.

```cpp
// ✅ BIEN — funciona con literal true/false Y con isOpen = showDialog
bool isOpen = EvalBool(comp, "isOpen", /*default=*/false);
```

```cpp
// ✅ BIEN — funciona con literal 18 Y con fontSize = miVariable
int fontSize = static_cast<int>(EvalNumber(comp, "fontSize", parent->FontSize()));
```

Si la propiedad es opcional y no querés pisar un valor ya seteado (por
ejemplo por el theme) cuando no está presente en el `.avaui`, guardá la
llamada detrás de un chequeo de existencia, usando el valor actual como
default:

```cpp
if (component->GetProperty("overlay")) {
    renderNode->SetOverlay(EvalBool(component, "overlay", renderNode->IsOverlay()));
}
```

### Qué hacen internamente

- `EvalBool`: si la propiedad es `Bool`, la devuelve tal cual. Si es
  `String`, la pasa por `Eval()` (que resuelve el identificador contra el
  estado actual) y compara el resultado contra `"true"`. Cualquier otro caso
  (o propiedad ausente) devuelve el `defaultValue`.
- `EvalNumber`: mismo mecanismo, pero parseando el resultado de `Eval()` con
  `std::strtod`.
- `Eval(raw)` ya existía para propiedades de texto (`text`, `title`, `href`,
  `borderColor`, etc.) — internamente llama al callback `evalText_`
  (`stateBridge->EvalIdentifier`), que resuelve un identificador de estado a
  su valor actual en texto.

## Checklist para una propiedad nueva

Antes de mandar un componente/propiedad nueva a `RenderTree.cpp`:

- [ ] ¿Es un booleano? → usar `EvalBool(comp, "nombre", default)`.
- [ ] ¿Es un número? → usar `EvalNumber(comp, "nombre", default)`.
- [ ] ¿Es texto? → usar `Eval(prop->AsString())` (patrón ya existente en el
      archivo, buscar los ~20 casos actuales como referencia).
- [ ] **Nunca** comparar `Type() == PropertyType::Bool` o
      `Type() == PropertyType::Number` como única condición para leer el
      valor — eso es exactamente el bug que rompe el binding a estado.
- [ ] Si el default debe ser "lo que ya tiene el render node" (para no pisar
      un valor puesto por el theme cuando la propiedad no está presente),
      envolver la llamada en `if (comp->GetProperty("nombre")) { ... }`.

## Límite conocido: propiedades de layout

`width`, `height`, `padding`, `margin`, `spacing`/`gap` **no** están
cubiertas por este mecanismo. Se leen en `LayoutEngineImpl.cpp` (helpers
`ReadNumber` / `TryReadNumber`), una etapa que corre **antes** que
`RenderTree` y que hoy no recibe el callback de evaluación de estado en
absoluto (`LayoutEngine::Compute` no tiene acceso a `evalText_`/
`stateBridge`).

Si en algún momento se necesita `width = miVariable` o similar, hace falta:

1. Threadear un evaluador de estado (equivalente a `RenderTree::SetEvalText`)
   hasta `LayoutEngine::Compute`, típicamente desde
   `ui_pipeline_dynamic_renderer.cpp`, donde ya se crea el `stateBridge`.
2. Agregar el mismo tipo de helper (`EvalNumber`) del lado del motor de
   layout, y migrar `ReadNumber`/`TryReadNumber` para que lo usen cuando el
   `PropertyValue` sea `String`.

Esto es un cambio de interfaz más grande que agregar un helper — no algo que
se resuelva tocando una sola función.

## Componentes que ya siguen esta regla (referencia)

Todos migrados en `RenderTree.cpp`:

| Propiedad     | Componente         | Tipo   |
|---------------|---------------------|--------|
| `isOpen`      | Dialog              | Bool   |
| `isChecked`   | CheckBox            | Bool   |
| `isSelected`  | RadioButton         | Bool   |
| `disabled`    | Button              | Bool   |
| `isEnabled`   | TextBox/otros       | Bool   |
| `overlay`     | cualquier componente| Bool   |
| `backdrop`    | cualquier componente| Bool   |
| `fontSize`    | Text/Label/Link     | Number |
| `borderWidth` | cualquier componente| Number |
| `borderRadius`| cualquier componente| Number |
| `zIndex`      | cualquier componente| Number |

Usalos como ejemplo directo al agregar una propiedad nueva.
