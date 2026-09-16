#pragma once

#include "runtime/avalang/platform/interfaces/services/ui/ITextInput.h"
#include "runtime/avalang/platform/interfaces/services/ui/ITouch.h"
#include "runtime/avalang/platform/interfaces/services/ui/IIme.h"
#include <vector>

namespace avalang {
namespace ui {
namespace platform {
namespace android {

class AndroidTextInput final : public ava::platform::ui::ITextInput {
public:
    std::string ConsumeCommittedText() override;
};

class AndroidTouch final : public ava::platform::ui::ITouch {
public:
    std::vector<ava::platform::ui::NativeTouchPoint> ActivePoints() const override;
};

class AndroidIme final : public ava::platform::ui::IIme {
public:
    ava::platform::ui::ImeComposition CurrentComposition() const override;
};

void AndroidInput_SetTouchPoints(const std::vector<ava::platform::ui::NativeTouchPoint>& points);
void AndroidInput_PushCommittedText(const std::string& text);
void AndroidInput_SetImeComposition(bool active, const std::string& text, int cursor);

} // namespace android
} // namespace platform
} // namespace ui
} // namespace avalang
