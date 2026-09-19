#pragma once

namespace avalang {
namespace ui {

class IComponent;
class ComponentTree;
using ComponentId = unsigned long long;

class ILayoutNode;
class LayoutEngine;
struct LayoutRect;

enum class InvalidationFlag : unsigned char;

class IState;
class StateBinding;

class IAvaView;

class IEvent;
class IPointerEvent;
class IKeyboardEvent;
class IEventDispatcher;
class IEventHandler;
enum class EventType : unsigned char;
enum class PointerButton : unsigned char;

class IRenderNode;
class IRenderTree;
enum class RenderNodeType : unsigned char;

class ISceneNode;
class ISceneGraph;
struct Transform;
struct ClipRect;
struct DirtyRegion;

class IRenderCommandSink;
struct RenderCommand;

class IRenderer;

}
}