// avapack_barekernel_gen -- Fase B1 de plan_avapack_barekernel.md.
//
// Lee un módulo .avb ya serializado (ava_module_serialize, formato
// .avbc/proto_io.h -- ver nota de doble extensión .avb/.avbc en el plan
// §3) y escribe un embedded_avb.cpp que define avapack_barekernel::
// kAvbBytes/kAvbSize/kEntryName (embedded_avb.h), listo para compilarse
// junto con main_barekernel.cpp.
//
// A diferencia de avapack_gen (target desktop, runtime/avapack/src/
// generator/main.cpp), este tool:
//   - NO cifra nada (sin AES-256-CTR, sin clave ofuscada embebida) -- ver
//     tabla "Qué SÍ reusar de avapack desktop, y qué NO" en el plan.
//   - NO calcula HMAC de integridad (Fase 5 del target desktop).
//   - NO linkea avalang -- no compila .ava, solo empaqueta bytes .avb que
//     YA vienen compilados (el paso "compilar app.ava con una VM de host"
//     es responsabilidad de `ava_cli build --target barekernel`, Fase B2
//     del plan, no de este tool). Por eso, a diferencia de avapack_gen
//     (que desde Fase 6 sí linkea avalang para --obfuscate), este binario
//     es tan liviano como avapack_gen lo era antes de Fase 6: solo toca
//     el filesystem del host.
//   - Un solo entry embebido, sin árbol de imports (EmbeddedFile[]) --
//     ver embedded_avb.h sobre por qué imports no aplica todavía acá.
//
// Uso:
//   avapack_barekernel_gen --avb <ruta/al/entry.avb> --out <ruta/salida.cpp>
//                           [--entry-name <nombre informativo, default: basename del --avb>]

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string avb_path;
    std::string out_cpp;
    std::string entry_name;
};

void PrintUsage() {
    std::cerr << "uso: avapack_barekernel_gen --avb <entry.avb> --out <salida.cpp> "
                 "[--entry-name <nombre>]\n";
}

bool ParseArgs(int argc, char** argv, Options* opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: falta valor para " << flag << "\n";
                return "";
            }
            return argv[++i];
        };
        if (arg == "--avb") {
            opts->avb_path = next("--avb");
        } else if (arg == "--out") {
            opts->out_cpp = next("--out");
        } else if (arg == "--entry-name") {
            opts->entry_name = next("--entry-name");
        } else {
            std::cerr << "error: argumento desconocido: " << arg << "\n";
            return false;
        }
    }
    if (opts->avb_path.empty() || opts->out_cpp.empty()) {
        return false;
    }
    if (opts->entry_name.empty()) {
        // basename simple, sin depender de <filesystem> -- este tool es
        // deliberadamente chico y sin dependencias extra.
        std::size_t slash = opts->avb_path.find_last_of("/\\");
        opts->entry_name = (slash == std::string::npos)
            ? opts->avb_path
            : opts->avb_path.substr(slash + 1);
    }
    return true;
}

bool ReadWholeFile(const std::string& path, std::vector<unsigned char>* out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    std::streamsize size = in.tellg();
    if (size < 0) return false;
    in.seekg(0, std::ios::beg);
    out->resize(static_cast<std::size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(out->data()), size)) {
        return false;
    }
    return true;
}

// Escapa un string para que sea seguro emitirlo como literal C++ ("...").
std::string EscapeCString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

// Emite el array de bytes como literal C++, 20 bytes por línea -- mismo
// estilo legible que usa avapack_gen (generator/main.cpp) para
// EmbeddedFile::cipher, aunque acá no hay nada cifrado.
void WriteByteArray(std::ostream& out, const std::vector<unsigned char>& bytes) {
    out << "{\n";
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i % 20 == 0) out << "    ";
        char buf[8];
        std::snprintf(buf, sizeof(buf), "0x%02x,", bytes[i]);
        out << buf;
        if (i % 20 == 19) out << "\n";
    }
    out << "\n};\n";
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    if (!ParseArgs(argc, argv, &opts)) {
        PrintUsage();
        return 1;
    }

    std::vector<unsigned char> avb_bytes;
    if (!ReadWholeFile(opts.avb_path, &avb_bytes)) {
        std::cerr << "error: no se pudo leer --avb '" << opts.avb_path << "'\n";
        return 1;
    }
    if (avb_bytes.empty()) {
        std::cerr << "error: '" << opts.avb_path << "' esta vacio\n";
        return 1;
    }

    std::ofstream out(opts.out_cpp, std::ios::binary);
    if (!out) {
        std::cerr << "error: no se pudo abrir --out '" << opts.out_cpp << "' para escritura\n";
        return 1;
    }

    out << "// GENERADO por avapack_barekernel_gen -- no editar a mano.\n";
    out << "// Fuente: " << opts.avb_path << " (" << avb_bytes.size() << " bytes)\n";
    out << "// Ver embedded_avb.h para el contrato que este archivo implementa.\n\n";
    out << "#include \"embedded_avb.h\"\n\n";
    out << "namespace avapack_barekernel {\n\n";
    out << "const unsigned char kAvbBytes[] = ";
    WriteByteArray(out, avb_bytes);
    out << "\n";
    out << "const size_t kAvbSize = sizeof(kAvbBytes);\n\n";
    out << "const char* const kEntryName = \"" << EscapeCString(opts.entry_name) << "\";\n\n";
    out << "} // namespace avapack_barekernel\n";
    out.close();

    std::cerr << "OK: " << avb_bytes.size() << " bytes de '" << opts.avb_path
              << "' embebidos en '" << opts.out_cpp << "' (entry: " << opts.entry_name << ")\n";
    return 0;
}
