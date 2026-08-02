# Phase 9-10: Renderer Interface & HTML Backend

## Phase 9: IRenderer (Interface)

Backend-agnostic interface. See above for architecture.

## Phase 10: HTML Backend

### Overview

`HTMLRenderer` converts render commands to HTML/CSS markup. Output is a complete HTML document consumable by avahost web server or any browser.

### Architecture

`HTMLRenderer extends BaseRenderer`:
- Overrides `OnDrawRectangle()`, `OnDrawText()`, `OnDrawImage()`
- Emits `<div>` elements with inline CSS styling
- Applies transforms via CSS `transform` property
- Applies clipping via CSS `clip-path`
- Tracks opacity via CSS `opacity`

### Output

- **Format**: Complete HTML5 document with embedded `<style>`
- **Viewport**: Fixed-size container div (configurable via width/height)
- **Elements**: Positioned absolutely, styled inline
- **CSS**: Modern browser CSS (transform, clip-path, opacity)

### CSS Features

- **Position**: `position: absolute` + `left`/`top` inline
- **Size**: `width`/`height` inline
- **Color**: Hex format (e.g., `#FF00FF`)
- **Border**: Inline style with thickness and color
- **Transform**: CSS `transform` with translate, scale, rotate
- **Clip**: CSS `clip-path: inset(...)` for clipping rectangles
- **Opacity**: CSS `opacity: 0.0 .. 1.0`
- **Text**: `font-size`, `font-family`, `color` inline
- **Images**: `<img>` tags with `src` attribute

### Color Format

RGB → hex conversion (e.g., Color{255, 0, 128} → `#FF0080`).
Alpha channel from render commands is applied as CSS opacity.

### Transform Stack

CSS `transform` property combines:
- `translate(tx, ty)` from Translate()
- `scale(sx, sy)` from Scale()
- `rotate(θdeg)` from Rotate() (converted from radians)

### Clipping

`clip-path: inset(top, right, bottom, left)` calculated from ClipRect stack.
Note: clip-path is browser-standard but may not work in older IE.

### Text Rendering

- Uses `<div>` with `white-space: nowrap`
- Font family defaults to "Arial" if not specified
- Font size in pixels
- Text content is HTML-escaped (not done yet; TODO for Phase 10+)

### Image Rendering

- Uses `<img>` with `src` attribute
- Assumes paths are URLs or data-URIs
- No caching or async loading in Phase 10

### Limitations (Documented)

- [ ] No HTML escaping for text content (XSS risk if untrusted text)
- [ ] No shadow/blur effects
- [ ] No gradients
- [ ] No SVG rendering (only HTML+CSS)
- [ ] Transform matrix multiplication is additive only
- [ ] Clip-path browser compatibility not checked
- [ ] No responsive layout (fixed dimensions)
- [ ] Image loading is synchronous (no error handling)
- [ ] No accessibility (alt text, ARIA labels)

### Usage

```cpp
auto renderer = IRenderer::Create("html", 800, 600);
renderer->BeginFrame();
renderer->DrawRectangle(10, 10, 100, 50, Color{255,0,0,255}, Color{0,0,0,255}, 2.0f);
renderer->DrawText(20, 20, "Hello", 16, "Arial", Color{0,0,0,255});
renderer->EndFrame();
const char* html = renderer->GetOutput();  // Full HTML string
```

### Platform Support

- **Windows**: Full implementation
- **Linux/macOS**: Not implemented (stub, inherits from BaseRenderer with no-op)

