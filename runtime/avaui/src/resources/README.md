# ui/src/resources

Fase 15: CERRADA.

Implementación de `IResourceProvider` (resolución de paths lógicos, filesystem backend) e `ILocalizationProvider` (interfaz, no-op implementation).

Ver docs/AVAUI_FASE15_RESOURCES.md para el reporte completo.

Archivos:
- ResourceProvider.h/.cpp: Windows filesystem provider, logical path resolution
- LocalizationProvider.cpp: no-op impl (design-only, real impl in Phase 16+)

Public interfaces en ui/include/avalang/ui/resources/:
- IResourceProvider.h
- ILocalizationProvider.h
