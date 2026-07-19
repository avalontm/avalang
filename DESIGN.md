# AvaLang Design Document

Working name "AvaLang" — rename freely, it is not load-bearing anywhere in the
grammar or bytecode format.

## 1. Pipeline overview (C++ core, C ABI, thin bindings)

The entire language — grammar, AST builder, compiler and VM — lives in a
single C++ core, compiled to a shared library (`avalang.dll` / `libavalang.so`
/ `libavalang.dylib`). The library exposes a plain **C API** (`include/ava.h`,
`extern "C"`, no C++ types crossing the boundary) so it can be consumed from
literally any language with an FFI/interop mechanism, without writing a
second implementation of anything:

```
source.ava
    |
  Lexer (ANTLR4 generated, C++ target)
    |
  Denter (whitespace -> INDENT/DEDENT token stream, hand-written wrapper)
    |
  Parser (ANTLR4 generated, C++ target)  -> parse tree
    |
  AST builder (visitor over parse tree)                    |
  Compiler (AST -> bytecode)                                }  all C++, once
  VM (register-based bytecode interpreter)                 |
    |
  libavalang.so / avalang.dll / libavalang.dylib
    |
  ava.h  (extern "C" API: opaque handles + ava_value_t)
    |
  +---------------+---------------+----------------+----------------+
  |               |               |                |                |
 C# binding   Python binding  Java binding     Rust binding     Godot / GDExtension
 (P/Invoke)   (ctypes/cffi)   (JNI/JNA)       (bindgen/FFI)     (direct C ABI)
```

This is the same approach Lua, SQLite, and most serious embeddable engines
use: **one native core, N thin bindings**. A binding only needs to translate
that host language's native types to/from `ava_value_t` and call the C API —
it never touches parsing, compiling, or the interpreter loop.

## 2. Value model

Dynamic typing, Lua-style tag union:

```
Nil
Bool
Number      (double, single numeric type, avoids int/float split bugs)
String      (immutable, interned for short strings)
List        (mutable, growable array)
Dict        (mutable, ordered hash map, string/number keys)
Function    (proto + captured upvalues = closure)
NativeFunc  (host-provided callback, opaque to the VM)
Instance    (class instance: class reference + field dict)
Class       (name, parent ref, method table)
Coroutine   (suspended execution state, see section 5)
```

Lists and dicts are kept as distinct types rather than unifying into a single
Lua-style table, per your v1 requirement. Classes are a thin layer on top of
Dict-backed instances plus a method table resolved through the class chain.

## 3. Bytecode: register-based, per-function

Each compiled function is a `Proto`:

```
Proto {
    num_registers   : u16
    num_params      : u8
    is_vararg       : bool
    constants       : Value[]      // literals used by LOADK
    upvalue_descs    : UpvalDesc[] // where each upvalue comes from
    instructions    : Instr[]
    child_protos    : Proto[]      // nested function/lambda bodies
    debug_lines     : u32[]        // instruction -> source line, optional
}
```

`Instr` is a fixed-width encoded op + up to 3 register/constant operands,
same layout family as Lua's own `iABC`/`iABx` encoding:

| Opcode        | Operands        | Effect                                      |
|---------------|-----------------|----------------------------------------------|
| LOADK         | A, Bx           | R[A] = K[Bx]                                  |
| LOADNIL       | A               | R[A] = nil                                    |
| LOADBOOL      | A, B            | R[A] = (bool)B                                |
| MOVE          | A, B            | R[A] = R[B]                                   |
| GETUPVAL      | A, B            | R[A] = Upval[B]                               |
| SETUPVAL      | A, B            | Upval[B] = R[A]                               |
| GETGLOBAL     | A, Bx           | R[A] = Globals[K[Bx]]                         |
| SETGLOBAL     | A, Bx           | Globals[K[Bx]] = R[A]                         |
| NEWLIST       | A               | R[A] = new empty List                         |
| NEWDICT       | A               | R[A] = new empty Dict                         |
| LISTAPPEND    | A, B            | R[A].append(R[B])                             |
| GETINDEX      | A, B, C         | R[A] = R[B][R[C]]                             |
| SETINDEX      | A, B, C         | R[A][R[B]] = R[C]                             |
| GETATTR       | A, B, Bx        | R[A] = R[B].K[Bx]                             |
| SETATTR       | A, Bx, C        | R[A].K[Bx] = R[C]                             |
| ADD/SUB/MUL/DIV/MOD/POW | A,B,C | R[A] = R[B] op R[C]                          |
| NEG           | A, B            | R[A] = -R[B]                                  |
| NOT           | A, B            | R[A] = !truthy(R[B])                          |
| EQ/LT/LE      | A, B, C         | R[A] = R[B] cmp R[C]                          |
| JMP           | sBx             | pc += sBx                                     |
| TEST          | A, C            | if truthy(R[A]) != C then pc++ (skip next JMP)|
| CALL          | A, B, C         | R[A..] = call R[A](R[A+1..A+B-1]), C results  |
| RETURN        | A, B            | return R[A..A+B-1]                            |
| CLOSURE       | A, Bx           | R[A] = make closure from child_protos[Bx]     |
| NEWCLASS      | A, Bx           | R[A] = new Class from class template K[Bx]    |
| NEWINSTANCE   | A, B            | R[A] = new Instance of class R[B]             |
| YIELD         | A, B            | suspend coroutine, yielding R[A..A+B-1]       |
| RESUME        | A, B, C         | resume coroutine R[B] with args, C results    |

This is intentionally close to Lua 5.1's real opcode table because that
design has been battle-tested for exactly this use case; deviating from it
mainly to add list/dict/class primitives Lua does not have natively.

## 4. VM execution model

The VM is an explicit loop, not host-language recursion:

```
loop:
    instr = frame.proto.instructions[frame.pc++]
    switch instr.opcode:
        case ADD: frame.registers[instr.A] = add(frame.registers[instr.B], frame.registers[instr.C])
        case CALL: push new CallFrame, continue loop with new frame
        case RETURN: pop CallFrame, copy results into caller registers
        ...
```

Call frames live on an explicit VM-managed stack (`List<CallFrame>`), not on
the host call stack. This is the property that makes coroutines possible
without OS threads: a coroutine is just its own independent frame stack that
can be swapped in and out of the interpreter loop.

## 5. Coroutines

```
Coroutine {
    frames       : CallFrame[]     // this coroutine's own call stack
    status       : Suspended | Running | Dead
}
```

- `RESUME co, args` swaps the VM's active frame stack to `co.frames`, pushes
  args into the resumed frame's expected registers, and continues the
  interpreter loop using that stack.
- `YIELD values` copies `values` out, marks the coroutine `Suspended`, and
  returns control to whichever frame stack called `RESUME` (saved on a small
  "resume stack" of coroutine references).
- No native recursion, no threads, no async/await machinery needed on any
  host. This is exactly Lua's approach and it is why Lua coroutines are
  cheap (a coroutine is just a heap-allocated frame stack).

Host-facing API (per language) is intentionally symmetric:

```
var co = vm.CreateCoroutine(function);
var (values, status) = vm.Resume(co, args);
```

## 6. Host binding (via the C API)

Bindings are thin wrappers over `ava.h`. `ava_value_t` is a small POD struct
(tag + double/int/opaque-ref union) that is trivial to marshal in every FFI
system, so a binding is typically a few hundred lines: map native types to
`ava_value_t`, wrap the opaque handles in a disposable/finalizable class, and
forward native-function callbacks through the host's own FFI callback
mechanism (P/Invoke delegates, `ctypes.CFUNCTYPE`, JNI native methods).

Example, C# via P/Invoke:

```csharp
[DllImport("avalang")] static extern IntPtr ava_vm_create();
[DllImport("avalang")] static extern void ava_vm_register_native(
    IntPtr vm, string name, AvaNativeFn fn, IntPtr userData);

AvaNativeFn print = (vm, args, count, userData) => {
    Console.WriteLine(string.Join(" ", MarshalArgs(args, count)));
    return AvaValue.Nil;
};
ava_vm_register_native(vm, "print", print, IntPtr.Zero);
```

Example, Python via ctypes:

```python
lib = ctypes.CDLL("./libavalang.so")
vm = lib.ava_vm_create()

@AVA_NATIVE_FN
def py_print(vm, args, count, user_data):
    print(*[marshal(args[i]) for i in range(count)])
    return AvaValue.nil()

lib.ava_vm_register_native(vm, b"print", py_print, None)
```

`CALL` on an `AVA_NATIVE` value just invokes the registered function pointer
directly — no bytecode frame is pushed for native calls, so crossing into
host code has effectively zero VM overhead beyond the FFI call itself.

## 7. Project layout

```
avalang/
  CMakeLists.txt
  include/
    ava.h                 <- the only header external bindings need
  grammar/
    AvaLang.g4
  src/
    ast/                  AST node types + ANTLR visitor -> AST builder
    compiler/              AST -> Proto/bytecode
    vm/                    value.cpp, vm.cpp (interpreter loop), coroutine.cpp
    api/                   c_api.cpp (implements ava.h over the internal VM)
    cli/                   ava_cli: standalone `ava run script.ava` binary,
                            useful for testing without any binding at all
  bindings/
    csharp/                NuGet-able P/Invoke wrapper
    python/                pip-installable ctypes/cffi wrapper
    java/                  JNI/JNA wrapper
  tests/
```

`ava_cli` matters early on: it lets you test the language end-to-end
(parse -> compile -> run) with zero bindings written, before any C#/Python
wrapper exists.

## 8. Suggested build order

1. Finish `AvaLang.g4` semantic details (operator precedence already laid
   out; classMember / trailer rules may need refinement once real scripts
   are written against it). Already validated: generates cleanly with the
   ANTLR4 `Cpp` target, no warnings.
2. Implement the denter token-stream wrapper in C++ (whitespace ->
   INDENT/DEDENT), sitting between the generated lexer and parser.
3. AST node types + a visitor over the ANTLR4 parse tree that builds the AST.
4. Compiler: AST -> Proto (start with expressions + assignment + if/while,
   add functions/closures next, classes and coroutines last).
5. VM interpreter loop (`src/vm/vm.cpp`) implementing the opcode table above.
6. `src/api/c_api.cpp` implementing `ava.h` on top of the internal VM;
   build `ava_cli` and validate real `.ava` scripts run correctly.
7. Only after the core is solid end-to-end: write the first thin binding
   (C# is a reasonable first pick given your stack) to validate the C API
   is ergonomic from a managed language, then Python/Java as needed.
