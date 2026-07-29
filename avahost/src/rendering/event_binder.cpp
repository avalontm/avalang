#include "rendering/event_binder.h"

#include <sstream>

#include "web/protocol/url_codec.h"

namespace avahost {

namespace {

std::string HtmlEscapeAttr(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

} // namespace

std::string EventBinder::RenderAttributes(AvaComponent* comp) {
    if (!comp) return "";

    size_t count = ava_ui_event_count(comp);
    std::ostringstream out;
    size_t emitted = 0;

    for (size_t i = 0; i < count; ++i) {
        const char* eventNamePtr = ava_ui_event_key_at(comp, i);
        if (!eventNamePtr || !*eventNamePtr) continue;
        std::string eventName = eventNamePtr; // copy before the next ava_ui_* call

        ava_value_t handlerValue = ava_ui_get_event(comp, eventName.c_str());
        if (handlerValue.type != AVA_STRING) continue; // handler isn't a plain name (e.g. unset)

        size_t len = 0;
        const char* data = ava_string_data(nullptr, handlerValue, &len);
        std::string handlerName = data ? std::string(data, len) : "";
        if (handlerName.empty()) continue;

        ++emitted;
        std::string suffix = emitted == 1 ? "" : ("-" + std::to_string(emitted));
        out << " data-event" << suffix << "=\"" << HtmlEscapeAttr(eventName) << "\""
            << " data-handler" << suffix << "=\"" << HtmlEscapeAttr(handlerName) << "\"";
    }

    return out.str();
}

std::string EventBinder::ExtractHandlerName(const std::string& urlEncodedBody) {
    for (const auto& [key, value] : ParseQueryString(urlEncodedBody)) {
        if (key == "handler") return value;
    }
    return "";
}

} // namespace avahost
