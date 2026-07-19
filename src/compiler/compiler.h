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
    std::shared_ptr<Proto> Compile(const std::shared_ptr<Chunk>& chunk);

private:
    struct JmpPatch {
        size_t instr_idx;
        int32_t offset;
    };

    std::shared_ptr<Proto> proto_;
    uint16_t next_reg_ = 0;
    uint16_t max_reg_ = 0;
    std::unordered_map<std::string, uint16_t> locals_;
    std::unordered_map<std::string, ClassObj*> compiled_classes_;
    ClassObj* current_base_class_ = nullptr;
    bool is_init_ = false;
    std::unordered_set<std::string> instance_attrs_;
    std::vector<std::pair<std::string, uint16_t>> parent_locals_;
    Compiler* parent_ = nullptr;

    std::vector<JmpPatch> pending_breaks_;
    std::vector<JmpPatch> pending_continues_;

    void Reset();

    uint16_t AllocReg();
    void FreeRegs(uint16_t count);
    uint16_t AddConstant(const Value& v);
    static Value MakeString(const std::string& s);

    void Emit(OpCode op, uint16_t a = 0, uint16_t b = 0, uint16_t c = 0);

    uint16_t CompileExpr(const std::shared_ptr<ExprNode>& expr);
    void CompileStmt(const std::shared_ptr<StmtNode>& stmt);
    void CompileChunk(const std::vector<std::shared_ptr<StmtNode>>& stmts);

    void PatchJump(size_t instr_idx);

    void CompileIf(const IfStmt* stmt);
    void CompileWhile(const WhileStmt* stmt);
    void CompileFor(const ForStmt* stmt);
    void CompileFunc(const FuncDef* func);
    void CompileClass(const ClassDef* cls);
    void CompileImport(const ImportStmt* stmt);
    uint16_t CompileFStringExpression(const std::string& expr_str);

    std::shared_ptr<ExprNode> ParseFStringExpr(const std::string& expr_str);

    static OpCode BinOpToOpcode(BinOp op);
    static bool IsShortCircuit(BinOp op);

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