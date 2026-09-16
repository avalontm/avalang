#pragma once

#include "../../../../avalang/platform/interfaces/services/ui/IWheel.h"
#include "../../../../avalang/platform/interfaces/services/ui/ITextInput.h"
#include "../../../../avalang/platform/interfaces/services/ui/ITouch.h"
#include "../../../../avalang/platform/interfaces/services/ui/IIme.h"
#include <cstdint>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

class WinWheel final : public ava::platform::ui::IWheel {
public:
    void ConsumeDelta(float& outDeltaX, float& outDeltaY) override;
};

class WinTextInput final : public ava::platform::ui::ITextInput {
public:
    std::string ConsumeCommittedText() override;
};

class WinTouch final : public ava::platform::ui::ITouch {
public:
    std::vector<ava::platform::ui::NativeTouchPoint> ActivePoints() const override;
};

class WinIme final : public ava::platform::ui::IIme {
public:
    ava::platform::ui::ImeComposition CurrentComposition() const override;
};

void WinInput_PushWheelDelta(float deltaX, float deltaY);
void WinInput_PushChar(unsigned int utf16CodeUnit);
void WinInput_SetTouchPoints(const std::vector<ava::platform::ui::NativeTouchPoint>& points);
void WinInput_SetImeComposition(bool active, const std::string& text, int cursor);

}
}
}
}
