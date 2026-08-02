# samples/

Proyectos de muestra completos, autocontenidos, ejecutables. Para usuarios humanos que quieran ver un ejemplo de app AvaLang en funcionamiento.

## Qué va aquí

Proyectos completos (`app.ava` + `appsettings.json` + `.avaui` + assets). Nada de código parcial, nada de tests, nada de demos sueltos.

## Qué NO va aquí

- Tests (esos van en `tests/`).
- Demos parciales o snippets sueltos.
- Código del agente.
- Código de `runtime/`.

## Convención de organización

`samples/<categoría>/<nombre>/`. Categoría = tipo de app (`web/`, `desktop/`, `cli/`, etc.). Nombre = nombre descriptivo del proyecto.

Ejemplos válidos:
- `samples/web/testproj/` — proyecto web con rutas, layouts y componentes.
- `samples/cli/hello-world/` — futuro: CLI de ejemplo.
- `samples/desktop/counter/` — futuro: app desktop de contador.

## Lo que ya existe

- `samples/web/testproj/` — proyecto web de muestra con `app.ava`, `appsettings.json`, rutas (`index.avaui`, `about.avaui`), layouts (`main.avaui`, `404.avaui`), componentes (`Navbar.avaui`, `Footer.avaui`) y assets (`wwwroot/css/app.css`).

## Regla para el agente

Leer estos proyectos para entender patrones de uso de la plataforma. **No modificar nada adentro de `samples/`**. Si querés agregar una muestra nueva, crear `samples/<categoría>/<nombre>/` desde cero con la estructura mínima (`app.ava` + `appsettings.json` + al menos una ruta).
