# avalang.ui.web

Paquete `avalang.ui.web` de Fase 21 (Packaging). Layout "Web" del plan:

```text
app/
├── avalang.ui.web
└── app.avaui
```

Contenido, extraído byte a byte de los literales embebidos que hoy
generan estos mismos archivos en runtime:

- `js/ava-viewport.js`  <- `ViewportDetectScriptTag()` en
  `runtime/avahost/src/web/server/app.cpp`
- `js/ava-hotreload.js` <- `AvaHostApp::HotReloadScriptTag()` en el
  mismo archivo
- `js/ava-runtime.js`   <- `AvaHostApp::EventScriptTag()` en el mismo
  archivo
- `css/ava-runtime.css` <- `HTMLRenderer::EmitStaticBaseCssLink()` en
  `runtime/avaui/src/renderer/HTMLRenderer.cpp`

`css/ava-project.css` NO forma parte de este paquete: a diferencia de
los 4 archivos de arriba (constantes, iguales para cualquier proyecto),
`ava-project.css` se genera por proyecto a partir del theme/estilos
dinámicos de cada app (`EmitProjectCssLink()`), no es un asset
versionable del framework.

## Qué es esto y qué no es

Esto define el paquete -- lo deja existir como unidad versionada,
copiable a `app/` como pide esta fase. **No** cambia el comportamiento
de `avahost` en runtime: `app.cpp`/`HTMLRenderer.cpp` siguen generando
estos mismos 4 archivos desde sus literales C++ embebidos vía
`WriteStaticAssetIfMissing()` (solo si el archivo no existe todavía en
`wwwroot/`), exactamente igual que antes de este paquete. Conectar
avahost para que copie desde `runtime/avaui/web/avalang.ui.web/` en vez
de escribir el literal -- y así tener una sola fuente de verdad -- es
trabajo real pendiente, no incluido aquí: es un cambio a `app.cpp` que
no se puede hacer con confianza sin compilar y correr el server para
verificar que las rutas HTTP (`/js/...`, `/css/...`) siguen sirviendo
lo mismo.

## Verificación hecha sin compilar

Comparados contra las copias ya escritas en disco por una corrida real
de `avahost` (`samples/web/testproj/wwwroot/`):

- `ava-viewport.js`, `ava-hotreload.js`, `css/ava-runtime.css`:
  idénticos byte a byte.
- `ava-runtime.js`: la copia extraída tiene ~20 líneas más (manejo de
  `touchstart`/`touchmove`/`touchend`) que la copia en
  `samples/web/testproj/wwwroot/js/ava-runtime.js`. No es un error de
  extracción -- `WriteStaticAssetIfMissing()` solo escribe si el
  archivo no existe, así que esa copia quedó desactualizada desde antes
  de que se agregara soporte táctil a `EventScriptTag()`. Confirma que
  esta extracción está al día con el `app.cpp` real, más que la copia
  vieja del sample.
