#pragma once

#include "Export.h"
#include "Fwd.h"
#include "events/IEventDispatcher.h"
#include "runtime/avalang/platform/interfaces/services/ui/IClipboard.h"

#include <memory>
#include <vector>

namespace avalang {
namespace ui {
namespace controls {

class AVA_UI_API TextBoxEditingController {
public:
    TextBoxEditingController(events::IEventDispatcher& dispatcher,
                              ava::platform::ui::IClipboard& clipboard);
    ~TextBoxEditingController();

    TextBoxEditingController(const TextBoxEditingController&) = delete;
    TextBoxEditingController& operator=(const TextBoxEditingController&) = delete;

    void Attach(IComponent* root);

private:
    class Handler;

    void AttachRecursive(IComponent* node);

    events::IEventDispatcher& dispatcher_;
    ava::platform::ui::IClipboard& clipboard_;
    std::vector<std::unique_ptr<Handler>> handlers_;
};

}
}
}
