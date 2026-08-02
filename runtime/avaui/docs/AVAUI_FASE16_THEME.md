# AVAUI — Fase 16: Theme system

**CERRADA.** Interfaz de tema con palette y tipografía; implementación default + helper de aplicación.

## Hecho

### Interfaz `ITheme`

**`ui/include/avalang/ui/theme/ITheme.h`** (nuevo):
- Struct `ThemeColor`: hex value (e.g., "FF5733")
- Struct `ThemeFont`: name, size (points), weight (400/700), italic flag
- Struct `ThemeSpacing`: paddingPx, marginPx, borderWidthPx, borderRadiusPx
- Interfaz `ITheme::Color(roleName, fallback) → ThemeColor`
- Interfaz `ITheme::Font(roleName, fallback) → ThemeFont`
- Interfaz `ITheme::Spacing() → ThemeSpacing`
- Interfaz `ITheme::Name() → string` (e.g., "Default Light")
- Interfaz `ITheme::HasColor(roleName) → bool`
- Interfaz `ITheme::HasFont(roleName) → bool`

### Interfaz `IThemeProvider`

- Interfaz `IThemeProvider::Current() → ITheme*`
- Interfaz `IThemeProvider::SetTheme(themeName) → bool`
- Interfaz `IThemeProvider::Register(theme, name) → bool`
- Factory `CreateDefaultThemeProvider() → IThemeProvider*`

### Implementación Default Theme

**`ui/src/theme/DefaultTheme.h/.cpp`** (nuevo):
- **DefaultTheme**: Fluent Design-inspired (Windows palette)
  - Primary: #0078D4 (Windows blue) + light/dark variants
  - Secondary: #605E57
  - Backgrounds: #FFFFFF (white), #F3F3F3 (surface), #E8E8E8 (variant)
  - Text: #333333 (primary), #767676 (secondary), #A19F9D (disabled)
  - Borders: #CCCCCC, #E0E0E0 (light), #999999 (dark)
  - Semantic: #107C10 (success), #D83B01 (error), #FFB900 (warning), #0078D4 (info)
  - Component-specific roles: buttonPrimary, buttonSecondary, buttonDisabled, inputBackground, inputBorder, linkDefault, etc.
  
- **Typography roles**:
  - Headings: heading1 (28pt, bold), heading2 (20pt, bold), heading3 (16pt, bold)
  - Body: body (12pt), bodySmall (11pt), bodySemibold (12pt, w600)
  - Captions: caption (11pt), label (12pt, w600)
  - Component-specific: button (12pt, w600), link (12pt), subtitle (14pt)
  
- **Spacing defaults**: padding=8px, margin=4px, borderWidth=1px, borderRadius=4px

- **ThemeProvider**: holds multiple themes, switches between them
  - Pre-registers "Default Light"
  - `Register(theme, name)`: adds new theme
  - `SetTheme(name)`: activates theme
  - `Current()`: returns active theme

### Theme Application Helper

**`ui/include/avalang/ui/theme/RenderTheme.h`** (nuevo):
**`ui/src/theme/RenderTheme.cpp`** (nuevo):
- `RenderTheme::Apply(tree, theme) → bool`
  - Walks ComponentTree recursively
  - For each component, calls ApplyToComponent()
  
- `RenderTheme::ApplyToComponent(comp, theme) → bool`
  - Checks component TypeName (case-insensitive)
  - Maps type to theme roles:
    - **Button**: buttonPrimary (bg), button (font), white text, border
    - **Text**: body (font), text color
    - **TextBox**: inputBackground (bg), inputBorder, body (font)
    - **Image**: borderRadius only
    - **Container/Row/Column/Stack**: surface (bg)
    - **Label**: label (font), text color
    - **Checkbox/RadioButton**: border, borderWidth
  - Only fills **empty** properties (GetProperty returns nullptr)
  - Never overwrites component-specific values (CSS cascade model)

### Design Decision: When to Apply Theme

**Policy**: Theme applied **before Layout** (Phase 3)
- ComponentTree filled with theme defaults
- LayoutEngine processes now-complete properties
- Theme acts as "default stylesheet" (CSS model)
- Component properties never overwritten by theme

**Alternative considered (Render Tree step)**: Apply in Render Tree (Phase 6) instead
- Rejected: changes to property bag after Layout complicates reasoning
- Render Tree design assumes properties stable from Layout

### Build

**`ui/CMakeLists.txt`**:
- `src/theme/DefaultTheme.cpp` agregado
- `src/theme/RenderTheme.cpp` agregado
- Headers en `ui/include/avalang/ui/theme/`

### ABI

- `UIModule::AbiVersion()` = 15 (Fase 16 en número de fase, versionado como 15)
- Comentario en `UIModule.h` actualizado

## Cambios a archivos existentes

- `ui/CMakeLists.txt`: + 2 source files
- `ui/src/UIModule.cpp`: AbiVersion() 14 → 15
- `ui/include/avalang/ui/UIModule.h`: + comment para Phase 16

## Sin cambios (por diseño)

- Ninguna interfaz congelada de Fases 1-15 se modificó
- ComponentTree structure unchanged (RenderTheme es visitor, no modifier del árbol)
- LayoutEngine (Fase 3) unchanged (recibe ComponentTree con propiedades ya llenas)

## Verificación

- Código compilable (g++ -std=c++20 sin errores)
- CMakeLists.txt parseable
- Role coverage: 25+ color roles, 14+ font roles, spacing struct

## Limitaciones documentadas (por diseño)

- Sin dark mode built-in (pero architecture permite registrar alternates vía ThemeProvider)
- Sin transiciones de tema (switch es inmediato)
- Sin override de spacing per-component (solo theme-wide)
- Sin gradients/shadows en theme (fase futura)
- Sin animations on theme change (out of scope, Phase 19)

## Próxima fase

**Fase 17 — Controls: Button (primer control real)**: valida Theme (Fase 16) y Resources (Fase 15) en caso de uso real.

**Bloqueadores resueltos**:
- ✓ Theme architecture decidida (applied before Layout)
- ✓ Color palette definida (26 roles)
- ✓ Font roles definidas (14 roles)
- ✓ Component-specific mappings documentadas
- ✓ CSS cascade model implementado (component props override theme)
