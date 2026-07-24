# Ava Studio -- Milestone 1 (motor + shell ImGui)

Compilado y validado de punta a punta en este sandbox (Linux, GCC 13,
GLFW+ImGui vía FetchContent) antes de entregarlo. Para integrarlo a tu
repo `avalang`:

1. Copia la carpeta `studio/` completa a la raíz del repo (junto a
   `core/`, `public/`, `grammar/`, etc.)
2. Reemplaza tu `CMakeLists.txt` raíz por el de este paquete (o aplica
   a mano los dos cambios: la opción `AVA_BUILD_STUDIO` cerca del tope
   del archivo, y el `add_subdirectory(studio)` al final -- son los
   únicos dos bloques nuevos).
3. Reemplaza `public/src/c_api.cpp` por el de este paquete -- arregla
   un bug real que encontré: las propiedades de tipo `string` en el
   Component Tree siempre serializaban como `""` en
   `ava_ui_tree_to_json` en vez del valor real. Sin este fix, el panel
   Preview de Ava Studio (y cualquier otro consumidor del JSON) nunca
   iba a ver el texto real de un botón/label.

## Build (Windows, con tu setup existente de vcpkg/antlr)

Opción rápida -- dos scripts nuevos, mismo estilo que tus `build.bat`/
`install.bat` de siempre, van en la raíz del repo junto a esos:

```bat
install_studio.bat        REM chequea git/cmake, avisa si falta install.bat, y compila
build_studio.bat run      REM solo compilar (+ abrir ava_studio.exe al terminar)
```

`build_studio.bat` usa su propia carpeta `build_studio\` (no toca tu
`build\` normal), agrega `-DAVA_BUILD_STUDIO=ON` automáticamente, y
recoge tu `VCPKG_ROOT` si ya corriste `install.bat` antes (así
`ava_studio.exe` también tiene el parser real, no el stub). Soporta
los mismos flags que `build.bat`: `clean`, `debug`, `ninja`, más el
nuevo `run`.

O a mano, si preferís no usar los scripts:

```bat
cmake -B build -DAVA_BUILD_STUDIO=ON
cmake --build build --config Release
build\studio\Release\ava_studio.exe
```

`AVA_BUILD_STUDIO` está OFF por default, así que tu build normal
(`build.bat`) no cambia en nada -- Ava Studio es completamente opt-in.
La primera vez que actives el flag, CMake va a bajar GLFW y Dear ImGui
(rama `docking`) vía FetchContent (necesita internet esa vez).

## Qué hace este milestone

- `ava_studio` enlaza DIRECTO contra tu librería `avalang` (misma
  librería que usa `ava_cli`) -- cero FFI, cero JSON entre procesos.
- Dockspace con Explorer / Code Editor / Properties / Preview / Output,
  como en tu boceto ASCII.
- Explorer lista los `.ava` de `scripts/` (relativo al directorio
  desde donde corras el exe) y los abre en el editor.
- Editor: F5 o el botón Run llama a `ava_compile` + `ava_run` sobre el
  texto del editor y muestra el resultado en Output. Ctrl+S guarda.
- Preview: como los builtins `page`/`stack`/`button` (ver
  `core/src/ui/builtins.cpp`) todavía no están registrados en el VM,
  un script `.ava` real todavía no puede producir un Component Tree.
  Por ahora Preview muestra un árbol de demo construido directo con la
  API `ava_ui_*` (page > stack > text + button), clickeable, que
  alimenta el panel Properties. Esto valida el camino completo
  Component/Property/Child/JSON que sí vas a necesitar cuando el
  Designer real exista.
- Output también muestra el JSON crudo de `ava_ui_tree_to_json` sobre
  ese árbol de demo, para comprobar la serialización a ojo.

## Lo que falta para el siguiente milestone (Designer real)

1. Registrar los builtins `page`/`stack`/`button`/etc. en
   `ava_vm_create()` (hoy `core/src/ui/builtins.cpp` está a propósito
   sin enganchar -- ver el comentario en ese archivo) para que un
   script `.ava` real produzca un Component Tree al correrlo.
2. Agregar al C API una forma de enumerar las keys de propiedades de
   un componente (hoy `ava_ui_get_property` requiere saber la key de
   antemano) -- lo necesitás para que Properties sea genérico en vez
   de leer una lista fija.
3. Reemplazar el outline clickeable de Preview por un canvas real:
   drag & drop desde un Toolbox, rectángulos posicionados, resize
   handles.
4. Doble clic en un componente del canvas -> genera el esqueleto del
   evento en el Editor (`func btnSave_Click(sender, e) ... end`) y
   hace scroll/foco ahí.
5. Cambiar el textbox plano del Editor por un widget con syntax
   highlighting (ImGuiColorTextEdit u otro) ahora que la mecánica base
   ya funciona.
