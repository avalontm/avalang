# AvaLang DSL Vision

## What is a DSL?
A Domain Specific Language is a specialized syntax focused on one domain.

## Philosophy
The compiler remains generic.
DSLs are implemented as libraries/frameworks on top of AvaLang.

Traditional API:

```ava
app.route("/", func()
    return "Hello"
end)
```

DSL:

```ava
page "/"
    text("Hello")
end
```

## Component Tree

Page
└── Column
    ├── Text
    └── Button

The tree is platform-independent.

## Renderers
The same tree can become:
- HTML
- Avalonia
- MAUI
- Godot
- Vulkan
- Metal

Only the renderer changes.

## Design Rules
1. Avoid compiler modifications for DSLs.
2. Preserve AvaLang syntax consistency.
3. Use `end` for blocks.
4. Build trees first, render later.
5. Prioritize readability.

## Vision
AvaLang should become a host language for multiple DSLs such as AvaUI, AvaWeb, AvaGame and AvaDatabase while sharing one compiler, one VM and one runtime.
