# AvaLang Architecture & Roadmap

> **Estado del proyecto**: En desarrollo activo  
> **Versión actual**: 0.1.x  
> **Última actualización**: 2026-07-20

---

## Tabla de Contenidos

1. [Visión y Filosofía](#1-visión-y-filosofía)
2. [Arquitectura de Capas](#2-arquitectura-de-capas)
3. [Capa 1: Core (avalang-core)](#3-capa-1-core-avalang-core)
4. [Capa 2: API C Pública](#4-capa-2-api-c-pública)
5. [Capa 3: Plugin System](#5-capa-3-plugin-system)
6. [Capa 4: Bindings](#6-capa-4-bindings)
7. [Capa 5: Frameworks](#7-capa-5-frameworks)
8. [Estructura de Directorios](#8-estructura-de-directorios)
9. [Decisiones Técnicas](#9-decisiones-técnicas)
10. [Roadmap](#10-roadmap)
11. [Guía de Contribución](#11-guía-de-contribución)

---

## 1. Visión y Filosofía

### Objetivo

Crear un lenguaje de scripting embeddable con las siguientes características:

- **Portable**: El núcleo vive en C++, bindings para C#, Python, Rust, Go, Java, etc.
- **Escalable**: Diseñado para funcionar desde scripts simples hasta motores de juego AAA
- **Minimalista**: Sintaxis limpia inspirada en Python, semántica dinámica inspirada en Lua
- **Seguro**: Sistema de tipos con manejo de memoria seguro, sandboxing opcional

### Modelo de Éxito

Seguimos el patrón de proyectos probados:

```
┌─────────────────────────────────────────────────────┐
│                    Lua / SQLite                      │
│                                                      │
│   Lua-core (C)                                       │
│       │                                               │
│   ┌───┼─────────┬──────────┐                        │
│   │   │         │          │                        │
│ C#  Python   Rust      Java                       │
│   │   │         │          │                        │
│Unity Godot   Bevy     Android                       │
└─────────────────────────────────────────────────────┘
```

**Regla dorada**: El lenguaje se implementa **una vez**. Todo lo demás son wrappers.

### Principios Fundamentales

1. **Separación estricta de capas** - El core nunca sabe que existen bindings
2. **Handles opacos** - Nunca exponer punteros internos de C++
3. **API C estable** - La ABI pública nunca rompe backwards compatibility
4. **Memoria managed** - Los bindings nunca alloc/free internos
5. **Extensibilidad via plugins** - Motor gráfico, platform SDK, etc. via plugin API

---

## 2. Arquitectura de Capas

```
┌─────────────────────────────────────────────────────────────────┐
│                         AvaLang                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Capa 5: Frameworks                                       │   │
│  │  GameObject, Node, Sprite, Window, Texture, etc.          │   │
│  │                                                           │   │
│  │  Godot Plugin │ Unity Plugin │ SDL Plugin │ Raylib Plugin │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Capa 4: Language Bindings                               │   │
│  │  C# (.NET) │ Python (pybind11) │ Rust │ Go │ Java       │   │
│  │                                                           │   │
│  │  Solo traducen. NO tienen lógica del lenguaje.           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Capa 3: Plugin System                                    │   │
│  │  ava_plugin_register() │ ava_plugin_load()                │   │
│  │                                                           │   │
│  │  Core conoce plugins via descriptor. Plugins no conocen  │   │
│  │  el core.                                                 │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Capa 2: API C Pública (avalang.h)                       │   │
│  │                                                           │   │
│  │  typedef struct AvaVM AvaVM;  // Opaque                  │   │
│  │  ava_vm_create(), ava_run(), ava_call(), etc.            │   │
│  │                                                           │   │
│  │  ESTABLE. Nunca rompe backwards compatibility.          │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│                              ▼                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Capa 1: Core (PRIVADO - No exportar)                    │   │
│  │                                                           │   │
│  │  lexer/ parser/ compiler/ optimizer/ vm/ gc/ builtins/    │   │
│  │                                                           │   │
│  │  Nadie fuera del proyecto conoce estas clases.           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Capa 1: Core (avalang-core)

### Responsabilidades

- Lexer y Parser (ANTLR4 o hand-written)
- Compilación a bytecode
- Optimización de bytecode (opcional)
- Máquina Virtual (register-based, estilo Lua)
- Garbage Collector
- Standard library (builtins en bytecode)

### NO debe hacer

- Conocer bindings específicos
- Exportar símbolos C++ internos
- Depender de frameworks externos

### Estructura interna del Core

```
src/
├── lexer/           # Tokenizer
├── parser/          # AST generation (ANTLR4 o recursive descent)
├── compiler/        # AST → Bytecode
├── optimizer/      # Peephole, constant folding, etc.
├── vm/
│   ├── core.h      # VM principal
│   ├── value.h     # Value tag-union
│   ├── gc.h        # Garbage collector
│   ├── opcodes.h   # Opcode definitions
│   ├── proto.h     # Function prototype
│   └── closure.h   # Closure objects
└── builtins/        # Standard library en bytecode
```

### Bytecode Format

- **Versioned**: Primera versión `0x01`
- **Header**: Magic + version + flags
- **Instructions**: Variable-length encoding
- **Constants pool**: Typed constants
- **Debug info**: Optional (line numbers, locals)

---

## 4. Capa 2: API C Pública

### Ubicación

`include/avalang.h` (futuro renombrar de `ava.h`)

### Principios de Diseño

1. **Solo tipos opacos**
   ```c
   typedef struct AvaVM AvaVM;        // ✓ Correcto
   typedef struct VM* VM;             // ✗ Incorrecto
   ```

2. **Handles para memoria managed**
   ```c
   typedef struct AvaRef { uint64_t id; } AvaRef;
   ```

3. **POD para valores cruzando ABI**
   ```c
   typedef struct ava_value_t {
       AvaValueType type;
       union {
           int      b;       // bool
           double   n;       // number
           AvaRef   ref;     // string/list/dict/function/instance
       } as;
   } ava_value_t;
   ```

4. **Error handling via out-parameters**
   ```c
   AvaModule* ava_compile(
       AvaVM* vm,
       const char* source,
       const char* source_name,
       char** out_error  // NULL si éxito, malloc'd string si error
   );
   ```

### Funciones de la API (Estado Actual)

```c
// VM
AvaVM* ava_vm_create(void);
void   ava_vm_destroy(AvaVM* vm);
void   ava_vm_register_native(AvaVM* vm, const char* name, AvaNativeFn fn, void* user_data);

// Compilación
AvaModule* ava_compile(AvaVM* vm, const char* source, const char* name, char** out_error);
void        ava_module_destroy(AvaModule* module);

// Ejecución
void ava_run(AvaVM* vm, AvaModule* module, ava_value_t* out_result, char** out_error);
void ava_call(AvaVM* vm, ava_value_t callable, const ava_value_t* args, size_t arg_count, 
              ava_value_t* out_result, char** out_error);

// Globales
ava_value_t ava_get_global(AvaVM* vm, const char* name);
void        ava_set_global(AvaVM* vm, const char* name, ava_value_t value);

// Corrutinas
AvaCoroutine* ava_coroutine_create(AvaVM* vm, ava_value_t function);
void           ava_coroutine_destroy(AvaCoroutine* co);
AvaCoStatus    ava_coroutine_resume(AvaVM* vm, AvaCoroutine* co, const ava_value_t* args, 
                                     size_t arg_count, ava_value_t* out_values, 
                                     size_t out_capacity, size_t* out_count);
AvaCoStatus    ava_coroutine_status(AvaVM* vm, AvaCoroutine* co);

// Value helpers
ava_value_t ava_string_create(AvaVM* vm, const char* utf8, size_t len);
const char* ava_string_data(AvaVM* vm, ava_value_t str, size_t* out_len);

ava_value_t ava_list_create(AvaVM* vm);
void         ava_list_append(AvaVM* vm, ava_value_t list, ava_value_t item);
// ... más helpers

// Ref counting
void ava_value_retain(AvaVM* vm, ava_value_t value);
void ava_value_release(AvaVM* vm, ava_value_t value);
```

### Extensiones Planeadas

```c
// Módulos (futuro)
AvaModule* ava_module_load(AvaVM* vm, const char* path, char** out_error);
void       ava_module_reload(AvaVM* vm, AvaModule* module);

// Serialización (futuro)
int ava_serialize_vm(AvaVM* vm, void* buffer, size_t buffer_size, size_t* out_size);
int ava_deserialize_vm(AvaVM* vm, const void* buffer, size_t buffer_size);

// Sandbox (futuro)
typedef struct AvaSandboxConfig {
    size_t max_memory_bytes;
    size_t max_instructions;
    int allow_io;
    int allow_network;
    int allow_file_access;
} AvaSandboxConfig;

void ava_vm_configure_sandbox(AvaVM* vm, const AvaSandboxConfig* config);
```

---

## 5. Capa 3: Plugin System

### Concepto

Los plugins permiten extender AvaLang sin modificar el core:

- Agregar funciones nativas categorizadas por dominio
- Integración con motores gráficos (Metal, Vulkan, DirectX, OpenGL)
- Acceso a platform SDKs (iOS, Android, WebGL)
- Integración con engines (Godot, Unity, Unreal)

### No deben hacer

- Modificar el bytecode del VM
- Agregar nuevos opcodes
- Reemplazar el GC

### API de Plugins

```c
// Plugin descriptor
typedef struct AvaPlugin {
    const char* name;
    const char* version;
    void* user_data;
} AvaPlugin;

// Función de plugin
typedef ava_value_t (*AvaPluginFn)(
    AvaVM* vm,
    const ava_value_t* args,
    size_t arg_count,
    void* user_data
);

// Callback de inicialización
typedef ava_value_t (*AvaPluginInit)(AvaVM* vm, void* user_data);

// Callback de shutdown
typedef void (*AvaPluginShutdown)(AvaVM* vm, void* user_data);

// Descriptor de plugin
typedef struct AvaPluginDesc {
    const char* name;
    const char* version;
    AvaPluginInit init;
    AvaPluginShutdown shutdown;
    const AvaPluginFn* functions;
    const char** function_names;
    size_t function_count;
    void* user_data;
} AvaPluginDesc;

// Funciones de la API
AvaPlugin* ava_plugin_load(AvaVM* vm, const char* path, char** out_error);
void        ava_plugin_unload(AvaPlugin* plugin);
void        ava_plugin_register(AvaVM* vm, const AvaPluginDesc* desc);

// Ejemplo de uso en script:
// import my_plugin
// my_plugin.create_window(800, 600)
```

### Plugins Planificados

| Plugin | Descripción | Prioridad |
|--------|-------------|-----------|
| `avalang-sdl` | Wrapper para SDL2 (2D graphics, audio, input) | Alta |
| `avalang-godot` | Integración nativa con Godot 4.x | Alta |
| `avalang-opengl` |绑定 OpenGL 3.3+ | Media |
| `avalang-vulkan` | Binding Vulkan | Baja |
| `avalang-imgui` | Integración Dear ImGui | Media |
| `avalang-sqlite` | Acceso a SQLite | Media |

---

## 6. Capa 4: Bindings

### Principios

1. **Solo traducciones** - No hay lógica del lenguaje
2. **Handles internos** - `IntPtr` en C#, `PyObject*` en Python
3. **Thread safety** - El VM debe ser thread-safe
4. **Exception handling** - Mapear errores de AvaLang a excepciones del lenguaje host

### Binding: C# (.NET)

```csharp
namespace AvaLang.Bindings.CSharp;

public sealed class AvaVM : IDisposable
{
    private readonly IntPtr _handle;
    
    public AvaVM()
    {
        _handle = Native.ava_vm_create();
    }
    
    public void Dispose()
    {
        if (_handle != IntPtr.Zero)
        {
            Native.ava_vm_destroy(_handle);
            _handle = IntPtr.Zero;
        }
    }
    
    public void RegisterFunction(string name, Func<AvaVM, AvaValue[], AvaValue> callback)
    {
        // Crear delegate y guardar en cache para GC
        // Llamar ava_vm_register_native
    }
    
    public AvaModule Compile(string source, string name = "<string>")
    {
        // Llamar ava_compile
    }
    
    public AvaValue Run(AvaModule module)
    {
        // Llamar ava_run
    }
}
```

### Binding: Python

```python
# avalang/__init__.py
import ctypes
import functools

class AvaVM:
    def __init__(self):
        self._handle = _lib.ava_vm_create()
    
    def __del__(self):
        if hasattr(self, '_handle'):
            _lib.ava_vm_destroy(self._handle)
    
    def register_function(self, name, callback):
        @functools.wraps(callback)
        def wrapper(vm, args, arg_count, user_data):
            # Llamar callback y retornar ava_value_t
            pass
        
        _lib.ava_vm_register_native(
            self._handle, 
            name.encode('utf-8'), 
            wrapper, 
            None
        )
    
    def compile(self, source, name="<string>"):
        # Llamar ava_compile
        pass
    
    def run(self, module):
        # Llamar ava_run
        pass
```

### Binding: Rust

```rust
// avalang-sys crate
use std::ffi::c_void;
use std::os::raw::c_char;

#[repr(C)]
pub struct AvaVM {
    _private: [u8; 0],
}

extern "C" {
    pub fn ava_vm_create() -> *mut AvaVM;
    pub fn ava_vm_destroy(vm: *mut AvaVM);
    // ... más funciones
}

// avalang crate (alto nivel)
use std::ptr;

pub struct VM {
    handle: *mut sys::AvaVM,
}

impl VM {
    pub fn new() -> Self {
        Self { handle: unsafe { sys::ava_vm_create() } }
    }
    
    pub fn register_function<F>(&mut self, name: &str, callback: F)
    where
        F: Fn(&VM, &[AvaValue]) -> AvaValue + 'static,
    {
        // Crear callback wrapper y registrar
    }
}
```

### Comparativa de Bindings

| Aspecto | C# | Python | Rust | Go |
|---------|-----|--------|------|-----|
| Memory management | GC | Refcount | Ownership | GC |
| Error handling | Exceptions | Exceptions | Result<T> | error |
| FFI tool | P/Invoke | ctypes | bindgen | cgo |
| Complexity | Media | Baja | Alta | Media |
| Use case | Unity, .NET | Scripting | Games, CLI | Cloud, CLI |

---

## 7. Capa 5: Frameworks

### Godot 4.x Integration

```
┌─────────────────────────────────────────────────────────┐
│                      Godot 4.x                          │
│                                                         │
│   ┌─────────────────────────────────────────────────┐   │
│   │  GDExtension Plugin (avalang-godot)            │   │
│   │                                                 │   │
│   │  class_name: AvaLang                           │   │
│   │  extends: RefCounted                           │   │
│   │                                                 │   │
│   │  func compile_script(source: String) -> bool  │   │
│   │  func run_script() -> Variant                  │   │
│   │  func call_function(name: String, args: [])   │   │
│   └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│   ┌─────────────────────────────────────────────────┐   │
│   │  Binding C++ (llama a avalang.h)               │   │
│   └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│   ┌─────────────────────────────────────────────────┐   │
│   │  avalang.dll (Core + API C)                    │   │
│   └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### SDL2 Integration

```
┌─────────────────────────────────────────────────────────┐
│                    Aplicación SDL2                       │
│                                                         │
│   ┌─────────────────────────────────────────────────┐   │
│   │  Script principal (scripts/game.ava)           │   │
│   │                                                 │   │
│   │  import sdl                                    │   │
│   │  window = sdl.create_window("Mi Juego", ...)  │   │
│   │  texture = sdl.load_texture("player.png")     │   │
│   └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│   ┌─────────────────────────────────────────────────┐   │
│   │  Plugin avalang-sdl                            │   │
│   │  Registra: sdl.create_window, sdl.load_texture│   │
│   └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│   ┌─────────────────────────────────────────────────┐   │
│   │  avalang.dll                                   │   │
│   └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Unity Integration

```
┌─────────────────────────────────────────────────────────┐
│                      Unity                             │
│                                                         │
│   ┌─────────────────────────────────────────────────┐   │
│   │  C# Binding Layer (AvaLangBehaviour)           │   │
│   │                                                 │   │
│   │  [AddComponentMenu("AvaLang/Script")]         │   │
│   │  public class AvaLangBehaviour : MonoBehaviour │   │
│   │  {                                              │   │
│   │      public TextAsset scriptFile;              │   │
│   │      private AvaVM _vm;                         │   │
│   │                                                 │   │
│   │      void Start() {                            │   │
│   │          _vm = new AvaVM();                    │   │
│   │          var mod = _vm.Compile(scriptFile);    │   │
│   │          _vm.Run(mod);                         │   │
│   │      }                                         │   │
│   │  }                                             │   │
│   └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---

## 8. Estructura de Directorios

### Estado Previo (antes de refactorización)

```
avalang/
├── include/           # Headers públicos
│   └── ava.h          # API C
├── src/
│   ├── api/           # Implementación de C API
│   ├── ast/           # AST nodes
│   ├── builtins/      # Builtin functions
│   ├── cli/           # ava_cli (host de referencia)
│   ├── compiler/      # Compiler
│   ├── frontend/      # ANTLR4 integration
│   └── vm/            # Virtual machine
├── scripts/           # Scripts de prueba
├── grammar/           # ANTLR4 grammar
├── build/             # Build output
└── CMakeLists.txt
```

### Estado Actual (refactorizado 2026-07-20)

```
avalang/
├── core/                          # Todo el lenguaje (PRIVADO)
│   └── src/
│       ├── ast/                   # AST nodes y builder
│       ├── builtins/              # Builtin functions
│       ├── compiler/              # Compiler (AST → bytecode)
│       ├── frontend/              # ANTLR4 integration
│       └── vm/                    # Virtual machine
│           ├── value.h            # Value tag-union
│           ├── vm.h               # VM principal
│           ├── opcodes.h         # Opcode definitions
│           ├── proto.h            # Function prototype
│           ├── closure.h          # Closure objects
│           ├── module.h           # Module system
│           └── coroutine.h        # Coroutines
│
├── public/                        # Solo esto se exporta
│   ├── include/
│   │   └── avalang.h              # API C estable
│   └── src/
│       ├── c_api.cpp              # C API implementation
│       └── main.cpp               # ava_cli (host de referencia)
│
├── plugins/                        # Plugins (futuro)
├── bindings/                       # Bindings (futuro)
├── examples/                       # Ejemplos (futuro)
│
├── scripts/                       # Scripts de prueba
├── grammar/                       # ANTLR4 grammar
├── docs/                          # Documentación
├── vcpkg/                         # Dependencias
│
├── CMakeLists.txt                 # Build principal
└── README.md
```

---

## 9. Decisiones Técnicas

### 9.1 Bytecode Format

**Decisión**: Register-based VM (estilo Lua), no stack-based.

**Razón**: Más fácil de optimizar, mejor para JIT, similar a Lua/LuaJIT.

### 9.2 Garbage Collector

**Decisión actual**: Reference counting simple con mark-and-sweep para ciclos.

**Futuro**: Posible migración a generational GC si el profiling lo requiere.

### 9.3 Manejo de Excepciones

**Decisión**: Modelo detry-except-raise nativo en AvaLang.

```ruby
try
    risky_function()
except Error e
    print("Error: " + str(e))
end

raise "custom error"
```

### 9.4 Sistema de Tipos

**Decisión**: Dynamic typing con type hints opcionales.

```ruby
# Sin tipo (dynamic)
def add(a, b)
    return a + b
end

# Con tipo (validación en runtime)
def add(a: number, b: number) -> number
    return a + b
end
```

### 9.5 Concurrencia

**Decisión**: Corrutinas nativas (generators/yield).

```ruby
func counter()
    i = 0
    while true do
        yield i
        i = i + 1
    end
end

co = coroutine.create(counter)
first, = resume(co)  # 0
second, = resume(co)  # 1
```

**No implementar** (por ahora): Threads nativos, async/await syntax sugar.

### 9.6 ABI Compatibility

**Regla**: Nunca romper backwards compatibility en `avalang.h`.

- Agregar nuevas funciones está bien
- Agregar campos a structs existentes NO está bien
- Usar `avalang_version` para detectar features

```c
#define AVALANG_VERSION_MAJOR 0
#define AVALANG_VERSION_MINOR 1
#define AVALANG_VERSION_PATCH 0

typedef struct AvaVersion {
    int major;
    int minor;
    int patch;
} AvaVersion;

AVA_API void ava_get_version(AvaVersion* out_version);
```

---

## 10. Roadmap

### Progreso General

```
██████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  40%
```

### 10.1 Fase: Core Fundacional ✅ COMPLETADO

| Tarea | Estado | Notas |
|-------|--------|-------|
| Lexer básico | ✅ Hecho | Hand-written tokenizer |
| Parser básico | ✅ Hecho | ANTLR4 + fallback stub |
| Compiler (AST→bytecode) | ✅ Hecho | Register-based |
| VM básica | ✅ Hecho | Opcodes fundamentales |
| C API básica | ✅ Hecho | ava.h estable |
| String/List/Dict builtins | ✅ Hecho | |
| Corrutinas | ✅ Hecho | |
| Try/Except/Raise | ✅ Hecho | |
| Class system básico | ✅ Hecho | Inheritance, init |
| F-strings | ✅ Hecho | |

### 10.2 Fase: Refactorización de Arquitectura ✅ COMPLETADO

| Tarea | Estado | Notas |
|-------|--------|-------|
| Nueva estructura de directorios | ✅ Hecho | core/, public/ |
| Separar core de API pública | ✅ Hecho | public/include/avalang.h |
| Actualizar CMakeLists.txt | ✅ Hecho | Paths actualizados |
| Verificar build funciona | ✅ Hecho | Build y tests pasan |
| Actualizar arquitectura.md | ✅ Hecho | Sección 8 actualizada |

**Checklist de progreso:**

```
[x] Crear architecture.md (este documento)
[x] Refactorizar estructura de directorios
[x] Separar core/ de public/
[x] Crear public/include/avalang.h (copia renombrada)
[x] Actualizar CMakeLists.txt con nuevos paths
[x] Verificar build funciona con scripts
[x] Actualizar architecture.md con nuevo estado
[ ] Crear public/include/avalang_plugin.h (stub)
[ ] Crear CMakeLists.txt independiente para core/
[ ] Crear CMakeLists.txt independiente para public/
```

### 10.3 Fase: Plugin System

| Tarea | Prioridad | Estado |
|-------|-----------|--------|
| Definir avalang_plugin.h | Alta | 🔄 Pendiente |
| Implementar ava_plugin_register | Alta | 🔄 Pendiente |
| Plugin avalang-sdl (ejemplo) | Alta | 🔄 Pendiente |
| Documentar desarrollo de plugins | Media | 🔄 Pendiente |

### 10.4 Fase: Optimización

| Tarea | Prioridad | Estado |
|-------|-----------|--------|
| Constant folding | Media | 🔄 Pendiente |
| Peephole optimizer | Media | 🔄 Pendiente |
| Better register allocation | Baja | 🔄 Pendiente |
| Inline cache para GETATTR/SETATTR | Baja | 🔄 Pendiente |

### 10.5 Fase: Bindings

| Binding | Prioridad | Estado |
|---------|-----------|--------|
| C# (.NET) | Alta | 🔄 Pendiente |
| Python (ctypes) | Alta | 🔄 Pendiente |
| Rust (bindgen) | Media | 🔄 Pendiente |
| Go (cgo) | Media | 🔄 Pendiente |

### 10.6 Fase: Integración con Motores

| Motor | Prioridad | Estado |
|-------|-----------|--------|
| Godot 4.x (GDExtension) | Alta | 🔄 Pendiente |
| SDL2 (plugin) | Alta | 🔄 Pendiente |
| Unity (C# binding) | Media | 🔄 Pendiente |
| Raylib (plugin) | Baja | 🔄 Pendiente |

### 10.7 Fase: Documentación y Release

| Tarea | Prioridad | Estado |
|-------|-----------|--------|
| Documentación de API | Alta | 🔄 Pendiente |
| Tutorial de scripting | Alta | 🔄 Pendiente |
| Ejemplos de bindings | Media | 🔄 Pendiente |
| Release 1.0.0 | Alta | 🔄 Pendiente |

---

## 11. Guía de Contribución

### 11.1 Reglas de Oro

1. **Nunca exportar clases de C++** - Solo `avalang.h` y `avalang_plugin.h`
2. **Handles opacos siempre** - `typedef struct X X;` no `class X;`
3. **Backwards compatibility** - No romper la API C existente
4. **Tests antes de merge** - Scripts de prueba para cada feature
5. **Documentar decisiones** - Cada decisión técnica en este documento

### 11.2 Commit Messages

```
tipo(alcance): descripción corta

- Tipo: feat, fix, refactor, docs, test, chore
- Alcance: core, api, cli, docs, build
```

Ejemplos:
```
feat(core): agregar soporte para f-strings
fix(api): corregir leak en ava_string_create
refactor(vm): reescribir register allocator
docs(architecture): actualizar roadmap
```

### 11.3 Code Review Checklist

- [ ] ¿Se usan solo handles opacos?
- [ ] ¿La API C es estable (no rompe backwards compat)?
- [ ] ¿Hay tests/scripts de prueba?
- [ ] ¿Se documentó en architecture.md si es feature nuevo?
- [ ] ¿El código sigue el style guide?

### 11.4 Style Guide

**C++ (Core)**
- C++20, `namespace ava {}`
- Clases: PascalCase
- Métodos: PascalCase
- Variables miembro: snake_case_
- Locals: snake_case
- Constantes: kPascalCase

**C (API pública)**
- Funciones: `ava_*`
- Structs: `Ava*`
- Enums: `AVA_*`
- Typedefs: `ava_*_t`

---

## Changelog

### 2026-07-20
- Creado architecture.md
- Documentada visión y filosofía
- Definida arquitectura de 5 capas
- Establecido roadmap con progreso
- Agregadas decisiones técnicas

---

## Referencias

- [Lua API Design](https://www.lua.org/pil/4.html)
- [SQLite C API](https://www.sqlite.org/capi3ref.html)
- [Wren VM Architecture](http://wren.io/compilation/)
- [C Foreign Function Interface Guide](https://docs.python.org/3/library/ctypes.html)
- [Rust FFI Omnibus](https://github.com/japaric/ffi-trials)