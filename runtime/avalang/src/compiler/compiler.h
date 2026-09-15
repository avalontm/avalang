#ifndef AVA_COMPILER_COMPILER_H
#define AVA_COMPILER_COMPILER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../ast/ast.h"
#include "../common/symbol.h"
#include "../vm/proto.h"

namespace ava {

struct InterfaceInfo {
    std::string name;
    std::vector<std::string> base_interfaces;
    std::unordered_map<std::string, InterfaceMethodSig> signatures;
    std::vector<std::string> signature_order;
    std::unordered_map<std::string, std::shared_ptr<Proto>> default_methods;
    std::unordered_map<std::string, std::string> default_method_origin;
    std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>> method_params;
    std::unordered_map<std::string, TypeRef> method_returns;
};

struct HarvestedClassInfo {
    std::unordered_map<std::string, ClassObj*> classes;
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> field_types;
    std::unordered_map<std::string, std::unordered_set<std::string>> dynamic_attrs;
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> method_returns;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>>
        method_params;
    std::unordered_map<std::string, InterfaceInfo> interfaces;
    std::vector<Value> keepalive;
};

class Compiler {
public:
    std::shared_ptr<Proto> Compile(const std::shared_ptr<Chunk>& chunk,
                                    const std::string& source_name = "");

    const std::unordered_map<std::string, ClassObj*>& CompiledClasses() const { return compiled_classes_; }

    HarvestedClassInfo SnapshotClassInfo() const {
        HarvestedClassInfo info;
        for (auto& [name, obj] : compiled_classes_) {
            if (!own_declared_types_.count(name)) continue;
            info.classes[name] = obj;
            auto ft = class_field_types_.find(name);
            if (ft != class_field_types_.end()) info.field_types[name] = ft->second;
            auto da = class_dynamic_attrs_.find(name);
            if (da != class_dynamic_attrs_.end()) info.dynamic_attrs[name] = da->second;
            auto mr = class_method_returns_.find(name);
            if (mr != class_method_returns_.end()) info.method_returns[name] = mr->second;
            auto mp = class_method_params_.find(name);
            if (mp != class_method_params_.end()) info.method_params[name] = mp->second;
        }
        for (auto& [name, iface_info] : compiled_interfaces_) {
            if (!own_declared_types_.count(name)) continue;
            info.interfaces[name] = iface_info;
        }
        info.keepalive.reserve(info.classes.size());
        for (auto& [name, obj] : info.classes) {
            (void)name;
            if (!obj) continue;
            Value v;
            v.type = ValueType::Class;
            v.obj = obj;
            Retain(v);
            info.keepalive.push_back(std::move(v));
        }
        return info;
    }

    void SetImportHarvestContext(
        std::unordered_set<std::string>* chain,
        std::unordered_map<std::string, HarvestedClassInfo>* cache) {
        import_chain_ = chain;
        import_class_cache_ = cache;
    }

    void AdoptImportedClassRefs(const std::vector<Value>& refs) {
        imported_class_keepalive_.insert(imported_class_keepalive_.end(), refs.begin(), refs.end());
    }

private:
    struct JmpPatch {
        size_t instr_idx;
        int32_t offset;
        size_t target_idx;
    };

    std::shared_ptr<Proto> proto_;
    int current_line_ = 0;
    int current_col_ = 0;
    std::string source_name_;
    std::string current_file_dir_;
    std::unordered_set<std::string>* import_chain_ = nullptr;
    std::unordered_map<std::string, HarvestedClassInfo>* import_class_cache_ = nullptr;

    std::vector<Value> imported_class_keepalive_;
    uint16_t next_reg_ = 0;
    uint16_t max_reg_ = 0;
    uint16_t result_reg_ = 0;
    std::unordered_map<std::string, uint16_t> locals_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>> known_funcs_;
    std::unordered_map<std::string, TypeRef> known_func_returns_;
    std::unordered_set<std::string> known_top_level_globals_;
    TypeRef current_return_type_;
    bool is_top_level_ = true;
    bool in_async_func_ = false;
    bool entry_found_ = false;
    std::string entry_func_name_;
    std::string entry_class_name_;
    int entry_line_ = 0;
    std::unordered_map<std::string, ClassObj*> compiled_classes_;
    std::unordered_map<std::string, InterfaceInfo> compiled_interfaces_;
    std::unordered_map<std::string, std::string> imported_type_owner_;
    std::unordered_set<std::string> own_declared_types_;
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> class_field_types_;
    std::unordered_map<std::string, std::unordered_set<std::string>> class_dynamic_attrs_;
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> class_method_returns_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>>
        class_method_params_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>>
        extern_func_params_;
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> extern_func_returns_;
    bool has_wildcard_import_ = false;
    ClassObj* current_base_class_ = nullptr;
    std::string current_class_name_;
    bool is_init_ = false;
    std::unordered_set<std::string> instance_attrs_;
    std::vector<std::pair<std::string, uint16_t>> parent_locals_;
    Compiler* parent_ = nullptr;

    std::vector<JmpPatch> pending_breaks_;
    std::vector<JmpPatch> pending_continues_;

    std::vector<const std::vector<std::shared_ptr<StmtNode>>*> pending_finally_stack_;

    uint32_t for_depth_ = 0;

    std::vector<std::unordered_set<std::string>> block_scoped_names_;

    int block_depth_ = 0;
    std::unordered_map<std::string, Symbol> floor_symbols_;
    int loop_depth_ = 0;

    void Reset();

    uint16_t AllocReg();
    void FreeRegs(uint16_t count);
    uint16_t AddConstant(const Value& v);
    static Value MakeString(const std::string& s);

    void Emit(OpCode op, uint16_t a = 0, uint16_t b = 0, uint16_t c = 0);

    void StampLine(const std::shared_ptr<StmtNode>& stmt);

    void DeclareSymbol(const std::string& name, const TypeRef& declared, const TypeRef& inferred = TypeRef{});

    TypeRef ResolveTypeName(const std::string& name);

    TypeRef InferExprTypeRef(const std::shared_ptr<ExprNode>& expr);

    Type InferExprType(const std::shared_ptr<ExprNode>& expr);

    void CheckReassignment(const AssignStmt* a, const std::string& name, const TypeRef& inferred);

    void CheckCallArgs(const std::string& func_name, const CallExpr* c);
    bool NameShadowsGlobalCallable(const std::string& name) const;

    void CheckMethodCallArgs(const AttrExpr* callee, const CallExpr* c);

    void CheckPrivateAccess(const std::string& class_name, const std::string& member,
                             int line, int col);
    void CheckCallArgsAgainst(const std::vector<std::pair<std::string, Type>>& params,
                               const std::string& label, const CallExpr* c);

    void CheckReturnType(const ReturnStmt* r);

    void CheckBinOpTypes(const BinOpExpr* b);
    void CheckUnOpTypes(const UnOpExpr* u);

    uint16_t CompileExpr(const std::shared_ptr<ExprNode>& expr);
    void CompileStmt(const std::shared_ptr<StmtNode>& stmt);
    void CompileChunk(const std::vector<std::shared_ptr<StmtNode>>& stmts);
    uint16_t CompileExprToReg(const std::shared_ptr<StmtNode>& stmt);
    void CompileBlock(const std::vector<std::shared_ptr<StmtNode>>& body);

    void PatchJump(size_t instr_idx);
    void PatchContinueJump(size_t instr_idx, size_t loop_start);

    void CompileIf(const IfStmt* stmt);
    void CompileWhile(const WhileStmt* stmt);
    void CompileFor(const ForStmt* stmt);
    // Fase 4 del plan break/continue/operadores: `for i = a to b [step s]`.
    void CompileForRange(const ForRangeStmt* stmt);
    void CompileFunc(const FuncDef* func);
    void EmitDefaultsPrologue(const std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>>& params,
                               uint16_t param_reg_base);
    void CompileClass(const ClassDef* cls);
    void ValidateEntryAttributes(const FuncDef* func, const std::string& owner_class);
    void CompileInterface(const InterfaceDef* iface);
    void CompileImport(const ImportStmt* stmt);
    void RegisterImportedClasses(const std::string& module_name);
    void AutoImportSiblings(const std::vector<std::shared_ptr<StmtNode>>& statements);
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

    void MarkBlockScoped(const std::string& name);

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
    std::shared_ptr<ExprNode> TryParseLambda(const std::string& s, size_t& pos);
};

} // namespace ava

#endif // AVA_COMPILER_COMPILER_H