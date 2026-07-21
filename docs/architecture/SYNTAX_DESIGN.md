# AvaLang — Syntax Design & Future Direction

Status: **design decisions from discussion, not yet implemented in the
grammar**. The current `grammar/AvaLang.g4` still uses the Python-style
(indentation + `:`) block syntax. This document records what the syntax
is moving *toward*, so implementation work (grammar rewrite, AST builder,
compiler) has a single source of truth to build against instead of
re-deriving it from chat history.

See `DESIGN.md` for the VM/bytecode/architecture side of the project,
which this document does not replace.

---

## 1. Block delimiters

**Decision: `then` / `do` to open a block, generic `end` to close any
block.** No significant indentation, no `:`.

This is the single biggest syntax change from the current grammar, and
it is not just an aesthetic choice — it removes the Denter layer
(`src/frontend/denter.h`, `denter.cpp`, `DenterTokenSource` in
`frontend_antlr.cpp`) entirely. Indentation becomes purely a style
convention; the parser does not need INDENT/DEDENT tokens at all, which
also removes a whole category of bugs (mixed tabs/spaces, ambiguous
dedent levels, etc.) that indentation-sensitive grammars are prone to.

```
if a > b then
    print("mayor")
elif a < b then
    print("menor")
else
    print("igual")
end

while x < 10 do
    x = x + 1
end

for item in lista do
    print(item)
end

func suma(a, b)
    return a + b
end

class Animal
    func hablar()
        print("...")
    end
end
```

Note `func` and `class` headers don't need `then`/`do` before the body —
the closing `)` (func) or the class name (class) is already unambiguous,
same as Lua.

### Condition parentheses: optional

**Decided.** `if`/`elif`/`while` conditions may optionally be wrapped in
parentheses; both forms are valid and mean the same thing:

```
if x == 1 then
    print("one")
end

if (x == 1) then
    print("one")
end
```

Grammar-wise this just falls out of `(expr)` already being a valid
primary (`groupAtom`) — no new rule needed, just confirming neither form
is rejected.

### Rejected alternatives (kept here so we don't re-litigate them)

- **Indentation + `:`** (current grammar): requires the Denter; every
  indentation edge case is a potential parser bug. Rejected mainly for
  implementation-complexity reasons, not because it looks bad.
- **`{ }`** (C/JS/Swift/Kotlin/Rust/Go style): actually the *majority*
  choice among languages people currently call "modern" — arguably more
  "modern" by raw popularity than `then/do/end`. Rejected here because
  the goal for AvaLang is a relaxed scripting feel (Lua/Ruby lineage),
  not a C-family feel. Worth revisiting if that goal changes.
- **`end if` / `end while` / `end func`** (VB style): rejected as too
  verbose — a single generic `end` does the same disambiguation job with
  less typing.

---

## 2. Variables, scope: `local` vs `global`

**Open decision, leaning toward Lua's model.** Today's grammar has a
`global NAME` statement but *no* `local` keyword — which means `global`
is currently meaningless, since every variable is already implicitly
global (confirmed in `ast_builder.cpp`/`compiler.cpp`: all `Name`
targets compile to `SETGLOBAL`/`GETGLOBAL`).

Proposed direction (matches `DESIGN.md`'s own stated goal of "Lua-like
dynamic semantics"):

```
x = 10              # global at top level

func foo()
    local y = 5      # local to foo; lives in a VM register, not globals
    x = x + y         # x still refers to the global above
end
```

Default is **global unless declared `local`** — the opposite of
Python's "local unless declared `global`" rule. This is simpler to
compile: Python's rule requires a full-body pre-scan of a function to
determine which names are local before compiling a single statement;
Lua's rule needs no pre-scan, a name is local from its `local`
declaration onward, textually.

This decision blocks real function/closure compilation (the next
build-order step in `DESIGN.md`) and should be settled before that work
starts.

---

## 3. Functions and lambdas

```
func suma(a, b)
    return a + b
end
```

Lambdas, two forms:

```
# Short form: single expression, arrow syntax (sugar)
callback = (x) => x * 2
on_click = (evento) => print("Click: " + evento.nombre)

# Long form: multi-statement body
on_click = func(evento)
    print("Click en: " + evento.nombre)
end
```

The short (`=>`) form is sugar for a `func(...) return <expr> end`
lambda — parity with how Kotlin/C#/Swift offer both a short
single-expression lambda and a full block form. Not yet implemented in
the grammar.

### `this` — implicit, not a declared parameter

```
func hablar()
    print(this.nombre)   # 'this' is available without being a parameter
end
```

Chosen over Python/Rust-style explicit `self`/`&self` first parameters,
matching the majority of what's currently called "modern" (JS, C#,
Java, Kotlin, Swift all do this implicitly). The compiler will need to
inject `this` as an implicit first local in every method body.

---

## 4. Classes and object construction

```
class Animal
    func init(nombre)
        this.nombre = nombre
    end

    func hablar()
        print(this.nombre + " hace un sonido")
    end
end

class Perro : Animal
    func hablar()
        print(this.nombre + " dice guau")
    end
end

p = Perro("Fido")   # no 'new' -- calling the class invokes init()
p.hablar()
```

No `new` keyword — calling a class like a function constructs an
instance and runs `init`. Matches Kotlin/Swift/Python/Ruby (all dropped
`new` deliberately); rejected the Java/C#/JS style of requiring it.

Inheritance via `class Child : Parent` (C#-style colon, chosen over an
earlier `class Child(Parent)` draft specifically because `:` is already
free — see §1, blocks use `then`/`do`/`end` instead of `:`). Single
inheritance only, no multiple inheritance — kept intentionally simple;
can be revisited later without breaking this syntax (C# itself extends
the same colon to a comma list for interfaces: `class Perro : Animal,
IMascota`, so there's room to grow here without a syntax change).

**Confirmed:** the class header only ever takes the parent (`class
Child : Parent`), never constructor parameters (rejecting a
`class Point(x, y)` primary-constructor-in-header pattern that surfaced
during implementation — see §6a). Constructor parameters always go on
`func init(...)` inside the body, as shown above and below.

### Constructors: one `init` with default parameters, not overloading

C# allows several constructors with different signatures
(`Animal(string, int)`, `Animal(string)`, `Animal()`), delegating between
them with `: this(...)`. AvaLang does not do overload resolution
(picking a function by argument count/type is real compiler work and
cuts against the "keep it simple" direction chosen throughout this
document). Since the grammar already supports default parameter values
(`param: NAME ('=' expr)?`), the idiomatic AvaLang equivalent — same
approach Python takes — is a single `init` with defaults:

```
class Animal
    func init(nombre = "Sin nombre", edad = 0)
        this.nombre = nombre
        this.edad = edad
    end
end

a1 = Animal("Rex", 3)
a2 = Animal("Fido")        # edad defaults to 0
a3 = Animal()               # both default
```

True overload resolution (multiple `init`s, dispatched by arity) was
discussed as an alternative (**Option 2** below) but is **not decided
yet** — see §5a.

**Option 2, for comparison** — real overloading, same name repeated,
resolved by argument count:

```
class Animal
    func init(nombre, edad)
        this.nombre = nombre
        this.edad = edad
    end

    func init(nombre)
        init(nombre, 0)     # delegates to the variant above
    end

    func init()
        init("Sin nombre", 0)
    end
end
```

This needs real compiler support (functions sharing a name, storing
each variant, dispatching by arg count at call time — and `base(...)`
would also need to pick which parent `init` to call) that the
default-parameter approach above avoids entirely. Leaning toward
Option 1 (defaults) to keep the compiler simpler, but not locked in.

### Calling the parent constructor/method: `base`

**New keyword, not yet in the grammar** (named `base` rather than the
more common `super`, matching C# rather than Java/JS/Python/Ruby).
`base(...)` calls the parent class's `init`; `base.method_name()` calls
the parent's version of an overridden method (needed once a child
redefines a method but still wants to invoke the base behavior from
inside the override).

```
class Animal
    func init(nombre)
        this.nombre = nombre
    end

    func hablar()
        print(this.nombre + " hace un sonido")
    end
end

class Perro : Animal
    func init(nombre, raza)
        base(nombre)               # calls Animal's init
        this.raza = raza
    end

    func hablar()
        print(this.nombre + " (" + this.raza + ") dice guau")
    end
end
```

### No `virtual`/`override` — decided

Considered a C#-style explicit model (base method marked `virtual`,
child override marked `override`, compiler error on mismatch) against
the simpler Python/Ruby model (every method redefinable in a subclass,
no keywords, no compiler-enforced intent). **Decision: no
`virtual`/`override` keywords** — any method can be redefined in a
subclass silently, consistent with the "simpler than C#" direction
chosen for the rest of the language. This does mean a typo'd method
name in a subclass silently fails to override anything instead of
erroring — an accepted tradeoff for less ceremony.

---

## 5. How this compares to current "modern" languages

| Element | AvaLang (proposed) | Aligns with |
|---|---|---|
| Block delimiters | `then`/`do` ... `end` | Lua, Ruby, Elixir (minority vs. the `{ }` majority) |
| Function declaration | `func name(...) ... end` | Swift, Go (keyword), differs only in block style |
| Short lambda | `(x) => expr` | Kotlin, C#, Swift, JS/TS |
| Long lambda | `func(x) ... end` | Lua |
| `this`/`self` | implicit | JS, C#, Java, Kotlin, Swift (majority) |
| Object construction | no `new` | Kotlin, Swift, Python, Ruby |
| Variable scope default | global unless `local` | Lua (opposite of Python's default-local) |

Net effect: AvaLang's block syntax reads as a scripting language
(Lua/Ruby lineage) while its function/object ergonomics track the
current mainstream (Kotlin/Swift/C#-style implicit `this`, no `new`,
short lambdas). This mix is a deliberate choice, not an oversight — flag
it here in case it needs revisiting once real usage exposes friction.

---

## 5a. Open questions not yet settled

- **Function-declaration keyword**: currently `func` throughout this
  document, but `fn` (Rust-style, terser) and `function` (Lua-style,
  matches this project's own "Lua-like semantics" framing) were both
  discussed as alternatives. Not decided — revisit before the grammar
  rewrite so it only needs to be typed once.
- **`local`/`global` default** (§2): leaning Lua-style (global unless
  declared `local`) but not formally locked in yet.
- **Constructor overloading** (§4): leaning toward Option 1 (single
  `init` with default parameters) over Option 2 (real overloading,
  multiple `init`s dispatched by arity) to avoid adding overload
  resolution to the compiler, but not formally locked in yet.
- **One-liner blocks with `:`**: whether to allow `if cond: stmt` as a
  shorthand exception on top of `then`/`do`/`end` for the multi-line
  form, or reject `:` entirely and always require the full form. Raised
  after `PROGRESS.md` (see §5b) showed one example still using `:` —
  no decision made either way yet.

## 5b. Known drift between this document and the actual codebase

`PROGRESS.md` (a snapshot of the real, currently-building implementation)
was compared against this document and diverged in a few places. Each
was checked against the person building it; the items below are
**confirmed decisions — the codebase is what needs to catch up, not this
document**:

- `self` (explicit first parameter) appeared in `PROGRESS.md`'s
  examples — confirmed wrong; §3's implicit `this` stands.
- `__init__` appeared instead of `init` — confirmed wrong; `init` stands.
- Both `base()` and `super()` appeared (inconsistently) — confirmed
  `base()` is the only one; drop `super()` entirely if it exists as a
  working alias in the current parser/compiler.
- `class Point(x, y)` (constructor parameters in the class header
  itself, as a primary-constructor pattern) appeared — confirmed
  rejected; see the note added under §4 above. Constructor parameters
  belong on `func init(...)` inside the class body only.
- `if (x == 1) then` (parenthesized condition) appeared — this one
  turned out fine, now explicitly documented as optional in §1.

If other examples in `PROGRESS.md` (or the real grammar file) turn up
more of these, treat this document as the source of truth for anything
marked "Decided" or "Confirmed" above, and flag anything not yet marked
either way (§5a) as a real open question to settle, not an
implementation bug.

---

## 6. Current implementation status (as of this document)

Updated from `PROGRESS.md` (2026-07-18), which reflects the actual
current codebase — this supersedes the older status snapshot this
section used to contain, from back when the grammar was still
Python-style and only expressions/assignment/`if`/`while` compiled.

- Grammar: already moved to `then`/`do`/`end` blocks (§1), no more
  indentation-based parsing — the Denter should no longer be needed;
  confirm it's actually been removed from the build, not just unused.
- VM: register-based interpreter with arithmetic, comparison,
  control-flow, lists, dicts, classes/instances (`GETATTR`/`SETATTR`,
  `NEWCLASS`/`NEWINSTANCE`, bound methods), inheritance (`BASECALL`),
  and closures (`CLOSURE` opcode). Coroutines (`YIELD`/`RESUME`) and
  upvalues (`GETUPVAL`/`SETUPVAL`) are declared but not yet wired up.
- Compiler/AST: functions (incl. nested/closures), classes with
  methods and `__init__`-or-`init` (see §5b — naming needs
  reconciling), `if`/`elif`/`else`, `while`, `for ... in ...`,
  `break`/`continue`, lists/dicts.
- C API + C# binding: working end-to-end.

Known gaps per `PROGRESS.md`: no `import`, no exceptions
(`try`/`except`), no decorators, no comprehensions, no coroutines, no
built-in functions yet (`len`, `range`, `map`, ...), no string/list
methods.

Remaining work to fully match this document, roughly in priority order:

1. Reconcile §5b's known drift (`self`→`this`, `__init__`→`init`, drop
   `super()`, drop class-header constructor params) in the real grammar
   and compiler.
2. Settle the still-open items in §5a (function keyword, `local`/
   `global`, constructor overloading, one-liner `:`).
3. Decide and implement the built-in/standard-library surface (`len`,
   `range`, string/list methods) — not a syntax question, but blocks
   writing realistic scripts either way.
4. Add the `=>` short-lambda sugar (§3) — not yet mentioned as done in
   `PROGRESS.md`.
5. `try`/`except`, `prop` (get/set), `static` members — all discussed
   in chat as candidate additions, none decided or built yet.

---

## 7. Future direction: a component framework (React/Blazor-like)

**This section is exploratory intent, not a committed design.** No
grammar or VM work should be done for this until section 6's list above
is complete — the component model would sit on top of real functions,
closures, and classes, and designing it earlier means guessing at a
foundation that can still move.

### What "React/Blazor as a concept" actually breaks down into

1. **Components** — a function or class that takes data (props) and
   returns a UI description.
2. **Reactive state** — a change to some value automatically triggers
   re-rendering of whatever depends on it.
3. **Virtual tree + diffing** — build a lightweight in-memory
   representation of the UI, diff it against the previous one, and
   apply only the minimal real changes.
4. **A bridge to a real renderer** — the browser DOM, or (more likely
   for a first version) the host application via the same C API /
   P/Invoke bridge this project already has working.

### What AvaLang's planned language features already cover

| Piece | Covered by | 
|---|---|
| Component = function/class returning a data structure | Yes, once classes ship — lists/dicts (`NEWLIST`/`NEWDICT`/`GETINDEX`/`SETINDEX`, already implemented) are enough to represent a virtual node like `{tag: "div", props: {...}, children: [...]}` |
| Event handlers capturing component state | Yes, once closures ship |
| Stateful objects (`this.count = this.count + 1`) | Yes, once classes + `this` ship |
| Host bridge (AvaLang <-> C#/DOM) | Already working today via `include/ava.h` + the C# P/Invoke binding |

### What is explicitly NOT part of the language and would be a
separate project built on top of it

- **The diffing algorithm** — a library written in AvaLang (or in C++
  exposed to AvaLang), not a language feature.
- **Automatic reactivity** — making `this.count = 5` trigger a
  re-render without an explicit call typically needs intercepted
  getters/setters or an explicit "signal" primitive. This may require
  `GETATTR`/`SETATTR` to support a hook, beyond plain dict-backed
  attribute storage.
- **The actual renderer** — translating the virtual tree into real
  HTML/DOM. Reusing an existing renderer (e.g. Blazor's own diffing
  engine via interop) is the pragmatic choice over writing a DOM
  renderer from scratch.

### Illustrative sketch only (not a proposal to build now)

```
component SaludoCard(nombre)
    render()
        div(class: "card")
            h1(nombre)
            if nombre == "" then
                p("Sin nombre")
            end
        end
    end
end
```

This shows why `then/do/end` (§1) matters for this future direction:
it leaves `:` free for component props/attributes syntax later, instead
of colliding with the block-delimiter `:` that Python-style syntax
would have used.

### Bottom line

Nothing in the current design closes this door. The language gives the
right primitives (first-class functions, closures, objects, and a
working native-host bridge) without committing to any specific
component/markup syntax yet — that syntax should be designed once real
usage of the base language (once closures and classes exist) shows what
actually feels ergonomic, rather than guessed at now.


## AI Language Design Constitution (Proposed)

### Core philosophy
- AvaLang is a modern embeddable scripting language.
- Primary inspiration: Visual Basic 6 block readability, not C-family braces.
- Secondary influences: Lua (embedding, end blocks), Python (clarity), modern languages (lambdas, slices, collections).
- Every future syntax decision must preserve readability over terseness.

### Non-negotiable syntax rules
1. All executable blocks end with `end`.
2. `then` opens conditional blocks.
3. `do` opens loop blocks.
4. Braces `{}` are reserved for data literals only (maps/dictionaries), never code blocks.
5. `[]` represent indexing, slicing and list literals.
6. `()` are only for grouping, calls and parameter lists.
7. Prefer one obvious way to express a construct.

### Identity
Do not evolve toward "another C#".
Do not evolve toward JavaScript syntax.
Target identity:
> Modern Visual Basic + Lua philosophy + DSL capabilities.

### Future DSL requirements
The grammar should naturally support declarative APIs such as:

```ava
component Login

    state username = ""

    view

        column

            text("Hello")

            button
                text = "Login"
            end

        end

    end

end
```

The parser should remain extensible enough that component-like DSLs can be introduced without breaking existing grammar.

### Checklist before adding grammar features
- Is it consistent with existing keywords?
- Does it preserve block syntax with `end`?
- Does it avoid introducing braces for executable blocks?
- Can it be represented cleanly in the AST?
- Does it improve DSL expressiveness?
- Does it preserve readability?
- Would a new user immediately recognize it as AvaLang?

