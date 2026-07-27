// Standalone round-trip test for core/src/ui/avaui_text.{h,cpp}
// Exercises: parse the section-3 example, write it back, re-parse the
// output, and diff the two parses structurally (not byte-for-byte,
// since WriteAvauiText normalizes quoting/spacing) -- plus a dedicated
// case for a call-form component (Navbar()) with no trailing blank
// line, to confirm the off-by-one fix from plan section 9.4 Entrega 1
// holds in the unified core/ parser.
//
// Not wired into CMakeLists.txt on purpose -- this is a quick,
// dependency-free check of the parser itself (see
// docs/architecture/08_DESIGNER_VIEW_PLAN.md section 9.5 point 1),
// not a permanent suite. Compile and run it directly:
//
//   g++ -std=c++20 -I core/src -I public/include \
//       core/src/ui/avaui_text.cpp core/src/ui/component.cpp \
//       core/src/ui/property.cpp core/src/ui/event.cpp \
//       core/src/vm/value.cpp core/tests/avaui_text_roundtrip_test.cpp \
//       -o avaui_text_roundtrip_test
//   ./avaui_text_roundtrip_test
#include "ui/avaui_text.h"
#include "ui/component.h"

#include <iostream>
#include <sstream>
#include <string>

using namespace ava;
using namespace ava::ui;

namespace {

int g_failures = 0;

void Check(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    } else {
        std::cout << "ok:   " << what << "\n";
    }
}

std::string PropStr(const Component& c, const std::string& key) {
    for (const auto& [k, v] : c.GetAllProperties()) {
        if (k == key) {
            return v.type == ValueType::String && v.obj
                       ? static_cast<StringObj*>(v.obj)->data
                       : "";
        }
    }
    return "<missing>";
}

std::string EventStr(const Component& c, const std::string& key) {
    for (const auto& [k, v] : c.GetAllEvents()) {
        if (k == key) {
            return v.type == ValueType::String && v.obj
                       ? static_cast<StringObj*>(v.obj)->data
                       : "";
        }
    }
    return "<missing>";
}

// Walks the tree produced by ParseAvauiText's `page` root and checks it
// against what the section-3 example (plan doc, section 3) should
// yield: page -> column[fill,padding,gap] -> [Navbar(), text, button].
void CheckSection3Shape(const ParsedAvaui& parsed, const std::string& label) {
    Check(parsed.root->GetType() == "page", label + ": root type is page");
    Check(parsed.state.size() == 1 && parsed.state[0].first == "counter" &&
              parsed.state[0].second == "0",
          label + ": state has counter = 0");
    Check(parsed.imports.size() == 1 && parsed.imports[0] == "components/navbar",
          label + ": imports has components/navbar");
    Check(parsed.root->GetChildren().size() == 1, label + ": page has 1 top-level view child (column)");
    if (parsed.root->GetChildren().empty()) return;

    const Component& column = *parsed.root->GetChildren()[0];
    Check(column.GetType() == "column", label + ": top child type is column");
    Check(PropStr(column, "fill") == "true", label + ": column.fill == true");
    Check(PropStr(column, "padding") == "20", label + ": column.padding == 20");
    Check(PropStr(column, "gap") == "16", label + ": column.gap == 16");
    Check(column.GetChildren().size() == 3, label + ": column has 3 children (Navbar/text/button)");
    if (column.GetChildren().size() != 3) return;

    const Component& navbar = *column.GetChildren()[0];
    Check(navbar.GetType() == "Navbar", label + ": child 0 is Navbar() call-form node");
    Check(navbar.GetAllProperties().empty() && navbar.GetChildren().empty(),
          label + ": Navbar() has no properties/children (unresolved import, per plan 9.2 pt.1)");

    const Component& text = *column.GetChildren()[1];
    Check(text.GetType() == "text", label + ": child 1 is text");
    Check(PropStr(text, "fontSize") == "32", label + ": text.fontSize == 32");
    // Expression props round-trip lossily as a quoted literal (documented
    // in avaui_text.h) -- just confirm it survives as *some* string.
    Check(PropStr(text, "value") != "<missing>", label + ": text.value present");

    const Component& button = *column.GetChildren()[2];
    Check(button.GetType() == "button", label + ": child 2 is button");
    Check(PropStr(button, "text") == "Guardar", label + ": button.text == Guardar");
    Check(EventStr(button, "click") == "btnGuardar_Click", label + ": button.click == btnGuardar_Click (event, not prop)");
}

const char* kSection3Example =
    "import \"components/navbar\"\n"
    "\n"
    "properties\n"
    "    title = \"Mi App\"\n"
    "end\n"
    "\n"
    "state\n"
    "    counter = 0\n"
    "end\n"
    "\n"
    "view\n"
    "    column\n"
    "        fill = \"true\"\n"
    "        padding = 20\n"
    "        gap = 16\n"
    "\n"
    "        Navbar()\n"
    "\n"
    "        text\n"
    "            value = \"Counter: \" + counter\n"
    "            fontSize = 32\n"
    "        end\n"
    "\n"
    "        button\n"
    "            text = \"Guardar\"\n"
    "            click = btnGuardar_Click\n"
    "        end\n"
    "    end\n"
    "end\n"
    "\n"
    "methods\n"
    "    func btnGuardar_Click()\n"
    "        -- handler\n"
    "    end\n"
    "end\n";

// Regression case for the off-by-one bug fixed in plan 9.4 Entrega 1:
// a call-form component (Navbar()) directly followed by a sibling with
// NO blank line in between. Before the fix this ate the `text` sibling
// on a second parse pass.
const char* kNavbarNoBlankLine =
    "view\n"
    "    column\n"
    "        Navbar()\n"
    "        text\n"
    "            value = \"hi\"\n"
    "        end\n"
    "    end\n"
    "end\n";

// Regression case for extends/route support (PLAN_CENTRALIZACION.md
// Fase B/C, avalang-dotnet repo): a page that extends a layout and
// declares more than one route, mirroring avalang-dotnet's real
// dashboard.avaui / productos.avaui examples.
const char* kExtendsRoutesExample =
    "extends \"admin\"\n"
    "\n"
    "route \"/productos\"\n"
    "route \"/productos/{id}\"\n"
    "route \"/productos/{id}/edit\"\n"
    "route \"/productos/{slug:slug}\"\n"
    "route \"/productos/{page?}\"\n"
    "\n"
    "state\n"
    "    selectedId = \"\"\n"
    "end\n"
    "\n"
    "view\n"
    "    column\n"
    "        text\n"
    "            value = \"Productos\"\n"
    "        end\n"
    "    end\n"
    "end\n";

} // namespace

int main() {
    std::cout << "=== 0. extends/route parsing ===\n";
    ParsedAvaui er = ParseAvauiText(kExtendsRoutesExample);
    Check(er.extends == "admin", "extends == admin");
    Check(er.routes.size() == 5, "5 route declarations");
    if (er.routes.size() == 5) {
        Check(er.routes[0].route_template == "/productos", "route[0] template");
        Check(er.routes[0].parameters.empty(), "route[0] has no params");
        Check(er.routes[1].route_template == "/productos/{id}", "route[1] template");
        Check(er.routes[1].parameters.size() == 1 && er.routes[1].parameters[0].name == "id" &&
                  er.routes[1].parameters[0].kind == RouteParameterKind::Required &&
                  er.routes[1].parameters[0].constraint.empty(),
              "route[1] param: id, required, no constraint");
        Check(er.routes[3].parameters.size() == 1 && er.routes[3].parameters[0].constraint == "slug",
              "route[3] param: slug constraint");
        Check(er.routes[4].parameters.size() == 1 &&
                  er.routes[4].parameters[0].kind == RouteParameterKind::Optional,
              "route[4] param: optional (page?)");
    }

    std::string er_written = WriteAvauiText(*er.root, er.state, er.imports, er.methods_text, er.extends, er.routes);
    ParsedAvaui er_reparsed = ParseAvauiText(er_written);
    Check(er_reparsed.extends == "admin", "round-trip: extends survives");
    Check(er_reparsed.routes.size() == 5, "round-trip: all 5 routes survive");

    std::string er_state_json = StateToJson(er.state);
    std::string er_routes_json = RoutesToJson(er.routes);
    auto er_routes_back = RoutesFromJson(er_routes_json);
    Check(er_routes_back.size() == 5, "RoutesToJson/RoutesFromJson round-trip: 5 routes");
    if (er_routes_back.size() == 5) {
        Check(er_routes_back[3].parameters[0].constraint == "slug", "RoutesFromJson: constraint survives JSON round-trip");
        Check(er_routes_back[4].parameters[0].kind == RouteParameterKind::Optional,
              "RoutesFromJson: optional flag survives JSON round-trip");
    }
    (void)er_state_json;


    std::cout << "=== 1. Parse the section 3 example (plan doc) ===\n";
    ParsedAvaui first = ParseAvauiText(kSection3Example);
    CheckSection3Shape(first, "first parse");
    Check(first.methods_text.find("btnGuardar_Click") != std::string::npos,
          "first parse: methods_text keeps btnGuardar_Click");

    std::cout << "\n=== 2. Write it back out (WriteAvauiText) ===\n";
    std::string written = WriteAvauiText(*first.root, first.state, first.imports, first.methods_text);
    Check(!written.empty(), "WriteAvauiText produced non-empty text");
    std::cout << "--- written .avaui ---\n" << written << "----------------------\n";

    std::cout << "\n=== 3. Re-parse the written text -- F7 round-trip (code -> design -> code) ===\n";
    ParsedAvaui second = ParseAvauiText(written);
    CheckSection3Shape(second, "round-trip parse");
    Check(second.methods_text.find("btnGuardar_Click") != std::string::npos,
          "round-trip parse: methods_text keeps btnGuardar_Click");

    std::cout << "\n=== 4. Second round-trip (write -> parse again) is stable (idempotent) ===\n";
    std::string written2 = WriteAvauiText(*second.root, second.state, second.imports, second.methods_text);
    Check(written == written2, "writer output is stable across a second round-trip (no drift)");

    std::cout << "\n=== 5. Navbar() call-form with NO trailing blank line (9.4 off-by-one regression) ===\n";
    ParsedAvaui navbar_case = ParseAvauiText(kNavbarNoBlankLine);
    Check(navbar_case.root->GetChildren().size() == 1, "navbar case: 1 top-level child (column)");
    if (!navbar_case.root->GetChildren().empty()) {
        const Component& column = *navbar_case.root->GetChildren()[0];
        Check(column.GetChildren().size() == 2, "navbar case: column has both children (Navbar + text), sibling not eaten");
        if (column.GetChildren().size() == 2) {
            Check(column.GetChildren()[0]->GetType() == "Navbar", "navbar case: child 0 is Navbar()");
            Check(column.GetChildren()[1]->GetType() == "text", "navbar case: child 1 is text (would be lost pre-fix)");
            Check(PropStr(*column.GetChildren()[1], "value") == "hi", "navbar case: text.value == hi survived");
        }
    }
    // Confirm this case also survives a full write/re-parse round-trip.
    std::string navbar_written = WriteAvauiText(*navbar_case.root, navbar_case.state,
                                                 navbar_case.imports, navbar_case.methods_text);
    ParsedAvaui navbar_reparsed = ParseAvauiText(navbar_written);
    if (!navbar_reparsed.root->GetChildren().empty()) {
        Check(navbar_reparsed.root->GetChildren()[0]->GetChildren().size() == 2,
              "navbar case: still 2 children after a full write/re-parse round-trip");
    }

    std::cout << "\n=== Summary: " << (g_failures == 0 ? "ALL CHECKS PASSED" : "FAILURES PRESENT") << " ===\n";
    return g_failures == 0 ? 0 : 1;
}
