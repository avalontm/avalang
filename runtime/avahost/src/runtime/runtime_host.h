#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "avalang.h"

namespace avahost {

struct RequestContext {
    std::string method;
    std::string path;  

    std::vector<std::pair<std::string, std::string>> params;

    std::vector<std::pair<std::string, std::string>> query;
};

class RuntimeHost {
public:
    RuntimeHost();
    ~RuntimeHost();

    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;

    void AddSearchPath(const std::string& path);
    void SetCurrentDir(const std::string& path);

    void SetRequestContext(const RequestContext& ctx);

    bool RunScript(const std::string& source, const std::string& scriptName,
                    std::string& outError);
    bool RunScriptCapturingOutput(const std::string& source, const std::string& scriptName,
                                   std::string& outOutput, std::string& outError);

    struct RouteParam {
        std::string name;
        bool optional = false;
        std::string constraint; 
    };

    struct RouteTemplate {
        std::string pathTemplate;
        std::vector<RouteParam> params;
    };

    std::vector<RouteTemplate> ParseRouteDeclarations(const std::string& text) const;

    bool ValidateAvaUiFile(const std::string& text, std::string& outError) const;

    void BindState(const std::string& stateJson);
    bool BindCodeBehind(const std::string& methodsText, std::string* outError = nullptr);

    bool InvokeHandler(const std::string& handlerName, std::string& outError);

    bool InvokeHandlerIfDefined(const std::string& handlerName, std::string& outError);
    std::string EvalPropertyExpr(const std::string& rawValue);

    std::string EvalExprToLiteral(const std::string& expr, bool& ok);

    bool EvalAssignGlobal(const std::string& name, const std::string& expr);

    std::string ExportStateJson(const std::string& templateStateJson);

    void BeginConsoleCapture();

    std::string EndConsoleCapture();

    AvaVM* GetVM() const { return vm_; }

private:
    AvaVM* vm_ = nullptr;
    std::string consoleCaptureBuffer_;

    static bool SplitNamespacedKey(const std::string& key, std::string& outNamespace, std::string& outField);

    ava_value_t GetOrCreateDictGlobal(const std::string& ns);
};

} // namespace avahost