# AvaLang

AvaLang is an embeddable scripting language for adding scripting support
to applications: dynamic typing, closures, classes with single
inheritance, and coroutines, built around a register-based bytecode VM.
The core is a single C++ library exposed through a plain C API
(`include/ava.h`), so it can be embedded from any host language with FFI
(C#, Python, Java, Rust, Godot/GDExtension, ...).

"AvaLang" is a working name -- rename freely, nothing in the code depends
on it.

## Features

- Dynamic typing: `nil`, `bool`, `number`, `string`, `list`, `dict`,
  `function`, `class`, `instance`, `coroutine`.
- Functions, closures, and lambdas.
- Classes with `__init__`, methods, and single inheritance via `base()`.
- Control flow: `if/elif/else`, `while`, `for`, `break`, `continue`.
- List/string slicing (`arr[1:3]`, `arr[::2]`, ...).
- F-string interpolation (`$"value: {x}"`).
- A set of built-in functions (`print`, `type`, `str`, `len`, `range`,
  `sorted`, `sum`, ...) and string/list/dict methods -- see `docs/`.
- Reference-counted values and a register-based bytecode VM.

See `DESIGN.md` for the full architecture writeup (value model, bytecode
opcode table, VM/coroutine design, host binding examples), and `docs/`
for the language reference.

## Quick example

```lua
class Dog(name)
    __init__(self, name)
        self.name = name
    end

    bark(self)
        print(self.name + " says Woof!")
    end
end

func greet(person)
    print($"Hello {person}!")
end

greet("Ava")

local d = Dog("Buddy")
d.bark()

local nums = [1, 2, 3, 4, 5]
print(nums[1:3])   # [2, 3]
print(nums[::2])   # [1, 3, 5]
```

## Building

Requires CMake >= 3.20 and a C++20 compiler. The full frontend needs the
ANTLR4 C++ runtime and an ANTLR4 tool jar (used only at build time, to
regenerate the parser from `grammar/AvaLang.g4`); without them, the
project still builds using a stub frontend that reports a clear error
instead of compiling scripts.

### Windows

Quickest path -- installs vcpkg + antlr4 (static-md triplet, no extra
DLLs to redistribute) + the ANTLR4 tool jar, then builds automatically:

```bat
install.bat
```

Run it once from a terminal with `git` on PATH (`java` is optional, only
used to regenerate the parser). See `install.bat`'s header comments for
options (`install.bat skipbuild`, custom vcpkg path, etc).

Or run `build.bat` directly if you already have vcpkg + antlr4 set up:

```bat
build.bat              REM Release build, Visual Studio generator
build.bat debug         REM Debug build
build.bat test           REM also builds and runs tests via ctest
build.bat ninja          REM use Ninja instead of MSBuild (needs ninja.exe on PATH)
build.bat clean debug test
build.bat help
```

If `VCPKG_ROOT` is set (pointing at a vcpkg install with
`vcpkg install antlr4` already run), its toolchain file is picked up
automatically so the full ANTLR4 frontend builds. Without it, the build
still succeeds using the stub frontend.

### Linux / macOS

```bash
mkdir build && cd build
cmake ..
cmake --build .
./ava_cli path/to/script.ava
```

- **Without** the ANTLR4 C++ runtime, CMake falls back to the stub
  frontend. The VM, C API, and CLI all still build and link;
  `ava_compile()` returns a clear error explaining what's missing.
- **With** `antlr4-runtime` discoverable by `find_package` AND an ANTLR4
  complete jar discoverable by CMake (checked under `/usr/local/lib`,
  `/usr/share/java`, and `third_party/`) AND `java` on PATH, CMake
  regenerates the parser from `grammar/AvaLang.g4` automatically and
  compiles the real frontend (`src/frontend/frontend_antlr.cpp`).

To get the full frontend building, install:
- the ANTLR4 C++ runtime (build from source or via your package manager;
  see https://github.com/antlr/antlr4/tree/master/runtime/Cpp)
- an `antlr-4.x-complete.jar` (build-time only, not a runtime dependency)

## Running a script

```bash
ava_cli path/to/script.ava
```

## Project layout

```
grammar/AvaLang.g4        The language grammar (ANTLR4)
include/ava.h              Public C API -- the only header host bindings need
src/
  frontend/                Lexer/denter/parser wiring (frontend_antlr.cpp)
                            + stub fallback (frontend_stub.cpp)
  ast/                      AST node types + AST builder (parse tree -> AST)
  compiler/                 AST -> bytecode compiler
  vm/                       Value types, opcodes, Proto, the VM itself
  builtins/                 Built-in functions and string/list/dict methods
  api/                      c_api.cpp implements ava.h over the VM
  cli/                      ava_cli, a standalone reference host
scripts/                    .ava example/test scripts
docs/                       Language reference (types, operators, control
                            flow, functions, classes, f-strings, builtins,
                            string/list/dict methods, opcodes)
DESIGN.md                   Full architecture writeup
```

## License

Not yet decided -- add a LICENSE file before publishing.
