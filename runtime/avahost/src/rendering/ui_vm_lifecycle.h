#pragma once
#include <string>
#include <vector>

namespace avalang {
namespace ui {
class IComponent;
}
}

namespace avahost {

class RuntimeHost;
class VmStateBridge;

class VmLifecycle {
public:
    static bool NotifyMount(RuntimeHost& host, VmStateBridge& stateBridge,
                             const std::vector<avalang::ui::IComponent*>& roots,
                             std::string& outError);

    static bool NotifyMount(RuntimeHost& host, VmStateBridge& stateBridge,
                             avalang::ui::IComponent* root, std::string& outError);

    static void NotifyUnmount(RuntimeHost& host, VmStateBridge& stateBridge,
                               const std::vector<avalang::ui::IComponent*>& roots);

    static void NotifyUnmount(RuntimeHost& host, VmStateBridge& stateBridge,
                               avalang::ui::IComponent* root);
};

}  // namespace avahost
