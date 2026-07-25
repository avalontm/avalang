# Ava Studio - IDE Visual para AvaLang

## Overview

Ava Studio es el IDE visual para crear aplicaciones con AvaUI. Proporciona:
- Editor de código con syntax highlighting
- Preview visual de UI en tiempo real
- Toolbox con componentes drag & drop
- Inspector de propiedades
- Explorador de archivos

**Estado actual**: En desarrollo - Milestone 1 completado.

## Arquitectura General

```
┌─────────────────────────────────────────────────────────────────┐
│                        Ava Studio                               │
│                    (ImGui + OpenGL)                             │
├─────────────────┬─────────────────┬─────────────────────────────┤
│    Explorer     │   Code Editor   │       Properties            │
│   (archivos)    │   (TextEditor)  │    (componente)            │
├─────────────────┼─────────────────┼─────────────────────────────┤
│                 │     Preview     │         Output              │
│                 │   (Component    │      (errores,              │
│                 │    Tree)        │       logs)                 │
├─────────────────┴─────────────────┴─────────────────────────────┤
│                       EngineBridge                              │
│              (Wrapper C++ del C API de AvaLang)                │
├─────────────────────────────────────────────────────────────────┤
│                    AvaLang Core Library                         │
│                    (avalang.dll / avalang.lib)                 │
├─────────────────────────────────────────────────────────────────┤
│                    AvaUI Framework                              │
│                   (Component Tree)                             │
└─────────────────────────────────────────────────────────────────┘
```

## Estructura de Directorios

```
avalang/
├── studio/                        # IDE Ava Studio
│   ├── CMakeLists.txt            # Configuración de build
│   ├── src/
│   │   ├── main.cpp              # Entry point + main loop
│   │   ├── theme.cpp/h          # Tema VSCode Dark+
│   │   ├── engine/
│   │   │   ├── engine_bridge.h  # Wrapper del motor
│   │   │   └── engine_bridge.cpp
│   │   ├── panels/
│   │   │   ├── editor_panel.cpp    # Editor de código
│   │   │   ├── explorer_panel.cpp  # Explorador de archivos
│   │   │   ├── output_panel.cpp    # Consola de salida
│   │   │   ├── preview_panel.cpp   # Preview de componentes
│   │   │   └── properties_panel.cpp # Inspector de propiedades
│   │   └── syntax/
│   │       └── syntax_avalang.h  # Syntax highlighting
│   └── test_*.ava                # Scripts de prueba
│
└── build_studio/                 # Directorio de build
    ├── studio/Release/
    │   └── ava_studio.exe       # Ejecutable del IDE
    └── _deps/                   # Dependencias (GLFW, ImGui, etc.)
```

## Dependencias (FetchContent)

| Dependencia | Repo | Tag | Propósito |
|-------------|------|-----|-----------|
| GLFW | github.com/glfw/glfw.git | 3.4 | Window + input |
| Dear ImGui | github.com/ocornut/imgui.git | docking | UI framework |
| ImGuiColorTextEdit | github.com/BalazsJako/ImGuiColorTextEdit | master | Code editor con syntax highlighting |

## Paneles

### 1. Explorer Panel
- Muestra árbol de archivos `.ava` en `scripts/`
- Click en archivo → abre en Editor
- Solo muestra scripts, no otros archivos

### 2. Editor Panel
- Widget: ImGuiColorTextEdit
- Syntax highlighting para AvaLang
- Atajos:
  - `Ctrl+S` → Guardar archivo
  - `F5` → Ejecutar script
- Marcadores de error (línea roja)

### 3. Preview Panel
- Muestra Component Tree como árbol expandible
- Click en nodo → selecciona en Properties
- MODO LECTURA en Milestone 1
- Próximamente: Preview visual de la UI

### 4. Properties Panel
- Muestra propiedades del componente seleccionado
- Formato tabla: Key | Value
- MODO LECTURA en Milestone 1

### 5. Output Panel
- Resultados de compilación/ejecución
- Mensajes de éxito/error
- JSON del Component Tree

## Workflow Actual (Milestone 1)

```
1. Usuario abre archivo .ava en Explorer
        ↓
2. Edita código en Editor Panel
        ↓
3. Presiona F5 (Run Script)
        ↓
4. EngineBridge.RunScript():
   - ava_compile() → compila a bytecode
   - ava_run() → ejecuta en VM
        ↓
5. Resultado mostrado en Output Panel
   - Éxito: "OK → resultado"
   - Error: mensaje con línea
        ↓
6. Preview muestra demo tree (hardcoded)
```

## Workflow Futuro (Post-Milestone 1)

```
1. Usuario arrastra componentes desde Toolbox al Canvas
        ↓
2. Preview muestra preview visual en tiempo real
        ↓
3. Click en componente → Properties muestra editor
        ↓
4. Double-click en Button → genera evento en Editor:
   func btnSave_Click(sender, e)
       -- handler code here
   end
        ↓
5. Run → Preview renderiza UI real
```

## Syntax Highlighting

**Archivo**: `studio/src/syntax/syntax_avalang.h`

### Paleta de Colores
| Elemento | Color | Hex |
|----------|-------|-----|
| Keywords | Naranja | #FFA500 |
| Strings | Azul claro | #5696D6 |
| Numbers | Verde claro | #B5CAA8 |
| Comments | Verde | #6A994C |
| Identifiers | Amarillo verdoso | #DCDCAA |

### Keywords Definidos
```
if, elif, else, while, for, then, end
func, class, return, base
local, break, continue, pass, raise, yield
try, catch, finally
import, as
true, false, nil
and, or, not
```

### Regex Patterns (orden de procesamiento)
1. Strings (incluyendo f-strings `f"..."`)
2. Numbers (hex `0x`, decimal, float `1.5`, scientific `1e10`)
3. Operators (`=>`, `++`, `--`, `**`, `<=`, `>=`, `==`, `!=`)
4. Comments (`#` hasta fin de línea)
5. Identifiers

### IMPORTANTE: Comments en TextEditor
El widget TextEditor tiene DOS sistemas de colorizado:
1. `ColorizeInternal()` - para comentarios de una línea
2. `ColorizeRange()` - para regex tokens

**Para que `#` funcione como comentario**, debe estar configurado:
```cpp
lang.mSingleLineComment = "#";
```

## EngineBridge

Wrapper C++ que conecta Studio con AvaLang.

### Clase EngineBridge
```cpp
namespace studio {

class EngineBridge {
    AvaVM* vm_;
    
public:
    EngineBridge();           // Crea VM
    ~EngineBridge();           // Destruye VM
    
    RunResult RunScript(const std::string& source, const std::string& name);
    DemoTree BuildDemoComponentTree();
};

} // namespace studio
```

### RunResult
```cpp
struct RunResult {
    bool success;
    std::string message;  // Error o resultado formateado
};
```

### DemoTree (Preview del Component Tree)
```cpp
struct PreviewNode {
    std::string type;
    std::string id;
    std::vector<std::pair<std::string, std::string>> properties;
    std::vector<PreviewNode> children;
};

struct DemoTree {
    PreviewNode root;     // Para Preview panel
    std::string json;     // Para Output panel
};
```

## Build Commands

```bash
# Build completo (incluye studio)
build.bat

# Build solo studio
cd build_studio
cmake --build . --config Release

# Ejecutar
build_studio\studio\Release\ava_studio.exe
```

## Tareas para Agentes

### Agregar Nuevo Panel
1. Crear `studio/src/panels/new_panel.h` y `.cpp`
2. Declarar función `DrawNewPanel(state)`
3. Agregar `#include` en `main.cpp`
4. Llamar en main loop:
```cpp
studio::DrawNewPanel(new_panel_state);
```

### Modificar Syntax Highlighting
1. Editar `studio/src/syntax/syntax_avalang.h`
2. Agregar/modificar regex en `AvaLangLanguageDef()`
3. Cambiar colores en `GetAvaLangPalette()`

### Integrar Nuevo Componente UI
1. Registrar builtin en `core/src/builtins/`
2. Agregar C API en `public/src/c_api.cpp`
3. Agregar constantes en `public/include/avalang.h`
4. Actualizar `BuildDemoComponentTree()` en Studio

## Tasks

### [PENDING] Registrar UI Builtins
- **Fecha**: Por definir
- **Archivos**: `core/src/builtins/`
- **Descripción**: `page()`, `stack()`, `column()`, `row()`, `button()`, `text()`, etc.

### [PENDING] Property Enumeration API
- **Fecha**: Por definir
- **Archivos**: `public/src/c_api.cpp`
- **Descripción**: `ava_ui_list_properties()` para listar todas las propiedades

### [PENDING] Real Preview Rendering
- **Fecha**: Por definir
- **Descripción**: Preview que renderiza UI real, no demo hardcoded

### [PENDING] Drag & Drop Toolbox
- **Fecha**: Por definir
- **Descripción**: Toolbox con componentes arrastrables al canvas