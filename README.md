# The AvaLang Ecosystem

📖 **[Visit the Wiki](docs/wiki/README.md)** for guides, tutorials, and in-depth documentation.

This repository brings together the core projects that make up the AvaLang ecosystem. Together they provide everything needed to design, build, and host applications using a single, unified technology stack.

From the first visual prototype to a running web application, every stage is designed to work seamlessly within the same ecosystem.

![General architecture](docs/images/diagram.png)

---

# The Ecosystem

|    | Project                            | Description                                                                            |
| -- | ---------------------------------- | -------------------------------------------------------------------------------------- |
| 🧠 | **[AvaLang](AVALANG.md)**          | The programming language, compiler, virtual machine, and runtime.                      |
| 🧩 | **[AvaUI](runtime/avaui/README.md)** | The UI framework: layout engine, render tree, and the `.avaui` component format.      |
| 🎨 | **[Ava Studio](studio/README.md)** | The visual IDE used to design, build, and manage Ava applications.                     |
| 🌐 | **[AvaHost](avahost/README.md)**   | The official hosting platform responsible for running AvaLang applications on the Web. |

Each project contains its own documentation with implementation details and development guidelines.

---

# How Everything Fits Together

The ecosystem is built around clear responsibilities.

* **AvaLang** provides the language, compiler, bytecode, virtual machine, and Stable C API.
* **AvaUI** provides the layout engine, render tree, and component model applications are built from.
* **Ava Studio** offers a modern visual development environment for creating applications.
* **AvaHost** hosts, executes, and renders Ava applications for the Web.

```text
            Ava Studio
                 │
                 ▼
             AvaHost
                 │
         Stable C API
                 │
             AvaLang
                 │
               AvaUI
                 │
          HTML Renderer
                 │
              Browser
```

Although every project can evolve independently, they are designed to work together as a unified platform.

---

# Project Goals

The AvaLang ecosystem is built around the following principles:

* Clean and readable language syntax.
* Stable and portable runtime.
* Cross-platform architecture.
* Visual application development.
* Modular and extensible design.
* Native performance.
* Long-term maintainability.

---

# Project Structure

The repository was recently reorganized from a flat layout into a single `Avalon/` hierarchy, grouping every runtime project under one root instead of scattering them as independent top-level folders:

```text
Avalon/
├── runtime/
│   ├── avalang/     AvaLang core: compiler, bytecode, VM, platform layer
│   ├── avaui/       AvaUI: parser, layout engine, render tree, controls
│   ├── avahost/     AvaHost: web/SSR host, HTML renderer bridge
│   ├── avastudio/   Ava Studio: ImGui-based C++ IDE
│   ├── avacli/      Command-line tooling
│   ├── bindings/    Language/API bindings
│   ├── renderers/   Renderer backends (HTML, GDI, ...)
│   └── libraries/   Shared/vendored third-party libraries
├── samples/         Example AvaLang/AvaUI applications
└── scripts/         Build, test, format, release, and CI scripts
```

All CMake files, `.bat` build scripts, and `#include` paths were updated to match this structure. See `AVALAND_STRUCT.md` for the full breakdown of each folder's responsibilities.

The project is developed in the open.

Contributions, bug reports, feature requests, discussions, and pull requests are welcome.

If you would like to contribute, please read the documentation for the corresponding project before submitting changes.

---

# License

This project is distributed under the **AvaLang Community License (ACL) v1.0**.

The source code is publicly available to encourage learning, research, collaboration, and community contributions while protecting the project from unauthorized commercial exploitation.

### You are allowed to

* Study the source code.
* Modify the source code.
* Fork the project.
* Contribute improvements.
* Use the project for personal use.
* Use the project for education.
* Use the project for research.
* Use the project in non-commercial open-source projects.

### Commercial use

Commercial use is **not permitted** under the Community License.

This includes, but is not limited to:

* Commercial software.
* SaaS platforms.
* Paid hosting services.
* Internal use within commercial organizations.
* Commercial products that embed AvaLang or AvaHost.

Commercial usage requires a separate commercial license from the copyright holder.

For the complete license terms, see the **LICENSE.md** file.

---

# Status

The AvaLang ecosystem is currently under active development.

Public APIs, project structure, and internal implementations may evolve before the first stable release.

Community feedback is highly appreciated.