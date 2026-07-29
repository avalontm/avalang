# AvaHost — Estado de Implementación

Estado real del hosting (`avahost/`), verificado contra el código fuente
(no contra el plan/roadmap). Pensado como checklist de "qué falta para
que un sitio básico funcione de punta a punta" — no cubre Ava Studio ni
el compilador de AvaLang en sí.

Leyenda: ✅ funciona hoy · ⬜ falta (bloquea o limita el uso real)

| # | ✅/⬜ | Función | Descripción / Estado |
|---|:---:|---|---|
| 1 | ✅ | **Archivos estáticos** (`wwwroot/`) | `StaticFileServer` sirve CSS/JS/imágenes/fuentes directo, con ETag + `Cache-Control` (`no-cache` en dev, `max-age` en prod) y guard anti path-traversal. |
| 2 | ✅ | **Rutas por convención de archivo** | `routes/index.avaui` → `/`, `routes/about.avaui` → `/about`, `routes/products/index.avaui` → `/products`. `.ava` como fallback para scripts planos. |
| 3 | ✅ | **Rutas declaradas con parámetros** | `route "/products/{id:int}"` dentro del `.avaui` — soporta `{name}`, `{name?}` y constraints (`int`, `long`, `guid`, `slug`, `alpha`). Tienen prioridad sobre la convención por archivo. |
| 4 | ✅ | **Layouts + `slot`** | `extends "main"` renderiza la página dentro de `layouts/main.avaui`; `slot` marca dónde se inserta el contenido de la página. |
| 5 | ✅ | **Hot reload (`--watch`)** | Re-escanea rutas declaradas al cambiar un archivo; el navegador hace polling a `/__avahost/hotreload` y recarga solo. |
| 6 | ✅ | **CSS / Tailwind en `<head>`** | Tailwind Play CDN (`@tailwindcss/browser@4`) inyectado como `<script>`; `css/app.css` se agrega como `<link>` si el archivo existe. |
| 7 | ✅ | **Configuración (`appsettings.json`)** | `host`, `port`, `environment`, `watch` + `routesDir`/`wwwrootDir`/`layoutsDir`/`componentsDir`/`pluginsDir`, todos con default explícito en el scaffold de `avahost new`. |
| 8 | ⬜ | **Import de componentes** (`import "components/x"` + `X()`) | El parser reconoce la sintaxis pero **no la resuelve**: `Navbar()` queda como nodo vacío y se renderiza como `<div></div>` sin contenido. Falta el paso de resolución en el render path de AvaHost (hoy solo existe, parcial, en Ava Studio). |
| 9 | ⬜ | **Eventos / `methods`** (`click`, handlers) | Se parsean (`doc.methodsText`) pero no están conectados al manejo de requests — falta portar el "state/event bridge" del Designer de Ava Studio (`design/state_eval.cpp`). |
| 10 | ⬜ | **Estado reactivo (`state` block)** | Las variables iniciales se parsean (`stateJson`) pero no hay binding real: cambiar `state` en el servidor no re-renderiza nada todavía. |
| 11 | ✅ | **Ciclo de vida (`OnLoad`)** | `RenderAvaUiRoute` llama `OnLoad()` de la página una vez por render (GET o POST de evento), si está definido en `code` — no-op si no existe. `OnShow`/`OnHide`/`OnUnload` siguen siendo no-op en AvaHost por diseño (requieren una instancia de página persistente, que este host no tiene). |

## Prioridad sugerida para "funciona al menos por ahora"

Con 1–7 ya se puede servir un sitio multi-página con estilos, layout
compartido y assets — suficiente para un sitio mayormente estático o
con lógica server-side vía rutas `.ava`. El salto grande es **#8**
(componentes), porque hoy cualquier `.avaui` que use `import` +
`Componente()` se ve roto en producción sin avisar (no da error, solo
renderiza vacío). **#9 y #10** son necesarios recién cuando se quiera
interactividad real (botones que hacen algo, contadores, formularios).
