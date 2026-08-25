#include "languages/keyword_docs.h"

#include <filesystem>

#include "util/csv.h"
#include "util/data_dir.h"

namespace studio {

namespace {

KeywordDoc Make(const std::string& name, std::vector<std::string> syntax, const std::string& example,
                 const std::string& doc) {
    KeywordDoc entry;
    entry.name = name;
    entry.syntax = std::move(syntax);
    entry.example = example;
    entry.doc = doc;
    return entry;
}

}

const std::unordered_map<std::string, KeywordDoc>& DefaultKeywordDocs() {
    static const std::unordered_map<std::string, KeywordDoc> table = [] {
        std::unordered_map<std::string, KeywordDoc> m;
        auto add = [&](KeywordDoc entry) { m[entry.name] = std::move(entry); };

        add(Make("if",
                 {"if condition then\n    ...\nelif other_condition then\n    ...\nelse\n    ...\nend"},
                 "if age >= 18 then\n    print(\"adult\")\nelse\n    print(\"minor\")\nend",
                 "Runs the first block whose condition is true. elif and else are optional; "
                 "the block always closes with end."));

        add(Make("then",
                 {"if condition then\n    ...\nend"},
                 "",
                 "Starts the body of an if or elif clause. Always follows the condition, "
                 "never stands on its own."));

        add(Make("elif",
                 {"if condition then\n    ...\nelif other_condition then\n    ...\nend"},
                 "if score >= 90 then\n    print(\"A\")\nelif score >= 80 then\n    print(\"B\")\nelse\n    "
                 "print(\"C\")\nend",
                 "Adds another condition to an if statement, checked only if the ones above "
                 "it were false. Optional, and there can be more than one."));

        add(Make("else",
                 {"if condition then\n    ...\nelse\n    ...\nend"},
                 "if is_raining then\n    print(\"bring an umbrella\")\nelse\n    print(\"enjoy the sun\")\nend",
                 "Runs when none of the if/elif conditions above it were true. Optional, and "
                 "must be the last clause before end."));

        add(Make("end",
                 {"if / while / for / func / class / try\n    ...\nend"},
                 "",
                 "Closes the block started by if, while, for, func, class, or try. Every one "
                 "of those needs a matching end."));

        add(Make("while",
                 {"while condition\n    ...\nend", "while (condition)\n    ...\nend"},
                 "count = 0\nwhile count < 5\n    print(count)\n    count = count + 1\nend",
                 "Repeats the block for as long as condition stays true. Parentheses around "
                 "the condition are optional."));

        add(Make("for",
                 {"for item in iterable then\n    ...\nend"},
                 "for name in [\"Ana\", \"Luis\", \"Sol\"] then\n    print(name)\nend",
                 "Iterates item over iterable (a list, a range(...), etc.), running the block "
                 "once per element. Needs then before the body, like if."));

        add(Make("in",
                 {"for item in iterable then\n    ...\nend"},
                 "",
                 "Pairs with for to name what's being iterated over."));

        add(Make("func",
                 {"func name(params)\n    ...\nend", "func(params) => expr"},
                 "func greet(name)\n    print(\"Hello, \" + name)\nend\n\ngreet(\"Ava\")",
                 "Declares a named function, or -- used as an expression, without a name -- "
                 "an anonymous one. (params) => expr is the short single-expression form."));

        add(Make("class",
                 {"class Name\n    ...\nend", "class Name : Base\n    ...\nend"},
                 "class Animal\n    func speak()\n        print(\"...\")\n    end\nend\n\nclass Dog : Animal\n    "
                 "func speak()\n        print(\"Woof!\")\n    end\nend",
                 "Declares a class. : Base is optional and gives it a single superclass."));

        add(Make("base",
                 {"base(args)"},
                 "class Dog : Animal\n    func Dog(name)\n        base(name)\n    end\nend",
                 "Inside a subclass, calls the superclass's constructor or method with args."));

        add(Make("return",
                 {"return", "return value"},
                 "func double(n)\n    return n * 2\nend",
                 "Exits the current function immediately, optionally handing back value. With "
                 "no value, the function returns nil."));

        add(Make("break",
                 {"break"},
                 "for n in range(10) then\n    if n == 5 then\n        break\n    end\n    print(n)\nend",
                 "Exits the innermost while or for loop immediately."));

        add(Make("continue",
                 {"continue"},
                 "for n in range(5) then\n    if n == 2 then\n        continue\n    end\n    print(n)\nend",
                 "Skips straight to the next iteration of the innermost while or for loop."));

        add(Make("pass",
                 {"pass"},
                 "if user.is_admin then\n    pass\nend",
                 "Does nothing. Useful as a placeholder body while a block is still empty, "
                 "since AvaLang has no truly empty block."));

        add(Make("import",
                 {"import module", "import module.submodule", "import module as alias"},
                 "import utils.math as m\nprint(m.square(4))",
                 "Loads another AvaLang file by module path and makes its top-level names "
                 "available, optionally under alias."));

        add(Make("as",
                 {"import module as alias"},
                 "",
                 "Gives an imported module a local alias to refer to it by."));

        add(Make("local",
                 {"local name = value"},
                 "x = 10\nfunc change()\n    local x = 20\n    print(x)\nend",
                 "Declares name as a new local variable in the current scope, instead of "
                 "assigning to an existing outer/global one of the same name."));

        add(Make("raise",
                 {"raise value"},
                 "if amount < 0 then\n    raise \"amount can't be negative\"\nend",
                 "Raises value as an exception, unwinding until a matching catch handles it "
                 "(or the program stops if none does)."));

        add(Make("try",
                 {"try\n    ...\ncatch (e)\n    ...\nfinally\n    ...\nend"},
                 "try\n    risky_call()\ncatch (e)\n    print(\"Error: \" + str(e))\nend",
                 "Runs the block, routing any raised exception to a catch clause. At least "
                 "one catch or a finally is required."));

        add(Make("catch",
                 {"catch (e)\n    ...", "catch e\n    ...\n"},
                 "",
                 "Handles an exception raised inside the try block above it, binding it to e. "
                 "Parentheses around e are optional."));

        add(Make("finally",
                 {"try\n    ...\nfinally\n    ...\nend"},
                 "try\n    open_file()\nfinally\n    close_file()\nend",
                 "Runs after the try block, whether or not an exception was raised or caught."));

        add(Make("yield",
                 {"yield", "yield value"},
                 "func counter()\n    n = 0\n    while true\n        yield n\n        n = n + 1\n    end\nend",
                 "Pauses the current function and hands value back to its caller, turning it "
                 "into a coroutine/generator that can be resumed later."));

        add(Make("or", {"a or b"},
                 "if is_weekend or is_holiday then\n    print(\"no work today\")\nend",
                 "Logical OR: true if either side is true. Short-circuits -- b isn't "
                 "evaluated if a is already true."));

        add(Make("and", {"a and b"},
                 "if age >= 18 and has_id then\n    print(\"allowed\")\nend",
                 "Logical AND: true only if both sides are true. Short-circuits -- b isn't "
                 "evaluated if a is already false."));

        add(Make("not", {"not a"},
                 "if not is_valid then\n    print(\"invalid\")\nend",
                 "Logical negation: true if a is false, and vice versa."));

        return m;
    }();

    return table;
}

namespace {

namespace fs = std::filesystem;

bool LoadKeywordDocsFromCsv(const std::string& path, std::unordered_map<std::string, KeywordDoc>& out) {
    std::string text;
    if (!util::ReadFileToString(path, text)) return false;

    auto rows = util::ParseCsv(text);
    if (rows.empty()) return false;

    std::unordered_map<std::string, KeywordDoc> parsed;

    for (size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (row.size() == 1 && row[0].empty()) continue;
        if (row.size() < 4) continue;

        KeywordDoc entry;
        entry.name = row[0];
        if (entry.name.empty()) continue;

        for (const auto& variant : util::SplitOn(row[1], "|||")) {
            entry.syntax.push_back(util::UnescapeCell(variant));
        }
        entry.example = util::UnescapeCell(row[2]);
        entry.doc = util::UnescapeCell(row[3]);
        parsed[entry.name] = std::move(entry);
    }

    if (parsed.empty()) return false;
    out = std::move(parsed);
    return true;
}

}

const std::unordered_map<std::string, KeywordDoc>& KeywordDocs() {
    static std::unordered_map<std::string, KeywordDoc> table;
    static fs::file_time_type last_loaded_time{};
    static bool loaded_from_csv = false;
    static bool first_call = true;

    const std::string csv_path = util::ResolveDataDir() + "docs/keyword_docs.csv";
    std::error_code ec;
    fs::file_time_type current_time = fs::last_write_time(csv_path, ec);
    bool csv_exists = !ec;

    bool should_reload = first_call || (csv_exists && current_time != last_loaded_time);

    if (should_reload) {
        first_call = false;
        if (csv_exists && LoadKeywordDocsFromCsv(csv_path, table)) {
            loaded_from_csv = true;
            last_loaded_time = current_time;
        } else if (!loaded_from_csv) {

            table = DefaultKeywordDocs();
        }
    }

    return table;
}

}
