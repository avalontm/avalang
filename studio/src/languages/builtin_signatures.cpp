#include "languages/builtin_signatures.h"

#include <filesystem>

#include "util/csv.h"
#include "util/data_dir.h"

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

const std::unordered_map<std::string, FunctionSignature>& DefaultBuiltinSignatures() {
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

namespace {

namespace fs = std::filesystem;

// data/builtin_signatures.csv columns: name,params,doc
//  - params: pipe-separated raw parameter tokens as they'd appear in
//    FunctionSignature::params ("value", "*values", "base|exponent" for
//    two params). has_var_args is derived from any token starting with
//    "*", rather than a separate column, since the token already carries
//    that information.
//  - doc: plain sentence(s).
bool LoadBuiltinSignaturesFromCsv(const std::string& path,
                                   std::unordered_map<std::string, FunctionSignature>& out) {
    std::string text;
    if (!util::ReadFileToString(path, text)) return false;

    auto rows = util::ParseCsv(text);
    if (rows.empty()) return false;

    std::unordered_map<std::string, FunctionSignature> parsed;
    // rows[0] is the header (name,params,doc) -- skip it.
    for (size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (row.size() == 1 && row[0].empty()) continue; // blank line
        if (row.size() < 3) continue; // malformed row -- skip rather than crash on it

        const std::string name = row[0];
        if (name.empty()) continue;

        std::vector<std::string> params;
        bool has_var_args = false;
        for (const auto& token : util::SplitOn(row[1], "|")) {
            if (token.empty()) continue;
            params.push_back(token);
            if (token[0] == '*') has_var_args = true;
        }

        FunctionSignature sig = Make(name, std::move(params), has_var_args, util::UnescapeCell(row[2]));
        parsed[name] = std::move(sig);
    }

    if (parsed.empty()) return false;
    out = std::move(parsed);
    return true;
}

} // namespace

const std::unordered_map<std::string, FunctionSignature>& BuiltinSignatures() {
    static std::unordered_map<std::string, FunctionSignature> table;
    static fs::file_time_type last_loaded_time{};
    static bool loaded_from_csv = false;
    static bool first_call = true;

    const std::string csv_path = util::ResolveDataDir() + "builtin_signatures.csv";
    std::error_code ec;
    fs::file_time_type current_time = fs::last_write_time(csv_path, ec);
    bool csv_exists = !ec;

    bool should_reload = first_call || (csv_exists && current_time != last_loaded_time);

    if (should_reload) {
        first_call = false;
        if (csv_exists && LoadBuiltinSignaturesFromCsv(csv_path, table)) {
            loaded_from_csv = true;
            last_loaded_time = current_time;
        } else if (!loaded_from_csv) {
            table = DefaultBuiltinSignatures();
        }
    }

    return table;
}

} // namespace studio
