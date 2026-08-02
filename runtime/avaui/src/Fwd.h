#ifndef AVA_UI_FWD_H
#define AVA_UI_FWD_H

// Phase 1 -- Core Architecture.
//
// Forward declarations only. Each type is owned by the phase that
// introduces it (see docs/AVALANG_UI_IMPLEMENTATION_PLAN.md). Listing a
// name here fixes its place in the dependency graph before it exists --
// it does NOT authorize implementing it ahead of its phase.
//
// Dependency rule (never point upward, no cycles):
//
//   Components -> Layout -> State -> Render Tree -> Scene Graph
//              -> Renderer -> Platform Services
//
// Concretely: components/ may not include layout/, render/, scene/,
// renderer/ or platform/ headers. layout/ may not include render/,
// scene/, renderer/ or platform/. state/ is consumed by components/
// and layout/ but must not depend on either. render/ may depend on
// components/, layout/ and state/ but not on scene/ or renderer/.
// scene/ may depend on render/ but not on renderer/. renderer/ and
// platform/ sit at the bottom -- nothing below them to depend on.
// events/ mirrors state/: consumed top-down, depends on platform/ for
// raw input only (see Phase 5).

namespace avalang {
namespace ui {

// Phase 2 -- Component Tree
class IComponent;
class ComponentTree;
using ComponentId = unsigned long long;

// Phase 3 -- Layout Engine
class ILayoutNode;
class LayoutEngine;
struct LayoutRect;

// Phase 4 -- State System
class IState;
class StateBinding;

// Phase 5 -- Event System
class IEvent;
class IMouseEvent;
class IKeyboardEvent;
class IEventDispatcher;
class IEventHandler;
enum class EventType : unsigned char;
enum class MouseButton : unsigned char;

// Phase 6 -- Render Tree
class IRenderNode;
class IRenderTree;
enum class RenderNodeType : unsigned char;

// Phase 7 -- Scene Graph
class ISceneNode;
class ISceneGraph;
struct Transform;
struct ClipRect;
struct DirtyRegion;

// Phase 8 -- Render Commands
class IRenderCommandSink;
struct RenderCommand;

// Phase 9 -- Renderer Interface
class IRenderer;

} // namespace ui
} // namespace avalang

#endif // AVA_UI_FWD_H
