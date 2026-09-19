#pragma once

#include "Export.h"
#include "Fwd.h"
#include "events/IEventDispatcher.h"

#include <memory>
#include <vector>

namespace avalang {
namespace ui {
namespace controls {

class AVA_UI_API ButtonController {
public:
    explicit ButtonController(events::IEventDispatcher& dispatcher);
    ~ButtonController();

    ButtonController(const ButtonController&) = delete;
    ButtonController& operator=(const ButtonController&) = delete;

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
