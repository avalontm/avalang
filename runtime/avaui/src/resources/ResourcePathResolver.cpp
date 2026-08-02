#include "resources/ResourcePathResolver.h"

namespace avalang::ui::resources {

bool HasLogicalPrefix(const std::string& path) {
    return !path.empty() && path[0] == '@';
}

std::string ResolveResourcePath(const std::string& logicalPath, ResourceBackend backend) {
    if (!HasLogicalPrefix(logicalPath)) {
        return logicalPath; // Ya es una ruta/URL normal -- sin cambios.
    }

    // "@prefix/resto/del/path" -> prefix="@prefix", resto="resto/del/path"
    size_t slashPos = logicalPath.find('/');
    if (slashPos == std::string::npos) {
        return logicalPath; // Prefijo sin "/name" -- no hay nada que resolver.
    }
    std::string prefix = logicalPath.substr(0, slashPos);
    std::string rest = logicalPath.substr(slashPos + 1);

    switch (backend) {
        case ResourceBackend::Web:
            // avahost sirve wwwroot/ en la raiz ("/") via
            // StaticFileServer -- ver runtime/avahost/src/web/static.
            // "@local" y "@root" apuntan ahi mismo (raiz del sitio
            // publico); "@icons"/"@fonts" a sus subcarpetas
            // convencionales dentro de wwwroot/. El archivo fisico
            // debe existir en esa ubicacion para que el <img> cargue.
            if (prefix == "@local" || prefix == "@root") {
                return "/" + rest;
            }
            if (prefix == "@icons") {
                return "/icons/" + rest;
            }
            if (prefix == "@fonts") {
                return "/fonts/" + rest;
            }
            // Prefijo desconocido: devolver tal cual (mismo
            // comportamiento "silencioso" que el resto del pipeline
            // usa para datos no reconocidos -- ver ColorParse.h).
            return logicalPath;

        case ResourceBackend::Desktop:
        default:
            // No wireado todavia -- ver el TODO en el .h. Se deja el
            // path sin tocar para no romper el comportamiento actual
            // de GdiRenderer (que ya funciona con rutas de disco
            // planas, solo no entiende "@" todavia).
            return logicalPath;
    }
}

} // namespace avalang::ui::resources
