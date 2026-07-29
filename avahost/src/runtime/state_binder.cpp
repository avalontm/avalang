#include "runtime/state_binder.h"

namespace avahost {

void StateBinder::Bind(const std::string& stateJson, const std::string& methodsText) {
    runtime_.BindState(stateJson);
    runtime_.BindCodeBehind(methodsText);
}

bool StateBinder::Dispatch(const std::string& handlerName, std::string& outError) {
    if (handlerName.empty()) return true; // nothing to dispatch -- ordinary GET render
    return runtime_.InvokeHandler(handlerName, outError);
}

bool StateBinder::DispatchLifecycle(const std::string& hookName, std::string& outError) {
    return runtime_.InvokeHandlerIfDefined(hookName, outError);
}

std::function<std::string(const std::string&)> StateBinder::TextEvaluator() {
    RuntimeHost* runtime = &runtime_;
    return [runtime](const std::string& raw) { return runtime->EvalPropertyExpr(raw); };
}

} // namespace avahost
