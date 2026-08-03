#include "tools.h"

#include "avalang_reference.h"
#include "context_builder.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

constexpr size_t kMaxReadFileChars = 20000;
constexpr size_t kMaxListedFiles = 500;

std::string ErrorJson(const std::string& message) {
    return json{{"error", message}}.dump();
}

// Resolves `requested_relative_path` against the project root and makes
// sure the result cannot escape it (no `../` traversal, no absolute
// path override) -- this is the only thing standing between "let the
// model read files" and "let the model read anything on disk", so it
// stays conservative: any resolution failure or root mismatch is a
// rejection, not a best-effort guess.
bool ResolveWithinRoot(const std::string& root, const std::string& requested_relative_path, fs::path* out_resolved) {
    if (root.empty() || requested_relative_path.empty()) return false;

    std::error_code ec;
    fs::path root_canonical = fs::weakly_canonical(fs::path(root), ec);
    if (ec) return false;

    fs::path candidate = root_canonical / fs::path(requested_relative_path);
    fs::path candidate_canonical = fs::weakly_canonical(candidate, ec);
    if (ec) return false;

    // mismatch() finds where the two paths' components diverge; if the
    // whole of root_canonical is consumed, candidate is at/under it.
    auto [root_end, _] = std::mismatch(root_canonical.begin(), root_canonical.end(), candidate_canonical.begin(),
                                        candidate_canonical.end());
    if (root_end != root_canonical.end()) return false;

    *out_resolved = candidate_canonical;
    return true;
}

std::string ToolReadFile(AvaStudioHost* host, const std::string& arguments_json) {
    std::string path_arg;
    try {
        json args = json::parse(arguments_json);
        path_arg = args.value("path", "");
    } catch (...) {
        return ErrorJson("argumentos invalidos: se esperaba {\"path\": \"...\"}");
    }
    if (path_arg.empty()) return ErrorJson("falta el parametro 'path'");

    const char* root_cstr = host->services.get_project_root(host);
    std::string root = (root_cstr && root_cstr[0] != '\0') ? root_cstr : "";
    if (root.empty()) return ErrorJson("no hay ningun proyecto abierto");

    fs::path resolved;
    if (!ResolveWithinRoot(root, path_arg, &resolved)) {
        return ErrorJson("ruta invalida o fuera de la carpeta del proyecto: " + path_arg);
    }

    std::error_code ec;
    if (!fs::exists(resolved, ec) || !fs::is_regular_file(resolved, ec)) {
        return ErrorJson("archivo no encontrado: " + path_arg);
    }

    std::ifstream file(resolved, std::ios::binary);
    if (!file) return ErrorJson("no se pudo abrir el archivo: " + path_arg);

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    bool truncated = content.size() > kMaxReadFileChars;
    if (truncated) content = content.substr(0, kMaxReadFileChars);

    json result;
    result["path"] = path_arg;
    result["content"] = content;
    result["truncated"] = truncated;
    return result.dump();
}

std::string ToolListProjectFiles(AvaStudioHost* host, const std::string& /*arguments_json*/) {
    const char* root_cstr = host->services.get_project_root(host);
    std::string root = (root_cstr && root_cstr[0] != '\0') ? root_cstr : "";
    if (root.empty()) return ErrorJson("no hay ningun proyecto abierto");

    std::vector<std::string> files = ListProjectFiles(root, kMaxListedFiles + 1);
    bool truncated = files.size() > kMaxListedFiles;
    if (truncated) files.resize(kMaxListedFiles);

    json result;
    result["files"] = files;
    result["truncated"] = truncated;
    return result.dump();
}

// Doesn't touch `host` at all -- avalang_reference.h's tables come from
// data/keyword_docs.csv and data/builtin_signatures.csv next to
// ava_studio.exe, not from the open project -- but it's still a plain
// JSON-in/JSON-out wrapper same as the other read-only tools, so it
// lives here rather than in its own file.
std::string ToolAvalangSyntaxLookup(const std::string& arguments_json) {
    std::string name_arg;
    try {
        json args = json::parse(arguments_json);
        name_arg = args.value("name", "");
    } catch (...) {
        return ErrorJson("argumentos invalidos: se esperaba {\"name\": \"...\"}");
    }
    if (name_arg.empty()) return ErrorJson("falta el parametro 'name'");

    return LookupAvalangSyntax(name_arg);
}

std::string ToolGetCompilerOutput(AvaStudioHost* host, const std::string& /*arguments_json*/) {
    const char* out_text = nullptr;
    bool had_error = false;
    if (!host->services.get_last_run_output(host, &out_text, &had_error)) {
        return json{{"ran", false}, {"message", "todavia no se corrio/compilo nada en esta sesion"}}.dump();
    }

    json result;
    result["ran"] = true;
    result["had_error"] = had_error;
    result["output"] = out_text ? out_text : "";
    return result.dump();
}

// Fase 5 -- write tools. Both just forward to the host's write
// services (AvaHostServices::apply_edit / ::run_project, see
// plugin_api.h) -- the actual approval gate and the actual run both
// live host-side, not here. This function only translates between the
// model's JSON arguments and those two C calls, same as the read-only
// tools above translate to get_project_root/get_last_run_output.
std::string ToolApplyEdit(AvaStudioHost* host, const std::string& arguments_json) {
    std::string path_arg;
    std::string new_content_arg;
    std::string description_arg;
    try {
        json args = json::parse(arguments_json);
        path_arg = args.value("path", "");
        new_content_arg = args.value("new_content", "");
        description_arg = args.value("description", "");
    } catch (...) {
        return ErrorJson("argumentos invalidos: se esperaba {\"path\": \"...\", \"new_content\": \"...\", \"description\": \"...\"}");
    }
    if (path_arg.empty()) return ErrorJson("falta el parametro 'path'");

    const char* out_error = nullptr;
    bool queued = host->services.apply_edit(host, path_arg.c_str(), new_content_arg.c_str(), description_arg.c_str(),
                                             &out_error);
    if (!queued) {
        return ErrorJson(out_error ? out_error : "no se pudo proponer el cambio");
    }

    // Deliberately does NOT say "applied" -- it wasn't. The model needs
    // to know its next reply to the person should say something like
    // "te deje el cambio propuesto para revisar", not "listo, lo
    // cambie".
    json result;
    result["queued"] = true;
    result["path"] = path_arg;
    result["message"] = "el cambio quedo propuesto para que el usuario lo revise (Aplicar/Rechazar) -- todavia no se aplico";
    return result.dump();
}

std::string ToolRunProject(AvaStudioHost* host, const std::string& /*arguments_json*/) {
    const char* out_output = nullptr;
    bool had_error = false;
    const char* out_error = nullptr;
    if (!host->services.run_project(host, &out_output, &had_error, &out_error)) {
        return ErrorJson(out_error ? out_error : "no se pudo ejecutar el proyecto");
    }

    json result;
    result["ran"] = true;
    result["had_error"] = had_error;
    result["output"] = out_output ? out_output : "";
    return result.dump();
}

// Fase 6 -- design_add_component/design_edit_component take properties
// as a flat "key=value;key2=value2" string (see plugin_api.h), while
// the model gives them to us as a normal JSON object -- this is the
// one place that converts between the two. Non-string values (numbers,
// booleans) are serialized via json::dump() rather than get<string>(),
// same as any AvaLang property value that isn't quoted text (e.g.
// `fontSize = 32`, not `fontSize = "32"`). Same limitation the C
// service documents: a value containing ';' or '=' isn't representable
// through this call.
std::string PropertiesJsonToKv(const json& properties) {
    std::string out;
    if (!properties.is_object()) return out;
    bool first = true;
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        const std::string value = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
        if (!first) out += ";";
        out += it.key() + "=" + value;
        first = false;
    }
    return out;
}

std::string ToolDesignAddComponent(AvaStudioHost* host, const std::string& arguments_json) {
    std::string parent_id;
    std::string type;
    std::string id;
    json properties = json::object();
    try {
        json args = json::parse(arguments_json);
        parent_id = args.value("parent_id", "");
        type = args.value("type", "");
        id = args.value("id", "");
        if (args.contains("properties") && args["properties"].is_object()) properties = args["properties"];
    } catch (...) {
        return ErrorJson("argumentos invalidos: se esperaba {\"type\": \"...\", \"parent_id\": \"...\", "
                          "\"id\": \"...\", \"properties\": {...}}");
    }
    if (type.empty()) return ErrorJson("falta el parametro 'type'");

    const std::string properties_kv = PropertiesJsonToKv(properties);
    const char* out_error = nullptr;
    const bool queued = host->services.design_add_component(host, parent_id.c_str(), type.c_str(), id.c_str(),
                                                              properties_kv.c_str(), &out_error);
    if (!queued) return ErrorJson(out_error ? out_error : "no se pudo proponer el componente nuevo");

    // Deliberately does NOT say "added" -- same reasoning as
    // ToolApplyEdit's result message above: it's queued for review,
    // not applied, and the model's next reply to the person has to
    // reflect that.
    json result;
    result["queued"] = true;
    result["type"] = type;
    result["parent_id"] = parent_id;
    result["message"] = "el componente nuevo quedo propuesto sobre el documento .avaui activo para que el "
                         "usuario lo revise (Aplicar/Rechazar) -- todavia no se agrego";
    return result.dump();
}

std::string ToolDesignEditComponent(AvaStudioHost* host, const std::string& arguments_json) {
    std::string node_id;
    std::string new_id;
    bool has_new_id = false;
    json properties = json::object();
    try {
        json args = json::parse(arguments_json);
        node_id = args.value("node_id", "");
        if (args.contains("new_id") && args["new_id"].is_string()) {
            new_id = args["new_id"].get<std::string>();
            has_new_id = true;
        }
        if (args.contains("properties") && args["properties"].is_object()) properties = args["properties"];
    } catch (...) {
        return ErrorJson("argumentos invalidos: se esperaba {\"node_id\": \"...\", \"properties\": {...}, "
                          "\"new_id\": \"...\"}");
    }
    if (node_id.empty()) return ErrorJson("falta el parametro 'node_id'");

    const std::string properties_kv = PropertiesJsonToKv(properties);
    const char* out_error = nullptr;
    const bool queued = host->services.design_edit_component(
        host, node_id.c_str(), properties_kv.c_str(), has_new_id ? new_id.c_str() : nullptr, &out_error);
    if (!queued) return ErrorJson(out_error ? out_error : "no se pudo proponer la edicion del componente");

    json result;
    result["queued"] = true;
    result["node_id"] = node_id;
    result["message"] = "la edicion quedo propuesta sobre el documento .avaui activo para que el usuario la "
                         "revise (Aplicar/Rechazar) -- todavia no se aplico";
    return result.dump();
}

} // namespace

std::vector<OpenRouterToolDef> BuildReadOnlyToolDefs() {
    std::vector<OpenRouterToolDef> tools;

    tools.push_back({"read_file", "Lee el contenido completo de un archivo del proyecto abierto, dada su ruta relativa a la raiz del proyecto. Uso de solo lectura -- nunca modifica nada.",
                      R"({
                          "type": "object",
                          "properties": {
                              "path": {
                                  "type": "string",
                                  "description": "Ruta relativa a la raiz del proyecto, ej: \"src/main.ava\""
                              }
                          },
                          "required": ["path"]
                      })"});

    tools.push_back({"list_project_files",
                      "Lista las rutas relativas de todos los archivos del proyecto abierto (excluye carpetas como .git, build, node_modules). Util para saber que archivos existen antes de pedir leer uno con read_file.",
                      R"({"type": "object", "properties": {}})"});

    tools.push_back({"get_compiler_output",
                      "Devuelve el resultado (texto y si hubo error) de la ultima vez que se compilo/corrio el proyecto en esta sesion de Ava Studio.",
                      R"({"type": "object", "properties": {}})"});

    tools.push_back({"avalang_syntax_lookup",
                      "Devuelve la sintaxis exacta (todas las variantes validas), un ejemplo concreto y la "
                      "explicacion de una palabra clave de AvaLang (if, for, func, class, try, ...) o de una "
                      "funcion built-in (len, range, print, ...). Usa esta herramienta antes de escribir o "
                      "corregir codigo AvaLang si no estas seguro de la sintaxis exacta -- AvaLang no es Python "
                      "ni JavaScript (usa 'end' para cerrar bloques, no indentacion; 'func'/'then' en vez de "
                      "'def'/':'), asi que no asumas la sintaxis de otro lenguaje.",
                      R"({
                          "type": "object",
                          "properties": {
                              "name": {
                                  "type": "string",
                                  "description": "La palabra clave o funcion built-in a consultar, ej: \"for\", \"try\", \"range\"."
                              }
                          },
                          "required": ["name"]
                      })"});

    return tools;
}

std::string ExecuteReadOnlyTool(AvaStudioHost* host, const std::string& tool_name,
                                 const std::string& arguments_json) {
    if (!host) return ErrorJson("host no disponible");

    if (tool_name == "read_file") return ToolReadFile(host, arguments_json);
    if (tool_name == "list_project_files") return ToolListProjectFiles(host, arguments_json);
    if (tool_name == "get_compiler_output") return ToolGetCompilerOutput(host, arguments_json);
    if (tool_name == "avalang_syntax_lookup") return ToolAvalangSyntaxLookup(arguments_json);

    return ErrorJson("herramienta desconocida: " + tool_name);
}

std::vector<OpenRouterToolDef> BuildWriteToolDefs() {
    std::vector<OpenRouterToolDef> tools;

    tools.push_back({"apply_edit",
                      "Propone reemplazar el contenido completo de un archivo del proyecto abierto con contenido "
                      "nuevo. NUNCA escribe el archivo directamente: el usuario ve un diff con botones Aplicar/"
                      "Rechazar y decide. Usa esta herramienta para cualquier cambio de codigo que el usuario haya "
                      "pedido -- nunca asumas que ya se aplico hasta que el usuario lo confirme en un mensaje "
                      "siguiente.",
                      R"({
                          "type": "object",
                          "properties": {
                              "path": {
                                  "type": "string",
                                  "description": "Ruta relativa a la raiz del proyecto, ej: \"src/main.ava\""
                              },
                              "new_content": {
                                  "type": "string",
                                  "description": "Contenido completo y final que deberia tener el archivo despues del cambio (no un diff ni un fragmento)."
                              },
                              "description": {
                                  "type": "string",
                                  "description": "Resumen breve y humano del cambio propuesto, ej: \"Arreglar el typo en el mensaje de error\"."
                              }
                          },
                          "required": ["path", "new_content"]
                      })"});

    tools.push_back({"run_project",
                      "Compila y corre el archivo actualmente activo en el editor (el mismo pipeline que la tecla "
                      "F5), y devuelve el resultado. A diferencia de apply_edit, esto se ejecuta directo sin pedir "
                      "aprobacion -- correr codigo que el usuario ya puede ver y correr el mismo no es una "
                      "capacidad nueva.",
                      R"({"type": "object", "properties": {}})"});

    return tools;
}

std::string ExecuteWriteTool(AvaStudioHost* host, const std::string& tool_name, const std::string& arguments_json) {
    if (!host) return ErrorJson("host no disponible");

    if (tool_name == "apply_edit") return ToolApplyEdit(host, arguments_json);
    if (tool_name == "run_project") return ToolRunProject(host, arguments_json);

    return ErrorJson("herramienta desconocida: " + tool_name);
}

std::vector<OpenRouterToolDef> BuildDesignToolDefs() {
    std::vector<OpenRouterToolDef> tools;

    tools.push_back(
        {"design_add_component",
         "Agrega un componente nuevo al documento de Designer (.avaui) actualmente activo. Requiere que el "
         "usuario tenga un archivo .avaui abierto como tab activo. El cambio queda propuesto para revision "
         "(Aplicar/Rechazar) igual que apply_edit -- no se agrega nada hasta que el usuario lo aprueba.",
         R"({
             "type": "object",
             "properties": {
                 "type": {
                     "type": "string",
                     "description": "Tipo de componente del catalogo, ej: \"button\", \"column\", \"text\"."
                 },
                 "parent_id": {
                     "type": "string",
                     "description": "Id del componente contenedor donde se agrega como ultimo hijo. Vacio o ausente = la raiz del documento."
                 },
                 "id": {
                     "type": "string",
                     "description": "Id que se le asigna al componente nuevo, ej: \"btnGuardar\". Puede quedar vacio."
                 },
                 "properties": {
                     "type": "object",
                     "description": "Propiedades iniciales del componente como pares clave-valor, ej: {\"text\": \"Guardar\", \"fontSize\": 16}."
                 }
             },
             "required": ["type"]
         })"});

    tools.push_back(
        {"design_edit_component",
         "Edita un componente existente del documento de Designer (.avaui) actualmente activo: cambia sus "
         "propiedades y/o su id. El cambio queda propuesto para revision (Aplicar/Rechazar) igual que "
         "apply_edit -- no se aplica nada hasta que el usuario lo aprueba.",
         R"({
             "type": "object",
             "properties": {
                 "node_id": {
                     "type": "string",
                     "description": "Id del componente a editar, ej: \"btnGuardar\"."
                 },
                 "properties": {
                     "type": "object",
                     "description": "Propiedades a cambiar/agregar como pares clave-valor -- una propiedad ya existente se reemplaza, una nueva se agrega."
                 },
                 "new_id": {
                     "type": "string",
                     "description": "Si se especifica, renombra el componente a este id nuevo."
                 }
             },
             "required": ["node_id"]
         })"});

    return tools;
}

std::string ExecuteDesignTool(AvaStudioHost* host, const std::string& tool_name, const std::string& arguments_json) {
    if (!host) return ErrorJson("host no disponible");

    if (tool_name == "design_add_component") return ToolDesignAddComponent(host, arguments_json);
    if (tool_name == "design_edit_component") return ToolDesignEditComponent(host, arguments_json);

    return ErrorJson("herramienta desconocida: " + tool_name);
}
