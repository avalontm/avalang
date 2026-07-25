#include "languages/function_index.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "languages/builtin_signatures.h"

namespace studio {

namespace {

bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Requiere que IsIdentStart(text[i]) ya haya sido verificado por el caller.
std::string ReadIdent(const std::string& text, size_t& i) {
    size_t start = i;
    while (i < text.size() && IsIdentChar(text[i])) ++i;
    return text.substr(start, i - start);
}

void SkipInlineWhitespace(const std::string& text, size_t& i) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
}

// Divide el texto crudo entre el '(' de una función y su ')' de cierre en
// parámetros individuales, respetando (), [], {} anidados y strings, para
// no partir en la coma interna de algo como `b=(1, 2)` o `c="a,b"`.
std::vector<std::string> SplitParams(const std::string& raw) {
    std::vector<std::string> params;
    int depth = 0;
    char in_string = '\0';
    std::string current;

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (in_string) {
            current += c;
            if (c == '\\' && i + 1 < raw.size()) { current += raw[++i]; continue; }
            if (c == in_string) in_string = '\0';
            continue;
        }
        if (c == '\'' || c == '"') { in_string = c; current += c; continue; }
        if (c == '(' || c == '[' || c == '{') { ++depth; current += c; continue; }
        if (c == ')' || c == ']' || c == '}') { --depth; current += c; continue; }
        if (c == ',' && depth == 0) { params.push_back(current); current.clear(); continue; }
        current += c;
    }
    if (!current.empty() || !params.empty()) params.push_back(current);

    std::vector<std::string> trimmed;
    for (auto& p : params) {
        size_t b = p.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue; // ignora "func f()" -> [""] y comas colgantes
        size_t e = p.find_last_not_of(" \t\r\n");
        trimmed.push_back(p.substr(b, e - b + 1));
    }
    return trimmed;
}

std::string BuildDisplay(const std::string& name, const std::vector<std::string>& params) {
    std::string display = name + "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i) display += ", ";
        display += params[i];
    }
    display += ")";
    return display;
}

std::string TrimTrailing(const std::string& s) {
    size_t e = s.find_last_not_of(" \t\r");
    return e == std::string::npos ? "" : s.substr(0, e + 1);
}

// Parsea una línea "## @param nombre: descripción" (ya sin el "## " y
// trimeada). Acepta ':' o '-' como separador opcional, o ninguno
// ("@param nombre descripción"). Devuelve false si no hay un identificador
// válido justo después de "@param".
bool ParseParamLine(const std::string& line, std::string& name, std::string& desc) {
    size_t i = 6; // strlen("@param")
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    size_t start = i;
    while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) ++i;
    if (i == start) return false;
    name = line.substr(start, i - start);
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i < line.size() && (line[i] == ':' || line[i] == '-')) {
        ++i;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    }
    desc = line.substr(i);
    return true;
}

// Reparte un bloque "##" ya juntado en pending_doc entre sig.doc (resumen
// general) y sig.param_docs (una entrada por cada línea "@param nombre:
// ..."), matcheando `nombre` contra ParamBaseName() de cada parámetro real
// -- así "## @param name: ..." documenta el param aunque en la firma
// tenga un default o sea *rest.
void ApplyDocBlock(FunctionSignature& sig, const std::vector<std::string>& pending_doc) {
    std::vector<std::string> summary_lines;
    for (const auto& raw_line : pending_doc) {
        std::string line = TrimTrailing(raw_line);
        std::string pname, pdesc;
        if (line.compare(0, 6, "@param") == 0 && ParseParamLine(line, pname, pdesc)) {
            sig.param_docs[pname] = pdesc;
            continue;
        }
        summary_lines.push_back(line);
    }
    std::string doc;
    for (const auto& line : summary_lines) {
        if (line.empty()) continue;
        if (!doc.empty()) doc += " ";
        doc += line;
    }
    sig.doc = doc;
}

} // namespace

// nombre "limpio" de un parámetro para matchear contra @param: quita el
// '*' de var-args y todo lo que venga después de un '=' (default value),
// así "@param items" matchea tanto "items" como "*items" o "items=[]".
std::string ParamBaseName(const std::string& raw_param) {
    std::string p = raw_param;
    size_t b = p.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    p = p.substr(b);
    if (!p.empty() && p[0] == '*') p = p.substr(1);
    size_t eq = p.find('=');
    if (eq != std::string::npos) p = p.substr(0, eq);
    size_t e = p.find_last_not_of(" \t");
    return e == std::string::npos ? "" : p.substr(0, e + 1);
}

void FunctionIndex::ScanText(const std::string& text, const std::string& source_file) {
    size_t i = 0;

    // "##" doc-comment convention: one or more consecutive lines starting
    // with "##" immediately above a `func` become that function's summary,
    // shown in the parameter hint tooltip the same way BuiltinSignatures()
    // docs already are (see DrawParameterHint in editor_panel.cpp). Plain
    // "#" comments are never picked up as doc -- otherwise any unrelated
    // comment sitting above a function would silently become its "doc",
    // which is worse than showing nothing. pending_doc is cleared by
    // anything (code, a blank "#" comment, a string) other than more "##"
    // lines or blank/whitespace, so the block must be truly immediately
    // above the `func` it documents.
    std::vector<std::string> pending_doc;

    while (i < text.size()) {
        char c = text[i];

        // Saltar comentarios (# hasta fin de línea) y strings para no
        // confundir la palabra "func" si aparece dentro de ellos.
        if (c == '#') {
            size_t start = i;
            while (i < text.size() && text[i] != '\n') ++i;
            std::string comment = text.substr(start, i - start);
            if (comment.size() >= 2 && comment[1] == '#') {
                size_t b = comment.find_first_not_of(" \t", 2);
                pending_doc.push_back(b == std::string::npos ? "" : comment.substr(b));
            } else {
                pending_doc.clear();
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            pending_doc.clear();
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(text, i);
            if (word != "func") { pending_doc.clear(); continue; }

            size_t save = i;
            SkipInlineWhitespace(text, i);
            if (i >= text.size() || !IsIdentStart(text[i])) { i = save; pending_doc.clear(); continue; }

            std::string name = ReadIdent(text, i);
            SkipInlineWhitespace(text, i);
            if (i >= text.size() || text[i] != '(') { i = save; pending_doc.clear(); continue; }

            size_t open = i;
            int depth = 0;
            size_t j = open;
            for (; j < text.size(); ++j) {
                if (text[j] == '(') ++depth;
                else if (text[j] == ')') { --depth; if (depth == 0) break; }
            }
            if (j >= text.size()) { i = save; pending_doc.clear(); continue; } // paréntesis sin cerrar, abandonar

            FunctionSignature sig;
            sig.name = name;
            sig.params = SplitParams(text.substr(open + 1, j - open - 1));
            sig.source_file = source_file;
            for (const auto& p : sig.params) {
                if (!p.empty() && p[0] == '*') { sig.has_var_args = true; continue; }
                if (p.find('=') == std::string::npos) sig.min_args++;
            }
            sig.display = BuildDisplay(name, sig.params);
            if (!pending_doc.empty()) ApplyDocBlock(sig, pending_doc);
            pending_doc.clear();

            if (signatures_.find(name) == signatures_.end()) {
                signatures_[name] = std::move(sig);
            }
            i = j + 1;
            continue;
        }

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') pending_doc.clear();
        ++i;
    }
}

void FunctionIndex::ScanImports(const std::string& text, const std::string& current_file_dir,
                                 std::unordered_set<std::string>& visited) {
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];

        if (c == '#') { while (i < text.size() && text[i] != '\n') ++i; continue; }
        if (c == '\'' || c == '"') {
            char quote = c;
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) i += 2; else ++i;
            }
            if (i < text.size()) ++i;
            continue;
        }

        if (IsIdentStart(c)) {
            std::string word = ReadIdent(text, i);
            if (word == "import") {
                std::vector<std::string> module_path;
                for (;;) {
                    SkipInlineWhitespace(text, i);
                    if (i >= text.size() || !IsIdentStart(text[i])) break;
                    module_path.push_back(ReadIdent(text, i));
                    if (i < text.size() && text[i] == '.') { ++i; continue; }
                    break;
                }
                if (!module_path.empty()) {
                    std::string path = ResolveImportPath(module_path, current_file_dir);
                    if (!path.empty() && visited.insert(path).second) {
                        std::ifstream file(path, std::ios::binary);
                        if (file) {
                            std::ostringstream ss;
                            ss << file.rdbuf();
                            // Solo un nivel: escaneamos símbolos del módulo
                            // importado, no seguimos SUS imports.
                            ScanText(ss.str(), path);
                        }
                    }
                }
            }
            continue;
        }

        ++i;
    }
}

std::string FunctionIndex::ResolveImportPath(const std::vector<std::string>& module_path,
                                              const std::string& current_file_dir) {
    if (module_path.empty()) return "";

    std::string rel;
    for (size_t k = 0; k < module_path.size(); ++k) {
        if (k) rel += "/";
        rel += module_path[k];
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path base = current_file_dir.empty() ? fs::current_path(ec) : fs::path(current_file_dir);

    fs::path candidate = base / (rel + ".ava");
    if (fs::exists(candidate, ec)) return candidate.string();

    candidate = base / rel / "index.ava";
    if (fs::exists(candidate, ec)) return candidate.string();

    return ""; // no resuelto -- se ignora en silencio, best-effort
}

void FunctionIndex::Rebuild(const std::string& text, const std::string& current_file_dir) {
    signatures_.clear();
    ScanText(text, "");   // definiciones locales -- siempre ganan (ver comentario en el .h)
    std::unordered_set<std::string> visited;
    ScanImports(text, current_file_dir, visited);

    // Builtins van al final y solo rellenan huecos: si el script ya
    // declaró (o importó) algo con ese nombre, esa entrada se queda tal
    // cual -- ver el comentario sobre "override" en builtin_signatures.h.
    for (const auto& [name, sig] : BuiltinSignatures()) {
        signatures_.emplace(name, sig);
    }
}

} // namespace studio
