#include "events/AutoBind.h"

#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/IComponent.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

namespace {

const std::set<std::string>& EventPropNamesSet() {
    static const std::set<std::string> names = {
        "click", "doubleClick", "rightClick",
        "pointerDown", "pointerUp", "pointerMove", "pointerEnter", "pointerLeave",
        "keyDown", "keyUp", "keyPress",
        "change", "input", "submit", "focus", "blur",
        "load", "unload", "resize", "scroll",
        "contextMenu", "drag", "drop", "dragStart", "dragEnd", "dragEnter", "dragLeave", "dragOver",
    };
    return names;
}

struct DefaultEvents {
    const char* type;
    const char* event;
};

const std::vector<DefaultEvents>& Defaults() {
    static const std::vector<DefaultEvents> defaults = {
        {"Button", "click"},
        {"Link", "click"},
        {"TextBox", "change"},
        {"CheckBox", "change"},
        {"RadioButton", "change"},
        {"ComboBox", "change"},
        {"Image", "click"},
        {"Text", "click"},
        {"Divider", ""},
        {"Spacer", ""},
        {"Dialog", "load"},
        {"Column", ""},
        {"Row", ""},
        {"Slot", ""},
    };
    return defaults;
}

std::string ToPascal(const std::string& s) {
    if (s.empty()) return s;
    std::string out;
    bool cap = true;
    for (char c : s) {
        if (c == '_' || c == '-' || c == ' ') { cap = true; continue; }
        out.push_back(cap ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
        cap = false;
    }
    return out;
}

std::string Capitalize(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

std::set<std::string> CollectFunctionNames(const std::string& code) {
    std::set<std::string> names;
    std::istringstream ss(code);
    std::string line;
    while (std::getline(ss, line)) {
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return std::string();
            size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        };
        std::string t = trim(line);
        if (t.empty()) continue;

        static const std::vector<std::string> declPrefixes = {"func "};
        for (const std::string& prefix : declPrefixes) {
            if (t.size() <= prefix.size()) continue;
            if (t.compare(0, prefix.size(), prefix) != 0) continue;

            std::string rest = t.substr(prefix.size());
            size_t paren = rest.find('(');
            if (paren == std::string::npos) break;
            std::string fname = rest.substr(0, paren);
            auto trimName = [](std::string s) {
                size_t a = s.find_first_not_of(" \t");
                if (a == std::string::npos) return std::string();
                size_t b = s.find_last_not_of(" \t");
                return s.substr(a, b - a + 1);
            };
            fname = trimName(fname);
            if (!fname.empty()) names.insert(fname);
            break;
        }
    }
    return names;
}

void AutoBindRecursive(IComponent* node, const std::set<std::string>& funcs) {
    if (!node) return;

    const std::string& typeName = node->TypeName();
    const PropertyValue* idProp = node->GetProperty("id");
    std::string id = idProp ? idProp->AsString() : "";

    for (const auto& def : Defaults()) {
        if (typeName != def.type) continue;
        if (!def.event || def.event[0] == '\0') break;
        if (node->HasProperty(def.event)) break;
        if (id.empty()) break;

        std::string pascalId = ToPascal(id);
        std::string handler = "On" + pascalId + Capitalize(def.event);
        if (funcs.count(handler)) {
            node->SetProperty(def.event, PropertyValue(handler));
        }
        break;
    }

    for (IComponent* child : node->Children()) {
        AutoBindRecursive(child, funcs);
    }
}

}

const std::unordered_set<std::string>& EventPropNames() {
    static const std::unordered_set<std::string> set(
        EventPropNamesSet().begin(), EventPropNamesSet().end());
    return set;
}

bool IsEventPropertyName(const std::string& name) {
    return EventPropNames().count(name) > 0;
}

void AutoBindEvents(IComponent* root, const std::string& codeText) {
    auto funcs = CollectFunctionNames(codeText);
    AutoBindRecursive(root, funcs);
}

const std::unordered_map<std::string, std::string>& NewEventPropertyNames() {
    static const std::unordered_map<std::string, std::string> names = {
        {"onClick", "click"},
        {"onChange", "change"},
        {"onFocus", "focus"},
        {"onBlur", "blur"},
    };
    return names;
}

bool IsNewEventPropertyName(const std::string& name) {
    return NewEventPropertyNames().count(name) > 0;
}

std::string ResolveEventPropertyName(const std::string& name) {
    const auto& names = NewEventPropertyNames();
    auto it = names.find(name);
    return it != names.end() ? it->second : name;
}

namespace {

std::string TrimCopy(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool IsSimpleIdentifier(const std::string& s) {
    if (s.empty()) return false;
    if (std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for (char c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }
    return true;
}

bool TryParseCallSyntax(const std::string& text, std::string* nameOut, std::string* argsOut) {
    if (text.empty() || text.back() != ')') return false;
    size_t open = text.find('(');
    if (open == std::string::npos) return false;

    std::string name = TrimCopy(text.substr(0, open));
    if (!IsSimpleIdentifier(name)) return false;

    if (nameOut) *nameOut = name;
    if (argsOut) *argsOut = text.substr(open + 1, text.size() - open - 2);
    return true;
}

}

EventHandlerInvocation ParseEventHandlerSource(const std::string& source) {
    EventHandlerInvocation result;
    std::string trimmed = TrimCopy(source);

    if (IsSimpleIdentifier(trimmed)) {
        result.kind = EventHandlerKind::Reference;
        result.handlerName = trimmed;
        return result;
    }

    std::string name, args;
    if (TryParseCallSyntax(trimmed, &name, &args)) {
        result.kind = EventHandlerKind::Call;
        result.handlerName = name;
        result.argsText = TrimCopy(args);
        return result;
    }

    result.kind = EventHandlerKind::Statement;
    result.statementText = trimmed;
    return result;
}

}
}
