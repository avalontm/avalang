#pragma once

#include "../../../../avalang/platform/interfaces/services/ui/IPlatformServices.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

class WinPlatformServices final : public ava::platform::ui::IPlatformServices {
public:
    bool OpenFileDialog(std::string& outPath) override;
    bool SaveFileDialog(std::string& outPath) override;
    void ShowNotification(const std::string& title, const std::string& body) override;
    void OpenUri(const std::string& uri) override;
};

}
}
}
}
