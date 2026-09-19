#pragma once
// Fase 0 de PLAN_DEBUG_MODE_AVASTUDIO.md: contrato de error estructurado,
// compartido por todos los "runners" (ava_cli, avapack_stub, avapack_stub_ui,
// el .exe con-repo de avapack, avanative.exe). Header-only a proposito -- no
// agrega ningun target/lib nueva que wirear en cada CMakeLists.txt, alcanza
// con agregar runtime/common al include path (ver comentarios en los
// CMakeLists.txt que lo consumen).
//
// Cada runner ya imprime su propio mensaje "para humanos" (PrintFormattedError
// en avacli/src/main.cpp, fprintf(stderr, "error: ...") en avapack,
// MessageBoxA en los binarios WIN32_EXECUTABLE sin consola) -- eso NO
// cambia, y no deberia: es el formato que ya lee cualquiera usando la
// terminal. Esta funcion agrega, ADEMAS, una sola linea con un prefijo fijo
// y JSON compacto para que AvaStudio (Fase 2 del plan, build_panel.cpp) la
// pueda parsear sin ambiguedad -- sin depender de adivinar el formato del
// texto humano, que puede cambiar de wording sin previo aviso.
//
// Formato de la linea (siempre UNA sola linea por error, sin saltos de
// linea reales -- van escapados como \n dentro del JSON):
//
//   @@AVA_ERROR@@ {"kind":"compile","message":"...","file":"main.ava","line":12,"col":4}
//
// `kind` es uno de: "compile", "runtime", "native_crash", "launch_failure".
// `file`/`line`/`col`/`stack` son opcionales -- se omiten del JSON si no
// se conocen (no se manda "" / 0 relleno, para no confundir "columna 0"
// con "no hay columna").
#include <cstdio>
#include <string>

namespace ava {
namespace diag {

enum class ErrorKind {
    kCompile,
    kRuntime,
    kNativeCrash,
    kLaunchFailure,
};

inline const char* ErrorKindName(ErrorKind kind) {
    switch (kind) {
        case ErrorKind::kCompile: return "compile";
        case ErrorKind::kRuntime: return "runtime";
        case ErrorKind::kNativeCrash: return "native_crash";
        case ErrorKind::kLaunchFailure: return "launch_failure";
    }
    return "unknown";
}

// Escapa un string para que sea un valor JSON valido entre comillas dobles
// (comillas, backslashes, control chars). Deliberadamente minimo -- esto NO
// es un serializador JSON general, alcanza para mensajes de error de una
// linea con eventual snippet de codigo fuente/stack trace multilinea.
inline std::string JsonEscape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 8);
    for (unsigned char c : raw) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

struct ErrorInfo {
    ErrorKind kind;
    std::string message;
    std::string file;    // vacio si no aplica
    int line = 0;         // 0 = desconocido
    int col = 0;           // 0 = desconocido
    std::string stack;    // opcional, multilinea (se escapa igual que message)
};

// Imprime una linea machine-readable por stderr con el prefijo fijo
// "@@AVA_ERROR@@ " seguido de un objeto JSON de una sola linea. NO
// reemplaza el mensaje humano existente en cada call site -- se llama
// ADEMAS de (antes o despues de) el fprintf/MessageBoxA/PrintFormattedError
// que ya este ahi. fflush explicito: varios de estos runners corren con
// stderr sin buffer (ava_cli ya hace setvbuf _IONBF) pero otros no, y esta
// linea tiene que llegar YA al pipe que AvaStudio esta leyendo, no quedar
// bufferizada hasta que el proceso termine.
inline void EmitStructuredError(const ErrorInfo& info) {
    std::fprintf(stderr, "@@AVA_ERROR@@ {\"kind\":\"%s\",\"message\":\"%s\"",
                 ErrorKindName(info.kind), JsonEscape(info.message).c_str());
    if (!info.file.empty()) {
        std::fprintf(stderr, ",\"file\":\"%s\"", JsonEscape(info.file).c_str());
    }
    if (info.line > 0) std::fprintf(stderr, ",\"line\":%d", info.line);
    if (info.col > 0) std::fprintf(stderr, ",\"col\":%d", info.col);
    if (!info.stack.empty()) {
        std::fprintf(stderr, ",\"stack\":\"%s\"", JsonEscape(info.stack).c_str());
    }
    std::fprintf(stderr, "}\n");
    std::fflush(stderr);
}

// Overload comoda para el caso mas comun (sin file/line/stack).
inline void EmitStructuredError(ErrorKind kind, const std::string& message) {
    ErrorInfo info;
    info.kind = kind;
    info.message = message;
    EmitStructuredError(info);
}

}  // namespace diag
}  // namespace ava
