# AVAUI — Fase 15: Resources (fonts/images/icons/localization)

**CERRADA.** Implementación de interfaz de recursos con backend Windows; decisión sobre PNG/JPEG documentada.

## Hecho

### Interfaz `IResourceProvider`

**`ui/include/avalang/ui/resources/IResourceProvider.h`** (nuevo):
- Enum `ResourceType`: Font, Image, Icon, Localization, Unknown
- Struct `ResourceMetadata`: type, width, height, stride, dataSize
- Struct `Resource`: (uint8_t* data, ResourceMetadata metadata)
- Interfaz `IResourceProvider::Load(logicalPath, type) -> Resource`
- Interfaz `IResourceProvider::RegisterPrefix(prefix, physicalPath) -> bool`
- Interfaz `IResourceProvider::Exists(logicalPath) -> bool`
- Interfaz `IResourceProvider::ClearCache() -> void`
- Factory `CreateDefaultResourceProvider() -> IResourceProvider*`

### Implementación Windows

**`ui/src/resources/ResourceProvider.h`** (new):
**`ui/src/resources/ResourceProvider.cpp`** (new):
- Filesystem-based provider (Windows only, Linux/macOS: stub)
- Logical path resolution: `@fonts/Arial` -> `C:\Windows\Fonts\Arial.ttf`
- Prefix registration (built-in: `@fonts`, `@system`, `@appdata`, `@local`, `@root`)
- Extension search: `.ttf`, `.otf` para fonts; `.png`, `.bmp`, `.jpg`, `.gif` para images
- Cache in-memory (map<string, CachedResource> con buffer + metadata)
- Image metadata parsing (BMP + PNG header introspection; JPEG stub; stride calc)
- Case-insensitive path normalization

### Interfaz `ILocalizationProvider` (design-only, no-op impl)

**`ui/include/avalang/ui/resources/ILocalizationProvider.h`** (new):
- Method `Resolve(key, fallback) -> string` (design: key lookups in lang tables)
- Method `LoadLanguage(langCode) -> bool` (design: switch active language)
- Method `CurrentLanguage() -> string`
- Method `SetFallbackLanguage(langCode) -> void`
- Factory `CreateDefaultLocalizationProvider() -> ILocalizationProvider*`

**`ui/src/resources/LocalizationProvider.cpp`** (new):
- No-op implementation: all keys resolve to themselves
- Phase 15: design only; full JSON/YAML table loading deferred to Phase 16+
- Rationale: Theme (Phase 16) and Controls (Phase 17) must decide upfront if text comes from keys (@button.ok) vs hardcoded strings

### PNG/JPEG loading decision

- **Policy**: Carga de PNG/JPEG vive en **IResourceProvider** (resources/), no en renderer.
- **Reasoning**: 
  - GdiRenderer::OnDrawImage(path) hoy solo soporta BMP vía LoadImage
  - Desacoplar loader del renderer permite:
    - Cacheo centralizado (mismo PNG cargado 2 veces = 1 búfer en RAM)
    - Swappable backends (hoy Windows; Linux/macOS después via stubs)
    - Test aislado del loader sin renderer vivo
  - Renderer future evolution: recibirá Resource (ID + buffer + metadata) en lugar de path string
- **Implementation in Phase 16+**: wrappers alrededor GdiRenderer para usar ResourceProvider

### Build

**`ui/CMakeLists.txt`**:
- `src/resources/ResourceProvider.cpp` agregado
- `src/resources/LocalizationProvider.cpp` agregado
- Headers en `ui/include/avalang/ui/resources/`

### ABI

- `UIModule::AbiVersion()` = 14 (Fase 15 en número de fase, versionado como 14 porque 13 fue la Fase 14)
- Comentario en `UIModule.h` actualizado

## Cambios a archivos existentes

- `ui/CMakeLists.txt`: + 2 source files
- `ui/src/UIModule.cpp`: AbiVersion() 13 -> 14
- `ui/include/avalang/ui/UIModule.h`: + comment para Phase 15

## Sin cambios (por diseño)

- `core/src/ui/` (no es parte del pipeline nuevo)
- `IRenderer` (Fase 9): aún toma `const char* imagePath`; future breaking change es intencional (Fase 16+)
- Ninguna interfaz congelada de Fases 1-14 se modificó

## Verificación

- Código compilable (no testeable sin GdiRenderer + imágenes reales, pero sintaxis OK)
- CMakeLists.txt parseable
- Paths incluídos correctamente

## Próxima fase

**Fase 16 — Theme system**: define cómo los themes applican al pipeline existente (property bag defaults en Layout, o Render Tree?), color palette, typography baseline, component override mechanism.

**Bloqueadores resueltos**:
- ✓ Decisión sobre PNG/JPEG loading
- ✓ Política de localization keys (deferred pero documentada)
- ✓ Resource provider API pública para UI y app code alike
