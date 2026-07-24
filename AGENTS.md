# AvaLang Developer Guide

Guidelines for AI agents working on AvaLang. An embeddable scripting language with Python-like syntax and Lua-like dynamic semantics.

**Specs**: C++20, CMake 3.20+, ANTLR4 runtime (optional - stub frontend builds without it)

## Build Commands

### Windows
```bash
build.bat              # Release build (lib + ava_cli.exe)
build.bat debug        # Debug build
build.bat ninja        # Use Ninja generator
build.bat clean        # Clean rebuild (required after critical fixes)
```

### Linux/macOS
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

### When to Clean Build

**CRITICAL**: After fixing bugs in VM, compiler, or core runtime, always do a clean build before testing. Old object files may cache incorrect behavior.

Common cases requiring `build.bat clean`:
- Changes to `core/src/vm/vm.cpp` (bytecode execution, opcodes)
- Changes to `core/src/compiler/` (AST compilation, code generation)
- Changes to `core/src/vm/value.h` (Value model, type tags)
- Changes to `core/src/vm/opcodes.h` (opcode definitions)
- Changes to class inheritance or `base()` calls
- Changes to register allocation logic

```bash
# Step 1: Clean
cmd //c "build.bat clean"

# Step 2: Verify build succeeded
# (check for "Build succeeded" in output)

# Step 3: Test
build/Release/ava_cli.exe scripts/test_class_inherit.ava
```

### Testing Scripts

Scripts `.ava` se testear con el CLI `ava_cli`:

```bash
# Ejecutar un script
build\Release\ava_cli.exe scripts\test_hello.ava

# Con debug build
build\Debug\ava_cli.exe scripts\test_hello.ava
```

Ejemplos de scripts disponibles en `scripts/`:
- `scripts/test_simple.ava` - Expresiones básicas
- `scripts/test_for.ava` - Loop for con range()
- `scripts/test_class_inherit.ava` - Herencia de clases

Scripts de prueba deben crearse en `scripts/`:

```bash
build\Release\ava_cli.exe scripts\<nombre_script>.ava
```

## Sintaxis AvaLang

Todos los bloques compuestos deben cerrarse con `end`:

```lua
if expr then
    ...
end

while expr do
    ...
end

for x in expr do
    ...
end

func name(args)
    ...
end

class Name(args)
    ...
end
```

Los paréntesis en expresiones son opcionales:
```lua
if (x == 1) then    # válido
if x == 1 then      # también válido
```

## Common Tasks

### Add New Opcode
1. Add to `core/src/vm/opcodes.h` enum
2. Implement in `core/src/vm/vm.cpp` `ExecuteFrame()` switch

### Add Native Function
1. Implement in `public/src/c_api.cpp`
2. Declare in `public/include/avalang.h`

### Directory Structure Reference
```
core/src/
├── vm/           # Value, VM, opcodes, Proto
├── ast/          # AST nodes and builder
├── compiler/     # AST → bytecode
├── frontend/     # ANTLR4 integration, denter
├── builtins/     # Builtin functions
public/src/
├── c_api.cpp     # C API
└── main.cpp      # CLI
```

### Class Style
```cpp
struct CallFrame {
    std::shared_ptr<Proto> proto;
    std::vector<Value> registers;
    uint32_t pc = 0;
};

class VM {
public:
    VM();
    Value Run(const std::shared_ptr<Proto>& main);
private:
    std::unordered_map<std::string, Value> globals_;
    std::vector<CallFrame> frames_;
};
```

## Architecture

### Value Model (`core/src/vm/value.h`)
Tag-union: `Nil | Bool | Number | String | List | Dict | Function | NativeFunc | Instance | Class | Coroutine`
Use factory methods: `Value::Number(1)`, `Value::Nil()`, etc.

### VM (`core/src/vm/vm.cpp`)
- Register-based bytecode interpreter (Lua-style, not stack-based)
- Frames on `VM::frames_` (not native call stack) - enables coroutines
- Opcodes in `core/src/vm/opcodes.h`

### Public C API (`public/include/avalang.h`)
- All functions `extern "C"`
- Opaque handles (`void*`) for objects
- `ava_value_t` is POD struct (tag + union)

### Architecture Overview

For complete architecture documentation, see `docs/architecture.md`.

```
avalang/
├── core/src/           # Language implementation (PRIVATE - do not export)
│   ├── vm/             # Value, VM, opcodes, Proto
│   ├── ast/            # AST nodes and builder
│   ├── compiler/       # AST → bytecode
│   ├── frontend/      # ANTLR4 integration, denter
│   └── builtins/       # Builtin functions
├── public/             # Public headers to export
│   ├── include/       # API headers (avalang.h)
│   └── src/            # C API impl + CLI
├── plugins/            # Plugin implementations (TODO: create)
├── bindings/           # Language bindings (TODO: create)
└── docs/               # Architecture and documentation
```

### Key Principles

1. **Core is PRIVATE** - Never export C++ classes, only C API
2. **Handles are opaque** - `typedef struct AvaVM AvaVM;` (no `class VM`)
3. **Memory managed** - Bindings never alloc/free internal objects
4. **Plugin System** - Extend via `ava_plugin_register()`, not modifying core

### Error Handling
- VM errors: throw `std::runtime_error`
- C API errors: return error codes / set error state
- No exceptions for control flow

## Common Tasks

### Add New Opcode
1. Add to `core/src/vm/opcodes.h` enum
2. Implement in `core/src/vm/vm.cpp` `ExecuteFrame()` switch

### Add Native Function
1. Implement in `public/src/c_api.cpp`
2. Declare in `public/include/avalang.h`

## Common Tasks
```
core/src/
├── vm/           # Value, VM, opcodes, Proto
├── ast/          # AST nodes and builder
├── compiler/     # AST → bytecode
├── frontend/     # ANTLR4 integration, denter
├── builtins/     # Builtin functions
public/src/
├── c_api.cpp     # C API
└── main.cpp      # CLI
```

## Timeout Prevention for AI Agents

The "Error: SSE read timed out" occurs when a task runs too long without producing output. To prevent this:

### Task Structure
- Divide long tasks into small sequential steps
- Each step should complete in < 30 seconds
- After each step, report progress before continuing

### Build Commands
- Use appropriate timeouts: `timeout="120000"` for builds, `timeout="10000"` for tests
- Use `&&` to chain dependent commands, but keep chains short (max 3 commands)

```bash
# BAD: Long chains without output
build.bat && ./test1 && ./test2 && ./test3 && ./test4

# GOOD: Single focused command per run
cd build && cmake --build . --config Debug
```

### During Debugging
- Use minimal debug output (only on first run to verify)
- Remove `fprintf(stderr, ...)` debug prints before finalizing
- If a script hangs, it will timeout - this is expected behavior for bugs

### When Debugging Scripts
- Run scripts individually with ava_cli.exe
- Check bytecode compilation output if needed
- Investigate the VM execution loop if infinite loop suspected

## Documentation Policy

When the user confirms a bug is fixed, document it using a task format in the corresponding architecture document.

### Task Format in Architecture Docs

When documenting fixes, use this format in the appropriate `docs/architecture/*.md`:

```markdown
## Tasks

### [COMPLETED] Bug #N: Brief Description
- **Date**: YYYY-MM-DD
- **File**: path/to/file.cs:line
- **Problem**: What was wrong
- **Solution**: How it was fixed
```

### Document Mapping

| Bug/Feature Type | Document |
|------------------|----------|
| AvaUI Web fixes | `docs/architecture/framework_ui_web.md` |
| Component tree/Renderer | `docs/architecture/07_COMPONENT_TREE.md` |
| Core language features | `docs/architecture.md` |

**AvaUI Web → `docs/architecture/framework_ui_web.md`**

## Plugin System (Planned)

Plugins allow extending AvaLang without modifying core. See `docs/architecture.md` for full design.

```c
// Plugin descriptor (concept, not implemented yet)
typedef struct AvaPluginDesc {
    const char* name;
    const char* version;
    void (*init)(AvaVM* vm);
    void (*shutdown)(AvaVM* vm);
    const char** function_names;
    size_t function_count;
} AvaPluginDesc;

void ava_plugin_register(AvaVM* vm, const AvaPluginDesc* desc);
```

### Planned Plugins

| Plugin | Description | Status |
|--------|-------------|--------|
| `avalang-sdl` | SDL2 wrapper (window, textures, audio) | Planned |
| `avalang-godot` | Godot 4.x GDExtension | Planned |
| `avalang-opengl` | OpenGL 3.3+ bindings | Planned |

## Research Documentation

Before working on any task, **read the relevant documentation first**:

### AvaUI Framework (Web/Desktop UI)
| Document | Purpose |
|----------|---------|
| `docs/architecture/AVAUI_FRAMEWORK.md` | Vision, principles, architecture overview |
| `docs/architecture/AVA_PAGE_COMPONENT_ARCHITECTURE.md` | Page/component structure, sections, lifecycle |
| `bindings/csharp/AvaLang.UI/docs/FRAMEWORK.md` | C# implementation, API reference |
| `docs/architecture/07_COMPONENT_TREE.md` | Component tree abstraction |

### AvaUI Web Specific
| Document | Purpose |
|----------|---------|
| `docs/architecture/framework_ui_web.md` | Web framework bugs, fixes, current state |
| `docs/architecture/05_AVAWEB_HOST.md` | Web host architecture, routing, rendering |

### AvaLang Core Language
| Document | Purpose |
|----------|---------|
| `docs/architecture.md` | Overall architecture, roadmap, layers |
| `docs/builtins.md` | Builtin functions reference |
| `docs/classes.md` | Class system and inheritance |
| `docs/operators.md` | Operator precedence and behavior |
| `docs/coroutines.md` | Coroutine/async behavior |

### Quick Reference: What to Read
| Task Type | Documents to Read |
|-----------|------------------|
| UI bug/feature | `AVAUI_FRAMEWORK.md` + `FRAMEWORK.md` |
| New UI component | `AVA_PAGE_COMPONENT_ARCHITECTURE.md` |
| Rendering issues | `framework_ui_web.md` |
| Core language bug | `docs/architecture.md` |
| Script syntax | `docs/builtins.md`, `docs/operators.md` |

---

**Important**: Builds with or without ANTLR4. Do not commit secrets.