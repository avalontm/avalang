# AvaHost

AvaHost es la pieza que convierte una app escrita en AvaLang en un
sitio web real, disponible en un navegador.

Piensa en él como el "servidor" del ecosistema: toma las páginas que
diseñaste, las convierte en HTML, sirve las imágenes/estilos/scripts
que las acompañan, y responde cuando alguien visita una dirección
como `/` o `/productos`. Cumple el mismo rol que herramientas como
PHP, ASP.NET Core o Node.js cumplen para otros lenguajes, pero
pensado desde cero para AvaLang y para AvaUI, el sistema de pantallas
del ecosistema.

## ¿Qué resuelve?

Cuando alguien visita tu sitio, alguien tiene que decidir qué
pantalla mostrar, armar el documento final que llega al navegador, y
entregar todo lo demás (una imagen, una hoja de estilos, una fuente)
tal cual está guardado. AvaHost se encarga de las tres cosas:

- **Encuentra la pantalla correcta** según la dirección visitada,
  simplemente por cómo están organizadas y nombradas tus pantallas
  en el proyecto — no hace falta llevar un listado aparte de qué
  dirección corresponde a qué archivo.
- **Arma la página final**, incluyendo el encabezado de tu sitio, el
  pie de página, y cualquier elemento que se repita en varias
  pantallas, sin que tengas que copiarlo y pegarlo en cada una.
- **Sirve todo lo demás** — imágenes, estilos, íconos — directo y
  rápido, sin pasar por el motor de AvaLang.

## Pensado para el día a día

Mientras estás construyendo el sitio, AvaHost puede quedar mirando
tus archivos y refrescar el navegador solo apenas guardas un cambio,
para no tener que ir y venir manualmente. Y cuando el sitio ya está
listo para publicarse, ese mismo proyecto se hospeda tal cual, sin
pasos extra de conversión.

## Para quién es

Para cualquiera que ya construyó algo en AvaLang / Ava Studio y
quiere que otras personas lo vean en un navegador — sin tener que
aprender un framework web aparte ni salir del ecosistema.

---

⬅️ [Volver al menú principal](../README.md)
