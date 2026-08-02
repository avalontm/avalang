# AvaUI – Arquitectura de Componentes y Modelo de Ciclo de Vida

**Versión:** 1.0  
**Estado:** Especificación de Implementación  
**Autor:** Avalon TM  
**Proyecto:** AvaLang Ecosystem  
**Última actualización:** 2026-07-29

---

## Introducción

Este documento define la arquitectura de **AvaUI**, el framework de interfaz de usuario del ecosistema AvaLang.

### Objetivo Principal

Mantener una filosofía consistente con AvaLang:

- ✅ Sintaxis simple y predecible
- ✅ Basado completamente en bloques
- ✅ Sin XML ni HTML
- ✅ Sin archivos separados para código y diseño
- ✅ Fácil de aprender
- ✅ Fácil de representar visualmente en AvaStudio

AvaUI **no es** una copia de XAML, Flutter o HTML. La interfaz es una extensión natural del propio lenguaje AvaLang.

---

## Filosofía Fundamental

### Un componente es un documento

Un componente no es únicamente código. Es un **documento estructurado** donde cada bloque representa una responsabilidad específica.

```
Componente (documento .avaui)
    ├── properties    (configuración pública)
    ├── state         (estado interno)
    ├── view          (estructura visual)
    ├── code          (lógica ejecutable)
    └── style         (apariencia visual)
```

Esta estructura es utilizada por:

- Compilador AvaLang
- Runtime AvaLang
- AvaUI Framework
- AvaStudio IDE
- Flow Designer

**Todos los editores trabajan sobre exactamente el mismo componente.**

### Separación de responsabilidades

Cada bloque tiene un **único propósito**. Nunca se mezclan responsabilidades.

| Bloque | Propósito |
|--------|-----------|
| `properties` | Configuración pública, parámetros de entrada |
| `state` | Estado interno mutable del componente |
| `view` | Estructura y composición visual (declarativa) |
| `code` | Toda lógica ejecutable: funciones, eventos, ciclo de vida |
| `style` | Apariencia: colores, tamaños, fuentes, animaciones |

### Todo el código vive en un solo lugar

**No existen bloques especiales** como `events`, `handlers`, `lifecycle`, `scripts`.

Todo comportamiento ejecutable pertenece al bloque **`code`**:

```avaui
code
    func OnLoad()
        -- ciclo de vida
    end

    func OnLoginClick()
        -- manejador de evento
    end

    func ValidarCredenciales()
        -- lógica auxiliar
    end
end
```

---

## Arquitectura Canónica de Bloques

### `properties` (Bloque Canónico)

Define la **configuración pública del componente**. Representa los parámetros de entrada (inputs).

**Para páginas**, AvaHost reconoce estas propiedades para generar metadatos HTML (Open Graph, Twitter Card):

- `title` ⭐ (requerida) → `<title>`, `og:title`, `twitter:title`
- `description` → `<meta name="description">`, `og:description`
- `image` → `og:image`, `twitter:image` (URL absoluta recomendada)
- `url` → `og:url`, `<link rel="canonical">`
- `siteName` → `og:site_name`
- `ogType` → `og:type` (default: `"website"`)
- `twitterCard` → `twitter:card` (default: `"summary_large_image"` si hay `image`)

**Para componentes**, `properties` define qué parámetros acepta:

```avaui
properties
    text = "Default"
    enabled = true
    visible = User.IsAdmin
end
```

Una propiedad puede contener valores constantes, variables o expresiones. El runtime detecta automáticamente dependencias para binding reactivo.

**Nota legal:** `metadata` es un alias legacy soportado en lectura (`metadata` → se lee como `properties`), pero `WriteAvauiText()` **nunca emite `metadata` en la salida** -- siempre emite `properties` como bloque canónico.

### `state` (Bloque Canónico)

Representa el **estado interno mutable del componente**. El estado pertenece **únicamente al componente** y no debe modificarse desde el exterior.

```avaui
state
    loading = false
    retries = 0
    errorMessage = ""
end
```

Se almacena internamente como lista de pares `[clave]=[valor]` en `ParsedAvaui::state`.

### `view` (Bloque Canónico)

Describe **únicamente la estructura visual**. No contiene lógica de negocio, algoritmos ni eventos explícitos.

Su única responsabilidad es definir la **composición visual**.

```avaui
view
    column
        text
            value = "Bienvenido"
        end

        button Guardar
            text = "Guardar"
        end
    end
end
```

Este bloque:

- Es utilizado por el Renderer para dibujar
- Es mostrado en el Designer de AvaStudio
- Alimenta el Preview y Hot Reload

#### Sintaxis Abreviada: `Type id`

En lugar de:

```avaui
button
    id = "Guardar"
    text = "Guardar"
end
```

Puedes escribir (shorthand):

```avaui
button Guardar
    text = "Guardar"
end
```

Ambas formas son equivalentes. La forma abreviada establecer automáticamente el atributo `id`. Esta sintaxis es especialmente útil para **auto-binding de eventos**.

### `code` (Bloque Canónico)

Todo comportamiento ejecutable pertenece aquí. Contiene:

- **Funciones de ciclo de vida**
  - `OnLoad()` - ejecutada una vez al crear el componente
  - `OnShow()` - ejecutada cuando se hace visible (puede ejecutarse múltiples veces)
  - `OnHide()` - ejecutada cuando deja de ser visible
  - `OnUnload()` - ejecutada justo antes de destruir

- **Manejadores de eventos** (automáticamente enlazados por convención)
  - `OnClickClick()`, `OnSubmitClick()`, etc.

- **Métodos auxiliares y lógica de negocio**

```avaui
code
    func OnLoad()
        LoadUsers()
    end

    func OnShow()
        RefreshData()
    end

    func OnLoginClick()
        Auth.Login()
    end

    func LoadUsers()
        -- lógica auxiliar
    end
end
```

**Nota legal:** `methods` es un alias legacy soportado en lectura (`methods` → se lee como `code`), pero `WriteAvauiText()` **nunca emite `methods` en la salida** -- siempre emite `code` como bloque canónico.

### `style` (Bloque Canónico)

Define la **apariencia visual** del componente. Se almacena como lista de pares `[clave]=[valor]` en `ParsedAvaui::style`, mismo formato que `state`.

Puede contener:

- Colores: `background`, `foreground`, `borderColor`
- Tamaños: `width`, `height`, `padding`, `margin`, `borderRadius`
- Fuentes: `fontFamily`, `fontSize`, `fontWeight`
- Tema y animaciones visuales

```avaui
style
    background = "#FF2E3F"
    borderRadius = 8
    padding = 16
    fontSize = 14
end
```

El bloque `style` es parseado y almacenado en `avaui_text.cpp`, pero aún está en roadmap para:

- Cruzar completamente el C ABI (`avalang.h`/`c_api.cpp`)
- Ser utilizado por AvaHost y AvaStudio

---

## Automatic Event Binding (Auto-bind por Convención)

### Problema que resuelve

Tradicionalmente necesitarías escribir:

```avaui
view
    button Guardar
        click = OnGuardarClick
    end
end

code
    func OnGuardarClick()
        -- handler
    end
end
```

AvaUI **simplifica esto mediante convención de nombres**:

### Solución: Convención `On{IdPascal}{EventPascal}`

Si:

1. Un componente tiene un `id` (incluyendo shorthand `button Guardar`)
2. Existe una función en `code` con el patrón `On{IdPascal}{EventPascal}`

Entonces se **enlaza automáticamente sin escribir el evento explícitamente**:

```avaui
view
    button Guardar    -- id implícito: "Guardar"
        text = "Guardar"
    end
end

code
    func OnGuardarClick()    -- se enlaza automáticamente
        SaveData()
    end
end
```

### Eventos soportados para auto-bind

Para cada tipo de componente, existe una lista de **eventos por defecto** que se intentan enlazar:

- **button**: `click`
- **text input**: `change`, `input`
- **checkbox**: `change`
- **select**: `change`
- **form**: `submit`
- **window/document**: `load`, `show`, `hide`, `unload`, etc.

Ejemplo: un `button` con `id = "Guardar"` buscará automáticamente:

- `OnGuardarClick()` (más probable: es el evento default del button)
- No buscará `OnGuardarChange()` ni `OnGuardarSubmit()` (no son eventos default para button)

### Precedencia: Explícito gana

Un evento **explícitamente declarado** en `view` **siempre toma precedencia** sobre el auto-bind:

```avaui
button Guardar
    click = MiHandlerPersonalizado
end

code
    func OnGuardarClick()
        -- esto NO se ejecutaría (explícito gana)
    end

    func MiHandlerPersonalizado()
        -- esto SÍ se ejecutaría
    end
end
```

---

## Ciclo de Vida de una Página

Toda página posee un conjunto predefinido de funciones de ciclo de vida que el runtime ejecuta automáticamente.

### Secuencia Temporal

```
┌─────────────────────┐
│  Crear Página       │
└──────────┬──────────┘
           │
           ▼
    ┌─────────────┐
    │  OnLoad()   │ (una única vez)
    └──────┬──────┘
           │
           ▼
    ┌─────────────┐
    │  OnShow()   │
    └──────┬──────┘
           │
           ▼
       ┌───────────┐
       │  Usuario  │
       │ interactúa│
       └───────────┘
           │
           ▼
    ┌─────────────┐
    │  OnHide()   │
    └──────┬──────┘
           │
        ¿Se destruye?
        /           \
      SÍ             NO
      │               │
      │               ▼
      │          ┌─────────────┐
      │          │  OnShow()   │ (nuevamente)
      │          └─────────────┘
      │
      ▼
 ┌──────────────┐
 │ OnUnload()   │
 └──────────────┘
```

### Detalles por Función

#### `OnLoad()` - Inicialización única

- Se ejecuta **una única vez** cuando la página es creada
- Responsabilidades típicas:
  - Inicializar datos
  - Leer parámetros de ruta
  - Crear servicios
  - Configuración inicial
  - Cargar datos desde API

```avaui
code
    func OnLoad()
        LoadInitialData()
        SetupEventListeners()
    end
end
```

#### `OnShow()` - Visualización

- Se ejecuta cuando la página pasa a ser **visible**
- **Puede ejecutarse múltiples veces** (ej.: al volver de otra página)
- Responsabilidades típicas:
  - Refrescar datos
  - Reanudar animaciones
  - Reactivar timers
  - Sincronizar estado con servidor

```avaui
code
    func OnShow()
        RefreshUserData()
        ResumePolling()
    end
end
```

#### `OnHide()` - Ocultamiento

- Se ejecuta cuando la página deja de ser **visible**
- Responsabilidades típicas:
  - Detener animaciones
  - Pausar timers
  - Suspender polling
  - Guardar estado temporal

```avaui
code
    func OnHide()
        PauseAnimations()
        StopPolling()
        SaveScrollPosition()
    end
end
```

#### `OnUnload()` - Liberación

- **Última función ejecutada** antes de destruir la página
- Responsabilidades típicas:
  - Liberar memoria
  - Cancelar tareas en vuelo
  - Guardar estado persistente
  - Cerrar conexiones (WebSocket, etc.)

```avaui
code
    func OnUnload()
        CloseWebSocket()
        SavePreferences()
        CancelPendingRequests()
    end
end
```

---

## Relación con AvaStudio

AvaStudio ofrecerá tres vistas sincronizadas sobre el **mismo documento** (sin duplicación):

### 1. Code View

- Edición directa del archivo `.avaui`
- Texto plano con syntax highlighting
- Todos los bloques visibles: `properties`, `state`, `view`, `code`, `style`

### 2. Design View

- Representación visual del bloque `view`
- Permite:
  - Arrastrar y soltar componentes
  - Modificar propiedades visuales
  - Editar estilos
  - Organizar layouts
- Cambios generan automáticamente el texto en Code View

### 3. Flow View (Roadmap)

- Representación visual del bloque `code`
- Cada función se visualiza como un flujo de nodos
- Permite edición visual de lógica sin escribir código
- Genera automáticamente el texto equivalente en `code`

**Importante:** Flow View no reemplaza el código, ambas vistas representan exactamente la misma lógica (single source of truth).

---

## Beneficios de esta Arquitectura

| Beneficio | Explicación |
|-----------|-------------|
| **Simplicidad** | Cada bloque tiene una responsabilidad clara, no hay ambigüedades |
| **Escalabilidad** | Nuevos componentes se integran sin modificar la gramática principal |
| **Integración con Studio** | Cada bloque tiene una representación visual natural |
| **Legibilidad** | Los archivos mantienen siempre la misma organización predecible |
| **Localización rápida** | Un desarrollador encuentra rápidamente: configuración, estado, interfaz, lógica, estilos |
| **Extensibilidad** | Frameworks (AvaWeb, AvaAuth, AvaData, AvaCharts) extienden AvaUI sin alterar la arquitectura |
| **Sincronización** | Código → Diseño → Comportamiento — todas las vistas del mismo modelo, sin copias |

---

## Estructura Final de un Componente

```
MiComponente.avaui
│
├── properties
│   └── configuración pública
│
├── state
│   └── variables mutables internas
│
├── view
│   ├── Button
│   ├── Text
│   ├── Column
│   │   └── ...
│   └── ...
│
├── code
│   ├── func OnLoad()
│   ├── func OnShow()
│   ├── func OnHide()
│   ├── func OnUnload()
│   ├── func OnButtonClick()
│   └── func ...
│
└── style
    └── apariencia visual
```

---

## Visión

AvaUI debe permitir que un mismo componente sea editado desde tres perspectivas **sin duplicar información**:

1. **Code View**: edición textual del componente
2. **Design View**: composición visual de la interfaz
3. **Flow View**: representación gráfica del comportamiento

Las tres vistas trabajan sobre el **mismo modelo interno**, garantizando:

✅ Sincronización bidireccional  
✅ Single source of truth (no múltiples versiones)  
✅ Experiencia de desarrollo integrada  

Este principio es **uno de los pilares fundamentales** del ecosistema AvaLang y la experiencia de desarrollo en AvaStudio.

---

## Estado de Implementación

### ✅ Implementado y Probado

- Parser canónico: `core/src/ui/avaui_text.h/.cpp`
  - Bloques canónicos: `properties`, `state`, `view`, `code`, `style`
  - Legacy support: `metadata` → `properties`, `methods` → `code`
  - Sintaxis abreviada: `button Guardar`
  - Auto-binding: `OnIdEventName()`
- Documentación: `17_AVAUI_FILE_FORMAT.md`
- Comentarios en `html_renderer.cpp` actualizados

### 🚧 En Roadmap

- Cruzar `style` a través del C ABI (`avalang.h`/`c_api.cpp`)
- Ejecutar ciclo de vida (`OnLoad`, `OnShow`, etc.) en AvaHost
- Resolver completamente `state`/`code` de componentes importados
- Flow View en AvaStudio
- Herencia de estilos (layouts, temas globales)

---

## Referencias Relacionadas

- `17_AVAUI_FILE_FORMAT.md` - Especificación técnica del formato `.avaui`
- `10_AVAUI.md` - Detalles del parser y convergencia Studio
- `12_LAYOUT.md` - Sistema de layout y renderizado
- `16_STUDIO.md` - Integración con AvaStudio
- `AVAHOST_IMPLEMENTATION_PLAN.md` - Plan de implementación en AvaHost
