#ifndef AVA_UI_RESOURCE_PATH_RESOLVER_H
#define AVA_UI_RESOURCE_PATH_RESOLVER_H

#include "Export.h"
#include <string>

namespace avalang::ui::resources {

/**
 * Resuelve un `src`/path "logico" (p.ej. "@local/logo.png",
 * "@icons/check.svg") a lo que cada backend de renderer realmente
 * necesita para cargarlo. Un path sin prefijo "@" se devuelve tal
 * cual (ya es una ruta/URL normal -- comportamiento actual, sin
 * romper nada existente).
 *
 * Por que esto vive separado de IResourceProvider/ResourceProvider
 * (resources/ResourceProvider.h): ese resuelve contra RUTAS DE
 * DISCO (pensado para el renderer nativo, GDI, que carga bytes
 * directo del filesystem). El renderer Web no puede usar una ruta
 * de disco como `src` de un <img> -- el browser necesita una URL
 * servible por avahost. Este resolver traduce el MISMO prefijo
 * logico a lo que cada backend entiende, sin que RenderTree/
 * DecomposeImage tengan que saber nada de backends (mantiene la
 * decision documentada en controls/Image.h: "loading lives in the
 * renderer, not Resources").
 */
enum class ResourceBackend {
    // avahost sirve wwwroot/ en la raiz del sitio (ver
    // StaticFileServer) -- por eso "@local/x" -> "/x" alcanza hoy,
    // sin necesitar una ruta HTTP nueva en el servidor. El archivo
    // fisico debe existir dentro de wwwroot/.
    Web,

    // TODO (futuro -- no wireado todavia): GdiRenderer::OnDrawImage
    // hoy llama LoadImageA(imagePath, ...) directo, ignorando
    // prefijos "@". Para soportarlos ahi, GdiRenderer deberia:
    //   1. Detectar el prefijo "@" (ver HasLogicalPrefix abajo).
    //   2. Pedirle los bytes a resources::ResourceProvider::Load()
    //      (ya resuelve "@local"->cwd, "@icons"/"@fonts"->carpetas
    //      del sistema -- ver ResourceProvider.cpp).
    //   3. Decodificar esos bytes a un HBITMAP (LoadImageA solo sabe
    //      leer de archivo, no de un buffer en memoria -- hace falta
    //      pasar por GDI+ / CreateDIBSection en vez de LoadImageA).
    // Este resolver ya deja el punto de entrada (ResolveResourcePath
    // con Backend::Desktop) para cuando se haga ese trabajo; hoy
    // simplemente devuelve el path sin tocar.
    Desktop,
};

/** True si `path` usa el esquema de prefijo logico ("@algo/..."). */
AVA_UI_API bool HasLogicalPrefix(const std::string& path);

/**
 * Resuelve `logicalPath` para el backend dado. Paths sin prefijo
 * "@" se devuelven sin cambios en cualquier backend.
 */
AVA_UI_API std::string ResolveResourcePath(const std::string& logicalPath, ResourceBackend backend);

} // namespace avalang::ui::resources

#endif // AVA_UI_RESOURCE_PATH_RESOLVER_H
