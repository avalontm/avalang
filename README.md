# The AvaLang Ecosystem

This repository brings together the core projects that make up the AvaLang ecosystem. Together they provide everything needed to design, build, and host applications using a single, unified technology stack.

From the first visual prototype to a running web application, every stage is designed to work seamlessly within the same ecosystem.

![General architecture](images/diagram.png)

---

# The Ecosystem

|    | Project                            | Description                                                                            |
| -- | ---------------------------------- | -------------------------------------------------------------------------------------- |
| 🧠 | **[AvaLang](AVALANG.md)**          | The programming language, compiler, virtual machine, and runtime.                      |
| 🎨 | **[Ava Studio](studio/README.md)** | The visual IDE used to design, build, and manage Ava applications.                     |
| 🌐 | **[AvaHost](avahost/README.md)**   | The official hosting platform responsible for running AvaLang applications on the Web. |

Each project contains its own documentation with implementation details and development guidelines.

---

# How Everything Fits Together

The ecosystem is built around clear responsibilities.

* **AvaLang** provides the language, compiler, bytecode, virtual machine, and Stable C API.
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

# Open Development

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
