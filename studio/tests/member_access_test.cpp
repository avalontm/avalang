// Standalone test for Fases 2-6 de TODO_autocompletado_miembros.md:
// VariableTypeIndex (Fase 2) + ResolveMemberAccess (Fase 3) +
// ClassIndex::FlattenedMembers/FilterForAccess (Fases 1/4/5/6), tal como
// los usa PopulateMemberSuggestions en editor_panel.cpp.
//
// No wired into CMakeLists.txt on purpose -- mismo criterio que
// core/tests/avaui_text_roundtrip_test.cpp: chequeo rápido y sin
// dependencias del engine gráfico, no una suite permanente. Compilar y
// correr directo:
//
//   g++ -std=c++20 -I studio/src \
//       studio/src/languages/function_index.cpp \
//       studio/src/languages/class_index.cpp \
//       studio/src/languages/member_access_resolver.cpp \
//       studio/src/languages/builtin_signatures.cpp \
//       studio/src/util/csv.cpp studio/src/util/data_dir.cpp \
//       studio/tests/member_access_test.cpp \
//       -o member_access_test
//   ./member_access_test scripts/
//
// (el único argumento es el directorio que contiene dog.ava y
// visibilidad_modificadores.ava -- por defecto "scripts/", asumiendo que
// se corre desde la raíz del repo).

#include "languages/class_index.h"
#include "languages/member_access_resolver.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace studio;

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

std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool HasMember(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

// Resuelve `identifier.` como si el cursor estuviera al final de esa línea
// dentro de `text`, y devuelve los nombres finales sugeridos (ya filtrados
// por FlattenedMembers + FilterForAccess), o {} si no se pudo resolver.
std::vector<std::string> SuggestFor(const std::string& text, const std::string& dir,
                                     int cursor_line, const std::string& before_cursor) {
    ClassIndex class_index;
    class_index.Rebuild(text, dir);
    VariableTypeIndex var_types;
    var_types.Rebuild(text, class_index);

    MemberAccessContext ctx;
    if (!ResolveMemberAccess(text, cursor_line, before_cursor, class_index, var_types, ctx)) {
        return {};
    }

    std::vector<ClassMember> members = class_index.FlattenedMembers(ctx.class_name);
    members = ClassIndex::FilterForAccess(members, ctx.kind, ctx.viewer_class);

    std::vector<std::string> names;
    for (const auto& m : members) names.push_back(m.name);
    return names;
}

} // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "scripts/";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '/';

    // --- Fase 7, caso 1: scripts/dog.ava -----------------------------
    // "dog = dog()" seguido de "dog." debe sugerir "say" -- el caso de
    // referencia explícito del TODO. Nótese que la variable y la clase
    // comparten nombre ("dog"): esto es justamente lo que verifica que
    // ResolveMemberAccess prioriza la variable sobre el nombre de clase
    // (ver el comentario en member_access_resolver.cpp).
    {
        std::string dog_text = ReadFile(dir + "dog.ava") + "\ndog = dog()\ndog.\n";
        auto suggestions = SuggestFor(dog_text, dir, 6, "dog.");
        Check(!suggestions.empty(), "dog.ava: dog. resuelve algo (no cae al fallback)");
        Check(HasMember(suggestions, "say"), "dog.ava: dog. sugiere say");
    }

    // --- Fase 7, caso 2: scripts/visibilidad_modificadores.ava -------
    std::string vis_text = ReadFile(dir + "visibilidad_modificadores.ava");
    Check(!vis_text.empty(), "visibilidad_modificadores.ava se pudo leer");

    // perro.saludar() / perro.energia / perro.comer() públicos -> sí.
    // perro.vidaSecreta / perro.regenerar() privados -> no.
    {
        auto suggestions = SuggestFor(vis_text, dir, 0, "perro.");
        Check(HasMember(suggestions, "saludar"), "perro. sugiere saludar (público, propio de Dog)");
        Check(HasMember(suggestions, "energia"), "perro. sugiere energia (público, heredado de Animal)");
        Check(HasMember(suggestions, "comer"), "perro. sugiere comer (público, heredado de Animal)");
        Check(!HasMember(suggestions, "vidaSecreta"), "perro. NO sugiere vidaSecreta (privado)");
        Check(!HasMember(suggestions, "regenerar"), "perro. NO sugiere regenerar (privado)");
    }

    // Dog.totalAnimales / Dog.especie() estáticos (heredados) -> sí.
    // Dog.saludar (no static) -> no debería aparecer para acceso por
    // nombre de clase.
    {
        auto suggestions = SuggestFor(vis_text, dir, 0, "Dog.");
        Check(HasMember(suggestions, "totalAnimales"), "Dog. sugiere totalAnimales (static heredado)");
        Check(HasMember(suggestions, "especie"), "Dog. sugiere especie (static heredado)");
        Check(!HasMember(suggestions, "saludar"), "Dog. NO sugiere saludar (no static)");
    }

    // Contador.validarLimite es static private: NO se sugiere desde
    // top-level (línea del "print(Contador.crear(50))", fuera de toda
    // clase -- viewer_class == "").
    {
        auto suggestions = SuggestFor(vis_text, dir, 0, "Contador.");
        Check(HasMember(suggestions, "crear"), "Contador. sugiere crear (static público)");
        Check(!HasMember(suggestions, "validarLimite"),
              "Contador. NO sugiere validarLimite desde fuera (static private)");
    }

    // Pero SÍ se sugiere -- y sin duplicar la lógica -- cuando el cursor
    // está dentro de un método de la propia clase Contador (viewer_class
    // == "Contador"): se simula reconstruyendo el prefijo hasta un punto
    // dentro de `func crear`.
    {
        size_t crear_pos = vis_text.find("func crear");
        Check(crear_pos != std::string::npos, "visibilidad_modificadores.ava tiene 'func crear'");
        size_t if_pos = vis_text.find("if Contador.validarLimite(n) then", crear_pos);
        Check(if_pos != std::string::npos, "encontrado el 'if Contador.validarLimite(n) then'");

        std::string up_to_dot = vis_text.substr(0, if_pos) + "Contador.";
        int line_count = static_cast<int>(std::count(up_to_dot.begin(), up_to_dot.end(), '\n'));
        size_t last_nl = up_to_dot.find_last_of('\n');
        std::string before = up_to_dot.substr(last_nl + 1);

        auto suggestions = SuggestFor(vis_text, dir, line_count, before);
        Check(HasMember(suggestions, "validarLimite"),
              "Contador. SÍ sugiere validarLimite desde dentro de la propia clase");
    }

    // this. dentro de comer() ve el propio privado (regenerar); this.
    // dentro de saludar() (en Dog) NO ve el privado heredado de Animal
    // (vidaSecreta / regenerar).
    {
        size_t comer_pos = vis_text.find("func comer()");
        size_t this_energia = vis_text.find("this.energia", comer_pos);
        std::string up_to = vis_text.substr(0, this_energia) + "this.";
        int line_count = static_cast<int>(std::count(up_to.begin(), up_to.end(), '\n'));
        std::string before = up_to.substr(up_to.find_last_of('\n') + 1);
        auto suggestions = SuggestFor(vis_text, dir, line_count, before);
        Check(HasMember(suggestions, "regenerar"), "this. dentro de Animal.comer() ve su propio regenerar (privado)");
    }
    {
        size_t saludar_pos = vis_text.find("func saludar()");
        size_t print_pos = vis_text.find("print(", saludar_pos);
        std::string up_to = vis_text.substr(0, print_pos) + "this.";
        int line_count = static_cast<int>(std::count(up_to.begin(), up_to.end(), '\n'));
        std::string before = up_to.substr(up_to.find_last_of('\n') + 1);
        auto suggestions = SuggestFor(vis_text, dir, line_count, before);
        Check(!HasMember(suggestions, "vidaSecreta"),
              "this. dentro de Dog.saludar() NO ve vidaSecreta (privado heredado de Animal)");
        Check(!HasMember(suggestions, "regenerar"),
              "this. dentro de Dog.saludar() NO ve regenerar (privado heredado de Animal)");
        Check(HasMember(suggestions, "nombre"), "this. dentro de Dog.saludar() SÍ ve nombre (público heredado)");
    }

    // --- Fase 6, casos límite -----------------------------------------
    // Encadenado "a.b." -> fuera de alcance, debe fallar (fallback seguro).
    {
        auto suggestions = SuggestFor("perro = Dog()\nperro.saludar.\n", dir, 1, "perro.saludar.");
        Check(suggestions.empty(), "encadenado 'a.b.' no resuelve (Fase 6, fuera de alcance)");
    }
    // Identificador desconocido -> fallback seguro, nunca fuerza nada.
    {
        auto suggestions = SuggestFor("algoQueNoExiste.\n", dir, 0, "algoQueNoExiste.");
        Check(suggestions.empty(), "identificador desconocido no resuelve (fallback seguro)");
    }
    // Variable reasignada a algo no inferible más adelante -> ya no debe
    // resolver a la clase vieja.
    {
        std::string text = "class Foo\n func saludo()\n  print(\"hola\")\n end\nend\nx = Foo()\nx = 5\nx.\n";
        auto suggestions = SuggestFor(text, dir, 7, "x.");
        Check(suggestions.empty(), "variable reasignada a algo no-clase invalida el tipo anterior");
    }
    // "this." fuera de toda clase (top-level) -> no resoluble.
    {
        auto suggestions = SuggestFor("this.\n", dir, 0, "this.");
        Check(suggestions.empty(), "'this.' fuera de una clase no resuelve");
    }

    std::cout << (g_failures == 0 ? "\nTODOS LOS CHECKS PASARON\n"
                                  : "\n" + std::to_string(g_failures) + " CHECK(S) FALLARON\n");
    return g_failures == 0 ? 0 : 1;
}
