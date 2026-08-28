// check_docs_sync.cpp
//
// Chequea que runtime/avastudio/data/docs/keyword_docs.csv y
// builtin_signatures.csv (las tooltips/autocompletado del editor) no se
// hayan desincronizado de las fuentes de verdad reales:
//   - keywords:  literales entre comillas simples en las reglas del parser
//                de runtime/avalang/grammar/AvaLang.g4
//   - builtins:  llamadas RegisterNative("nombre", ...) en
//                runtime/avalang/src/builtins/builtin_init.cpp y
//                builtin_registry.cpp (NO RegisterBuiltinMethod: esas son
//                str_*/list_*/dict_*, se llaman como metodo -obj.upper()-
//                y no viven en este CSV)
//
// No parsea el compilador ni genera nada: es exactamente la misma tecnica
// de comparacion manual que ya se hizo para investigar este punto, solo
// que automatizada para poder correr en CI. tools/dump_docs.cpp, citado
// como precedente en una nota de progreso anterior, no existe en este
// repo -- ver PLAN_FASES_IMPLEMENTACION.md, seccion Fase 4.
//
// Uso:
//   check_docs_sync <path-a-la-raiz-del-repo> [--allowlist <archivo>]
//
// Exit code 0: todo sincronizado (o toda discrepancia esta en la
//              allowlist, es decir, es una decision de producto pendiente
//              y explicitamente reconocida, no un desliz).
// Exit code 1: hay una discrepancia SIN allowlistear -- pensada para
//              hacer fallar un paso de CI.
//
// La allowlist (una linea por nombre, '#' para comentarios) es a
// proposito un archivo de texto plano separado del codigo: que
// "keyword X todavia no se documenta" sea una decision visible y
// versionada, no algo que esta herramienta decida sola.

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string ReadFile(const std::string& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        *ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    *ok = true;
    return ss.str();
}

// Extrae los literales 'palabra' (solo [a-zA-Z_][a-zA-Z0-9_]*) de las
// reglas del parser en el .g4. Son las keywords reales del lenguaje tal
// como las ve ANTLR -- cualquier keyword nueva que se agregue al
// lenguaje tiene que aparecer aca, por construccion.
std::set<std::string> ExtractGrammarKeywords(const std::string& g4_text) {
    std::set<std::string> result;
    static const std::regex lit_re("'([a-zA-Z_][a-zA-Z0-9_]*)'");
    auto begin = std::sregex_iterator(g4_text.begin(), g4_text.end(), lit_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        result.insert((*it)[1].str());
    }
    return result;
}

// Extrae los nombres registrados via RegisterNative("nombre", ...) --
// deliberadamente NO RegisterBuiltinMethod (ver comentario de arriba).
std::set<std::string> ExtractRegisteredNatives(const std::string& cpp_text) {
    std::set<std::string> result;
    static const std::regex call_re(
        "\\bRegisterNative\\(\\s*\"([a-zA-Z_][a-zA-Z0-9_]*)\"");
    auto begin = std::sregex_iterator(cpp_text.begin(), cpp_text.end(), call_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        result.insert((*it)[1].str());
    }
    return result;
}

// Parser de CSV minimo: solo necesita la PRIMERA columna de cada fila de
// datos (el nombre), y los nombres de keyword/builtin son identificadores
// simples que nunca llevan comas ni comillas -- así que basta con leer
// hasta la primera coma de cada linea que no sea el header. Si esa
// asuncion alguna vez deja de valer (un "name" con coma) esta funcion
// hay que reemplazarla por un parser de CSV completo, no antes.
std::set<std::string> ExtractCsvNames(const std::string& csv_text) {
    std::set<std::string> result;
    std::istringstream stream(csv_text);
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        if (first) { first = false; continue; } // header
        if (line.empty()) continue;
        auto comma = line.find(',');
        std::string name = (comma == std::string::npos) ? line : line.substr(0, comma);
        if (!name.empty()) result.insert(name);
    }
    return result;
}

std::set<std::string> LoadAllowlist(const std::string& path) {
    std::set<std::string> result;
    bool ok = false;
    std::string text = ReadFile(path, &ok);
    if (!ok) return result; // allowlist ausente == vacia, no es error
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        // trim
        auto start = line.find_first_not_of(" \t\r\n");
        auto stop = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        result.insert(line.substr(start, stop - start + 1));
    }
    return result;
}

// Reporta nombres en `source` sin fila en `csv` (documentacion faltante)
// y nombres en `csv` sin respaldo en `source` (documentacion de algo que
// ya no existe / se renombro). Devuelve cuantas discrepancias NO estaban
// en la allowlist.
int ReportDiff(const std::string& label,
                const std::set<std::string>& source,
                const std::set<std::string>& csv,
                const std::set<std::string>& allowlist) {
    int unlisted = 0;

    std::vector<std::string> missing_in_csv;
    std::set_difference(source.begin(), source.end(), csv.begin(), csv.end(),
                         std::back_inserter(missing_in_csv));
    std::vector<std::string> stale_in_csv;
    std::set_difference(csv.begin(), csv.end(), source.begin(), source.end(),
                         std::back_inserter(stale_in_csv));

    if (missing_in_csv.empty() && stale_in_csv.empty()) {
        std::cout << "[OK] " << label << ": sincronizado (" << source.size()
                  << " nombres)\n";
        return 0;
    }

    for (const auto& name : missing_in_csv) {
        bool listed = allowlist.count(name) > 0;
        std::cout << (listed ? "[allowlisted] " : "[FALTA] ") << label
                  << ": '" << name
                  << "' existe en el codigo pero no tiene fila en el CSV\n";
        if (!listed) ++unlisted;
    }
    for (const auto& name : stale_in_csv) {
        bool listed = allowlist.count(name) > 0;
        std::cout << (listed ? "[allowlisted] " : "[SOBRA] ") << label
                  << ": '" << name
                  << "' tiene fila en el CSV pero no se encontro en el codigo\n";
        if (!listed) ++unlisted;
    }
    return unlisted;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0]
                  << " <raiz-del-repo> [--allowlist <archivo>]\n";
        return 2;
    }
    std::string repo_root = argv[1];
    std::string allowlist_path =
        repo_root + "/runtime/avalang/tools/docs_sync_allowlist.txt";
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--allowlist" && i + 1 < argc) {
            allowlist_path = argv[++i];
        }
    }

    bool ok = false;
    std::string g4 = ReadFile(repo_root + "/runtime/avalang/grammar/AvaLang.g4", &ok);
    if (!ok) { std::cerr << "No pude leer AvaLang.g4\n"; return 2; }

    std::string init_cpp =
        ReadFile(repo_root + "/runtime/avalang/src/builtins/builtin_init.cpp", &ok);
    if (!ok) { std::cerr << "No pude leer builtin_init.cpp\n"; return 2; }

    std::string registry_cpp = ReadFile(
        repo_root + "/runtime/avalang/src/builtins/builtin_registry.cpp", &ok);
    if (!ok) { std::cerr << "No pude leer builtin_registry.cpp\n"; return 2; }

    const std::string kw_docs_a = repo_root + "/runtime/avastudio/data/docs/keyword_docs.csv";
    const std::string kw_docs_b = repo_root + "/runtime/avastudio/data/keyword_docs.csv";
    const std::string bi_docs_a = repo_root + "/runtime/avastudio/data/docs/builtin_signatures.csv";
    const std::string bi_docs_b = repo_root + "/runtime/avastudio/data/builtin_signatures.csv";

    std::string kw_csv_a = ReadFile(kw_docs_a, &ok);
    if (!ok) { std::cerr << "No pude leer " << kw_docs_a << "\n"; return 2; }
    std::string kw_csv_b = ReadFile(kw_docs_b, &ok);
    if (!ok) { std::cerr << "No pude leer " << kw_docs_b << "\n"; return 2; }
    std::string bi_csv_a = ReadFile(bi_docs_a, &ok);
    if (!ok) { std::cerr << "No pude leer " << bi_docs_a << "\n"; return 2; }
    std::string bi_csv_b = ReadFile(bi_docs_b, &ok);
    if (!ok) { std::cerr << "No pude leer " << bi_docs_b << "\n"; return 2; }

    std::set<std::string> allowlist = LoadAllowlist(allowlist_path);

    int problems = 0;

    // 1) Las dos copias de cada CSV (data/docs/ para el editor,
    //    data/ raiz para el plugin ai_agent) deben ser identicas byte a
    //    byte -- hoy coinciden por disciplina manual, no por ningun
    //    mecanismo (ver PLAN_FASES_IMPLEMENTACION.md).
    if (kw_csv_a != kw_csv_b) {
        std::cout << "[DESYNC] data/docs/keyword_docs.csv y data/keyword_docs.csv difieren\n";
        ++problems;
    } else {
        std::cout << "[OK] las dos copias de keyword_docs.csv coinciden\n";
    }
    if (bi_csv_a != bi_csv_b) {
        std::cout << "[DESYNC] data/docs/builtin_signatures.csv y data/builtin_signatures.csv difieren\n";
        ++problems;
    } else {
        std::cout << "[OK] las dos copias de builtin_signatures.csv coinciden\n";
    }

    // 2) Keywords: gramatica vs CSV.
    std::set<std::string> grammar_keywords = ExtractGrammarKeywords(g4);
    std::set<std::string> csv_keywords = ExtractCsvNames(kw_csv_a);
    problems += ReportDiff("keyword_docs.csv", grammar_keywords, csv_keywords, allowlist);

    // 3) Builtins: RegisterNative(...) vs CSV.
    std::set<std::string> natives = ExtractRegisteredNatives(init_cpp);
    std::set<std::string> more_natives = ExtractRegisteredNatives(registry_cpp);
    natives.insert(more_natives.begin(), more_natives.end());
    std::set<std::string> csv_builtins = ExtractCsvNames(bi_csv_a);
    problems += ReportDiff("builtin_signatures.csv", natives, csv_builtins, allowlist);

    if (problems > 0) {
        std::cout << "\n" << problems
                  << " discrepancia(s) sin allowlistear. Agrega una fila al CSV "
                     "correspondiente, o si es una omision deliberada agregala a "
                  << allowlist_path << " con un comentario explicando por que.\n";
        return 1;
    }
    std::cout << "\nTodo sincronizado (discrepancias listadas arriba, si las "
                 "hay, estan todas en la allowlist).\n";
    return 0;
}
