# scripts/

Pares `.bat` (Windows) / `.sh` (Linux/macOS/WSL) para compilar e
instalar AvaLang y sus componentes. Cada `.sh` es la contraparte
directa de su `.bat` -- mismos flags, mismas opciones -- salvo donde
se indica lo contrario abajo.

| `.bat`                  | `.sh`                    |
|--------------------------|---------------------------|
| `build.bat`               | `build.sh`                 |
| `build_cli.bat`           | `build_cli.sh`             |
| `build_avahost.bat`       | `build_avahost.sh`         |
| `build_avaui.bat`         | `build_avaui.sh`           |
| `build_studio.bat`        | `build_studio.sh`          |
| `build_pack_tools.bat`    | `build_pack_tools.sh`      |
| `build_plugin.bat`        | `build_plugin.sh`          |
| `install.bat`             | `install.sh`               |
| `install_studio.bat`      | `install_studio.sh`        |
| `measure_release.bat`     | `measure_release.sh`       |
| `build_barekernel.bat`    | (sin equivalente -- litekernel usa su propio toolchain cross, ver `docs/kernel/`) |
| `build_barekernel_app.bat`| (sin equivalente -- idem) |
| `sign_release.bat`        | **sin equivalente automatico todavia.** Firma de codigo Authenticode via `signtool` es especifico de Windows; no es un "port" 1:1. Si mas adelante hace falta distribuir en macOS, el equivalente es notarizacion/`codesign` -- una tarea aparte, no cubierta por este plan (ver Fase 4 de `PLAN_LIBRERIAS_NATIVAS_MULTIPLATAFORMA.md`). |

`test_studio_dragdrop.sh` (sin `.bat`) compila y ejecuta con `g++`, sin CMake ni
ImGui/OpenGL, las pruebas de drag & drop del Design de AvaStudio
(`tests/unit/avastudio/`); ver `DRAGDROP_CONTAINERS_PLAN.md`.

Todos los `.sh` deben correrse dentro de WSL o de un shell Linux/macOS
nativo:

```
bash scripts/build_cli.sh
```
