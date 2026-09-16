#pragma once

#include "runtime/avalang/platform/interfaces/services/ui/IClipboard.h"

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidClipboard final : public ava::platform::ui::IClipboard {
public:
    void SetText(const std::string& text) override;
    std::string GetText() const override;
    bool HasText() const override;
};

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
