// Caso 3 (agregado por mi durante la verificacion, no está literal en la
// seccion 6 del plan): recursion DENTRO del cuerpo del `for`, no despues.
//
// Los casos 1 y 2 de la seccion 6 del plan recursan DESPUES de que el
// `for` ya terminó de iterar por completo (la llamada recursiva es la
// última sentencia de la función, fuera del `for`). Con la
// implementación actual (secuencial, sin coroutines/callbacks reales
// entrelazados), eso significa que nunca hay dos invocaciones "vivas" del
// mismo `for` al mismo tiempo -- el bug de las globales no se dispara
// (confirmado corriendo repro_recursion.cpp: caso 1 da 18, caso 2 da
// 1 2 3 4 5 6, ambos correctos incluso con el compilador SIN arreglar).
//
// El disparador real (ver PLAN_FIX_FOR_GLOBALS.md, seccion "Por qué esto
// rompe cosas reales" -> "Recursión"): una llamada recursiva HECHA DESDE
// ADENTRO del cuerpo del `for`, cuando esa llamada recursiva a su vez
// entra a OTRO `for`. Ahí sí hay dos `for` con estado "en progreso" al
// mismo tiempo (el externo está a mitad de iterar cuando el interno pisa
// __for_idx/__for_list/__for_len).
//
// func walk(n)
//     if n <= 0 then return end
//     for i in [10, 20, 30] do
//         print(n, i)
//         walk(n - 1)
//     end
// end
// walk(2)
//
// Esperado (semántica correcta, "for" con estado aislado por frame):
//   walk(2) itera sus 3 elementos completos; por cada uno llama walk(1),
//   que a su vez itera sus 3 elementos completos y llama walk(0) (que no
//   hace nada, n<=0). Total: 3 (outer) + 3*3 (inner, uno por cada
//   iteracion del outer) = 12 líneas "n i".
//
// Bug (estado global compartido): al volver de la llamada recursiva
// walk(n-1) DENTRO del cuerpo del for, el __for_idx/__for_len que lee el
// `for` externo para decidir si sigue iterando fueron pisados por el
// `for` interno (que sí llegó a terminar su propio loop antes de
// retornar) -> el `for` externo lee un índice ya en el tope y CORTA la
// iteración externa después de la primera vuelta.
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
    return compiler.Compile(std::make_shared<Chunk>(chunk), "repro_case3.ava");
}

static VM* MakeVM() {
    VM* vm = new VM();
    RegisterBuiltinMethods(reinterpret_cast<AvaVM*>(vm));
    RegisterBuiltinGlobals(reinterpret_cast<AvaVM*>(vm));
    return vm;
}

int main() {
    std::cout << "=== Caso 3: recursion DENTRO del cuerpo del for (disparador real) ===\n";
    std::cout << "--- esperado: 12 lineas 'n i' (3 del walk(2) externo, 9 de los walk(1) internos) ---\n\n";

    auto body = std::vector<std::shared_ptr<StmtNode>>{
        If(Bin(BinOp::Le, Name("n"), Num(0)), { Return() }),
        For("i", List({Num(10), Num(20), Num(30)}),
            {
                Expr(Call("print", {Name("n"), Name("i")})),
                Expr(Call("walk", {Bin(BinOp::Sub, Name("n"), Num(1))}))
            })
    };

    auto chunk = std::vector<std::shared_ptr<StmtNode>>{
        Func("walk", {"n"}, body),
        Expr(Call("walk", {Num(2)}))
    };

    auto proto = CompileChunkPublic(chunk);
    auto vm = MakeVM();
    vm->Run(proto);
    delete vm;

    std::cout << "\n(contá las lineas de arriba: 12 = correcto / aislado por frame, "
                 "menos de 12 = bug de globales compartidas confirmado)\n";
    return 0;
}
