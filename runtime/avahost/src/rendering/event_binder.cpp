#include "rendering/event_binder.h"

#include "web/protocol/url_codec.h"

namespace avahost {

namespace {

constexpr const char* kControlValueFields[] = {
    "text", "selectedValue", "checked", "selected", "value"
};

}

std::string EventBinder::ExtractField(const std::string& urlEncodedBody, const std::string& key) {
    for (const auto& [k, value] : ParseQueryString(urlEncodedBody)) {
        if (k == key) return value;
    }
    return "";
}

std::string EventBinder::ExtractHandlerName(const std::string& urlEncodedBody) {
    return ExtractField(urlEncodedBody, "handler");
}

std::string EventBinder::ExtractControlValue(const std::string& urlEncodedBody) {
    const auto parsed = ParseQueryString(urlEncodedBody);
    for (const char* field : kControlValueFields) {
        for (const auto& [k, value] : parsed) {
            if (k == field) return value;
        }
    }
    return "";
}

std::string EventBinder::ExtractCompId(const std::string& urlEncodedBody) {
    return ExtractField(urlEncodedBody, "compId");
}

} // namespace avahost
