#pragma once

#include "Export.h"
#include "Fwd.h"
#include "events/IEventDispatcher.h"

#include <memory>
#include <vector>

namespace avalang {
namespace ui {
namespace controls {

class AVA_UI_API ComboBoxController {
public:
    explicit ComboBoxController(events::IEventDispatcher& dispatcher);
    ~ComboBoxController();

    ComboBoxController(const ComboBoxController&) = delete;
    ComboBoxController& operator=(const ComboBoxController&) = delete;

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
