#pragma once
#include <string>

namespace avalang {
namespace ui {
class IComponent;
namespace events {
class IEventDispatcher;
}  // namespace events
}  // namespace ui
}  // namespace avalang

namespace avahost {

class RuntimeHost;
class VmStateBridge;

int WireVmEventHandlers(avalang::ui::IComponent* root, avalang::ui::events::IEventDispatcher& dispatcher,
                         RuntimeHost& host, VmStateBridge& stateBridge);

// Exposes/collects the same `id.property` globals WireVmEventHandlers'
// VmEventHandler::OnEvent binds around a dispatcher-fired event, for the
// call sites (avahost run/watch's direct-by-name handler invocation) that
// never go through IEventDispatcher::Dispatch. Call BindComponentRefs
// before invoking a handler and ExportComponentProps right after it
// succeeds, so component property reads/writes (e.g. `Label1.text = "..."`)
// inside that handler actually reach the render tree.
void BindComponentRefs(RuntimeHost& host, avalang::ui::IComponent* root);
void ExportComponentProps(RuntimeHost& host, avalang::ui::IComponent* root);

}  // namespace avahost
