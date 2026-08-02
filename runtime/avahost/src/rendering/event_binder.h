#pragma once
// AvaHost.Rendering.Event -- server side of the click/data-handler POST
// flow. The avaui engine's own SceneCommandWalker emits data-event/
// data-handler attributes for bound components; this class parses that
// POST body back into a handler name plus, for TextBox/ComboBox
// changes, the control's new value and the id of the component that
// sent it (AvaHostApp::HandleEventRoute).
#include <string>

namespace avahost {

class EventBinder {
public:
    static std::string ExtractHandlerName(const std::string& urlEncodedBody);
    static std::string ExtractControlValue(const std::string& urlEncodedBody);
    static std::string ExtractCompId(const std::string& urlEncodedBody);
    static std::string ExtractField(const std::string& urlEncodedBody, const std::string& key);
};

} // namespace avahost
