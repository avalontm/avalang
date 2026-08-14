// Caso 3 del plan (seccion 4.5 / 6.3: `for` anidados con el mismo nombre
// de variable) pero compilado DENTRO de una funcion (!is_top_level_),
// no a nivel de modulo. El caso 3 tal como esta en
// modules/tests/for_recursion.ava (raiz del repo) esta escrito a nivel
// de modulo, asi que ejercita la rama SETGLOBAL/GETGLOBAL sin tocar
// (is_top_level_ == true, sin cambios por este plan) -- nunca prueba el
// camino de registros locales que sí toca este fix.
//
// func test()
//     for x in [1, 2] do
//         for x in [10, 20] do
//             print(x)
//         end
//         print("outer x=" + str(x))
//     end
// end
// test()
//
// Segun 4.5 del plan: esta VM no tiene scope de bloque, asi que el `x`
// interno y el externo son la MISMA variable (mismo registro local)
// tanto dentro de una funcion como a nivel de modulo -- no deberia haber
// diferencia de comportamiento entre ambos casos, solo de mecanismo
// (registro local vs global). Se documenta explicitamente para que quede
// como evidencia, no se infiere del caso top-level.
//
// Esperado (igual que el caso 3 top-level de for_recursion.ava):
//   10 20 "outer x=20" 10 20 "outer x=20"
#include "ast_dsl.h"
#include "compiler/compiler.h"
#include "vm/vm.h"
#include "builtins/builtin.h"
#include <iostream>

using namespace ava;
using namespace dsl;

static std::shared_ptr<Proto> CompileChunkPublic(std::vector<std::shared_ptr<StmtNode>> stmts) {
    Chunk chunk;
    chunk.statements = std::move(stmts);
    Compiler compiler;
    return compiler.Compile(std::make_shared<Chunk>(chunk), "repro_case3_same_var_local.ava");
}

static VM* MakeVM() {
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    return vm;
}

int main() {
    std::cout << "=== Caso 3 (mismo nombre de variable, for anidados) EN SCOPE LOCAL ===\n";
    std::cout << "--- esperado: 10 20 \"outer x=20\" 10 20 \"outer x=20\" (identico al caso top-level) ---\n\n";

    auto test_body = std::vector<std::shared_ptr<StmtNode>>{
        For("x", List({Num(1), Num(2)}),
            {
                For("x", List({Num(10), Num(20)}),
                    {
                        Expr(Call("print", {Name("x")}))
                    }),
                Expr(Call("print", {
                    Bin(BinOp::Add, Str("outer x="), Call("str", {Name("x")}))
                }))
            })
    };

    auto chunk = std::vector<std::shared_ptr<StmtNode>>{
        Func("test", {}, test_body),
        Expr(Call("test", {}))
    };

    auto proto = CompileChunkPublic(chunk);
    auto vm = MakeVM();
    vm->Run(proto);
    delete vm;

    std::cout << "\n(si coincide con lo esperado arriba, 4.5 se comporta igual "
                 "dentro de una funcion que a nivel de modulo)\n";
    return 0;
}
