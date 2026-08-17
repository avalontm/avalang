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






































// Carries a structured source position alongside the message, mirroring
// ava::AvaError (common/ava_error.h) on the .ava side so both frontends
// can eventually be surfaced the same way (avahost error responses,
// AvaStudio's HighlightError). column and source default to "unknown"
// (0 / empty) so every existing 2-arg `throw ParseError(msg, line)` call
// site keeps compiling unchanged -- they just report column 0 until
// Phase 5 gives them a real column.
class AVA_UI_API ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, int line, int column = 0,
               std::string source = "");
    int Line() const { return line_; }
    int Column() const { return column_; }
    const std::string& Source() const { return source_; }

    // Fase 2: the message passed to the constructor, before the
    // "(file:line:col)" suffix gets baked into what() below. Parse()
    // needs this so it can relabel a ParseError with the real
    // sourcePath (once it's known, at the Parse() boundary) without
    // re-wrapping an already-formatted what() string.
    const std::string& RawMessage() const { return rawMessage_; }

private:
    int line_;
    int column_;
    std::string source_;
    std::string rawMessage_;
};

// Fase 3: plain-data mirror of ParseError's fields for callers (avahost,
// AvaStudio) that want the structured position without catching the
// exception themselves -- an optional out-param filled in alongside the
// usual bool-return/outError convention already used across this
// codebase (e.g. RenderAvauiDynamicWithState's outParseError).
struct ParseErrorInfo {
    std::string message;
    int line = 0;
    int column = 0;
    std::string source;
};

// Fase 3: minimal port of frontend_antlr.cpp's formatError (the .ava
// side) so a .avaui error can be logged/displayed in the same
// "source:line:col: message" + offending line + "^~~~" caret shape
// instead of a bare message. `sourceText` is the raw file contents the
// error came from, used to pull out and underline the offending line;
// pass "" if unavailable (e.g. only the info survived, not the original
// text) and you'll just get the header line with no excerpt.
AVA_UI_API std::string FormatParseError(const ParseErrorInfo& info, const std::string& sourceText);


































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




    // sourcePath identifies which .avaui file `source` came from. It's
    // threaded into every ParseError thrown while parsing this source, so
    // a syntax error inside an imported component or an `extends` layout
    // reports its OWN file rather than whichever file happened to trigger
    // the render (see PLAN_DIAGNOSTICOS_AVAUI.md, Fase 2/6). Defaults to
    // "" so existing 1-arg call sites keep compiling and just report an
    // unlabeled error, same as before this parameter existed.
    static ParsedAvaui Parse(const std::string& source,
                              const std::string& sourcePath = "");
};

}
}
}

#endif