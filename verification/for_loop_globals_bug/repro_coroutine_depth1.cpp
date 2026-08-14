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
    return compiler.Compile(std::make_shared<Chunk>(chunk), "coroutine_depth1.ava");
}

int main() {
    auto gen_body = std::vector<std::shared_ptr<StmtNode>>{
        Yield({Num(100)}), Yield({Num(200)}), Yield({Num(300)})
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
        Expr(Call("walk", {Num(1)}))
    };
    auto proto = CompileChunkPublic(chunk);
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    vm->Run(proto);
    delete vm;
    return 0;
}
