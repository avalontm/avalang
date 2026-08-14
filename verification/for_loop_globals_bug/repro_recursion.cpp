// Verificacion ad-hoc (no forma parte del codigo del proyecto): construye
// a mano el AST de los casos 1 y 2 de la seccion 6 del plan
// (PLAN_FIX_FOR_GLOBALS.md) -- bypass del frontend ANTLR, mismo patron que
// test_proto_io_obfuscate.cpp -- para poder compilar+correr en este sandbox
// sin vcpkg/antlr4, y así demostrar el bug ANTES del fix / la corrección
// DESPUES del fix.
#include "ast_dsl.h"
#include "compiler/compiler.h"
#include "vm/vm.h"
#include "builtins/builtin.h"
#include <iostream>

using namespace ava;
using namespace dsl;

// Compiler::Compile es la unica API publica de Compiler -- no hace falta
// tocar nada privado.

static std::shared_ptr<Proto> CompileChunkPublic(std::vector<std::shared_ptr<StmtNode>> stmts) {
    Chunk chunk;
    chunk.statements = std::move(stmts);
    Compiler compiler;
    return compiler.Compile(std::make_shared<Chunk>(chunk), "repro.ava");
}

static VM* MakeVM() {
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    return vm;
}

// --- Caso 1: recursion simple con `for` (seccion 6.1 del plan) ---------
// func sum_to(n)
//     if n <= 0 then return 0 end
//     total = 0
//     for i in [1, 2, 3] do total = total + i end
//     return total + sum_to(n - 1)
// end
// print(sum_to(3))
static void Case1_SumToRecursion() {
    std::cout << "\n=== Caso 1: recursion simple con `for` -- esperado: 18 ===\n";

    auto body = std::vector<std::shared_ptr<StmtNode>>{
        If(Bin(BinOp::Le, Name("n"), Num(0)),
           { Return(Num(0)) }),
        Assign("total", Num(0)),
        For("i", List({Num(1), Num(2), Num(3)}),
            { Assign("total", Bin(BinOp::Add, Name("total"), Name("i"))) }),
        Return(Bin(BinOp::Add, Name("total"),
                    Call("sum_to", {Bin(BinOp::Sub, Name("n"), Num(1))})))
    };

    auto chunk = std::vector<std::shared_ptr<StmtNode>>{
        Func("sum_to", {"n"}, body),
        Expr(Call("print", {Call("sum_to", {Num(3)})}))
    };

    auto proto = CompileChunkPublic(chunk);
    auto vm = MakeVM();
    vm->Run(proto);
    delete vm;
}

// --- Caso 2: recursion con `for` sobre listas de distinto tamano por nivel
// (seccion 6.2 del plan; usamos un indice en vez de slicing `lists[1:]`
// porque el builtin global `slice` que usa CompileExpr(SliceExpr) no esta
// registrado en este arbol -- gap preexistente y no relacionado con este
// bug; el indice ejercita exactamente el mismo patron de "un `for` por
// nivel de recursion, con distinto largo cada vez").
// func walk(lists, idx)
//     if idx >= len(lists) then return end
//     for x in lists[idx] do print(x) end
//     walk(lists, idx + 1)
// end
// walk([[1,2,3],[4,5],[6]], 0)
static void Case2_WalkVaryingLengths() {
    std::cout << "\n=== Caso 2: for recursivo, largo de lista distinto por nivel ===\n";
    std::cout << "--- esperado: 1 2 3 4 5 6 (cada uno en su propia linea, sin mezclarse) ---\n";

    auto body = std::vector<std::shared_ptr<StmtNode>>{
        If(Bin(BinOp::Ge, Name("idx"), Call("len", {Name("lists")})),
           { Return() }),
        For("x", Idx(Name("lists"), Name("idx")),
            { Expr(Call("print", {Name("x")})) }),
        Expr(Call("walk", {Name("lists"), Bin(BinOp::Add, Name("idx"), Num(1))}))
    };

    auto chunk = std::vector<std::shared_ptr<StmtNode>>{
        Func("walk", {"lists", "idx"}, body),
        Expr(Call("walk", {
            List({ List({Num(1),Num(2),Num(3)}), List({Num(4),Num(5)}), List({Num(6)}) }),
            Num(0)
        }))
    };

    auto proto = CompileChunkPublic(chunk);
    auto vm = MakeVM();
    vm->Run(proto);
    delete vm;
}

int main() {
    Case1_SumToRecursion();
    Case2_WalkVaryingLengths();
    return 0;
}
