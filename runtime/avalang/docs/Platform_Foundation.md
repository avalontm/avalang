# PLATFORM_EVOLUTION_PLAN.md

> **Project:** AvaLang
>
> **Status:** Design Planning
>
> **Version:** 1.0
>
> **Applies To:** avalang.dll
>
> **Future Target:** avalang.ui.dll

---

# Introduction

The introduction of the Platform Abstraction Layer (PAL) is one of the most significant architectural milestones in AvaLang.

Its purpose is not only to support multiple operating systems today, but to become the common foundation for the entire Ava ecosystem.

This includes:

- avalang.dll
- avalang.ui.dll
- avahost.exe
- avastudio.exe
- future AvaLang tools

The PAL must be designed as a reusable infrastructure layer rather than a runtime implementation detail.

---

# Long-Term Vision

The Ava ecosystem should be composed of independent modules.

Each module has a single responsibility.

```
                Ava Ecosystem

                avastudio.exe
                      │
                      ▼
                  avacli.exe
                      │
      ┌───────────────┼───────────────┐
      ▼                               ▼
  avalang.dll                 avalang.ui.dll
      │                               │
      └───────────────┬───────────────┘
                      ▼
            Platform Abstraction Layer
                      │
        ┌─────────────┼──────────────┐
        ▼             ▼              ▼
     Windows        Linux         macOS
```

This architecture guarantees that every module behaves consistently regardless of the operating system.

---

# Current Goal

The current objective is **not** to implement AvaUI.

The current objective is to prepare avalang.dll so AvaUI can be integrated naturally in the future.

This means designing the PAL with future requirements in mind instead of only current runtime needs.

---

# Design Principles

The Platform Abstraction Layer must follow these principles.

## 1. Platform Independence

No component outside the PAL may call operating system APIs directly.

Every operating system interaction must pass through platform interfaces.

---

## 2. Small Interfaces

Avoid giant platform managers.

Instead, every responsibility should have its own interface.

Example:

- IFileSystem
- IThread
- IMutex
- ILibrary
- IClock
- IConsole

Small interfaces are easier to maintain, mock and extend.

---

## 3. Platform Isolation

All operating system code must remain inside platform implementations.

Example:

```
platform/

    windows/

    linux/

    macos/
```

The compiler, runtime and VM should never contain platform-specific code.

---

## 4. Future Compatibility

The PAL must expose interfaces that are useful not only today, but also for future modules.

This reduces future refactoring.

---

# Preparing avalang.dll for AvaUI

Although AvaUI does not yet exist, the PAL should already reserve the required extension points.

This avoids breaking changes later.

The following interfaces should be considered during the evolution of the PAL.

---

# Window Abstraction

Future Interface

```
IWindow
```

Responsibilities:

- Create window
- Destroy window
- Resize
- Show
- Hide
- Window state

This interface is not required by avalang.dll today.

It exists to prepare the architecture.

---

# Input System

Future Interfaces

```
IMouse

IKeyboard

ICursor
```

These interfaces will become the input layer used by AvaUI.

They should remain completely independent from rendering.

---

# Clipboard

Future Interface

```
IClipboard
```

Required by:

- TextBox
- TextEditor
- IDE
- UI Controls

---

# Render Surface

Future Interface

```
IRenderSurface
```

Important:

This interface DOES NOT perform rendering.

It only represents a native drawing surface.

Examples:

Windows

HWND

Linux

Wayland

X11

macOS

NSView

The renderer itself belongs to avalang.ui.dll.

---

# Timer Services

Future Interface

```
ITimer
```

Required for:

- async/await
- animations
- dispatcher
- delayed execution
- frame scheduling

---

# Display Information

Future Interface

```
IDisplay
```

Responsibilities:

- Monitor information
- DPI
- Scaling
- Resolution

Required by AvaUI.

---

# Platform Services

In the future the PAL may expose higher-level platform services.

Examples:

- File Dialog
- Native Notifications
- Drag & Drop
- Clipboard
- URI Launcher

These services should remain optional.

---

# Why Prepare This Now?

Because Avalang.dll and Avalang.ui.dll will share the same platform layer.

If the PAL is designed only around today's compiler needs, AvaUI will require a major redesign.

Designing for future expansion avoids architecture breaks.

---

# Responsibilities

## avalang.dll

Responsible for:

- Lexer
- Parser
- AST
- Semantic Analysis
- Compiler
- Bytecode
- Virtual Machine
- Runtime
- Module Loader
- FFI
- Async Runtime

It does not know how windows, keyboards or renderers work.

---

## avalang.ui.dll

Responsible for:

- Component Tree
- Layout Engine
- State Management
- Event System
- Animation
- Theme Engine
- Render Tree
- Renderer
- UI Controls

It does not know operating system APIs directly.

Instead it consumes PAL interfaces.

---

# Future Dependency Graph

```
                avastudio.exe

                       │

                avalang.ui.dll

                       │

                 avalang.dll

                       │

         Platform Abstraction Layer

                       │

      Windows / Linux / macOS
```

Each layer depends only on the layer below.

No circular dependencies are allowed.

---

# Development Strategy

The recommended implementation order is:

Phase 1

Complete PAL for avalang.dll.

Phase 2

Remove remaining platform-specific code.

Phase 3

Stabilize interfaces.

Phase 4

Implement Extern/FFI using ILibrary.

Phase 5

Implement Async Runtime.

Phase 6

Begin avalang.ui.dll.

No UI development should begin before the PAL is considered stable.

---

# Architecture Rule

Every new operating system feature added to AvaLang must answer one question first:

"Should this belong to the Platform Abstraction Layer?"

If the answer is yes, it must be implemented inside the PAL before being consumed by the runtime or UI.

This keeps the architecture consistent across the entire ecosystem.

---

# Final Objective

The ultimate goal is to build a unified platform architecture shared by every Ava component.

When avalang.ui.dll is created, it should not require redesigning avalang.dll.

Instead, it should simply consume the platform interfaces that already exist.

By designing the PAL with future expansion in mind, AvaLang gains:

- Better maintainability
- Cleaner architecture
- Cross-platform consistency
- Easier testing
- Lower coupling
- Faster future development

The Platform Abstraction Layer is not only the foundation of avalang.dll.

It is the foundation of the entire Ava ecosystem.