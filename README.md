# AvaLang

An embeddable scripting language: Python-like syntax, Lua-like dynamic
semantics (dynamic typing, closures, coroutines), single C++ core exposed
through a plain C API so it can be embedded from any language with FFI
(C#, Python, Java, Rust, Godot/GDExtension, ...).

See `DESIGN.md` for the full architecture writeup (value model, bytecode
opcode table, VM/coroutine design, host binding examples).

"AvaLang" is a working name -- rename freely, nothing in the code depends
on it.

## Current status

This is the project skeleton, not a finished language yet. What is real
and working right now:

- `grammar/AvaLang.g4` -- validated: generates a clean C++ parser via
  ANTLR4 with zero errors/warnings.
- `src/vm/` -- a working register-based bytecode VM. Verified: hand-built
  bytecode (`tests/vm_smoke_test.cpp`) executes correctly end to end.
- `include/ava.h` + `src/api/c_api.cpp` -- the public C API, implemented
  over the VM (VM lifecycle, globals, native function registration,
  string/list/dict value helpers). Coroutine resume/yield are stubbed.
- `src/frontend/denter.*` -- the INDENT/DEDENT token injector, written
  against the ANTLR4 C++ runtime API.
- `src/cli/main.cpp` (`ava_cli`) -- standalone binary that compiles and
  runs a `.ava` file; builds and links today even without ANTLR present
  (it will report a clear "frontend not built" error until the AST
  builder + compiler are implemented).

What is intentionally NOT implemented yet (see DESIGN.md, "Suggested
build order"):

- AST node types + the visitor that builds an AST from the ANTLR4 parse
  tree (`src/frontend/frontend_antlr.cpp` has the lexer/denter/parser
  wiring done, and stops right before this step).
- The compiler (AST -> Proto/bytecode).
- Closures/upvalues, classes, and coroutine resume/yield in the VM
  (opcodes are defined in `src/vm/opcodes.h`; several are not yet
  handled by `VM::ExecuteFrame`).

## Building

Requires CMake >= 3.20 and a C++20 compiler.

### Windows

Quickest path -- installs vcpkg + antlr4 (static-md triplet, no extra
DLLs to redistribute) + the ANTLR4 tool jar, then builds automatically:

```bat
install.bat
```

Run it once from an elevated or regular terminal (needs `git` on PATH;
`java` is optional, only used to regenerate the parser). See
`install.bat` header comments for options (`install.bat skipbuild`,
custom vcpkg path, etc).

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
still succeeds using the stub frontend (see "Current status" below).
See `third_party/README.md` for the alternative of dropping an ANTLR4
"complete" jar in manually.

### Linux / macOS

```bash
mkdir build && cd build
cmake -DAVA_BUILD_TESTS=ON ..
cmake --build .
./tests/vm_smoke_test     # exercises the VM directly with hand-built bytecode
./ava_cli path/to/script.ava
```

The build works with or without ANTLR4 installed:

- **Without** the ANTLR4 C++ runtime, CMake falls back to a stub frontend.
  The VM, C API, and CLI all still build and link; `ava_compile()` returns
  a clear error explaining what's missing.
- **With** `antlr4-runtime` discoverable by `find_package` AND an ANTLR4
  complete jar discoverable by CMake (checked under `/usr/local/lib`,
  `/usr/share/java`, and `third_party/`) AND `java` on PATH, CMake
  regenerates the parser from `grammar/AvaLang.g4` automatically and
  compiles the real frontend (`src/frontend/frontend_antlr.cpp`).

To get the full frontend building, install:
- the ANTLR4 C++ runtime (build from source or via your package manager;
  see https://github.com/antlr/antlr4/tree/master/runtime/Cpp)
- an `antlr-4.x-complete.jar` (used only at build time to regenerate the
  parser, not a runtime dependency)

## Project layout

```
grammar/AvaLang.g4        The language grammar
include/ava.h              Public C API -- the only header bindings need
src/
  frontend/                Lexer/denter/parser wiring (frontend_antlr.cpp)
                            + stub fallback (frontend_stub.cpp)
  vm/                       Value types, opcodes, Proto, the VM itself
  api/                      c_api.cpp implements ava.h over the VM
  cli/                      ava_cli, a standalone reference host
bindings/
  csharp/ python/ java/     Placeholder folders for FFI wrappers
tests/                      vm_smoke_test.cpp, hand-built bytecode test
DESIGN.md                   Full architecture writeup
```

## License

Not yet decided -- add a LICENSE file before publishing.
