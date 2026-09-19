#pragma once

#include "Export.h"
#include "Fwd.h"
#include "events/IEventDispatcher.h"

#include <memory>
#include <vector>

namespace avalang {
namespace ui {
namespace controls {

class AVA_UI_API RadioButtonController {
public:
    explicit RadioButtonController(events::IEventDispatcher& dispatcher);
    ~RadioButtonController();

    RadioButtonController(const RadioButtonController&) = delete;
    RadioButtonController& operator=(const RadioButtonController&) = delete;

    void Attach(IComponent* root);

private:
    class Handler;

    void AttachRecursive(IComponent* node);

    events::IEventDispatcher& dispatcher_;
    std::vector<std::unique_ptr<Handler>> handlers_;
};

}
}
}
