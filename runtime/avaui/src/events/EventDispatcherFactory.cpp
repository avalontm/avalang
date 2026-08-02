#include "events/IEventDispatcher.h"
#include "events/EventDispatcher.h"

namespace avalang {
namespace ui {
namespace events {

IEventDispatcher* IEventDispatcher::Create() {
    return new EventDispatcher();
}

} // namespace events
} // namespace ui
} // namespace avalang
