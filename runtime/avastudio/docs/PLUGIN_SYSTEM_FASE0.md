# Ava Studio -- Sistema de plugins, Fase 0

Ver `PLAN_agente_ia_openrouter.md` (raíz del repo) para el plan completo
de 8 fases. Este doc cubre solo la Fase 0: el sistema de plugins en sí,
sin nada específico del agente de IA todavía.

## Qué se agregó

```
runtime/avastudio/src/plugins/
  plugin_api.h          -- el ABI en C, versionado. Único header que
                            cruza el límite host/plugin.
  plugin_host.h/.cpp     -- carga .dll/.so desde plugins/, valida ABI,
                            mantiene el registro de paneles.
  plugin_ui_bridge.h/.cpp -- traduce las primitivas de AvaUiApi a
                            llamadas reales de ImGui. Vive solo del
                            lado del host.

runtime/avastudio/plugins/hello_world/
  hello_world_plugin.cpp -- plugin de ejemplo, criterio de aceptación
                            de esta fase.
  CMakeLists.txt          -- lo compila como .dll/.so separado.
```

Cambios en archivos existentes:
- `runtime/avastudio/src/main.cpp`: construye un `PluginHost`, lo carga
  al arranque (`plugin_host.LoadAll(ResolvePluginsDir())`), dockea sus
  paneles en el layout inicial, los dibuja cada frame, y los descarga
  (`UnloadAll()`) antes de destruir el contexto de ImGui/GLFW.
- `runtime/avastudio/src/engine/engine_bridge.h`: se agregó
  `EngineBridge::LogExternal(line)` -- así `AvaHostServices::log()` cae
  en el mismo Output panel que ya usa Run/compile, en vez de necesitar
  su propia superficie de log.
- `runtime/avastudio/CMakeLists.txt`: los dos `.cpp` nuevos entran a
  `STUDIO_SOURCES`, y se agregó `add_subdirectory(plugins/hello_world)`
  después de definir el target `ava_studio` (el plugin de ejemplo
  copia su artefacto a `$<TARGET_FILE_DIR:ava_studio>/plugins/` como
  POST_BUILD, así que necesita que ese target ya exista).

## El ABI (`plugin_api.h`)

Un plugin es un `.dll`/`.so` que exporta exactamente tres símbolos C:

```c
int  ava_plugin_abi_version(void);   // devuelve AVA_STUDIO_PLUGIN_ABI_VERSION
bool ava_plugin_init(AvaStudioHost* host);
void ava_plugin_shutdown(void);
```

`ava_plugin_abi_version()` se llama y valida **antes** de que el host
llame `ava_plugin_init()` con un `AvaStudioHost*` real -- un plugin que
reporta una versión más nueva que la que este build del host entiende
se rechaza directamente, en vez de recibir un struct que podría
malinterpretar. Los campos nuevos de `AvaStudioHost`/`AvaUiApi`/
`AvaHostServices` siempre se agregan al final, nunca se reordenan ni se
eliminan -- así un plugin viejo sigue cargando contra un host más nuevo
sin cambios.

`AvaStudioHost` trae:
- `ui` -- primitivas mínimas para armar un panel: `label`,
  `text_wrapped`, `button`, `input_text`, `input_text_multiline`,
  `combo`, `separator`, `same_line`, `spacing`, `begin_child`/
  `end_child`, `scroll_to_bottom`.
- `services` -- de solo lectura por ahora: `get_project_root`,
  `get_active_file` (ruta + contenido; el rango de selección queda en
  `-1/-1` hasta la Fase 3), `get_last_run_output`, `log`.
- `register_panel(host, &registration)` -- solo válido llamarlo desde
  dentro de `ava_plugin_init`.

Un plugin **nunca** incluye ImGui ni ningún header de `avastudio/` --
solo `plugin_api.h`. Esa es la razón de ser de todo esto: Dear ImGui no
tiene ABI estable entre compilaciones, así que un `.dll` de terceros no
puede llamar `ImGui::Begin()` de forma segura salvo que use exactamente
el mismo compilador/flags que el host.

## Cómo probar el criterio de aceptación

1. Compilar normalmente (`cmake --build ...`). Esto ahora también
   compila `hello_world_plugin` y lo copia a
   `<build>/.../plugins/hello_world.dll` (o `.so` en Linux/macOS,
   aunque el proyecto sigue targeteando Windows como plataforma
   primaria).
2. Correr `ava_studio.exe`. Al arrancar debería aparecer una pestaña
   "Hello World Plugin" dockeada junto a Preview/Output (slot
   `AVA_DOCK_BOTTOM`).
3. Escribir un nombre, tocar "Saludar" -- el Output panel debería
   mostrar una línea `[hello_world.dll] Hola, <nombre>! (click #N)`.
4. Cerrar y volver a abrir Ava Studio: el layout dockeado se respeta
   igual que cualquier otro panel (ImGui ya lo persiste en el
   `imgui.ini` de siempre).

Si el `.dll` no está en `plugins/` al lado del `.exe`, Ava Studio
arranca igual, sin esa pestaña -- `PluginHost::LoadAll` no falla si la
carpeta no existe o está vacía.

## Deliberadamente fuera de alcance en esta fase

- **Escritura**: no hay `apply_edit`/`run_project` todavía -- eso es
  Fase 5, y requiere sumar servicios de escritura al host que hoy no
  existen.
- **Selección real** en `get_active_file`: los `out_selection_*`
  siempre vuelven `-1`. La firma ya está pensada para no tener que
  romper el ABI cuando la Fase 3 la implemente.
- **Registrar paneles después de `ava_plugin_init`**: no soportado (no
  hay rollback parcial tampoco -- si `ava_plugin_init` devuelve
  `false` después de haber registrado un panel, ese panel queda
  igual en la lista; ver el comentario en `plugin_host.cpp`).
- **Linux/macOS**: la carga (`dlopen`/`dlsym`/`dlclose`) está
  implementada y debería andar, pero no se probó -- el proyecto sigue
  siendo Windows-primero como el resto de AvaLang.

## Retrocompatibilidad de ABI (revisado en la Fase 7.4)

`AVA_STUDIO_PLUGIN_ABI_VERSION` subió a 3 con las Fases 5 y 6 (sumaron
`apply_edit`/`run_project` y después `design_add_component`/
`design_edit_component` al final de `AvaHostServices`). La regla de
`PluginHost::LoadAll` (`plugin_host.cpp`) es "carga si `plugin_abi <=
AVA_STUDIO_PLUGIN_ABI_VERSION` del host" -- confirmado leyendo el check
`plugin_abi > AVA_STUDIO_PLUGIN_ABI_VERSION || plugin_abi <= 0` (rechaza
solo si el plugin pide una versión más nueva que la que el host sabe
servir, o un valor inválido).

Con eso, `hello_world_plugin.cpp` sigue cargando sin cambios contra el
host ABI 3: nunca tocó `AvaHostServices`, así que los campos nuevos
simplemente no le importan.

**Salvedad a tener en cuenta:** `hello_world_plugin.cpp` reporta
`ava_plugin_abi_version()` devolviendo el macro `AVA_STUDIO_PLUGIN_ABI_VERSION`
tal cual está definido *en el momento en que se lo compila* -- no un
valor viejo fijado a mano. Como vive en el mismo repo y se recompila
junto con el host en cada build, en la práctica siempre reporta la
versión más nueva (hoy 3), nunca ABI 1 o 2. Esto valida que el check
de `LoadAll` no rechaza a un plugin que coincide exactamente con el
ABI del host, pero **no** ejercita el caso real de "un `.dll` viejo,
ya compilado contra ABI 1, cargando contra un host ABI 3" -- para
probar eso de verdad haría falta guardar un binario de
`hello_world_plugin` compilado antes de la Fase 5 (o hardcodear
`return 1;` en una copia aparte) y correrlo contra el host actual.
Queda anotado como pendiente si se quiere una prueba de
retrocompatibilidad más estricta que la lectura de código.

## Siguiente paso

Fase 1 del plan: el plugin `ai_agent` real (cliente OpenRouter +
streaming SSE + panel de chat), construido enteramente sobre este ABI,
sin tocar nada de `avastudio` más allá de lo que ya se agregó acá.
