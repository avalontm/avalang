#ifndef AVA_PLATFORM_SERVICES_UI_ICLIPBOARD_H
#define AVA_PLATFORM_SERVICES_UI_ICLIPBOARD_H

#include <string>

namespace ava {
namespace platform {
namespace ui {

// Phase 0 stub. Required by TextBox / TextEditor / IDE / UI controls once
// avalang.ui.dll exists (see docs/Platform_Foundation.md).
class IClipboard {
public:
    virtual ~IClipboard() = default;

    virtual void SetText(const std::string& text) = 0;
    virtual std::string GetText() const = 0;
    virtual bool HasText() const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_ICLIPBOARD_H
