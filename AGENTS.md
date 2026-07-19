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
- Changes to `src/vm/vm.cpp` (bytecode execution, opcodes)
- Changes to `src/compiler/` (AST compilation, code generation)
- Changes to `src/vm/value.h` (Value model, type tags)
- Changes to `src/vm/opcodes.h` (opcode definitions)
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

Ejemplos de scripts disponibles en la raíz del proyecto:
- `test_hello.ava` - Hello world básico
- `test_for.ava` - Loop for con range()
- `test_list.ava` - Listas y diccionarios
- `test_simple.ava` - Expresiones básicas

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

### CMake Options
| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Debug or Release |
| `AVA_BUILD_SHARED` | ON | Build shared library (.dll/.so) |
| `AVA_BUILD_CLI` | ON | Build ava_cli executable |

## Code Style

### General
- C++20, `namespace ava { }` for internal code
- No comments unless explaining non-obvious logic
- Prefer `const`/`constexpr`; use `std::make_shared` (no raw `new`/`delete`)

### Naming
| Element | Convention | Example |
|---------|-----------|---------|
| Classes/Structs | PascalCase | `class VM` |
| Methods/Functions | PascalCase | `RegisterNative` |
| Member variables | snake_case_ with `_` | `globals_` |
| Local variables | snake_case | `proto`, `result` |
| Constants | kPascalCase | `kMaxRegisters` |

### Include Order
1. Corresponding header (for .cpp files)
2. Standard library (`<vector>`, `<string>`, etc.)
3. Third-party headers
4. Internal headers

### Header Guards
Use `#ifndef`/`#define` guards, NOT `#pragma once`:
```cpp
#ifndef AVA_VM_VM_H
#define AVA_VM_VM_H
#endif // AVA_VM_VM_H
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

### Value Model (`src/vm/value.h`)
Tag-union: `Nil | Bool | Number | String | List | Dict | Function | NativeFunc | Instance | Class | Coroutine`
Use factory methods: `Value::Number(1)`, `Value::Nil()`, etc.

### VM (`src/vm/vm.cpp`)
- Register-based bytecode interpreter (Lua-style, not stack-based)
- Frames on `VM::frames_` (not native call stack) - enables coroutines
- Opcodes in `src/vm/opcodes.h`

### Public C API (`include/ava.h`)
- All functions `extern "C"`
- Opaque handles (`void*`) for objects
- `ava_value_t` is POD struct (tag + union)

### Error Handling
- VM errors: throw `std::runtime_error`
- C API errors: return error codes / set error state
- No exceptions for control flow

## Common Tasks

### Add New Opcode
1. Add to `src/vm/opcodes.h` enum
2. Implement in `src/vm/vm.cpp` `ExecuteFrame()` switch

### Add Native Function
1. Implement in `src/api/c_api.cpp`
2. Declare in `include/ava.h`

## Common Tasks
```
src/
├── vm/           # Value, VM, opcodes, Proto
├── ast/          # AST nodes and builder
├── compiler/     # AST → bytecode
├── frontend/     # ANTLR4 integration, denter
├── api/          # C API
└── cli/          # ava_cli binary
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

---

**Important**: Builds with or without ANTLR4. Do NOT commit secrets.
