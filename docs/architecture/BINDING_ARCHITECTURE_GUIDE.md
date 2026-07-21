# AvaLang Binding Architecture Guide

## Purpose
This document defines the architectural rules for creating bindings for AvaLang.

Host Language
    ↓
Binding
    ↓
Stable C API
    ↓
AvaLang Runtime

## Core Principle
A binding is only a translator. It must never implement compiler, parser, VM or runtime logic.

## The C API SHOULD expose
- VM lifecycle
- Script execution
- Module loading
- Function invocation
- Value access
- Error reporting
- Opaque handles

Example:

```c
typedef struct AvaVM AvaVM;
typedef struct AvaValue AvaValue;
```

## The C API MUST NOT expose
- Parser
- Lexer
- Compiler
- AST
- VM internals
- GC internals
- std::string
- std::vector
- C++ templates

## Rule
Everything below the C API is private. Bindings must depend only on the stable C API.

## Long-term Vision
One runtime.
Many bindings.
C#, Python, Rust, Go, Java, Swift and others should all execute the same runtime.
