#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "components/ComponentTree.h"
#include "Export.h"

namespace avalang {
namespace ui {
namespace parser {

class AVA_UI_API ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, int line, int column = 0,
               std::string source = "");
    int Line() const { return line_; }
    int Column() const { return column_; }
    const std::string& Source() const { return source_; }

    const std::string& RawMessage() const { return rawMessage_; }

private:
    int line_;
    int column_;
    std::string source_;
    std::string rawMessage_;
};

struct ParseErrorInfo {
    std::string message;
    int line = 0;
    int column = 0;
    std::string source;
};

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
    std::unordered_set<std::string> constNames;
    std::vector<AnimationSpec> animations;
};

class AVA_UI_API AvauiParser {
public:
    static ParsedAvaui Parse(const std::string& source,
                             const std::string& sourcePath = "");
};

}
}
}