#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Export.h"
#include "Fwd.h"

namespace avalang {
namespace ui {

AVA_UI_API const std::unordered_set<std::string>& EventPropNames();

AVA_UI_API bool IsEventPropertyName(const std::string& name);

AVA_UI_API void AutoBindEvents(IComponent* root, const std::string& codeText);

enum class EventHandlerKind {
    Reference,
    Call,
    Statement,
};

struct EventHandlerInvocation {
    EventHandlerKind kind = EventHandlerKind::Reference;
    std::string handlerName;
    std::string argsText;
    std::string statementText;
};

AVA_UI_API const std::unordered_map<std::string, std::string>& NewEventPropertyNames();

AVA_UI_API bool IsNewEventPropertyName(const std::string& name);

AVA_UI_API std::string ResolveEventPropertyName(const std::string& name);

AVA_UI_API EventHandlerInvocation ParseEventHandlerSource(const std::string& source);

}
}