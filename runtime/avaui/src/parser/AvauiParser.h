#ifndef AVA_UI_PARSER_AVAUIPARSER_H
#define AVA_UI_PARSER_AVAUIPARSER_H

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/ComponentTree.h"
#include "Export.h"

namespace avalang {
namespace ui {
namespace parser {






































class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, int line);
    int Line() const { return line_; }

private:
    int line_;
};


































struct AnimationSpec {
    ComponentId target = 0;
    std::string property;
    std::string fromRaw, toRaw;
    std::string duration;
    std::string easing;
    std::string trigger;
    std::string mode;
};

enum class RouteParameterKind { Required, Optional };

struct RouteParameter {
    std::string name;
    RouteParameterKind kind = RouteParameterKind::Required;
    std::string constraint;
};

struct RouteDeclaration {
    std::string route_template;
    std::vector<RouteParameter> parameters;
};

// Declares one argument a component accepts at its call site, e.g.
// `ProductCard(productId = 3, name = "Latte")` requires ProductCard's
// own .avaui to declare:
//     params
//         productId
//         name
//         price = "0.00"
//     end
// A bare name (`productId`) is required -- every call site MUST pass
// it. `name = default` is optional -- call sites may omit it, and
// `defaultValue` (parsed the same way a call-site argument's own
// value is, via InferValue) is substituted in its place. Declaring a
// `params` block at all switches the component into validated mode:
// ComponentResolver then rejects (at resolve time, not silently at
// click time) any call site missing a required param or passing an
// argument the component never declared -- see
// ComponentResolver::ValidateCallSiteArgs. Components with no
// `params` block keep today's permissive, unvalidated behavior for
// backward compatibility.
struct ParamDeclaration {
    std::string name;
    bool hasDefault = false;
    PropertyValue defaultValue;
};

struct ParsedAvaui {
    std::unique_ptr<ComponentTree> tree;
    std::string extends;
    std::vector<RouteDeclaration> routes;
    std::vector<std::string> imports;
    std::unordered_map<std::string, std::string> properties;
    std::unordered_map<std::string, std::string> state;
    std::unordered_map<std::string, std::string> style;
    std::vector<ParamDeclaration> params;
    std::string code;
    std::vector<AnimationSpec> animations;
};








class AVA_UI_API AvauiParser {
public:




    static ParsedAvaui Parse(const std::string& source);
};

}
}
}

#endif