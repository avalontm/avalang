// Fase 7 -- verificacion end-to-end real de `import system` en Linux,
// via el C API publico (avalang.h) unicamente -- el mismo camino que
// usaria un binding externo real (avahost, C#, Python...). No depende
// del frontend ANTLR (que no compila en este entorno, ver
// AVALANG_IMPORT_SYSTEM_PLAN.md Fase 7): en vez de compilar un .ava,
// ejercita el modulo nativo directamente con ava_import/ava_dict_get/
// ava_call, exactamente lo que el bytecode generado por `import system`
// + `system.Console.WriteLine(...)` terminaria llamando de todos modos.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include "avalang.h"

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s (linea %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        std::fprintf(stderr, "ok:   %s\n", msg); \
    } \
} while (0)

static ava_value_t GetMember(AvaVM* vm, ava_value_t dict, const char* key) {
    return ava_dict_get(vm, dict, key);
}

int main() {
    AvaVM* vm = ava_vm_create();
    CHECK(vm != nullptr, "ava_vm_create");

    char* err = nullptr;

    // --- sanity check: un builtin viejo (print), NADA de system_module,
    // para descartar que el problema sea generico del C API / ava_call
    // en este build, antes de sospechar de system_module.cpp.
    {
        ava_value_t print_fn = ava_get_global(vm, "print");
        std::fprintf(stderr, "      print_fn.type = %d\n", (int)print_fn.type);
        ava_value_t pargs[1] = { ava_string_create(vm, "sanity check print", strlen("sanity check print")) };
        ava_value_t pout; char* perr = nullptr;
        ava_call(vm, print_fn, pargs, 1, &pout, &perr);
        CHECK(perr == nullptr, "print('sanity check print') no tira error");
    }

    // --- import system (forma plana, Fase 1/2/3/4/5/5.5) ---
    // OJO: `import system` (un solo segmento) hace flatten -- vuelca
    // Console/DateTime/Environment/IO/Diagnostics directo al scope
    // global (equivalente a "from system import *"), NO crea un global
    // llamado `system` -- confirmado releyendo PlaceModuleInScope
    // (vm_import.cpp) y samples/test/system_console_demo.ava, que usa
    // `Console.WriteLine(...)` a secas, no `system.Console...`. La
    // forma `system.X` solo existe despues de un import CON PUNTO
    // (Fase 6, `import system.io`), que crea (o reemplaza) el global
    // `system` apuntando directo al Dict de esa area. Primer intento de
    // este test asumia mal la forma plana y crasheaba con un dict nulo
    // -- corregido acá, no es un bug del propio system_module.
    ava_value_t imported = ava_import(vm, "system", nullptr, &err);
    CHECK(err == nullptr, "import system no genera error");
    (void)imported;

    ava_value_t console_direct = ava_get_global(vm, "Console");
    CHECK(console_direct.type == AVA_DICT, "Console quedo en el scope global tras 'import system' (flatten, no system.Console)");

    // --- Console ---
    ava_value_t console = ava_get_global(vm, "Console");
    CHECK(console.type == AVA_DICT, "system.Console existe");
    ava_value_t write_line = GetMember(vm, console, "WriteLine");
    CHECK(write_line.type == AVA_FUNCTION || write_line.type == AVA_NATIVE, "system.Console.WriteLine es invocable");
    {
        ava_value_t args[1];
        args[0] = ava_string_create(vm, "hola desde Fase 7", strlen("hola desde Fase 7"));
        ava_value_t out; char* call_err = nullptr;
        ava_call(vm, write_line, args, 1, &out, &call_err);
        CHECK(call_err == nullptr, "system.Console.WriteLine(...) no tira error");
    }

    // --- Environment.GetCommandLineArgs (el gap que se acaba de arreglar) ---
    ava_value_t environment = ava_get_global(vm, "Environment");
    CHECK(environment.type == AVA_DICT, "system.Environment existe");
    ava_value_t get_args = GetMember(vm, environment, "GetCommandLineArgs");
    {
        ava_value_t out; char* call_err = nullptr;
        ava_call(vm, get_args, nullptr, 0, &out, &call_err);
        CHECK(call_err == nullptr, "system.Environment.GetCommandLineArgs() no tira error");
        CHECK(out.type == AVA_LIST, "GetCommandLineArgs() devuelve una List");
        size_t n = ava_list_length(vm, out);
        std::fprintf(stderr, "      GetCommandLineArgs() devolvio %zu elementos\n", n);
        CHECK(n > 0, "GetCommandLineArgs() NO esta vacia (regresion del gap de Fase 7 ya arreglado)");
        if (n > 0) {
            ava_value_t first = ava_list_get(vm, out, 0);
            size_t slen = 0;
            const char* s = ava_string_data(vm, first, &slen);
            std::fprintf(stderr, "      args[0] = %.*s\n", (int)slen, s);
        }
    }

    // --- DateTime ---
    ava_value_t datetime = ava_get_global(vm, "DateTime");
    CHECK(datetime.type == AVA_DICT, "system.DateTime existe");
    ava_value_t now_fn = GetMember(vm, datetime, "Now");
    ava_value_t now_val;
    {
        char* call_err = nullptr;
        ava_call(vm, now_fn, nullptr, 0, &now_val, &call_err);
        CHECK(call_err == nullptr, "system.DateTime.Now() no tira error");
        CHECK(now_val.type == AVA_DICT, "DateTime.Now() devuelve un Dict");
        ava_value_t year = ava_dict_get(vm, now_val, "Year");
        CHECK(year.type == AVA_NUMBER && year.as.n >= 2026, "DateTime.Now().Year es plausible (>= 2026)");
    }
    ava_value_t to_string_fn = GetMember(vm, datetime, "ToString");
    {
        ava_value_t args[1] = { now_val };
        ava_value_t out; char* call_err = nullptr;
        ava_call(vm, to_string_fn, args, 1, &out, &call_err);
        CHECK(call_err == nullptr, "system.DateTime.ToString(now) no tira error");
        CHECK(out.type == AVA_STRING, "DateTime.ToString(now) devuelve string");
        if (out.type == AVA_STRING) {
            size_t slen = 0;
            const char* s = ava_string_data(vm, out, &slen);
            std::fprintf(stderr, "      ToString(now) = %.*s\n", (int)slen, s);
        }
    }

    // --- IO.File / IO.Directory ---
    ava_value_t io = ava_get_global(vm, "IO");
    CHECK(io.type == AVA_DICT, "system.IO existe");
    ava_value_t file_ns = GetMember(vm, io, "File");
    CHECK(file_ns.type == AVA_DICT, "system.IO.File existe");
    {
        const char* path = "/tmp/avalang_fase7_test.txt";
        ava_value_t write_fn = GetMember(vm, file_ns, "WriteAllText");
        ava_value_t read_fn = GetMember(vm, file_ns, "ReadAllText");
        ava_value_t exists_fn = GetMember(vm, file_ns, "Exists");
        ava_value_t delete_fn = GetMember(vm, file_ns, "Delete");

        ava_value_t wargs[2] = {
            ava_string_create(vm, path, strlen(path)),
            ava_string_create(vm, "contenido de prueba fase 7", strlen("contenido de prueba fase 7"))
        };
        ava_value_t wout; char* werr = nullptr;
        ava_call(vm, write_fn, wargs, 2, &wout, &werr);
        CHECK(werr == nullptr && wout.type == AVA_BOOL && wout.as.b, "IO.File.WriteAllText(...) escribe ok");

        ava_value_t eargs[1] = { ava_string_create(vm, path, strlen(path)) };
        ava_value_t eout; char* eerr = nullptr;
        ava_call(vm, exists_fn, eargs, 1, &eout, &eerr);
        CHECK(eerr == nullptr && eout.type == AVA_BOOL && eout.as.b, "IO.File.Exists(...) da true tras escribir");

        ava_value_t rargs[1] = { ava_string_create(vm, path, strlen(path)) };
        ava_value_t rout; char* rerr = nullptr;
        ava_call(vm, read_fn, rargs, 1, &rout, &rerr);
        CHECK(rerr == nullptr && rout.type == AVA_STRING, "IO.File.ReadAllText(...) devuelve string");
        if (rout.type == AVA_STRING) {
            size_t slen = 0;
            const char* s = ava_string_data(vm, rout, &slen);
            std::string content(s, slen);
            CHECK(content == "contenido de prueba fase 7", "IO.File.ReadAllText(...) devuelve el contenido escrito");
        }

        ava_value_t dargs[1] = { ava_string_create(vm, path, strlen(path)) };
        ava_value_t dout; char* derr = nullptr;
        ava_call(vm, delete_fn, dargs, 1, &dout, &derr);
        CHECK(derr == nullptr && dout.type == AVA_BOOL && dout.as.b, "IO.File.Delete(...) limpia ok");
    }

    // --- Diagnostics.Process ---
    ava_value_t diagnostics = ava_get_global(vm, "Diagnostics");
    CHECK(diagnostics.type == AVA_DICT, "system.Diagnostics existe");
    ava_value_t process_ns = GetMember(vm, diagnostics, "Process");
    CHECK(process_ns.type == AVA_DICT, "system.Diagnostics.Process existe");
    {
        ava_value_t get_pid = GetMember(vm, process_ns, "GetCurrentId");
        ava_value_t pid_out; char* pid_err = nullptr;
        ava_call(vm, get_pid, nullptr, 0, &pid_out, &pid_err);
        CHECK(pid_err == nullptr && pid_out.type == AVA_NUMBER && pid_out.as.n > 0, "Process.GetCurrentId() devuelve un pid > 0");
        std::fprintf(stderr, "      pid = %.0f\n", pid_out.as.n);

        ava_value_t start_fn = GetMember(vm, process_ns, "Start");
        ava_value_t args_list = ava_list_create(vm);
        ava_list_append(vm, args_list, ava_string_create(vm, "fase7", strlen("fase7")));
        ava_value_t sargs[2] = {
            ava_string_create(vm, "echo", strlen("echo")),
            args_list
        };
        ava_value_t sout; char* serr = nullptr;
        ava_call(vm, start_fn, sargs, 2, &sout, &serr);
        CHECK(serr == nullptr, "Process.Start(echo, [fase7]) no tira error");
        CHECK(sout.type == AVA_DICT, "Process.Start(...) devuelve un Dict (no nil) para un comando real");
        if (sout.type == AVA_DICT) {
            ava_value_t exit_code = ava_dict_get(vm, sout, "ExitCode");
            ava_value_t stdout_v = ava_dict_get(vm, sout, "Stdout");
            CHECK(exit_code.type == AVA_NUMBER && exit_code.as.n == 0, "echo termino con ExitCode 0");
            if (stdout_v.type == AVA_STRING) {
                size_t slen = 0;
                const char* s = ava_string_data(vm, stdout_v, &slen);
                std::fprintf(stderr, "      stdout de 'echo fase7' = %.*s", (int)slen, s);
                std::string out_str(s, slen);
                CHECK(out_str.find("fase7") != std::string::npos, "stdout de echo contiene el argumento pasado");
            }
        }

        // comando inexistente -> nil, no un crash ni un ExitCode inventado
        ava_value_t bad_args = ava_list_create(vm);
        ava_value_t bargs[2] = {
            ava_string_create(vm, "comando_que_no_existe_fase7", strlen("comando_que_no_existe_fase7")),
            bad_args
        };
        ava_value_t bout; char* berr = nullptr;
        ava_call(vm, start_fn, bargs, 2, &bout, &berr);
        CHECK(berr == nullptr, "Process.Start(comando inexistente) no tira excepcion");
        CHECK(bout.type == AVA_NIL, "Process.Start(comando inexistente) devuelve nil");
    }

    // --- azucar de submodulos con punto (Fase 6): import system.io ---
    {
        char* ioerr = nullptr;
        ava_import(vm, "system.io", nullptr, &ioerr);
        CHECK(ioerr == nullptr, "import system.io no genera error");
        ava_value_t sys_after = ava_get_global(vm, "system");
        CHECK(sys_after.type == AVA_DICT, "system sigue siendo Dict tras import system.io");
        ava_value_t file_after = ava_dict_get(vm, sys_after, "File");
        CHECK(file_after.type == AVA_DICT, "system.File existe tras import system.io (reemplaza el global, doc. en Fase 6)");
    }

    ava_vm_destroy(vm);

    std::fprintf(stderr, "\n=== %s (%d fallas) ===\n", g_failures == 0 ? "TODO OK" : "HUBO FALLAS", g_failures);
    return g_failures == 0 ? 0 : 1;
}
