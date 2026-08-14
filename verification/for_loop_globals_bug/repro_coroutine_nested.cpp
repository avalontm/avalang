// Analogo al caso 3 (walk_nested) pero para CompileForCoroutine en vez de
// CompileForList: recursion DESDE ADENTRO del cuerpo de un
// `for v in coroutine(...) do`, cuando esa llamada recursiva a su vez
// entra a OTRO `for` sobre coroutine.
//
// func gen()
//     yield 100
//     yield 200
//     yield 300
// end
//
// func walk(n)
//     if n <= 0 then return end
//     for v in coroutine(gen) do
//         print(n, v)
//         walk(n - 1)
//     end
// end
// walk(2)
//
// gen() siempre yieldea la misma secuencia fija (100, 200, 300) sin
// depender de n, para aislar el bug de __iter/__resume/__val de
// cualquier otro problema de estado compartido -- si la corrutina fuera
// parametrizada por n via una global, un resultado incorrecto podria
// venir de esa otra global en vez de la que este fix corrige.
//
// Esperado (for-coroutine con estado aislado por frame): 12 lineas
// "n v", igual estructura que el caso 3 de listas (3 del walk(2)
// externo, 9 de los walk(1) internos, walk(0) no hace nada).
//
// Bug (antes del fix): al volver de walk(n-1) DENTRO del cuerpo del for,
// el __iter/__resume/__val globales que lee el for externo fueron
// pisados por el for interno (que sí agoto su propia corrutina antes de
// retornar) -> el for externo lee __val ya en nil y corta despues de la
// primera iteracion.
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
    return compiler.Compile(std::make_shared<Chunk>(chunk), "repro_coroutine_nested.ava");
}

static VM* MakeVM() {
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    return vm;
}

int main() {
    std::cout << "=== Caso coroutine anidada: recursion DENTRO de un for-coroutine ===\n";
    std::cout << "--- esperado: 12 lineas 'n v' (3 del walk(2) externo, 9 de los walk(1) internos) ---\n\n";

    auto gen_body = std::vector<std::shared_ptr<StmtNode>>{
        Yield({Num(100)}),
        Yield({Num(200)}),
        Yield({Num(300)})
    };

    auto walk_body = std::vector<std::shared_ptr<StmtNode>>{
        If(Bin(BinOp::Le, Name("n"), Num(0)), { Return() }),
        For("v", Call("coroutine", {Name("gen")}),
            {
                Expr(Call("print", {Name("n"), Name("v")})),
                Expr(Call("walk", {Bin(BinOp::Sub, Name("n"), Num(1))}))
            })
    };

    auto chunk = std::vector<std::shared_ptr<StmtNode>>{
        Func("gen", {}, gen_body),
        Func("walk", {"n"}, walk_body),
        Expr(Call("walk", {Num(2)}))
    };

    auto proto = CompileChunkPublic(chunk);
    auto vm = MakeVM();
    vm->Run(proto);
    delete vm;

    std::cout << "\n(conta las lineas de arriba: 12 = correcto / aislado por frame, "
                 "menos de 12 = bug de globales compartidas confirmado)\n";
    return 0;
}
