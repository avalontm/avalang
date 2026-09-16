#ifndef AVA_PLATFORM_SERVICES_UI_ITEXTINPUT_H
#define AVA_PLATFORM_SERVICES_UI_ITEXTINPUT_H

#include <string>

namespace ava {
namespace platform {
namespace ui {

class ITextInput {
public:
    virtual ~ITextInput() = default;

    virtual std::string ConsumeCommittedText() = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_ITEXTINPUT_H
