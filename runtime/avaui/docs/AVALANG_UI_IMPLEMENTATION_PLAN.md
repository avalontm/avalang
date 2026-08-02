# AVALANG_UI_IMPLEMENTATION_PLAN.md

**Project:** AvaLang

**Module:** avalang.ui.dll

**Status:** Planning

**Version:** 1.0

---

# Overview

This document defines the implementation roadmap for **avalang.ui.dll**.

AvaUI is not intended to be a collection of native operating system controls.

Instead, AvaUI is designed as a platform-independent UI framework that generates a common rendering model capable of targeting multiple rendering backends.

The architecture follows the same philosophy used throughout the Ava ecosystem:

- Small modules
- Clear responsibilities
- Platform abstraction
- Long-term maintainability

The objective is to build a modern UI framework that can be rendered on multiple platforms while sharing a single UI definition.

---

# Goals

The primary goals are:

- Platform independent UI
- Shared Component Tree
- Shared Layout Engine
- Shared Render Tree
- Shared Event System
- Shared State Management
- Multiple Rendering Backends
- Native AvaLang integration

---

# Non Goals

AvaUI is NOT:

- HTML
- CSS
- WinForms
- WPF
- Qt
- Immediate Mode GUI

AvaUI defines its own rendering architecture.

---

# High Level Architecture

```
             .avaui
                |
             Parser
                |
         Component Tree
                |
         Layout Engine
                |
          Render Tree
                |
          Scene Graph
                |
        Render Commands
                |
          Renderer API
      +---------+---------+
      v         v         v
   HTML      Native      PDF
```

Every rendering backend consumes exactly the same Render Commands.

---

# Development Phases

The implementation should follow strict phases.

No phase should begin before the previous one is stable.

---

# Phase 1 - Core Architecture

Goal: Define the entire internal architecture.

Deliverables: Folder structure, Public interfaces, Internal interfaces, Dependency rules.

No rendering. No controls.

---

# Phase 2 - Component Tree

Goal: Create the internal representation of the UI.

Responsibilities: Component hierarchy, Parent/Child relationships, Properties, Identifiers, Slots.

No layout calculations.

---

# Phase 3 - Layout Engine

Goal: Calculate positions and sizes.

Responsibilities: Width, Height, Margin, Padding, Alignment, Row, Column, Stack.

The layout engine never draws.

---

# Phase 4 - State System

Goal: Provide reactive state.

Responsibilities: State, Binding, Change notifications, Property updates.

No rendering.

---

# Phase 5 - Event System

Goal: Dispatch user input.

Responsibilities: Mouse, Keyboard, Pointer, Focus, Click, Input events.

Platform input is obtained through the PAL.

---

# Phase 6 - Render Tree

Goal: Convert components into drawable objects.

A Button becomes: Background -> Border -> Text -> Icon.

The Render Tree contains no platform-specific code.

---

# Phase 7 - Scene Graph

Goal: Prepare rendering.

Responsibilities: Transform hierarchy, Visibility, Clipping, Opacity, Z Order, Dirty Regions.

The Scene Graph exists to optimize rendering.

---

# Phase 8 - Render Commands

Goal: Generate rendering instructions.

Examples: DrawRectangle, DrawRoundedRectangle, DrawText, DrawImage, PushClip, PopClip, Translate, Scale, Rotate.

No renderer-specific logic exists here.

---

# Phase 9 - Renderer Interface

Goal: Create a common rendering contract (IRenderer: DrawRectangle, DrawText, DrawImage, DrawPath, BeginFrame, EndFrame).

Every backend implements this interface.

---

# Phase 10 - HTML Backend

Goal: Generate HTML output. Used by avahost.exe.

---

# Phase 11 - Native Backend

Goal: Provide desktop rendering.

Uses: Platform Abstraction Layer, Render Surface, Window, Mouse, Keyboard, Clipboard.

The rendering technology should remain replaceable.

---

# Folder Structure

```
ui/
    include/avalang/ui/     (public API)
    src/
        parser/
        components/
        layout/
        render/
        scene/
        renderer/
        state/
        events/
        animation/
        themes/
        resources/
        controls/
        platform/
            windows/
            linux/
            macos/
        common/
```

---

# Platform Layer

AvaUI never talks directly to Windows, Linux or macOS.

Instead it consumes services already exposed by the Platform Abstraction Layer (core/platform/interfaces/services/ui/): IWindow, IRenderSurface, IMouse, IKeyboard, ICursor, IClipboard, IDisplay, ITimer.

No operating system APIs should appear outside platform implementations.

---

# Component Model

Every visual element is a Component: Page, Column, Row, Stack, Text, Button, Image, Container, TextBox, Custom Component.

Every component shares the same base architecture.

---

# Rendering Pipeline

```
.avaui -> Parser -> Component Tree -> Layout Engine -> State Updates
       -> Render Tree -> Scene Graph -> Render Commands -> Renderer -> Backend
```

The pipeline is immutable. Each stage has one responsibility.

---

# Dependency Rules

```
Components -> Layout -> State -> Render Tree -> Scene Graph -> Renderer -> Platform Services
```

Dependencies never point upward. Circular dependencies are forbidden.

---

# Integration With AvaLang

avalang.dll provides Parser, Runtime, VM, Async Runtime, Extern.

AvaUI consumes the runtime but remains an independent module. The language never depends on the UI framework.

---

# Integration With AvaHost

AvaHost loads .avaui -> Parser -> Component Tree -> HTML Renderer -> HTML Response.

AvaHost never manipulates components directly.

---

# Integration With AvaStudio

AvaStudio uses the same rendering pipeline as production. No separate preview renderer should exist.

---

# Future Features

Reactive State, Animations, Themes, Localization, Accessibility, Custom Components, Virtualized Lists, Hot Reload, Live Preview, Plugin Components. Without requiring architectural changes.

---

# Success Criteria

- Every stage has a single responsibility.
- Components never perform rendering.
- Renderers never calculate layout.
- Platform APIs remain isolated behind the PAL.
- New rendering backends can be added without modifying the UI core.
- AvaStudio and AvaHost share the exact same rendering pipeline.
