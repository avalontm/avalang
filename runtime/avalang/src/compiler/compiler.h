#ifndef AVA_COMPILER_COMPILER_H
#define AVA_COMPILER_COMPILER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../ast/ast.h"
#include "../vm/proto.h"

namespace ava {

class Compiler {
public:
    std::shared_ptr<Proto> Compile(const std::shared_ptr<Chunk>& chunk,
                                    const std::string& source_name = "");

private:
    struct JmpPatch {
        size_t instr_idx;
        int32_t offset;
        size_t target_idx;
    };

    std::shared_ptr<Proto> proto_;
    // Source line of the statement currently being compiled (see
    // CompileStmt), stamped onto every instruction Emit() produces while
    // compiling it so Proto::debug_lines can map instructions back to
    // source for error reporting. 0 = unknown (e.g. instructions emitted
    // outside any CompileStmt call, such as an implicit trailing RETURN).
    int current_line_ = 0;
    // 1-based source column matching current_line_ above; 0 = unknown.
    // Only used for AvaError reporting (debug_lines only tracks line).
    int current_col_ = 0;
    // Path of the file being compiled; stamped onto proto_->source_name
    // (top-level Compile()) and onto every sub-Compiler's proto_ (lambda,
    // free function, class method) so runtime errors can report the
    // correct file, including inside imported modules.
    std::string source_name_;
    uint16_t next_reg_ = 0;
    uint16_t max_reg_ = 0;
    uint16_t result_reg_ = 0;
    std::unordered_map<std::string, uint16_t> locals_;
    // true solo para el Compiler raíz (nivel de módulo/script, ver
    // Compile()). Cada función/método/lambda compila en su propio
    // `Compiler sub` (CompileFunc, CompileClass, LambdaExpr) y ese sub
    // pone esto en false. Decide si una asignación a un nombre simple
    // (`x = ...`, con o sin `local` delante -- la gramática descarta esa
    // palabra clave, ver ast_builder.cpp:visitLocalStatement) se
    // resuelve como variable local a ese scope (registro persistente) o
    // como variable global (SETGLOBAL/GETGLOBAL), ver CompileStmt.
    bool is_top_level_ = true;
    // true solo dentro del `Compiler sub` que compila el cuerpo de un
    // `async func` (ver CompileFunc). Igual que is_top_level_, no se
    // hereda a un `func` normal anidado dentro de uno async -- cada
    // función/método/lambda compila en su propio `Compiler sub` desde
    // cero, así que un `func` anidado empieza con esto en false, igual
    // que en JS/Python/C# (await solo vale en el cuerpo léxico directo
    // de la función async que lo contiene). Ver CompileExpr(AwaitExpr).
    bool in_async_func_ = false;
    std::unordered_map<std::string, ClassObj*> compiled_classes_;
    ClassObj* current_base_class_ = nullptr;
    bool is_init_ = false;
    std::unordered_set<std::string> instance_attrs_;
    std::vector<std::pair<std::string, uint16_t>> parent_locals_;
    Compiler* parent_ = nullptr;

    std::vector<JmpPatch> pending_breaks_;
    std::vector<JmpPatch> pending_continues_;

    // Bug #42: pila de bloques 'finally' de los try/except que se estan
    // compilando en este momento (uno por cada TryStmt anidado que
    // todavia no termino de compilarse), de mas interno a mas externo.
    // CompileStmt lo consulta al compilar un ReturnStmt para inyectar
    // esos finally's (mas interno primero) antes del OpCode::RETURN --
    // ver CompileTry y CompileStmt. Guarda punteros porque vive dentro
    // del AST (TryStmt ya es dueño del vector), no hace falta copiarlo.
    std::vector<const std::vector<std::shared_ptr<StmtNode>>*> pending_finally_stack_;

    uint32_t for_depth_ = 0;

    void Reset();

    uint16_t AllocReg();
    void FreeRegs(uint16_t count);
    uint16_t AddConstant(const Value& v);
    static Value MakeString(const std::string& s);

    void Emit(OpCode op, uint16_t a = 0, uint16_t b = 0, uint16_t c = 0);

    // Punto UNICO donde current_line_/current_col_ se actualizan a partir de
    // una statement. CompileStmt y CompileExprToReg son los dos puntos de
    // entrada que compilan una StmtNode "de punta a punta" (CompileChunk
    // manda la ultima statement del chunk por CompileExprToReg en vez de
    // CompileStmt, para soportar el implicit-return del ultimo valor) --
    // los dos DEBEN llamar a esto antes de emitir nada, o current_line_
    // queda pegado en lo que dejo la statement anterior y los errores se
    // reportan en la statement equivocada. Si en el futuro aparece un
    // tercer punto de entrada de este tipo, tiene que llamar a esto tambien
    // en vez de reimplementar el check.
    void StampLine(const std::shared_ptr<StmtNode>& stmt);

    uint16_t CompileExpr(const std::shared_ptr<ExprNode>& expr);
    void CompileStmt(const std::shared_ptr<StmtNode>& stmt);
    void CompileChunk(const std::vector<std::shared_ptr<StmtNode>>& stmts);
    uint16_t CompileExprToReg(const std::shared_ptr<StmtNode>& stmt);

    void PatchJump(size_t instr_idx);
    void PatchContinueJump(size_t instr_idx, size_t loop_start);

    void CompileIf(const IfStmt* stmt);
    void CompileWhile(const WhileStmt* stmt);
    void CompileFor(const ForStmt* stmt);
    void CompileFunc(const FuncDef* func);
    void EmitDefaultsPrologue(const std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>>& params,
                               uint16_t param_reg_base);
    void CompileClass(const ClassDef* cls);
    void CompileImport(const ImportStmt* stmt);
    void CompileExtern(const ExternStmt* stmt);
    void CompileTry(const TryStmt* stmt);
    void CompileRaise(const RaiseStmt* stmt);
    void CompileMultiAssign(const MultiAssignStmt* stmt);
    uint16_t CompileFStringExpression(const std::string& expr_str);

    enum class IteratorKind { List, Coroutine, Dict };
    IteratorKind DetectIteratorKind(const std::shared_ptr<ExprNode>& iterable);
    void CompileForIterator(const ForStmt* stmt);
    void CompileForList(const ForStmt* stmt, uint32_t depth);
    void CompileForCoroutine(const ForStmt* stmt, uint32_t depth);
    void CompileForDict(const ForStmt* stmt, uint32_t depth);
    void CompileForDynamic(const ForStmt* stmt, uint32_t depth);

    std::shared_ptr<ExprNode> ParseFStringExpr(const std::string& expr_str);

    static OpCode BinOpToOpcode(BinOp op);
    static bool IsShortCircuit(BinOp op);

    int16_t FindUpvalue(const std::string& name);

private:
    std::shared_ptr<ExprNode> ParseExpr(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseOrExpr(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseAndExpr(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseComparison(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseAddSub(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseMulDiv(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseUnary(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParsePower(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParsePostfix(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParsePrimary(const std::string& s, size_t& pos);
};

} // namespace ava

#endif // AVA_COMPILER_COMPILER_H