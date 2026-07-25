#include "languages/builtin_signatures.h"

namespace studio {

namespace {

FunctionSignature Make(const std::string& name, std::vector<std::string> params, bool has_var_args,
                        const std::string& doc) {
    FunctionSignature sig;
    sig.name = name;
    sig.params = std::move(params);
    sig.has_var_args = has_var_args;
    for (const auto& p : sig.params) {
        if (!p.empty() && p[0] == '*') continue;
        if (p.find('=') == std::string::npos) sig.min_args++;
    }
    std::string display = name + "(";
    for (size_t i = 0; i < sig.params.size(); ++i) {
        if (i) display += ", ";
        display += sig.params[i];
    }
    display += ")";
    sig.display = std::move(display);
    sig.doc = doc;
    sig.is_builtin = true;
    // Every builtin here is registered via VM::RegisterNative, which goes
    // through the same SetGlobal() as a top-level `func` -- so all of
    // them are overridable by a same-named local declaration.
    sig.overridable = true;
    return sig;
}

} // namespace

const std::unordered_map<std::string, FunctionSignature>& BuiltinSignatures() {
    static const std::unordered_map<std::string, FunctionSignature> table = [] {
        std::unordered_map<std::string, FunctionSignature> m;

        auto add = [&](FunctionSignature sig) { m[sig.name] = std::move(sig); };

        add(Make("type", {"value"}, false,
                 "Returns the type name of value as a string: \"nil\", \"bool\", \"number\", "
                 "\"string\", \"list\", \"dict\", \"function\", \"instance\", \"class\", "
                 "\"coroutine\", \"native\", \"bound\", or \"exception\"."));
        add(Make("str", {"value"}, false,
                 "Converts value to its display string (the same formatting print() uses)."));
        add(Make("int", {"value"}, false,
                 "Truncates value toward zero and returns it as a number. Strings/bools are "
                 "coerced first (invalid strings become 0)."));
        add(Make("float", {"value"}, false,
                 "Coerces value to a floating-point number (strings/bools are converted; "
                 "invalid strings become 0)."));
        add(Make("print", {"*values"}, true,
                 "Prints every argument, converted with the same rules as str(), separated by "
                 "single spaces, followed by one newline. Accepts zero or more arguments of any "
                 "type -- print() alone just prints a blank line."));

        add(Make("abs", {"number"}, false, "Returns the absolute value of number."));
        add(Make("round", {"number"}, false, "Rounds number to the nearest integer (half away from zero)."));
        add(Make("floor", {"number"}, false, "Rounds number down toward negative infinity."));
        add(Make("ceil", {"number"}, false, "Rounds number up toward positive infinity."));
        add(Make("pow", {"base", "exponent"}, false, "Returns base raised to exponent (base ** exponent)."));
        add(Make("sqrt", {"number"}, false, "Returns the square root of number."));

        add(Make("min", {"*values"}, true,
                 "Returns the smallest value. Call it either as min(a, b, ...) with two or more "
                 "arguments, or as min(list) with a single list -- returns nil if given nothing "
                 "and no list."));
        add(Make("max", {"*values"}, true,
                 "Returns the largest value. Same two calling forms as min(): max(a, b, ...) or "
                 "max(list)."));
        add(Make("sum", {"*values"}, true,
                 "Returns the numeric total. Same two calling forms as min(): sum(a, b, ...) or "
                 "sum(list); non-numeric items coerce to 0."));
        add(Make("any", {"*values"}, true,
                 "Returns true if at least one value/item is truthy. Same two calling forms as "
                 "min(): any(a, b, ...) or any(list)."));
        add(Make("all", {"*values"}, true,
                 "Returns true only if every value/item is truthy. Same two calling forms as "
                 "min(): all(a, b, ...) or all(list)."));

        add(Make("sorted", {"list"}, false,
                 "Returns a new list with list's items sorted ascending (numbers compare "
                 "numerically, strings lexicographically). Non-list input returns an empty list."));
        add(Make("reversed", {"list"}, false,
                 "Returns a new list with list's items in reverse order. Non-list input returns "
                 "an empty list."));

        add(Make("len", {"value"}, false,
                 "Returns the length of a string, list, or dict (character count / item count / "
                 "entry count). Returns 0 for any other type."));

        add(Make("range", {"end"}, true,
                 "Builds a list of numbers. Three calling forms: range(end) counts 0..end-1; "
                 "range(start, end) counts start..end-1; range(start, end, step) steps by step "
                 "(negative step counts down). Overload note: this entry shows the 1-argument "
                 "form -- range(start, end) and range(start, end, step) are also valid."));

        return m;
    }();
    return table;
}

} // namespace studio
