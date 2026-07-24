# AvaLang

**A small, embeddable scripting language with a Visual&nbsp;Basic-flavored,
Lua-hearted syntax.** Dynamic typing, closures, classes with single
inheritance, and coroutines — all built around a register-based bytecode
VM and exposed through one plain C API, so any host language with FFI
(C#, Python, Java, Rust, Godot/GDExtension, ...) can embed it in an
afternoon.

> "AvaLang" is a working name — rename freely, nothing in the code
> depends on it. License: **not yet decided**, add a `LICENSE` file
> before publishing.

---

## Table of Contents

- [Why AvaLang?](#why-avalang)
- [Features](#features)
- [Quick Example](#quick-example)
- [Building](#building)
  - [Windows](#windows)
  - [Linux / macOS](#linux--macos)
- [Running a Script](#running-a-script)
- [Project Layout](#project-layout)
- [Documentation](#documentation)
- [Status](#status)
- [License](#license)

---

## Why AvaLang?

Most embeddable scripting languages either copy Python's indentation
rules or C's braces. AvaLang does neither on purpose. Blocks open with
`then`/`do` and close with a single generic `end` — no significant
whitespace, no mixed-tabs-and-spaces bugs, no `:` — closer to Visual
Basic and Lua than to Python or JavaScript. On top of that scripting
feel, object and function ergonomics track what's currently mainstream:
implicit `this`, no `new`, short arrow lambdas.

The result is meant to read like a relaxed scripting language while
staying easy to parse, easy to embed, and easy to extend with new
domain-specific syntax.

## Features

**Language**
- Dynamic typing: `nil`, `bool`, `number`, `string`, `list`, `dict`,
  `function`, `class`, `instance`, `coroutine`.
- Functions, closures, and both long-form and short arrow lambdas.
- Classes with an `init` constructor, methods, and single inheritance
  (`class Dog : Animal`, calling the parent constructor with `base(...)`).
- Control flow: `if` / `elif` / `else`, `while`, `for`, `break`,
  `continue`, `try` / `catch` / `finally`.
- List/string slicing (`arr[1:3]`, `arr[::2]`, ...).
- F-string interpolation (`$"value: {x}"`).
- A growing set of built-in functions (`print`, `type`, `str`, `len`,
  `range`, `sorted`, `sum`, ...) plus string/list/dict methods.

**Runtime**
- Reference-counted values.
- Register-based bytecode VM (fast dispatch, no tree-walking
  interpreter overhead).
- Coroutines (`yield` / `resume`).

**Embedding**
- The entire engine is one C++ library behind a stable, minimal C API
  (`include/ava.h`) — the only header a host binding ever needs.
- No STL types, no C++ templates, and no parser/AST/VM internals
  leak across that boundary, so bindings in any FFI-capable language
  stay simple translators, never re-implementations.

See [`DESIGN.md`](DESIGN.md) for the full architecture writeup (value
model, bytecode opcode table, VM/coroutine design, host binding
examples).

## Quick Example

```lua
class Animal
    func init(name)
        this.name = name
    end

    func speak()
        print("...")
    end
end

class Dog : Animal
    func init(name)
        base(name)
    end

    func speak()
        print(this.name + " says Woof!")
    end
end

func greet(person)
    print($"Hello {person}!")
end

greet("Ava")

d = Dog("Buddy")
d.speak()

nums = [1, 2, 3, 4, 5]
print(nums[1:3])   # [2, 3]
print(nums[::2])   # [1, 3, 5]

for n in nums then
    print(n)
end

i = 0
while i < 3
    print("tick " + str(i))
    i = i + 1
end

try
    raise "something went wrong"
catch e
    print("Caught: " + str(e))
end
```

## Building

Requires CMake >= 3.20 and a C++20 compiler. The full frontend needs the
ANTLR4 C++ runtime and an ANTLR4 tool jar (used only at build time, to
regenerate the parser from `grammar/AvaLang.g4`); without them, the
project still builds using a stub frontend that reports a clear error
instead of compiling scripts.

### Windows

Quickest path — installs vcpkg + antlr4 (static-md triplet, no extra
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
build.bat debug        REM Debug build
build.bat test         REM also builds and runs tests via ctest
build.bat ninja        REM use Ninja instead of MSBuild (needs ninja.exe on PATH)
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

## Running a Script

```bash
ava_cli path/to/script.ava
```

## Project Layout

```
grammar/AvaLang.g4        The language grammar (ANTLR4)
include/ava.h             Public C API -- the only header host bindings need
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
DESIGN.md                   Full architecture writeup
```

## Documentation

- [`DESIGN.md`](DESIGN.md) — VM/bytecode/value-model deep dive.

## Status

Core language, VM, and C API build and run end-to-end today (see the
Quick Example above — all of it runs). Known gaps and open syntax
questions are tracked in the docs rather than hidden:

- `docs/SYNTAX_DESIGN.md` records settled vs. still-open syntax
  decisions (e.g. the `func`/`fn`/`function` keyword choice, constructor
  overloading).
- `docs/architecture/07_SYNTAX_ERRATA.md` tracks known mismatches
  between the grammar and the design docs (e.g. `while`/`for` loop
  delimiters) that haven't been resolved yet — check there before
  relying on an edge case not shown in this README.

## License

Not yet decided — add a `LICENSE` file before publishing.

