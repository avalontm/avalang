Version: 0.1 Draft
Status: Planning
Project: AvaHost
Author: AvalonTM
Target: AvaLang Ecosystem
Target Platforms: Windows, Linux (por el momento -- cross-platform desde el diseño inicial, no agregado después)

---

# 1. Vision

AvaHost is the official application hosting platform for the AvaLang ecosystem.

Its responsibility is to execute, host and manage AvaLang applications independently of the execution environment.

The first supported platform is Web.

AvaHost must be multiplatform at the operating system level from its first
version: it must build and run on both Windows and Linux. This is not a
future goal -- it is a hard requirement of the initial implementation.
macOS and other operating systems may be added later, but are out of scope
for now.

Future versions may support:

- Desktop
- Services
- WASM
- Embedded
- Mobile

AvaHost must never contain compiler logic.

AvaHost must never know parser internals.

AvaHost communicates with AvaLang only through the Stable C API.

---

# 2. Objectives

The purpose of AvaHost is to provide an application hosting platform similar to PHP, ASP.NET Core and NodeJS while keeping the AvaLang ecosystem simple.

Main goals:

• Host AvaLang applications.
• Resolve routes.
• Execute bytecode.
• Render AvaUI.
• Serve static files.
• Support Hot Reload.
• Keep startup fast.
• Keep memory usage low.
• Be cross platform: run on Windows and Linux from the first version (macOS and
  others are future work, not part of this plan yet).

---

# 3. Non Goals

The following features are intentionally excluded from the first version.

- ORM
- Identity Framework
- MVC
- Razor
- SPA Framework
- WebSockets
- Authentication
- Authorization
- Database Abstractions
- Distributed Cache

These features belong to independent plugins.

---

# 4. Architecture Principles

The architecture of AvaHost follows these principles.

## Single Responsibility

Each subsystem owns one responsibility.

Example:

HTTP Server
only handles HTTP.

Router
only resolves routes.

Renderer
only transforms Component Trees.

Runtime
only executes bytecode.

---

## Loose Coupling

AvaHost never depends on compiler internals.

Everything goes through the Stable C API.

Bad

AvaHost
↓

Compiler

Good

AvaHost
↓

Stable C API
↓

Compiler

---

## Platform Independent

The Runtime must never know whether it is running on:

- Web
- Desktop
- Linux
- Windows

That decision belongs to the Host.

Concretely, for this plan: AvaHost itself (not just the Runtime) targets
Windows and Linux as the two supported operating systems for now. Nothing in
AvaHost.Core, AvaHost.Web, AvaHost.CLI, etc. may assume a Windows-only or
Linux-only API (file paths, process handling, sockets) without an
abstraction that also works on the other. macOS support is not a goal yet,
but the architecture should not deliberately block it either.

---

## Component Driven

Pages never generate HTML.

Pages generate Components.

Component Tree

↓

Renderer

↓

HTML

This allows future renderers.

---

# 5. High Level Architecture

                AvaHost

                   │

        HTTP Request Received

                   │

             HTTP Listener

                   │

             Middleware

                   │

             Route Resolver

                   │

           Stable C API

                   │

             Ava Runtime

                   │

           Component Tree

                   │

           HTML Renderer

                   │

             HTTP Response

---

# 6. Solution Structure

AvaHost/

    src/

        AvaHost.Core/

        AvaHost.Web/

        AvaHost.CLI/

        AvaHost.Runtime/

        AvaHost.Rendering.Html/

        AvaHost.StaticFiles/

        AvaHost.Configuration/

        AvaHost.Logging/

        AvaHost.FileWatcher/

        AvaHost.PluginLoader/

    docs/

    tests/

    samples/

---

# 7. AvaHost.Core

Purpose

Contains all hosting abstractions.

Responsibilities

- Host lifecycle
- Dependency registration
- Configuration
- Middleware
- Plugin loading
- Logging abstraction

Must never contain:

- HTML rendering
- HTTP implementation
- AvaUI
- Compiler code

---

# 8. AvaHost.Web

Purpose

Provides Web hosting support.

Responsibilities

- HTTP Server
- Routing
- Static Files
- Cookies
- Sessions
- HTTP Responses

Everything Web specific lives here.

---

# 9. Runtime Integration

AvaHost never parses source code directly.

Flow

HTTP Request

↓

Resolve Route

↓

Load Bytecode

↓

Execute VM

↓

Receive Component Tree

↓

Render HTML

↓

Return Response

The Runtime is treated as a black box.

---

# 10. Stable C API

Every interaction with AvaLang goes through avalang.h.

Examples

Create Runtime

Compile Script

Load Bytecode

Execute Bytecode

Destroy Runtime

No internal structures are exposed.

---

# 11. Routing

Routing is convention based.

Example

routes/index.ava

↓

/

routes/about.ava

↓

/about

routes/products/index.ava

↓

/products

routes/products/details.ava

↓

/products/details

No route registration required.

---

# 12. Component Rendering

Pages never emit HTML.

Example

page "/"

    button
        text = "Login"
    end

end

Execution

Parser

↓

AST

↓

Component Tree

↓

HTML Renderer

↓

HTML Output

The Component Tree is renderer independent.

---

# 13. Static Files

Folder

wwwroot/

Contains

css/

js/

images/

fonts/

icons/

favicon.ico

These files are served directly.

They never go through AvaLang.

---

# 14. Bytecode Cache

Development

Source

↓

Compile

↓

Execute

Production

Source

↓

Compile Once

↓

Store Bytecode

↓

Execute Bytecode

The compiler should never execute during every request.

---

# 15. Hot Reload

Watch

.ava

.avaui

.css

.js

When a source file changes:

Recompile

↓

Replace Bytecode

↓

Refresh Browser

---

# 16. Configuration

Configuration file

appsettings.json

Example

{
    "host":"localhost",
    "port":8080,
    "environment":"Development",
    "watch":true
}

---

# 17. Plugin System

Plugins extend AvaHost.

Plugins never modify AvaLang.

Allowed

- Middleware
- Services
- Renderers
- CLI Commands
- Configuration
- Diagnostics

Forbidden

- Parser
- Compiler
- Lexer
- VM

---

# 18. Command Line Interface

Supported commands

avahost new

Creates a new project.

avahost run

Runs the current project.

avahost watch

Runs with Hot Reload.

avahost build

Compiles the project.

avahost publish

Publishes a deployable application.

avahost doctor

Runs diagnostics.

---

# 19. Project Structure

MyApplication/

    appsettings.json

    routes/

    layouts/

    components/

    services/

    models/

    wwwroot/

---

# 20. Future Versions

Version 0.2

• Route Parameters
• Layout Engine
• Dependency Injection

Version 0.3

• Sessions
• Authentication
• Middleware Pipeline

Version 0.4

• Publish
• Compression
• Response Cache

Version 1.0

• Plugin SDK
• HTTPS
• HTTP/2
• Production Optimizations

---

# Final Statement

AvaHost is not a web framework.

AvaHost is the official hosting platform of the AvaLang ecosystem.

Its responsibility is to host applications.

Its responsibility is not to extend or modify the language.

Every subsystem must remain independent, modular and replaceable.

The Stable C API is the only communication layer between AvaHost and AvaLang.

This rule must never be broken.