#include "ui_vm_lifecycle.h"

#include "components/IComponent.h"
#include "runtime/runtime_host.h"
#include "ui_vm_event_bridge.h"
#include "ui_vm_state_bridge.h"
#include "view/IAvaView.h"

namespace avahost {

bool VmLifecycle::NotifyMount(RuntimeHost& host, VmStateBridge& stateBridge,
                               const std::vector<avalang::ui::IComponent*>& roots,
                               std::string& outError) {
    avalang::ui::IAvaView* view = host.CurrentView();
    if (view && view->IsLoaded()) {
        std::string handlerError;
        if (!view->OnLoad(handlerError)) {
            outError = "onLoad handler failed: " + handlerError;
            return false;
        }
        stateBridge.RefreshAll();
        return true;
    }

    for (avalang::ui::IComponent* root : roots) {
        BindComponentRefs(host, root);
    }

    for (avalang::ui::IComponent* root : roots) {
        ExportComponentProps(host, root);
    }
    stateBridge.RefreshAll();
    return true;
}

bool VmLifecycle::NotifyMount(RuntimeHost& host, VmStateBridge& stateBridge,
                               avalang::ui::IComponent* root, std::string& outError) {
    return NotifyMount(host, stateBridge, std::vector<avalang::ui::IComponent*>{root}, outError);
}

void VmLifecycle::NotifyUnmount(RuntimeHost& host, VmStateBridge& stateBridge,
                                 const std::vector<avalang::ui::IComponent*>& roots) {
    avalang::ui::IAvaView* view = host.CurrentView();
    if (view && view->IsLoaded()) {
        std::string handlerError;
        view->OnUnload(handlerError);
        stateBridge.RefreshAll();
        return;
    }

    for (avalang::ui::IComponent* root : roots) {
        BindComponentRefs(host, root);
    }
    stateBridge.RefreshAll();
}

void VmLifecycle::NotifyUnmount(RuntimeHost& host, VmStateBridge& stateBridge,
                                 avalang::ui::IComponent* root) {
    NotifyUnmount(host, stateBridge, std::vector<avalang::ui::IComponent*>{root});
}

}  // namespace avahost
