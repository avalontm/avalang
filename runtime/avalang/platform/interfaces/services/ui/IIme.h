#ifndef AVA_PLATFORM_SERVICES_UI_IIME_H
#define AVA_PLATFORM_SERVICES_UI_IIME_H

#include <string>

namespace ava {
namespace platform {
namespace ui {

struct ImeComposition {
    bool active = false;
    std::string text;
    int cursor = 0;
};

class IIme {
public:
    virtual ~IIme() = default;

    virtual ImeComposition CurrentComposition() const = 0;
};

} // namespace ui
} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_SERVICES_UI_IIME_H
