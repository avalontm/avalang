# Phase 8: Render Commands

## Overview

Accumulates rendering directives (DrawRectangle, DrawText, DrawImage, transforms, clipping) emitted from the scene graph traversal. Commands are buffered in order for consumption by the `IRenderer` implementation (Phase 9).

## Command Types

- **DrawRectangle**: Filled rect with optional border.
- **DrawText**: Rendered text at (x,y) with font name and size.
- **DrawImage**: Bitmap/texture at (x,y) with size.
- **PushClip/PopClip**: Clipping stack management.
- **Translate/Scale/Rotate**: Transform matrix operations.

## Architecture

### `IRenderCommandSink` (Public Interface)

- `Emit(RenderCommand)` — buffer raw command.
- `DrawRectangle/DrawText/DrawImage()` — convenience methods that construct and emit commands.
- `Translate/Scale/Rotate()` — emit transform commands.
- `PushClipRect()/PopClipRect()` — manage clipping stack.
- `BeginFrame()` / `EndFrame()` — frame lifecycle.

### `RenderCommandSink` (Concrete Implementation)

- Stores commands in `std::vector<RenderCommand>`.
- Maintains `std::stack<ClipRect>` for clipping state.
- Non-copyable (deleted copy constructor/assignment).
- Factory: `IRenderCommandSink::Create()` returns `std::unique_ptr<RenderCommandSink>`.

## Data Structures

### `RenderCommand` (in `RenderCommand.h`)

Union-like struct with discriminant `type` (RenderCommandType enum):
- Stores all possible parameter sets for each command type.
- Space-efficient for a fixed set of commands.

### `Color`

RGBA packed as `{r, g, b, a}` (0-255).

### `ClipRect` (reused from Phase 7)

Clipping bounds + `enabled` flag.

## Usage Flow

1. **BeginFrame()**: Clear command buffer, reset clip stack.
2. **Emit() / DrawRectangle() / ... (from scene graph traversal)**
3. **EndFrame()**: Finalize frame.
4. **GetCommands()**: Renderer fetches command list.

## Limitations (Documented for Future Phases)

- No command batching (Phase 9 renderer may optimize).
- Clipping stack stored, no scissor test applied (renderer responsibility).
- Transform commands are recorded linearly (no matrix multiplication until Phase 9).
- No command validation or error recovery (renderer decides behavior).
- String pointers (text, imagePath) are borrowed—caller must guarantee lifetime.

## Platform Support

- **Windows**: Full implementation.
- **Linux/macOS**: Stub (compiled but not used).

## Dependencies

- `IRenderCommandSink` declared in `Fwd.h` (Phase 1).
- `RenderCommand` struct defined in `RenderCommand.h`.
- `ClipRect` (reused from Phase 7).
