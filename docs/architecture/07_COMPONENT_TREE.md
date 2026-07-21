# AvaLang Architecture
# 07 - Component Tree

Version: Draft 1.0

---

# Overview

This document describes the Component Tree architecture used by future AvaLang frameworks such as AvaUI, AvaWeb and AvaGame.

The Component Tree is the abstraction layer between AvaLang and every rendering backend.

```
Ava Source
    │
Compiler
    │
Bytecode
    │
VM
    │
Component Tree
    ├── HTML Renderer
    ├── Avalonia Renderer
    ├── Godot Renderer
    ├── Metal Renderer
    └── Vulkan Renderer
```

---

# Philosophy

The language never renders directly.

It builds a platform-independent tree of components.

---

# AST vs Component Tree

AST represents source code.

Component Tree represents the UI hierarchy.

Example:

```ava
window

    column

        text("Hello")

        button

        end

    end

end
```

Produces:

```
Window
└── Column
    ├── Text
    └── Button
```

---

# Design Rules

1. The tree must be platform independent.
2. Every UI element is a ComponentNode.
3. Renderers never modify the tree.
4. The compiler must not know about HTML or native widgets.
5. New frameworks should build trees instead of rendering directly.

---

# Long-Term Vision

Future frameworks:

- AvaUI
- AvaWeb
- AvaGame
- AvaEditor

All should generate the same abstract component tree while different renderers transform it into platform-specific output.
