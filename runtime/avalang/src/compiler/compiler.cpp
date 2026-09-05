#include "compiler.h"
#include "../vm/vm_extern.h"
#include "../common/ava_error.h"
#include "../builtins/builtin_names.h"
#include <stdexcept>
#include <cstdio>
#include <unordered_set>

namespace ava {

Value Compiler::MakeString(const std::string& s) {
    Value v;
    v.type = ValueType::String;
    v.obj = new StringObj(s);
    return v;
}

void Compiler::Reset() {
    proto_ = std::make_shared<Proto>();
    current_line_ = 0;
    next_reg_ = 0;
    max_reg_ = 0;
    locals_.clear();
    pending_breaks_.clear();
    pending_continues_.clear();
    pending_finally_stack_.clear();
    parent_locals_.clear();
    parent_ = nullptr;
    loop_depth_ = 0;
}

uint16_t Compiler::AllocReg() {
    return next_reg_++;
}

void Compiler::FreeRegs(uint16_t count) {
    next_reg_ -= count;
}

bool ConstantsEqual(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case ValueType::Nil: return true;
        case ValueType::Bool: return a.b == b.b;
        case ValueType::Number: return a.n == b.n;
        case ValueType::String: {
            auto* sa = static_cast<StringObj*>(a.obj);
            auto* sb = static_cast<StringObj*>(b.obj);
            return sa->data == sb->data;
        }
        default: return false;
    }
}

uint16_t Compiler::AddConstant(const Value& v) {
    for (uint16_t i = 0; i < proto_->constants.size(); ++i) {
        if (ConstantsEqual(proto_->constants[i], v)) {
            return i;
        }
    }
    proto_->constants.push_back(v);
    return static_cast<uint16_t>(proto_->constants.size() - 1);
}

void Compiler::Emit(OpCode op, uint16_t a, uint16_t b, uint16_t c) {
    proto_->instructions.push_back({op, static_cast<uint8_t>(a), b, c});

    proto_->debug_lines.push_back(static_cast<uint32_t>(current_line_));
    proto_->debug_columns.push_back(static_cast<uint32_t>(current_col_));
    if (next_reg_ > max_reg_) max_reg_ = next_reg_;
}

void Compiler::PatchJump(size_t instr_idx) {
    auto& instr = proto_->instructions[instr_idx];
    int32_t offset = static_cast<int32_t>(proto_->instructions.size()) -
                     static_cast<int32_t>(instr_idx) - 1;
    instr.bx32 = offset;
}

void Compiler::PatchContinueJump(size_t instr_idx, size_t loop_start) {
    auto& instr = proto_->instructions[instr_idx];
    int32_t offset = static_cast<int32_t>(loop_start) -
                     static_cast<int32_t>(instr_idx) - 1;
    instr.bx32 = offset;
}

OpCode Compiler::BinOpToOpcode(BinOp op) {
    switch (op) {
        case BinOp::Add: return OpCode::ADD;
        case BinOp::Sub: return OpCode::SUB;
        case BinOp::Mul: return OpCode::MUL;
        case BinOp::Div: return OpCode::DIV;
        case BinOp::IDiv: return OpCode::IDIV;
        case BinOp::Mod: return OpCode::MOD;
        case BinOp::Pow: return OpCode::POW;
        case BinOp::Eq:  return OpCode::EQ;
        case BinOp::Ne:  return OpCode::NE;
        case BinOp::Lt:  return OpCode::LT;
        case BinOp::Le:  return OpCode::LE;
        case BinOp::Gt:  return OpCode::GT;
        case BinOp::Ge:  return OpCode::GE;
        case BinOp::BAnd: return OpCode::BAND;
        case BinOp::BOr:  return OpCode::BOR;
        case BinOp::BXor: return OpCode::BXOR;
        case BinOp::Shl:  return OpCode::SHL;
        case BinOp::Shr:  return OpCode::SHR;
        default: break;
    }
    throw std::runtime_error("BinOpToOpcode: and/or not handled here");
}

bool Compiler::IsShortCircuit(BinOp op) {
    return op == BinOp::And || op == BinOp::Or;
}

int16_t Compiler::FindUpvalue(const std::string& name) {
    for (size_t upval_idx = 0; upval_idx < proto_->upvalue_descs.size(); ++upval_idx) {
        auto& uvd = proto_->upvalue_descs[upval_idx];
        for (auto& [pname, preg] : parent_locals_) {
            if (pname == name && preg == uvd.index) {
                return static_cast<int16_t>(upval_idx);
            }
        }
    }
    return -1;
}

static void RejectMemberModifiersOutsideClass(bool is_static, bool is_private, const char* kind,
                                               int line, int col, const std::string& source_name) {
    if (is_static || is_private) {
        std::string msg = std::string("'static'/'private' are only valid inside a class body (") +
                           kind + ")";
        if (line > 0) {
            msg += " (line " + std::to_string(line) + ")";
        }
        throw AvaError(msg, line, col, source_name);
    }
}

static void RejectDuplicateFuncDefs(const std::vector<std::shared_ptr<StmtNode>>& stmts,
                                     const std::string& source_name) {
    std::unordered_set<std::string> seen;
    for (auto& stmt : stmts) {
        auto* f = dynamic_cast<FuncDef*>(stmt.get());
        if (!f) continue;
        if (!seen.insert(f->name).second) {
            std::string msg = "function '" + f->name + "' is already defined";
            if (f->line > 0) {
                msg += " (line " + std::to_string(f->line) + ")";
            }
            throw AvaError(msg, f->line, f->col, source_name);
        }
    }
}

static TypeRef ResolveTypeNameAgainst(const std::string& name,
                                       const std::unordered_map<std::string, ClassObj*>& compiled_classes) {
    Type prim = TypeFromName(name);
    if (prim != Type::Unknown) return TypeRef{prim, "", nullptr, nullptr};
    if (compiled_classes.count(name)) return TypeRef{Type::Object, name, nullptr, nullptr};
    return TypeRef{Type::Unknown, "", nullptr, nullptr};
}

static void RecordIfNameTarget(const std::shared_ptr<ExprNode>& target,
                                std::unordered_set<std::string>& out_globals) {
    if (auto* n = dynamic_cast<NameExpr*>(target.get())) out_globals.insert(n->name);
}

static void CollectFuncSignatures(
        const std::vector<std::shared_ptr<StmtNode>>& stmts,
        std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>& out,
        std::unordered_map<std::string, TypeRef>& out_returns,
        const std::unordered_map<std::string, ClassObj*>& compiled_classes,
        std::unordered_set<std::string>& out_globals) {
    for (auto& stmt : stmts) {
        if (auto* f = dynamic_cast<FuncDef*>(stmt.get())) {
            std::vector<std::pair<std::string, Type>> sig;
            sig.reserve(f->params.size());
            for (size_t i = 0; i < f->params.size(); ++i) {
                Type t = (i < f->param_types.size()) ? TypeFromName(f->param_types[i]) : Type::Unknown;
                sig.push_back({f->params[i].first, t});
            }
            out[f->name] = std::move(sig);
            out_returns[f->name] = ResolveTypeNameAgainst(f->return_type, compiled_classes);
            continue;
        }

        if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
            RecordIfNameTarget(a->target, out_globals);
        } else if (auto* ma = dynamic_cast<MultiAssignStmt*>(stmt.get())) {
            for (auto& tgt : ma->targets) RecordIfNameTarget(tgt, out_globals);
        } else if (auto* aug = dynamic_cast<AugAssignStmt*>(stmt.get())) {
            RecordIfNameTarget(aug->target, out_globals);
        }
    }
}

static void CollectDynamicThisAttrs(const std::vector<std::shared_ptr<StmtNode>>& stmts,
                                     std::unordered_set<std::string>& out);

static void RecordIfThisAttrTarget(const std::shared_ptr<ExprNode>& target,
                                    std::unordered_set<std::string>& out) {
    auto* a = dynamic_cast<AttrExpr*>(target.get());
    if (!a) return;
    auto* obj_name = dynamic_cast<NameExpr*>(a->obj.get());
    if (obj_name && obj_name->name == "this") out.insert(a->attr);
}

static void WalkExprForLambdas(const std::shared_ptr<ExprNode>& expr,
                                std::unordered_set<std::string>& out) {
    if (!expr) return;

    if (auto* l = dynamic_cast<LambdaExpr*>(expr.get())) {
        CollectDynamicThisAttrs(l->body, out);
        for (auto& [pname, pdefault] : l->defaults) WalkExprForLambdas(pdefault, out);
        return;
    }
    if (auto* b = dynamic_cast<BinOpExpr*>(expr.get())) {
        WalkExprForLambdas(b->left, out);
        WalkExprForLambdas(b->right, out);
        return;
    }
    if (auto* u = dynamic_cast<UnOpExpr*>(expr.get())) {
        WalkExprForLambdas(u->operand, out);
        return;
    }
    if (auto* t = dynamic_cast<TernaryExpr*>(expr.get())) {
        WalkExprForLambdas(t->condition, out);
        WalkExprForLambdas(t->then_expr, out);
        WalkExprForLambdas(t->else_expr, out);
        return;
    }
    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        WalkExprForLambdas(c->callee, out);
        for (auto& arg : c->args) WalkExprForLambdas(arg, out);
        return;
    }
    if (auto* ba = dynamic_cast<BaseExpr*>(expr.get())) {
        for (auto& arg : ba->args) WalkExprForLambdas(arg, out);
        return;
    }
    if (auto* ix = dynamic_cast<IndexExpr*>(expr.get())) {
        WalkExprForLambdas(ix->obj, out);
        WalkExprForLambdas(ix->index, out);
        return;
    }
    if (auto* sl = dynamic_cast<SliceExpr*>(expr.get())) {
        WalkExprForLambdas(sl->obj, out);
        WalkExprForLambdas(sl->start, out);
        WalkExprForLambdas(sl->end, out);
        WalkExprForLambdas(sl->step, out);
        return;
    }
    if (auto* at = dynamic_cast<AttrExpr*>(expr.get())) {
        WalkExprForLambdas(at->obj, out);
        return;
    }
    if (auto* li = dynamic_cast<ListExpr*>(expr.get())) {
        for (auto& item : li->items) WalkExprForLambdas(item, out);
        return;
    }
    if (auto* di = dynamic_cast<DictExpr*>(expr.get())) {
        for (auto& [k, v] : di->entries) WalkExprForLambdas(v, out);
        return;
    }
    if (auto* y = dynamic_cast<YieldExpr*>(expr.get())) {
        for (auto& v : y->values) WalkExprForLambdas(v, out);
        return;
    }
    if (auto* aw = dynamic_cast<AwaitExpr*>(expr.get())) {
        WalkExprForLambdas(aw->value, out);
        return;
    }
}

static void CollectDynamicThisAttrs(const std::vector<std::shared_ptr<StmtNode>>& stmts,
                                     std::unordered_set<std::string>& out) {
    for (auto& stmt : stmts) {
        if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
            RecordIfThisAttrTarget(a->target, out);
            WalkExprForLambdas(a->value, out);
        } else if (auto* aug = dynamic_cast<AugAssignStmt*>(stmt.get())) {
            RecordIfThisAttrTarget(aug->target, out);
            WalkExprForLambdas(aug->value, out);
        } else if (auto* ma = dynamic_cast<MultiAssignStmt*>(stmt.get())) {
            for (auto& tgt : ma->targets) RecordIfThisAttrTarget(tgt, out);
            for (auto& v : ma->values) WalkExprForLambdas(v, out);
        } else if (auto* es = dynamic_cast<ExprStmt*>(stmt.get())) {
            WalkExprForLambdas(es->expr, out);
        } else if (auto* rs = dynamic_cast<ReturnStmt*>(stmt.get())) {
            WalkExprForLambdas(rs->value, out);
        } else if (auto* rz = dynamic_cast<RaiseStmt*>(stmt.get())) {
            WalkExprForLambdas(rz->value, out);
        } else if (auto* id = dynamic_cast<IncDecStmt*>(stmt.get())) {
            WalkExprForLambdas(id->target, out);
        } else if (auto* iff = dynamic_cast<IfStmt*>(stmt.get())) {
            WalkExprForLambdas(iff->condition, out);
            CollectDynamicThisAttrs(iff->then_body, out);
            for (auto& [cond, body] : iff->elif_clauses) {
                WalkExprForLambdas(cond, out);
                CollectDynamicThisAttrs(body, out);
            }
            CollectDynamicThisAttrs(iff->else_body, out);
        } else if (auto* w = dynamic_cast<WhileStmt*>(stmt.get())) {
            WalkExprForLambdas(w->condition, out);
            CollectDynamicThisAttrs(w->body, out);
        } else if (auto* f = dynamic_cast<ForStmt*>(stmt.get())) {
            WalkExprForLambdas(f->iterable, out);
            CollectDynamicThisAttrs(f->body, out);
        } else if (auto* fr = dynamic_cast<ForRangeStmt*>(stmt.get())) {
            WalkExprForLambdas(fr->start, out);
            WalkExprForLambdas(fr->stop, out);
            WalkExprForLambdas(fr->step, out);
            CollectDynamicThisAttrs(fr->body, out);
        } else if (auto* ts = dynamic_cast<TryStmt*>(stmt.get())) {
            CollectDynamicThisAttrs(ts->try_body, out);
            for (auto& eb : ts->except_bodies) CollectDynamicThisAttrs(eb, out);
            for (auto& ee : ts->except_exprs) WalkExprForLambdas(ee, out);
            CollectDynamicThisAttrs(ts->finally_body, out);
        }
    }
}

static std::string DisplayType(const TypeRef& t) {
    if (t.type == Type::Object) return "class " + t.class_name;
    if (t.type == Type::List) {
        return t.element_type ? ("list of " + DisplayType(*t.element_type)) : "list";
    }
    if (t.type == Type::Dict) {
        if (!t.key_type && !t.element_type) return "dict";
        std::string key = t.key_type ? DisplayType(*t.key_type) : "unknown";
        std::string val = t.element_type ? DisplayType(*t.element_type) : "unknown";
        return "dict of " + key + " to " + val;
    }
    return std::string(TypeName(t.type));
}

static void ValidateTypeAnnotation(const std::string& explicit_type, const TypeRef& declared_type,
                                    const TypeRef& inferred_type, int line, int col,
                                    const std::string& source_name) {
    if (explicit_type.empty()) return;
    if (declared_type.type == Type::Unknown) {
        throw AvaError("unknown type '" + explicit_type + "' in annotation", line, col, source_name);
    }
    if (inferred_type.type == Type::Unknown) return;

    if (!TypeRefEquals(declared_type, inferred_type)) {
        std::string msg = "type mismatch: expected " + DisplayType(declared_type) +
                           ", received " + DisplayType(inferred_type);
        throw AvaError(msg, line, col, source_name);
    }
}

static void ValidateReassignment(const Symbol* existing, const TypeRef& inferred_type,
                                  int line, int col, const std::string& source_name) {
    if (!existing || existing->effectiveType == Type::Unknown) return;
    if (inferred_type.type == Type::Unknown) return;

    TypeRef existing_ref{existing->effectiveType, existing->effectiveClassName,
                          existing->effectiveElementType, existing->effectiveKeyType};
    if (!TypeRefEquals(existing_ref, inferred_type)) {
        std::string msg = "type mismatch: expected " + DisplayType(existing_ref) +
                           ", received " + DisplayType(inferred_type);
        throw AvaError(msg, line, col, source_name);
    }
}

static const char* BinOpSymbol(BinOp op) {
    switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Sub: return "-";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::IDiv: return "//";
        case BinOp::Mod: return "%";
        case BinOp::Pow: return "**";
        case BinOp::Eq:  return "==";
        case BinOp::Ne:  return "!=";
        case BinOp::Lt:  return "<";
        case BinOp::Le:  return "<=";
        case BinOp::Gt:  return ">";
        case BinOp::Ge:  return ">=";
        case BinOp::And: return "and";
        case BinOp::Or:  return "or";
        case BinOp::BAnd: return "&";
        case BinOp::BOr:  return "|";
        case BinOp::BXor: return "^";
        case BinOp::Shl:  return "<<";
        case BinOp::Shr:  return ">>";
    }
    return "?";
}

static const char* UnOpSymbol(UnOp op) {
    switch (op) {
        case UnOp::Neg: return "-";
        case UnOp::Not: return "not";
        case UnOp::Inc: return "++";
        case UnOp::Dec: return "--";
        case UnOp::BNot: return "~";
    }
    return "?";
}

static void ValidateBinOpTypes(BinOp op, Type lt, Type rt, int line, int col,
                                const std::string& source_name) {
    if (lt == Type::Unknown || rt == Type::Unknown) return;

    auto ThrowBoolMismatch = [&]() {
        std::string msg = std::string("operator type mismatch: '") + BinOpSymbol(op) +
                           "' does not support bool operands, received " +
                           TypeName(lt) + " and " + TypeName(rt);
        throw AvaError(msg, line, col, source_name);
    };

    switch (op) {
        case BinOp::Add:
            if (lt == Type::String || rt == Type::String) return;
            if (lt == Type::Bool || rt == Type::Bool) ThrowBoolMismatch();
            return;
        case BinOp::Sub:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::IDiv:
        case BinOp::Mod:
        case BinOp::Pow:
        case BinOp::Lt:
        case BinOp::Le:
        case BinOp::Gt:
        case BinOp::Ge:
            if (lt == Type::Bool || rt == Type::Bool) ThrowBoolMismatch();
            return;
        case BinOp::Eq:
        case BinOp::Ne:
        case BinOp::And:
        case BinOp::Or:
            return;
        case BinOp::BAnd:
        case BinOp::BOr:
        case BinOp::BXor:
        case BinOp::Shl:
        case BinOp::Shr:
            if ((lt != Type::Int && lt != Type::Float) ||
                (rt != Type::Int && rt != Type::Float)) {
                std::string msg = std::string("operator type mismatch: '") + BinOpSymbol(op) +
                                   "' requires number operands, received " +
                                   TypeName(lt) + " and " + TypeName(rt);
                throw AvaError(msg, line, col, source_name);
            }
            return;
    }
}

static void ValidateUnOpTypes(UnOp op, Type operand_type, int line, int col,
                               const std::string& source_name) {
    if (operand_type == Type::Unknown) return;
    switch (op) {
        case UnOp::Neg:
        case UnOp::Inc:
        case UnOp::Dec:
            if (operand_type == Type::Bool) {
                std::string msg = std::string("operator type mismatch: '") + UnOpSymbol(op) +
                                   "' does not support a bool operand, received bool";
                throw AvaError(msg, line, col, source_name);
            }
            return;
        case UnOp::Not:
            return;
        case UnOp::BNot:
            if (operand_type != Type::Int && operand_type != Type::Float) {
                std::string msg = std::string("operator type mismatch: '") + UnOpSymbol(op) +
                                   "' requires a number operand, received " +
                                   TypeName(operand_type);
                throw AvaError(msg, line, col, source_name);
            }
            return;
    }
}

uint16_t Compiler::CompileExpr(const std::shared_ptr<ExprNode>& expr) {
    if (auto* n = dynamic_cast<NumberExpr*>(expr.get())) {
        auto reg = AllocReg();
        auto idx = AddConstant(Value::Number(n->value));
        Emit(OpCode::LOADK, reg, idx);
        return reg;
    }

    if (auto* s = dynamic_cast<StringExpr*>(expr.get())) {
        auto reg = AllocReg();
        auto idx = AddConstant(MakeString(s->value));
        Emit(OpCode::LOADK, reg, idx);
        return reg;
    }

    if (auto* f = dynamic_cast<FStringExpr*>(expr.get())) {
        std::vector<uint16_t> result_regs;

        for (auto& [is_expr, content] : f->fragments) {
            if (!is_expr) {
                auto reg = AllocReg();
                auto idx = AddConstant(MakeString(content));
                Emit(OpCode::LOADK, reg, idx);
                result_regs.push_back(reg);
            } else {
                auto expr_reg = CompileFStringExpression(content);
                result_regs.push_back(expr_reg);
            }
        }

        if (result_regs.empty()) {
            auto reg = AllocReg();
            auto idx = AddConstant(MakeString(""));
            Emit(OpCode::LOADK, reg, idx);
            return reg;
        }

        if (result_regs.size() == 1) {
            return result_regs[0];
        }

        auto result_reg = AllocReg();
        auto str_func_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, str_func_reg, AddConstant(MakeString("str")));
        FreeRegs(1);

        uint16_t first = result_regs[0];
        for (size_t i = 1; i < result_regs.size(); ++i) {
            auto temp_reg = AllocReg();
            Emit(OpCode::ADD, temp_reg, first, result_regs[i]);
            first = temp_reg;
        }

        return first;
    }

    if (auto* b = dynamic_cast<BoolExpr*>(expr.get())) {
        auto reg = AllocReg();
        Emit(OpCode::LOADBOOL, reg, b->value ? 1 : 0);
        return reg;
    }

    if (dynamic_cast<NilExpr*>(expr.get())) {
        auto reg = AllocReg();
        Emit(OpCode::LOADNIL, reg);
        return reg;
    }

    if (auto* n = dynamic_cast<NameExpr*>(expr.get())) {
        auto it = locals_.find(n->name);
        if (it != locals_.end()) {
            return it->second;
        }
        if (n->name == "this") {
            if (locals_.find("this") != locals_.end()) {
                return locals_.at("this");
            }
        }

        for (size_t upval_idx = 0; upval_idx < proto_->upvalue_descs.size(); ++upval_idx) {
            auto& uvd = proto_->upvalue_descs[upval_idx];
            for (auto& [pname, preg] : parent_locals_) {
                if (pname == n->name && preg == uvd.index) {
                    auto reg = AllocReg();
                    Emit(OpCode::GETUPVAL, reg, static_cast<uint16_t>(upval_idx));
                    return reg;
                }
            }
        }

        bool in_method = locals_.find("this") != locals_.end();
        bool has_local = locals_.find(n->name) != locals_.end();
        bool is_known_attr = instance_attrs_.find(n->name) != instance_attrs_.end();
        if (in_method && !has_local && is_known_attr) {
            auto reg = AllocReg();
            auto attr_idx = AddConstant(MakeString(n->name));
            auto this_reg = locals_.at("this");
            Emit(OpCode::GETATTR, reg, this_reg, attr_idx);
            return reg;
        }
        auto reg = AllocReg();
        auto idx = AddConstant(MakeString(n->name));
        Emit(OpCode::GETGLOBAL, reg, idx);
        return reg;
    }

    if (auto* b = dynamic_cast<BinOpExpr*>(expr.get())) {
        CheckBinOpTypes(b);
        if (IsShortCircuit(b->op)) {
            auto zero_idx = AddConstant(Value::Number(0));
            if (b->op == BinOp::And) {
                auto left_reg = CompileExpr(b->left);
                auto result_reg = AllocReg();
                Emit(OpCode::NEK, result_reg, left_reg, zero_idx);
                Emit(OpCode::TEST, result_reg, 0);
                size_t jmp_falsy = proto_->instructions.size();
                Emit(OpCode::JMP, 0);
                auto right_reg = CompileExpr(b->right);
                Emit(OpCode::MOVE, result_reg, right_reg);
                size_t jmp_end = proto_->instructions.size();
                Emit(OpCode::JMP, 0);
                PatchJump(jmp_falsy);
                Emit(OpCode::MOVE, result_reg, left_reg);
                PatchJump(jmp_end);
                return result_reg;
            } else {
                auto left_reg = CompileExpr(b->left);
                auto result_reg = AllocReg();
                Emit(OpCode::EQK, result_reg, left_reg, zero_idx);
                Emit(OpCode::TEST, result_reg, 0);
                size_t jmp_truthy = proto_->instructions.size();
                Emit(OpCode::JMP, 0);
                auto right_reg = CompileExpr(b->right);
                Emit(OpCode::MOVE, result_reg, right_reg);
                size_t jmp_end = proto_->instructions.size();
                Emit(OpCode::JMP, 0);
                PatchJump(jmp_truthy);
                Emit(OpCode::MOVE, result_reg, left_reg);
                PatchJump(jmp_end);
                return result_reg;
            }
        }

        auto left_reg = CompileExpr(b->left);
        auto right_reg = CompileExpr(b->right);
        auto result_reg = AllocReg();
        Emit(BinOpToOpcode(b->op), result_reg, left_reg, right_reg);
        return result_reg;
    }

    if (auto* u = dynamic_cast<UnOpExpr*>(expr.get())) {
        CheckUnOpTypes(u);
        auto reg = CompileExpr(u->operand);
        if (u->op == UnOp::Neg) {
            auto result = AllocReg();
            Emit(OpCode::NEG, result, reg);
            return result;
        }
        if (u->op == UnOp::Inc) {
            if (auto* n = dynamic_cast<NameExpr*>(u->operand.get())) {
                auto original = AllocReg();
                Emit(OpCode::MOVE, original, reg);
                auto result = AllocReg();
                Emit(OpCode::INC, result, reg);

                if (!is_top_level_ && (n->name != "this") && locals_.find(n->name) != locals_.end()) {
                    Emit(OpCode::MOVE, locals_.at(n->name), result);
                } else {
                    auto idx = AddConstant(MakeString(n->name));
                    Emit(OpCode::SETGLOBAL, result, idx);
                }
                FreeRegs(1);
                return original;
            }
            auto result = AllocReg();
            Emit(OpCode::INC, result, reg);
            return result;
        }
        if (u->op == UnOp::Dec) {
            if (auto* n = dynamic_cast<NameExpr*>(u->operand.get())) {
                auto original = AllocReg();
                Emit(OpCode::MOVE, original, reg);
                auto result = AllocReg();
                Emit(OpCode::DEC, result, reg);
                if (!is_top_level_ && (n->name != "this") && locals_.find(n->name) != locals_.end()) {
                    Emit(OpCode::MOVE, locals_.at(n->name), result);
                } else {
                    auto idx = AddConstant(MakeString(n->name));
                    Emit(OpCode::SETGLOBAL, result, idx);
                }
                FreeRegs(1);
                return original;
            }
            auto result = AllocReg();
            Emit(OpCode::DEC, result, reg);
            return result;
        }
        if (u->op == UnOp::BNot) {
            auto result = AllocReg();
            Emit(OpCode::BNOT, result, reg);
            return result;
        }
        auto result = AllocReg();
        Emit(OpCode::NOT, result, reg);
        return result;
    }

    if (auto* t = dynamic_cast<TernaryExpr*>(expr.get())) {
        uint16_t regs_before = next_reg_;
        auto cond_reg = CompileExpr(t->condition);
        Emit(OpCode::TEST, cond_reg, 1);
        size_t jmp_to_else = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        FreeRegs(next_reg_ - regs_before);

        auto result_reg = AllocReg();

        auto then_reg = CompileExpr(t->then_expr);
        Emit(OpCode::MOVE, result_reg, then_reg);
        FreeRegs(next_reg_ - (result_reg + 1));

        size_t jmp_end = proto_->instructions.size();
        Emit(OpCode::JMP, 0);

        PatchJump(jmp_to_else);

        auto else_reg = CompileExpr(t->else_expr);
        Emit(OpCode::MOVE, result_reg, else_reg);
        FreeRegs(next_reg_ - (result_reg + 1));

        PatchJump(jmp_end);

        return result_reg;
    }

    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        if (auto* callee_name = dynamic_cast<NameExpr*>(c->callee.get())) {
            CheckCallArgs(callee_name->name, c);
        } else if (auto* callee_attr = dynamic_cast<AttrExpr*>(c->callee.get())) {
            CheckMethodCallArgs(callee_attr, c);
        }
        auto callee_val_reg = CompileExpr(c->callee);
        auto callee_reg = AllocReg();
        Emit(OpCode::MOVE, callee_reg, callee_val_reg);
        uint8_t argc = static_cast<uint8_t>(c->args.size());

        uint16_t call_frame_end = callee_reg + 1 + argc;
        if (next_reg_ < call_frame_end) {
            next_reg_ = call_frame_end;
            if (next_reg_ > max_reg_) max_reg_ = next_reg_;
        }
        for (size_t i = 0; i < c->args.size(); ++i) {
            uint16_t regs_before_arg = next_reg_;
            auto arg_reg = CompileExpr(c->args[i]);
            Emit(OpCode::MOVE, static_cast<uint16_t>(callee_reg + 1 + i), arg_reg);
            FreeRegs(next_reg_ - regs_before_arg);
        }
        Emit(OpCode::CALL, callee_reg, argc, 1);
        return callee_reg;
    }

    if (auto* l = dynamic_cast<ListExpr*>(expr.get())) {
        auto reg = AllocReg();
        Emit(OpCode::NEWLIST, reg);
        for (auto& item : l->items) {
            auto item_reg = CompileExpr(item);
            Emit(OpCode::LISTAPPEND, reg, item_reg);
        }
        return reg;
    }

    if (auto* d = dynamic_cast<DictExpr*>(expr.get())) {
        auto reg = AllocReg();
        Emit(OpCode::NEWDICT, reg);
        for (auto& [key, val] : d->entries) {
            uint16_t regs_before = next_reg_;
            auto val_reg = CompileExpr(val);
            auto key_idx = AddConstant(MakeString(key));
            auto saved_idx = AllocReg();
            Emit(OpCode::LOADK, saved_idx, key_idx);
            Emit(OpCode::SETINDEX, reg, saved_idx, val_reg);
            FreeRegs(next_reg_ - (reg + 1));
        }
        return reg;
    }

    if (auto* i = dynamic_cast<IndexExpr*>(expr.get())) {
        auto obj_reg = CompileExpr(i->obj);
        auto idx_reg = CompileExpr(i->index);
        auto result_reg = AllocReg();
        Emit(OpCode::GETINDEX, result_reg, obj_reg, idx_reg);
        return result_reg;
    }

    if (auto* s = dynamic_cast<SliceExpr*>(expr.get())) {
        auto func_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, func_reg, AddConstant(MakeString("slice")));

        auto obj_slot = AllocReg();
        auto start_slot = AllocReg();
        auto end_slot = AllocReg();
        auto step_slot = AllocReg();

        auto obj_reg = CompileExpr(s->obj);
        Emit(OpCode::MOVE, obj_slot, obj_reg);

        if (s->start) {
            auto start_reg = CompileExpr(s->start);
            Emit(OpCode::MOVE, start_slot, start_reg);
        } else {
            Emit(OpCode::LOADNIL, start_slot);
        }

        if (s->end) {
            auto end_reg = CompileExpr(s->end);
            Emit(OpCode::MOVE, end_slot, end_reg);
        } else {
            Emit(OpCode::LOADNIL, end_slot);
        }

        if (s->step) {
            auto step_reg = CompileExpr(s->step);
            Emit(OpCode::MOVE, step_slot, step_reg);
        } else {
            Emit(OpCode::LOADNIL, step_slot);
        }

        Emit(OpCode::CALL, func_reg, 4, 1);

        uint16_t min_next = func_reg + 5;
        if (next_reg_ < min_next) next_reg_ = min_next;

        return func_reg;
    }

    if (auto* a = dynamic_cast<AttrExpr*>(expr.get())) {
        auto obj_reg = CompileExpr(a->obj);
        auto attr_idx = AddConstant(MakeString(a->attr));
        auto result_reg = AllocReg();
        Emit(OpCode::GETATTR, result_reg, obj_reg, attr_idx);
        return result_reg;
    }

    if (auto* s = dynamic_cast<BaseExpr*>(expr.get())) {
        if (locals_.find("this") == locals_.end() && FindUpvalue("this") < 0) {
            throw AvaError("base() can only be used inside a method", current_line_, current_col_, source_name_);
        }
        if (!current_base_class_) {
            throw AvaError("base() can only be used inside a class that extends another class "
                            "(this class has no ': ParentClass')",
                            current_line_, current_col_, source_name_);
        }

        auto method_idx = AddConstant(MakeString(s->method_name));
        uint8_t argc = static_cast<uint8_t>(s->args.size());

        auto base_reg = AllocReg();
        if (locals_.find("this") != locals_.end()) {
            Emit(OpCode::MOVE, base_reg, 0);
        } else {
            int16_t this_upval = FindUpvalue("this");
            Emit(OpCode::GETUPVAL, base_reg, static_cast<uint16_t>(this_upval));
        }

        uint16_t call_frame_end = base_reg + 1 + argc;
        if (next_reg_ < call_frame_end) {
            next_reg_ = call_frame_end;
            if (next_reg_ > max_reg_) max_reg_ = next_reg_;
        }
        for (size_t i = 0; i < s->args.size(); ++i) {
            uint16_t regs_before_arg = next_reg_;
            auto arg_reg = CompileExpr(s->args[i]);
            Emit(OpCode::MOVE, static_cast<uint16_t>(base_reg + 1 + i), arg_reg);
            FreeRegs(next_reg_ - regs_before_arg);
        }
        Emit(OpCode::BASECALL, base_reg, method_idx, argc);
        return base_reg;
    }

    if (auto* l = dynamic_cast<LambdaExpr*>(expr.get())) {
        Compiler sub;
        sub.is_top_level_ = false;
        sub.proto_ = std::make_shared<Proto>();
        sub.source_name_ = source_name_;
        sub.proto_->source_name = source_name_;
        sub.proto_->debug_name = l->name;
        sub.proto_->num_params = static_cast<uint8_t>(l->defaults.size());
        sub.proto_->is_vararg = l->is_vararg;
        sub.compiled_classes_ = compiled_classes_;
        sub.class_field_types_ = class_field_types_;
        sub.class_dynamic_attrs_ = class_dynamic_attrs_;
        sub.class_method_returns_ = class_method_returns_;
        sub.class_method_params_ = class_method_params_;
        sub.known_funcs_ = known_funcs_;
        sub.known_func_returns_ = known_func_returns_;
        sub.known_top_level_globals_ = known_top_level_globals_;
        sub.extern_func_params_ = extern_func_params_;
        sub.extern_func_returns_ = extern_func_returns_;
        sub.has_wildcard_import_ = has_wildcard_import_;
        sub.current_base_class_ = current_base_class_;

        sub.next_reg_ = 1;
        sub.max_reg_ = sub.next_reg_;
        for (auto& [pname, def] : l->defaults) {
            sub.locals_[pname] = sub.next_reg_;
            sub.next_reg_++;
        }

        for (auto& [name, reg] : locals_) {
            sub.parent_locals_.push_back({name, reg});
            sub.proto_->upvalue_descs.push_back({true, reg});
            sub.next_reg_++;
        }

        for (size_t i = 0; i < parent_locals_.size(); ++i) {
            auto& pname = parent_locals_[i].first;
            if (locals_.count(pname)) continue;  // el local propio del padre ya gana (loop de arriba)
            sub.parent_locals_.push_back({pname, static_cast<uint16_t>(i)});
            sub.proto_->upvalue_descs.push_back({false, static_cast<uint16_t>(i)});
            sub.next_reg_++;
        }

        sub.EmitDefaultsPrologue(l->defaults, 1);

        sub.current_return_type_ = ResolveTypeName(l->return_type);

        sub.CompileChunk(l->body);
        uint8_t ret_a = sub.result_reg_ > 0 ? static_cast<uint8_t>(sub.result_reg_) : 0;
        uint8_t ret_b = sub.result_reg_ > 0 ? 1 : 0;
        sub.Emit(OpCode::RETURN, ret_a, ret_b);
        sub.proto_->num_registers = std::max<uint16_t>(sub.max_reg_ + 1, sub.next_reg_);

        uint16_t child_idx = static_cast<uint16_t>(proto_->child_protos.size());
        proto_->child_protos.push_back(sub.proto_);

        auto reg = AllocReg();
        Emit(OpCode::CLOSURE, reg, child_idx);
        return reg;
    }

    if (auto* y = dynamic_cast<YieldExpr*>(expr.get())) {
        auto base_reg = AllocReg();
        uint8_t count = static_cast<uint8_t>(y->values.size());

        uint16_t yield_frame_end = base_reg + count;
        if (next_reg_ < yield_frame_end) {
            next_reg_ = yield_frame_end;
            if (next_reg_ > max_reg_) max_reg_ = next_reg_;
        }
        for (size_t i = 0; i < y->values.size(); ++i) {
            uint16_t regs_before = next_reg_;
            auto val_reg = CompileExpr(y->values[i]);
            Emit(OpCode::MOVE, static_cast<uint16_t>(base_reg + i), val_reg);
            FreeRegs(next_reg_ - regs_before);
        }
        Emit(OpCode::YIELD, base_reg, count);
        return base_reg;
    }

    if (auto* aw = dynamic_cast<AwaitExpr*>(expr.get())) {
        if (!in_async_func_) {
            std::string msg = "'await' can only be used inside an 'async func'";
            if (current_line_ > 0) {
                msg += " (line " + std::to_string(current_line_) + ")";
            }
            throw AvaError(msg, current_line_, current_col_, source_name_);
        }

        auto base_reg = AllocReg();
        uint16_t regs_before = next_reg_;
        auto val_reg = CompileExpr(aw->value);
        Emit(OpCode::MOVE, base_reg, val_reg);
        FreeRegs(next_reg_ - regs_before);
        Emit(OpCode::AWAIT, base_reg, 0);
        return base_reg;
    }

    throw std::runtime_error("unknown expr type in compiler");
}

void Compiler::StampLine(const std::shared_ptr<StmtNode>& stmt) {
    if (stmt->line > 0) { current_line_ = stmt->line; current_col_ = stmt->col; }
}

void Compiler::DeclareSymbol(const std::string& name, const TypeRef& declared, const TypeRef& inferred) {
    auto it = symbols_.find(name);
    if (it == symbols_.end()) {
        Symbol sym;
        sym.name = name;
        sym.declaredType = declared.type;
        sym.inferredType = inferred.type;
        sym.declaredClassName = declared.class_name;
        sym.inferredClassName = inferred.class_name;
        sym.declaredElementType = declared.element_type;
        sym.inferredElementType = inferred.element_type;
        sym.declaredKeyType = declared.key_type;
        sym.inferredKeyType = inferred.key_type;
        sym.RefreshEffectiveType();
        symbols_[name] = sym;
        return;
    }
    if (declared.type != Type::Unknown) {
        it->second.declaredType = declared.type;
        it->second.declaredClassName = declared.class_name;
        it->second.declaredElementType = declared.element_type;
        it->second.declaredKeyType = declared.key_type;
    }
    if (inferred.type != Type::Unknown) {
        it->second.inferredType = inferred.type;
        it->second.inferredClassName = inferred.class_name;
        it->second.inferredElementType = inferred.element_type;
        it->second.inferredKeyType = inferred.key_type;
    }
    it->second.RefreshEffectiveType();
}

Type Compiler::InferExprType(const std::shared_ptr<ExprNode>& expr) {
    if (!expr) return Type::Unknown;

    if (auto* num = dynamic_cast<NumberExpr*>(expr.get())) {
        return num->is_float ? Type::Float : Type::Int;
    }
    if (dynamic_cast<StringExpr*>(expr.get()) || dynamic_cast<FStringExpr*>(expr.get())) {
        return Type::String;
    }
    if (dynamic_cast<BoolExpr*>(expr.get())) {
        return Type::Bool;
    }
    if (auto* n = dynamic_cast<NameExpr*>(expr.get())) {
        auto it = symbols_.find(n->name);
        return it != symbols_.end() ? it->second.effectiveType : Type::Unknown;
    }
    if (auto* u = dynamic_cast<UnOpExpr*>(expr.get())) {
        switch (u->op) {
            case UnOp::Not:
                return Type::Bool;
            case UnOp::Neg:
            case UnOp::Inc:
            case UnOp::Dec:
                return InferExprType(u->operand);
            case UnOp::BNot:
                return Type::Int;
        }
        return Type::Unknown;
    }
    if (auto* b = dynamic_cast<BinOpExpr*>(expr.get())) {
        switch (b->op) {
            case BinOp::Eq: case BinOp::Ne:
            case BinOp::Lt: case BinOp::Le:
            case BinOp::Gt: case BinOp::Ge:
            case BinOp::And: case BinOp::Or:
                return Type::Bool;
            case BinOp::BAnd: case BinOp::BOr: case BinOp::BXor:
            case BinOp::Shl: case BinOp::Shr:
                return Type::Int;
            case BinOp::Add: {
                Type lt = InferExprType(b->left);
                Type rt = InferExprType(b->right);
                if (lt == Type::String || rt == Type::String) return Type::String;
                if (lt == Type::Unknown || rt == Type::Unknown) return Type::Unknown;
                if (lt == rt) return lt;
                if ((lt == Type::Int && rt == Type::Float) || (lt == Type::Float && rt == Type::Int)) {
                    return Type::Float;
                }
                return Type::Unknown;
            }
            default:
                break;
        }
        Type lt = InferExprType(b->left);
        Type rt = InferExprType(b->right);
        if (lt == Type::Unknown || rt == Type::Unknown) return Type::Unknown;
        if (lt == rt) return lt;
        if ((lt == Type::Int && rt == Type::Float) || (lt == Type::Float && rt == Type::Int)) {
            return Type::Float;
        }

        return Type::Unknown;
    }

    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        if (auto* callee_name = dynamic_cast<NameExpr*>(c->callee.get())) {
            auto it = known_func_returns_.find(callee_name->name);
            if (it != known_func_returns_.end()) return it->second.type;
            if (compiled_classes_.count(callee_name->name)) return Type::Object;
        }
        return Type::Unknown;
    }

    if (dynamic_cast<ListExpr*>(expr.get())) return Type::List;
    if (dynamic_cast<DictExpr*>(expr.get())) return Type::Dict;
    if (dynamic_cast<IndexExpr*>(expr.get())) return InferExprTypeRef(expr).type;
    if (dynamic_cast<SliceExpr*>(expr.get())) return InferExprTypeRef(expr).type;

    return Type::Unknown;
}

TypeRef Compiler::ResolveTypeName(const std::string& name) {
    return ResolveTypeNameAgainst(name, compiled_classes_);
}

TypeRef Compiler::InferExprTypeRef(const std::shared_ptr<ExprNode>& expr) {
    if (!expr) return TypeRef{};

    if (auto* n = dynamic_cast<NameExpr*>(expr.get())) {

        auto it = symbols_.find(n->name);
        if (it == symbols_.end()) return TypeRef{};
        return TypeRef{it->second.effectiveType, it->second.effectiveClassName, nullptr, nullptr};
    }

    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        if (auto* callee_name = dynamic_cast<NameExpr*>(c->callee.get())) {
            auto cit = compiled_classes_.find(callee_name->name);
            if (cit != compiled_classes_.end()) {
                return TypeRef{Type::Object, callee_name->name, nullptr, nullptr};
            }
            auto rit = known_func_returns_.find(callee_name->name);
            if (rit != known_func_returns_.end()) return rit->second;
            return TypeRef{};
        }
        if (auto* callee_attr = dynamic_cast<AttrExpr*>(c->callee.get())) {

            if (auto* obj_name = dynamic_cast<NameExpr*>(callee_attr->obj.get())) {
                auto eit = extern_func_returns_.find(obj_name->name);
                if (eit != extern_func_returns_.end()) {
                    auto fit = eit->second.find(callee_attr->attr);
                    if (fit != eit->second.end()) return fit->second;
                    return TypeRef{};
                }
            }
            TypeRef obj_type = InferExprTypeRef(callee_attr->obj);
            if (obj_type.type != Type::Object || obj_type.class_name.empty()) return TypeRef{};
            auto cit = class_method_returns_.find(obj_type.class_name);
            if (cit == class_method_returns_.end()) return TypeRef{};
            auto mit = cit->second.find(callee_attr->attr);
            if (mit == cit->second.end()) return TypeRef{};
            return mit->second;
        }
        return TypeRef{};
    }

    if (auto* a = dynamic_cast<AttrExpr*>(expr.get())) {
        TypeRef obj_type = InferExprTypeRef(a->obj);
        if (obj_type.type != Type::Object || obj_type.class_name.empty()) return TypeRef{};
        auto cit = class_field_types_.find(obj_type.class_name);
        if (cit == class_field_types_.end()) return TypeRef{};
        auto fit = cit->second.find(a->attr);
        if (fit == cit->second.end()) return TypeRef{};
        return fit->second;
    }

    if (auto* le = dynamic_cast<ListExpr*>(expr.get())) {
        TypeRef result;
        result.type = Type::List;
        if (!le->items.empty()) {
            TypeRef first = InferExprTypeRef(le->items[0]);
            bool uniform = first.type != Type::Unknown;
            for (size_t idx = 1; uniform && idx < le->items.size(); ++idx) {
                if (!TypeRefEquals(first, InferExprTypeRef(le->items[idx]))) uniform = false;
            }
            if (uniform) result.element_type = std::make_shared<TypeRef>(first);
        }
        return result;
    }
    if (auto* de = dynamic_cast<DictExpr*>(expr.get())) {
        TypeRef result;
        result.type = Type::Dict;
        if (!de->entries.empty()) {
            result.key_type = std::make_shared<TypeRef>(TypeRef{Type::String, "", nullptr, nullptr});
            TypeRef first = InferExprTypeRef(de->entries[0].second);
            bool uniform = first.type != Type::Unknown;
            for (size_t idx = 1; uniform && idx < de->entries.size(); ++idx) {
                if (!TypeRefEquals(first, InferExprTypeRef(de->entries[idx].second))) uniform = false;
            }
            if (uniform) result.element_type = std::make_shared<TypeRef>(first);
        }
        return result;
    }

    if (auto* idx = dynamic_cast<IndexExpr*>(expr.get())) {
        TypeRef obj_type = InferExprTypeRef(idx->obj);
        if ((obj_type.type == Type::List || obj_type.type == Type::Dict) && obj_type.element_type) {
            return *obj_type.element_type;
        }
        return TypeRef{};
    }

    if (auto* sl = dynamic_cast<SliceExpr*>(expr.get())) {
        TypeRef obj_type = InferExprTypeRef(sl->obj);
        if (obj_type.type == Type::List) return obj_type;
        return TypeRef{};
    }

    return TypeRef{InferExprType(expr), "", nullptr, nullptr};
}

void Compiler::CheckCallArgsAgainst(const std::vector<std::pair<std::string, Type>>& params,
                                     const std::string& label, const CallExpr* c) {
    size_t n = std::min(params.size(), c->args.size());
    for (size_t i = 0; i < n; ++i) {
        Type expected = params[i].second;
        if (expected == Type::Unknown) continue;
        Type actual = InferExprType(c->args[i]);
        if (actual == Type::Unknown) continue;
        if (actual != expected) {
            std::string msg = "argument type mismatch: " + label + " parameter '" +
                               params[i].first + "' expects " + TypeName(expected) +
                               ", received " + TypeName(actual);
            throw AvaError(msg, current_line_, current_col_, source_name_);
        }
    }
}

static bool IsBuiltinGlobal(const std::string& name) {
    static const std::unordered_set<std::string> kBuiltinGlobals = {
#define AVA_BUILTIN_NAME_ONLY(bname, fn) #bname,
        AVA_BUILTIN_GLOBALS(AVA_BUILTIN_NAME_ONLY)
#undef AVA_BUILTIN_NAME_ONLY
    };
    return kBuiltinGlobals.count(name) != 0;
}

bool Compiler::NameShadowsGlobalCallable(const std::string& name) const {
    if (locals_.count(name)) return true;
    if (name == "this" && locals_.count("this")) return true;

    for (size_t upval_idx = 0; upval_idx < proto_->upvalue_descs.size(); ++upval_idx) {
        auto& uvd = proto_->upvalue_descs[upval_idx];
        for (auto& [pname, preg] : parent_locals_) {
            if (pname == name && preg == uvd.index) return true;
        }
    }
    bool in_method = locals_.count("this") != 0;
    if (in_method && instance_attrs_.count(name)) return true;

    if (known_top_level_globals_.count(name)) return true;
    return false;
}

void Compiler::CheckCallArgs(const std::string& func_name, const CallExpr* c) {
    auto it = known_funcs_.find(func_name);
    if (it != known_funcs_.end()) {
        CheckCallArgsAgainst(it->second, "'" + func_name + "'", c);
        return;
    }

    if (!compiled_classes_.count(func_name)) {

        if (IsBuiltinGlobal(func_name)) return;

        if (NameShadowsGlobalCallable(func_name)) return;

        if (has_wildcard_import_) return;
        throw AvaError("function '" + func_name + "' is not defined",
                        current_line_, current_col_, source_name_);
    }
    auto cit = class_method_params_.find(func_name);
    if (cit == class_method_params_.end()) return;
    auto init_it = cit->second.find("__init__");
    if (init_it == cit->second.end()) return;
    CheckCallArgsAgainst(init_it->second, "class '" + func_name + "' constructor", c);
}

void Compiler::CheckMethodCallArgs(const AttrExpr* callee, const CallExpr* c) {

    if (auto* obj_name = dynamic_cast<NameExpr*>(callee->obj.get())) {
        auto eit = extern_func_params_.find(obj_name->name);
        if (eit != extern_func_params_.end()) {
            auto fit = eit->second.find(callee->attr);
            if (fit != eit->second.end()) {
                CheckCallArgsAgainst(fit->second, "extern '" + obj_name->name + "." + callee->attr + "'", c);
            }
            return;
        }
    }

    TypeRef obj_type = InferExprTypeRef(callee->obj);
    if (obj_type.type != Type::Object || obj_type.class_name.empty()) return;
    auto cit = class_method_params_.find(obj_type.class_name);
    if (cit != class_method_params_.end()) {
        auto mit = cit->second.find(callee->attr);
        if (mit != cit->second.end()) {
            CheckCallArgsAgainst(mit->second, "method '" + obj_type.class_name + "." + callee->attr + "'", c);
            return;
        }
    }

    auto fit = class_field_types_.find(obj_type.class_name);
    if (fit != class_field_types_.end() && fit->second.count(callee->attr)) return;
    auto dit = class_dynamic_attrs_.find(obj_type.class_name);
    if (dit != class_dynamic_attrs_.end() && dit->second.count(callee->attr)) return;
    throw AvaError("method '" + callee->attr + "' is not defined on class '" +
                        obj_type.class_name + "'",
                    current_line_, current_col_, source_name_);
}

void Compiler::CheckReturnType(const ReturnStmt* r) {

    if (current_return_type_.type == Type::Unknown) return;
    if (!r->value) {
        std::string msg = "return type mismatch: '" + proto_->debug_name + "' expects " +
                           DisplayType(current_return_type_) + ", received nothing (empty 'return')";
        throw AvaError(msg, current_line_, current_col_, source_name_);
    }
    TypeRef actual = InferExprTypeRef(r->value);
    if (actual.type == Type::Unknown) return;

    if (!TypeRefEquals(actual, current_return_type_)) {
        std::string msg = "return type mismatch: '" + proto_->debug_name + "' expects " +
                           DisplayType(current_return_type_) + ", received " + DisplayType(actual);
        throw AvaError(msg, current_line_, current_col_, source_name_);
    }
}

void Compiler::CheckBinOpTypes(const BinOpExpr* b) {
    Type lt = InferExprType(b->left);
    Type rt = InferExprType(b->right);
    ValidateBinOpTypes(b->op, lt, rt, current_line_, current_col_, source_name_);
}

void Compiler::CheckUnOpTypes(const UnOpExpr* u) {
    Type operand_type = InferExprType(u->operand);
    ValidateUnOpTypes(u->op, operand_type, current_line_, current_col_, source_name_);
}

void Compiler::CheckReassignment(const AssignStmt* a, const std::string& name, const TypeRef& inferred) {
    if (!a->explicit_type.empty() || a->is_local) return;
    auto it = symbols_.find(name);
    ValidateReassignment(it != symbols_.end() ? &it->second : nullptr, inferred,
                          a->line, a->col, source_name_);
}

void Compiler::CompileStmt(const std::shared_ptr<StmtNode>& stmt) {
    StampLine(stmt);

    if (auto* e = dynamic_cast<ExprStmt*>(stmt.get())) {

        uint16_t regs_before = next_reg_;
        CompileExpr(e->expr);
        FreeRegs(next_reg_ - regs_before);
        return;
    }

    if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
        RejectMemberModifiersOutsideClass(a->is_static, a->is_private, "asignacion", a->line, a->col, source_name_);
        if (!a->target) {
            FreeRegs(1);
            return;
        }

        if (!a->value) {

            TypeRef declared_type = ResolveTypeName(a->explicit_type);
            ValidateTypeAnnotation(a->explicit_type, declared_type, TypeRef{}, a->line, a->col, source_name_);
            if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
                DeclareSymbol(n->name, declared_type, TypeRef{});
            }
            return;
        }
        TypeRef declared_type = ResolveTypeName(a->explicit_type);
        TypeRef inferred_type = InferExprTypeRef(a->value);
        ValidateTypeAnnotation(a->explicit_type, declared_type, inferred_type, a->line, a->col, source_name_);
        if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
            bool in_method = locals_.find("this") != locals_.end();
            bool has_local = locals_.find(n->name) != locals_.end();
            bool is_known_attr = instance_attrs_.find(n->name) != instance_attrs_.end();
            uint16_t regs_before = next_reg_;

            if (in_method && (n->name != "this") && !has_local && is_known_attr) {
                auto val_reg = CompileExpr(a->value);
                auto attr_idx = AddConstant(MakeString(n->name));
                auto this_reg = locals_.at("this");
                Emit(OpCode::SETATTR, this_reg, attr_idx, val_reg);
                FreeRegs(next_reg_ - regs_before);
                return;
            }

            if (!is_top_level_ && (n->name != "this")) {
                if (a->is_local) {
                    uint16_t local_reg = AllocReg();
                    auto val_reg = CompileExpr(a->value);
                    Emit(OpCode::MOVE, local_reg, val_reg);
                    locals_[n->name] = local_reg;
                    MarkBlockScoped(n->name);
                    CheckReassignment(a, n->name, inferred_type);
                    DeclareSymbol(n->name, declared_type, inferred_type);
                    FreeRegs(next_reg_ - (local_reg + 1));
                    return;
                }
                if (has_local) {
                    uint16_t local_reg = locals_.at(n->name);
                    auto val_reg = CompileExpr(a->value);
                    Emit(OpCode::MOVE, local_reg, val_reg);
                    CheckReassignment(a, n->name, inferred_type);
                    DeclareSymbol(n->name, declared_type, inferred_type);
                    FreeRegs(next_reg_ - regs_before);
                    return;
                }

                int16_t upval_idx = FindUpvalue(n->name);
                if (upval_idx >= 0) {
                    auto val_reg = CompileExpr(a->value);
                    Emit(OpCode::SETUPVAL, val_reg, static_cast<uint16_t>(upval_idx));
                    FreeRegs(next_reg_ - regs_before);
                    return;
                }

                if (known_top_level_globals_.count(n->name)) {
                    auto val_reg = CompileExpr(a->value);
                    auto idx = AddConstant(MakeString(n->name));
                    Emit(OpCode::SETGLOBAL, val_reg, idx);
                    CheckReassignment(a, n->name, inferred_type);
                    DeclareSymbol(n->name, declared_type, inferred_type);
                    FreeRegs(next_reg_ - regs_before);
                    return;
                }

                uint16_t local_reg = AllocReg();
                auto val_reg = CompileExpr(a->value);
                Emit(OpCode::MOVE, local_reg, val_reg);
                locals_[n->name] = local_reg;
                CheckReassignment(a, n->name, inferred_type);
                DeclareSymbol(n->name, declared_type, inferred_type);
                FreeRegs(next_reg_ - (local_reg + 1));
                return;
            }

            auto val_reg = CompileExpr(a->value);
            auto idx = AddConstant(MakeString(n->name));
            Emit(OpCode::SETGLOBAL, val_reg, idx);
            CheckReassignment(a, n->name, inferred_type);
            DeclareSymbol(n->name, declared_type, inferred_type);
            FreeRegs(next_reg_ - regs_before);
            return;
        }
        if (auto* i = dynamic_cast<IndexExpr*>(a->target.get())) {
            uint16_t regs_before = next_reg_;
            auto val_reg = CompileExpr(a->value);
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            auto saved_idx = AllocReg();
            Emit(OpCode::MOVE, saved_idx, idx_reg);
            auto saved_obj = AllocReg();
            Emit(OpCode::MOVE, saved_obj, obj_reg);
            Emit(OpCode::SETINDEX, saved_obj, saved_idx, val_reg);
            FreeRegs(next_reg_ - regs_before);
            return;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_this = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if ((name->name == "this")) {
                    is_this = true;
                }
            }

            uint16_t regs_before = next_reg_;
            auto val_reg = CompileExpr(a->value);

            uint16_t obj_reg;
            if (is_this && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }

            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, val_reg);

            FreeRegs(next_reg_ - regs_before);
            return;
        }
        FreeRegs(1);
        return;
    }

    if (auto* a = dynamic_cast<AugAssignStmt*>(stmt.get())) {
        uint16_t regs_before = next_reg_;
        auto target_reg = CompileExpr(a->target);
        auto val_reg = CompileExpr(a->value);
        auto result_reg = AllocReg();
        Emit(BinOpToOpcode(a->op), result_reg, target_reg, val_reg);
        if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {

            if (!is_top_level_ && (n->name != "this") && locals_.find(n->name) != locals_.end()) {
                Emit(OpCode::MOVE, locals_.at(n->name), result_reg);
                FreeRegs(next_reg_ - regs_before);
                return;
            }
            auto idx = AddConstant(MakeString(n->name));
            Emit(OpCode::SETGLOBAL, result_reg, idx);
            FreeRegs(next_reg_ - regs_before);
            return;
        }
        if (auto* i = dynamic_cast<IndexExpr*>(a->target.get())) {
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            Emit(OpCode::SETINDEX, obj_reg, idx_reg, result_reg);
            FreeRegs(next_reg_ - regs_before);
            return;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_this = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if ((name->name == "this")) {
                    is_this = true;
                }
            }

            uint16_t obj_reg;
            if (is_this && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }

            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, result_reg);

            FreeRegs(next_reg_ - regs_before);
            return;
        }
        FreeRegs(next_reg_ - regs_before);
        return;
    }

    if (auto* r = dynamic_cast<ReturnStmt*>(stmt.get())) {
        CheckReturnType(r);
        if (r->value) {
            uint16_t regs_before = next_reg_;
            auto reg = CompileExpr(r->value);
            Emit(OpCode::RETURN, reg, 1);
            FreeRegs(next_reg_ - regs_before);
        } else {
            Emit(OpCode::RETURN, 0, 0);
        }
        return;
    }

    if (auto* b = dynamic_cast<BreakStmt*>(stmt.get())) {
        (void)b;
        if (loop_depth_ == 0) {
            throw AvaError("'break' outside loop", current_line_, current_col_, source_name_);
        }
        pending_breaks_.push_back({proto_->instructions.size()});
        Emit(OpCode::JMP, 0);
        return;
    }

    if (auto* c = dynamic_cast<ContinueStmt*>(stmt.get())) {
        (void)c;
        if (loop_depth_ == 0) {
            throw AvaError("'continue' outside loop", current_line_, current_col_, source_name_);
        }
        pending_continues_.push_back({proto_->instructions.size()});
        Emit(OpCode::JMP, 0);
        return;
    }

    if (dynamic_cast<PassStmt*>(stmt.get())) {
        return;
    }

    if (auto* i = dynamic_cast<IfStmt*>(stmt.get())) {
        CompileIf(i);
        return;
    }

    if (auto* w = dynamic_cast<WhileStmt*>(stmt.get())) {
        CompileWhile(w);
        return;
    }

    if (auto* f = dynamic_cast<ForStmt*>(stmt.get())) {
        CompileFor(f);
        return;
    }

    if (auto* fr = dynamic_cast<ForRangeStmt*>(stmt.get())) {
        CompileForRange(fr);
        return;
    }

    if (auto* f = dynamic_cast<FuncDef*>(stmt.get())) {
        CompileFunc(f);
        return;
    }

    if (auto* c = dynamic_cast<ClassDef*>(stmt.get())) {
        CompileClass(c);
        return;
    }

    if (auto* i = dynamic_cast<ImportStmt*>(stmt.get())) {
        CompileImport(i);
        return;
    }

    if (auto* ext = dynamic_cast<ExternStmt*>(stmt.get())) {
        CompileExtern(ext);
        return;
    }

    if (auto* t = dynamic_cast<TryStmt*>(stmt.get())) {
        CompileTry(t);
        return;
    }

    if (auto* r = dynamic_cast<RaiseStmt*>(stmt.get())) {
        CompileRaise(r);
        return;
    }

    if (auto* ma = dynamic_cast<MultiAssignStmt*>(stmt.get())) {
        CompileMultiAssign(ma);
        return;
    }

    (void)stmt;
}

void Compiler::CompileIf(const IfStmt* stmt) {

    uint16_t regs_before = next_reg_;
    auto cond_reg = CompileExpr(stmt->condition);
    Emit(OpCode::TEST, cond_reg, 1);
    size_t jmp_to_else_or_next = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    FreeRegs(next_reg_ - regs_before);

    CompileBlock(stmt->then_body);

    std::vector<size_t> exit_jmps;
    size_t jmp_after_if = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    exit_jmps.push_back(jmp_after_if);

    PatchJump(jmp_to_else_or_next);

    for (auto& [elif_cond, elif_body] : stmt->elif_clauses) {
        uint16_t elif_regs_before = next_reg_;
        auto ec_reg = CompileExpr(elif_cond);
        Emit(OpCode::TEST, ec_reg, 1);
        size_t jmp_to_next = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        FreeRegs(next_reg_ - elif_regs_before);

        CompileBlock(elif_body);

        size_t jmp_out = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        exit_jmps.push_back(jmp_out);

        PatchJump(jmp_to_next);
    }

    if (!stmt->else_body.empty()) {
        CompileBlock(stmt->else_body);
    }

    for (size_t idx : exit_jmps) {
        PatchJump(idx);
    }
}

void Compiler::CompileWhile(const WhileStmt* stmt) {
    auto saved_breaks = std::move(pending_breaks_);
    auto saved_continues = std::move(pending_continues_);
    pending_breaks_.clear();
    pending_continues_.clear();

    size_t loop_start = proto_->instructions.size();

    uint16_t regs_before = next_reg_;
    auto cond_reg = CompileExpr(stmt->condition);
    Emit(OpCode::TEST, cond_reg, 0);
    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    FreeRegs(next_reg_ - regs_before);

    loop_depth_++;
    CompileBlock(stmt->body);
    loop_depth_--;

    size_t jmp_back = proto_->instructions.size();
    int32_t offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(jmp_back) - 1;
    Emit(OpCode::JMP);
    proto_->instructions.back().bx32 = offset;

    PatchJump(jmp_out);

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchContinueJump(patch.instr_idx, loop_start);
    }
    pending_continues_.clear();

    pending_breaks_ = std::move(saved_breaks);
    pending_continues_ = std::move(saved_continues);
}

void Compiler::CompileFor(const ForStmt* stmt) {
    CompileForIterator(stmt);
}

void Compiler::CompileForRange(const ForRangeStmt* stmt) {
    auto saved_breaks = std::move(pending_breaks_);
    auto saved_continues = std::move(pending_continues_);
    pending_breaks_.clear();
    pending_continues_.clear();

    uint32_t depth = for_depth_++;
    loop_depth_++;

    const auto& var_name = stmt->var_name;
    bool use_locals = !is_top_level_;

    enum class StepSign { Positive, Negative, Runtime };
    StepSign sign = StepSign::Positive;
    if (stmt->step) {
        if (auto* n = dynamic_cast<NumberExpr*>(stmt->step.get())) {
            sign = (n->value < 0) ? StepSign::Negative : StepSign::Positive;
        } else {
            sign = StepSign::Runtime;
        }
    }

    if (!use_locals) {
        std::string d = std::to_string(depth);
        auto idx_var = AddConstant(MakeString(var_name));
        auto stop_var = AddConstant(MakeString("__for_range_stop_" + d));
        auto step_var = AddConstant(MakeString("__for_range_step_" + d));

        auto start_reg = CompileExpr(stmt->start);
        Emit(OpCode::SETGLOBAL, start_reg, idx_var);
        FreeRegs(1);

        auto stop_reg = CompileExpr(stmt->stop);
        Emit(OpCode::SETGLOBAL, stop_reg, stop_var);
        FreeRegs(1);

        if (stmt->step) {
            auto step_reg = CompileExpr(stmt->step);
            Emit(OpCode::SETGLOBAL, step_reg, step_var);
            FreeRegs(1);
        } else {
            auto one_reg = AllocReg();
            Emit(OpCode::LOADK, one_reg, AddConstant(Value::Number(1)));
            Emit(OpCode::SETGLOBAL, one_reg, step_var);
            FreeRegs(1);
        }

        size_t loop_start = proto_->instructions.size();

        uint16_t regs_before = next_reg_;
        auto idx_reg = AllocReg();
        auto stop_reg2 = AllocReg();
        auto cond_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, idx_reg, idx_var);
        Emit(OpCode::GETGLOBAL, stop_reg2, stop_var);

        if (sign == StepSign::Positive) {
            Emit(OpCode::LE, cond_reg, idx_reg, stop_reg2);
        } else if (sign == StepSign::Negative) {
            Emit(OpCode::GE, cond_reg, idx_reg, stop_reg2);
        } else {
            auto step_reg = AllocReg();
            auto zero_reg = AllocReg();
            auto is_nonneg_reg = AllocReg();
            Emit(OpCode::GETGLOBAL, step_reg, step_var);
            Emit(OpCode::LOADK, zero_reg, AddConstant(Value::Number(0)));
            Emit(OpCode::GE, is_nonneg_reg, step_reg, zero_reg);
            Emit(OpCode::TEST, is_nonneg_reg, 1);
            size_t jmp_to_negative = proto_->instructions.size();
            Emit(OpCode::JMP, 0);
            Emit(OpCode::LE, cond_reg, idx_reg, stop_reg2);
            size_t jmp_after_cmp = proto_->instructions.size();
            Emit(OpCode::JMP, 0);
            PatchJump(jmp_to_negative);
            Emit(OpCode::GE, cond_reg, idx_reg, stop_reg2);
            PatchJump(jmp_after_cmp);
        }

        Emit(OpCode::TEST, cond_reg, 0);
        size_t jmp_out = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        FreeRegs(next_reg_ - regs_before);

        CompileBlock(stmt->body);

        size_t continue_target = proto_->instructions.size();

        regs_before = next_reg_;
        auto idx_get = AllocReg();
        auto step_get = AllocReg();
        auto add_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, idx_get, idx_var);
        Emit(OpCode::GETGLOBAL, step_get, step_var);
        Emit(OpCode::ADD, add_reg, idx_get, step_get);
        Emit(OpCode::SETGLOBAL, add_reg, idx_var);
        FreeRegs(next_reg_ - regs_before);

        int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
        Emit(OpCode::JMP);
        proto_->instructions.back().bx32 = back_offset;

        PatchJump(jmp_out);

        for (auto& patch : pending_breaks_) {
            PatchJump(patch.instr_idx);
        }
        pending_breaks_.clear();

        for (auto& patch : pending_continues_) {
            PatchContinueJump(patch.instr_idx, continue_target);
        }
        pending_continues_.clear();

        pending_breaks_ = std::move(saved_breaks);
        pending_continues_ = std::move(saved_continues);
        loop_depth_--;
        for_depth_--;
        return;
    }

    bool has_local_idx = locals_.find(var_name) != locals_.end();
    uint16_t idx_reg_local = has_local_idx ? locals_.at(var_name) : AllocReg();
    if (!has_local_idx) {
        locals_[var_name] = idx_reg_local;
    }
    auto start_reg = CompileExpr(stmt->start);
    Emit(OpCode::MOVE, idx_reg_local, start_reg);
    FreeRegs(next_reg_ - (idx_reg_local + 1));

    uint16_t stop_reg_local = AllocReg();
    auto stop_val = CompileExpr(stmt->stop);
    Emit(OpCode::MOVE, stop_reg_local, stop_val);
    FreeRegs(next_reg_ - (stop_reg_local + 1));

    uint16_t step_reg_local = AllocReg();
    if (stmt->step) {
        auto step_val = CompileExpr(stmt->step);
        Emit(OpCode::MOVE, step_reg_local, step_val);
    } else {
        auto one_reg = AllocReg();
        Emit(OpCode::LOADK, one_reg, AddConstant(Value::Number(1)));
        Emit(OpCode::MOVE, step_reg_local, one_reg);
    }
    FreeRegs(next_reg_ - (step_reg_local + 1));

    size_t loop_start = proto_->instructions.size();

    uint16_t regs_before = next_reg_;
    auto cond_reg = AllocReg();
    if (sign == StepSign::Positive) {
        Emit(OpCode::LE, cond_reg, idx_reg_local, stop_reg_local);
    } else if (sign == StepSign::Negative) {
        Emit(OpCode::GE, cond_reg, idx_reg_local, stop_reg_local);
    } else {
        auto zero_reg = AllocReg();
        auto is_nonneg_reg = AllocReg();
        Emit(OpCode::LOADK, zero_reg, AddConstant(Value::Number(0)));
        Emit(OpCode::GE, is_nonneg_reg, step_reg_local, zero_reg);
        Emit(OpCode::TEST, is_nonneg_reg, 1);
        size_t jmp_to_negative = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        Emit(OpCode::LE, cond_reg, idx_reg_local, stop_reg_local);
        size_t jmp_after_cmp = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        PatchJump(jmp_to_negative);
        Emit(OpCode::GE, cond_reg, idx_reg_local, stop_reg_local);
        PatchJump(jmp_after_cmp);
    }
    Emit(OpCode::TEST, cond_reg, 0);
    FreeRegs(next_reg_ - regs_before);

    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    CompileBlock(stmt->body);

    size_t continue_target = proto_->instructions.size();

    regs_before = next_reg_;
    auto add_reg = AllocReg();
    Emit(OpCode::ADD, add_reg, idx_reg_local, step_reg_local);
    Emit(OpCode::MOVE, idx_reg_local, add_reg);
    FreeRegs(next_reg_ - regs_before);

    int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
    Emit(OpCode::JMP);
    proto_->instructions.back().bx32 = back_offset;

    PatchJump(jmp_out);

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchContinueJump(patch.instr_idx, continue_target);
    }
    pending_continues_.clear();

    pending_breaks_ = std::move(saved_breaks);
    pending_continues_ = std::move(saved_continues);
    loop_depth_--;
    for_depth_--;
}

void Compiler::CompileForIterator(const ForStmt* stmt) {
    uint32_t my_depth = for_depth_++;
    loop_depth_++;
    IteratorKind kind = DetectIteratorKind(stmt->iterable);

    if (kind == IteratorKind::Coroutine) {
        CompileForCoroutine(stmt, my_depth);
        for_depth_--;
        loop_depth_--;
        return;
    }
    if (dynamic_cast<DictExpr*>(stmt->iterable.get())) {
        CompileForDict(stmt, my_depth);
        for_depth_--;
        loop_depth_--;
        return;
    }
    if (dynamic_cast<ListExpr*>(stmt->iterable.get()) ||
        dynamic_cast<StringExpr*>(stmt->iterable.get()) ||
        dynamic_cast<CallExpr*>(stmt->iterable.get())) {
        CompileForList(stmt, my_depth);
        for_depth_--;
        loop_depth_--;
        return;
    }

    CompileForDynamic(stmt, my_depth);
    for_depth_--;
    loop_depth_--;
}

Compiler::IteratorKind Compiler::DetectIteratorKind(const std::shared_ptr<ExprNode>& iterable) {
    if (auto* call = dynamic_cast<CallExpr*>(iterable.get())) {
        if (auto* name = dynamic_cast<NameExpr*>(call->callee.get())) {
            if (name->name == "coroutine") {
                return IteratorKind::Coroutine;
            }
        }
    }
    return IteratorKind::List;
}

void Compiler::CompileForList(const ForStmt* stmt, uint32_t depth) {
    auto saved_breaks = std::move(pending_breaks_);
    auto saved_continues = std::move(pending_continues_);
    pending_breaks_.clear();
    pending_continues_.clear();

    const auto& var_name = stmt->var_name;
    bool use_locals = !is_top_level_;

    if (!use_locals) {
        std::string d = std::to_string(depth);
        auto list_var = AddConstant(MakeString("__for_list_" + d));
        auto len_var = AddConstant(MakeString("__for_len_" + d));
        auto idx_var = AddConstant(MakeString("__for_idx_" + d));
        auto elem_var = AddConstant(MakeString(var_name));

        auto list_reg = CompileExpr(stmt->iterable);
        Emit(OpCode::SETGLOBAL, list_reg, list_var);
        FreeRegs(1);

        auto len_func = AllocReg();
        Emit(OpCode::GETGLOBAL, len_func, AddConstant(MakeString("len")));
        auto len_arg = AllocReg();
        Emit(OpCode::GETGLOBAL, len_arg, list_var);
        Emit(OpCode::CALL, len_func, 1, 1);
        Emit(OpCode::SETGLOBAL, len_func, len_var);
        FreeRegs(2);

        auto zero_c = AddConstant(Value::Number(0));
        auto zero_reg = AllocReg();
        Emit(OpCode::LOADK, zero_reg, zero_c);
        Emit(OpCode::SETGLOBAL, zero_reg, idx_var);
        FreeRegs(1);

        size_t loop_start = proto_->instructions.size();

        auto elem_reg = AllocReg();
        auto idx_get = AllocReg();
        auto list_get = AllocReg();
        auto len_get = AllocReg();
        auto cond_reg = AllocReg();
        auto one_c = AddConstant(Value::Number(1));
        auto add_reg = AllocReg();

        Emit(OpCode::GETGLOBAL, cond_reg, idx_var);
        Emit(OpCode::GETGLOBAL, len_get, len_var);
        Emit(OpCode::LT, cond_reg, cond_reg, len_get);
        Emit(OpCode::TEST, cond_reg, 0);

        size_t jmp_out = proto_->instructions.size();
        Emit(OpCode::JMP, 0);

        Emit(OpCode::GETGLOBAL, list_get, list_var);
        Emit(OpCode::GETGLOBAL, idx_get, idx_var);
        Emit(OpCode::GETINDEX, elem_reg, list_get, idx_get);
        Emit(OpCode::SETGLOBAL, elem_reg, elem_var);

        CompileBlock(stmt->body);

        size_t continue_target = proto_->instructions.size();

        Emit(OpCode::GETGLOBAL, idx_get, idx_var);
        Emit(OpCode::LOADK, add_reg, one_c);
        Emit(OpCode::ADD, add_reg, idx_get, add_reg);
        Emit(OpCode::SETGLOBAL, add_reg, idx_var);

        int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
        Emit(OpCode::JMP);
        proto_->instructions.back().bx32 = back_offset;

        PatchJump(jmp_out);

        for (auto& patch : pending_breaks_) {
            PatchJump(patch.instr_idx);
        }
        pending_breaks_.clear();

        for (auto& patch : pending_continues_) {
            PatchContinueJump(patch.instr_idx, continue_target);
        }
        pending_continues_.clear();

        pending_breaks_ = std::move(saved_breaks);
        pending_continues_ = std::move(saved_continues);
        return;
    }

    uint16_t list_reg_local = AllocReg();
    auto iter_reg = CompileExpr(stmt->iterable);
    Emit(OpCode::MOVE, list_reg_local, iter_reg);
    FreeRegs(next_reg_ - (list_reg_local + 1));

    uint16_t len_reg_local = AllocReg();
    auto len_func = AllocReg();
    Emit(OpCode::GETGLOBAL, len_func, AddConstant(MakeString("len")));
    auto len_arg = AllocReg();
    Emit(OpCode::MOVE, len_arg, list_reg_local);
    Emit(OpCode::CALL, len_func, 1, 1);
    Emit(OpCode::MOVE, len_reg_local, len_func);
    FreeRegs(next_reg_ - (len_reg_local + 1));

    uint16_t idx_reg_local = AllocReg();
    auto zero_c = AddConstant(Value::Number(0));
    auto zero_reg = AllocReg();
    Emit(OpCode::LOADK, zero_reg, zero_c);
    Emit(OpCode::MOVE, idx_reg_local, zero_reg);
    FreeRegs(next_reg_ - (idx_reg_local + 1));

    bool has_local_elem = locals_.find(var_name) != locals_.end();
    uint16_t elem_reg_local = has_local_elem ? locals_.at(var_name) : AllocReg();
    if (!has_local_elem) {
        locals_[var_name] = elem_reg_local;
    }

    auto one_c = AddConstant(Value::Number(1));
    size_t loop_start = proto_->instructions.size();

    uint16_t regs_before = next_reg_;
    auto cond_reg = AllocReg();
    Emit(OpCode::LT, cond_reg, idx_reg_local, len_reg_local);
    Emit(OpCode::TEST, cond_reg, 0);
    FreeRegs(next_reg_ - regs_before);

    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    regs_before = next_reg_;
    auto elem_get = AllocReg();
    Emit(OpCode::GETINDEX, elem_get, list_reg_local, idx_reg_local);
    Emit(OpCode::MOVE, elem_reg_local, elem_get);
    FreeRegs(next_reg_ - regs_before);

    CompileBlock(stmt->body);

    size_t continue_target = proto_->instructions.size();

    regs_before = next_reg_;
    auto add_reg = AllocReg();
    Emit(OpCode::LOADK, add_reg, one_c);
    Emit(OpCode::ADD, add_reg, idx_reg_local, add_reg);
    Emit(OpCode::MOVE, idx_reg_local, add_reg);
    FreeRegs(next_reg_ - regs_before);

    int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
    Emit(OpCode::JMP);
    proto_->instructions.back().bx32 = back_offset;

    PatchJump(jmp_out);

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchContinueJump(patch.instr_idx, continue_target);
    }
    pending_continues_.clear();

    pending_breaks_ = std::move(saved_breaks);
    pending_continues_ = std::move(saved_continues);
}

void Compiler::CompileForCoroutine(const ForStmt* stmt, uint32_t depth) {
    auto saved_breaks = std::move(pending_breaks_);
    auto saved_continues = std::move(pending_continues_);
    pending_breaks_.clear();
    pending_continues_.clear();

    const auto& var_name = stmt->var_name;
    bool use_locals = !is_top_level_;

    if (!use_locals) {
        std::string d = std::to_string(depth);
        auto iter_var = AddConstant(MakeString("__iter_" + d));
        auto resume_var = AddConstant(MakeString("__resume_" + d));
        auto val_var = AddConstant(MakeString("__val_" + d));
        auto elem_var = AddConstant(MakeString(var_name));

        auto iter_reg = CompileExpr(stmt->iterable);
        Emit(OpCode::SETGLOBAL, iter_reg, iter_var);
        FreeRegs(1);

        auto resume_func = AllocReg();
        Emit(OpCode::GETGLOBAL, resume_func, AddConstant(MakeString("resume")));
        Emit(OpCode::SETGLOBAL, resume_func, resume_var);
        FreeRegs(1);

        size_t loop_start = proto_->instructions.size();

        auto resume_get = AllocReg();
        Emit(OpCode::GETGLOBAL, resume_get, resume_var);
        auto arg_get = AllocReg();
        Emit(OpCode::GETGLOBAL, arg_get, iter_var);

        Emit(OpCode::CALL, resume_get, 1, 1);

        auto val_reg = AllocReg();
        Emit(OpCode::MOVE, val_reg, resume_get);
        Emit(OpCode::SETGLOBAL, val_reg, val_var);

        auto nil_k = AddConstant(Value::Nil());
        auto nil_reg = AllocReg();
        Emit(OpCode::LOADK, nil_reg, nil_k);
        auto cond_reg = AllocReg();
        Emit(OpCode::NE, cond_reg, val_reg, nil_reg);
        Emit(OpCode::TEST, cond_reg, 0);

        size_t jmp_out = proto_->instructions.size();
        Emit(OpCode::JMP, 0);

        auto zero_k = AddConstant(Value::Number(0));
        auto elem_get = AllocReg();
        Emit(OpCode::GETGLOBAL, elem_get, val_var);
        auto idx_reg = AllocReg();
        Emit(OpCode::LOADK, idx_reg, zero_k);
        auto first_elem = AllocReg();
        Emit(OpCode::GETINDEX, first_elem, elem_get, idx_reg);
        Emit(OpCode::SETGLOBAL, first_elem, elem_var);

        CompileBlock(stmt->body);

        int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
        Emit(OpCode::JMP);
        proto_->instructions.back().bx32 = back_offset;

        PatchJump(jmp_out);

        for (auto& patch : pending_breaks_) {
            PatchJump(patch.instr_idx);
        }
        pending_breaks_.clear();

        for (auto& patch : pending_continues_) {
            PatchContinueJump(patch.instr_idx, loop_start);
        }
        pending_continues_.clear();

        pending_breaks_ = std::move(saved_breaks);
        pending_continues_ = std::move(saved_continues);
        return;
    }

    uint16_t iter_reg_local = AllocReg();
    auto iterable_reg = CompileExpr(stmt->iterable);
    Emit(OpCode::MOVE, iter_reg_local, iterable_reg);
    FreeRegs(next_reg_ - (iter_reg_local + 1));

    uint16_t resume_reg_local = AllocReg();
    auto resume_func = AllocReg();
    Emit(OpCode::GETGLOBAL, resume_func, AddConstant(MakeString("resume")));
    Emit(OpCode::MOVE, resume_reg_local, resume_func);
    FreeRegs(next_reg_ - (resume_reg_local + 1));

    uint16_t val_reg_local = AllocReg();

    bool has_local_elem = locals_.find(var_name) != locals_.end();
    uint16_t elem_reg_local = has_local_elem ? locals_.at(var_name) : AllocReg();
    if (!has_local_elem) {
        locals_[var_name] = elem_reg_local;
    }

    size_t loop_start = proto_->instructions.size();

    uint16_t regs_before = next_reg_;
    auto resume_get = AllocReg();
    Emit(OpCode::MOVE, resume_get, resume_reg_local);
    auto arg_get = AllocReg();
    Emit(OpCode::MOVE, arg_get, iter_reg_local);
    Emit(OpCode::CALL, resume_get, 1, 1);
    Emit(OpCode::MOVE, val_reg_local, resume_get);
    FreeRegs(next_reg_ - regs_before);

    regs_before = next_reg_;
    auto nil_k = AddConstant(Value::Nil());
    auto nil_reg = AllocReg();
    Emit(OpCode::LOADK, nil_reg, nil_k);
    auto cond_reg = AllocReg();
    Emit(OpCode::NE, cond_reg, val_reg_local, nil_reg);
    Emit(OpCode::TEST, cond_reg, 0);
    FreeRegs(next_reg_ - regs_before);

    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    regs_before = next_reg_;
    auto zero_k = AddConstant(Value::Number(0));
    auto idx_reg = AllocReg();
    Emit(OpCode::LOADK, idx_reg, zero_k);
    auto first_elem = AllocReg();
    Emit(OpCode::GETINDEX, first_elem, val_reg_local, idx_reg);
    Emit(OpCode::MOVE, elem_reg_local, first_elem);
    FreeRegs(next_reg_ - regs_before);

    CompileBlock(stmt->body);

    int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
    Emit(OpCode::JMP);
    proto_->instructions.back().bx32 = back_offset;

    PatchJump(jmp_out);

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchContinueJump(patch.instr_idx, loop_start);
    }
    pending_continues_.clear();

    pending_breaks_ = std::move(saved_breaks);
    pending_continues_ = std::move(saved_continues);
}

void Compiler::CompileForDynamic(const ForStmt* stmt, uint32_t depth) {
    auto saved_breaks = std::move(pending_breaks_);
    auto saved_continues = std::move(pending_continues_);
    pending_breaks_.clear();
    pending_continues_.clear();

    const auto& var_name = stmt->var_name;

    std::string d = std::to_string(depth);
    auto iter_var   = AddConstant(MakeString("__for_iter_" + d));
    auto len_var    = AddConstant(MakeString("__for_len_" + d));
    auto idx_var    = AddConstant(MakeString("__for_idx_" + d));
    auto elem_var   = AddConstant(MakeString(var_name));
    auto resume_var = AddConstant(MakeString("__for_resume_" + d));
    auto val_var    = AddConstant(MakeString("__for_val_" + d));

    {
        uint16_t regs_before = next_reg_;
        auto iter_reg = CompileExpr(stmt->iterable);
        Emit(OpCode::SETGLOBAL, iter_reg, iter_var);
        FreeRegs(next_reg_ - regs_before);
    }

    uint16_t is_coro_reg = AllocReg();
    {
        uint16_t regs_before = next_reg_;
        auto type_func = AllocReg();
        Emit(OpCode::GETGLOBAL, type_func, AddConstant(MakeString("type")));
        Emit(OpCode::GETGLOBAL, static_cast<uint16_t>(type_func + 1), iter_var);
        Emit(OpCode::CALL, type_func, 1, 1);
        auto coro_str_reg = AllocReg();
        Emit(OpCode::LOADK, coro_str_reg, AddConstant(MakeString("coroutine")));
        Emit(OpCode::EQ, is_coro_reg, type_func, coro_str_reg);
        FreeRegs(next_reg_ - (is_coro_reg + 1));
    }

    Emit(OpCode::TEST, is_coro_reg, 0, 1);
    size_t jmp_to_coro_init = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    {
        uint16_t regs_before = next_reg_;
        auto lf = AllocReg();
        Emit(OpCode::GETGLOBAL, lf, AddConstant(MakeString("len")));
        Emit(OpCode::GETGLOBAL, static_cast<uint16_t>(lf + 1), iter_var);
        Emit(OpCode::CALL, lf, 1, 1);
        Emit(OpCode::SETGLOBAL, lf, len_var);
        FreeRegs(next_reg_ - regs_before);

        auto zr = AllocReg();
        Emit(OpCode::LOADK, zr, AddConstant(Value::Number(0)));
        Emit(OpCode::SETGLOBAL, zr, idx_var);
        FreeRegs(1);
    }
    size_t jmp_after_init = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    PatchJump(jmp_to_coro_init);
    {
        uint16_t regs_before = next_reg_;
        auto rf = AllocReg();
        Emit(OpCode::GETGLOBAL, rf, AddConstant(MakeString("resume")));
        Emit(OpCode::SETGLOBAL, rf, resume_var);
        FreeRegs(next_reg_ - regs_before);
    }
    PatchJump(jmp_after_init);

    size_t loop_start = proto_->instructions.size();

    Emit(OpCode::TEST, is_coro_reg, 0);
    size_t jmp_to_list_loop = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    std::vector<size_t> coro_exit_jmps;

    {
        uint16_t regs_before = next_reg_;
        auto rg = AllocReg();
        Emit(OpCode::GETGLOBAL, rg, resume_var);
        Emit(OpCode::GETGLOBAL, static_cast<uint16_t>(rg + 1), iter_var);
        Emit(OpCode::CALL, rg, 1, 1);
        Emit(OpCode::SETGLOBAL, rg, val_var);
        auto nil_reg = AllocReg();
        Emit(OpCode::LOADK, nil_reg, AddConstant(Value::Nil()));
        auto val_get = AllocReg();
        Emit(OpCode::GETGLOBAL, val_get, val_var);
        auto cr = AllocReg();
        Emit(OpCode::NE, cr, val_get, nil_reg);
        Emit(OpCode::TEST, cr, 0);
        FreeRegs(next_reg_ - regs_before);

        size_t jmp_coro_exit = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        coro_exit_jmps.push_back(jmp_coro_exit);

        {
            uint16_t rb2 = next_reg_;
            auto eg = AllocReg();
            Emit(OpCode::GETGLOBAL, eg, val_var);
            auto ir = AllocReg();
            Emit(OpCode::LOADK, ir, AddConstant(Value::Number(0)));
            auto fe = AllocReg();
            Emit(OpCode::GETINDEX, fe, eg, ir);
            Emit(OpCode::SETGLOBAL, fe, elem_var);
            FreeRegs(next_reg_ - rb2);
        }
    }

    size_t jmp_to_body_from_coro = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    PatchJump(jmp_to_list_loop);

    {
        uint16_t regs_before = next_reg_;
        auto lg = AllocReg();
        Emit(OpCode::GETGLOBAL, lg, iter_var);
        auto ig = AllocReg();
        Emit(OpCode::GETGLOBAL, ig, idx_var);
        auto fe = AllocReg();
        Emit(OpCode::GETINDEX, fe, lg, ig);
        Emit(OpCode::SETGLOBAL, fe, elem_var);
        FreeRegs(next_reg_ - regs_before);
    }

    PatchJump(jmp_to_body_from_coro);

    CompileBlock(stmt->body);

    size_t continue_target = proto_->instructions.size();

    Emit(OpCode::TEST, is_coro_reg, 0);
    size_t jmp_to_list_inc = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    {
        int32_t back_offset = static_cast<int32_t>(loop_start) -
                              static_cast<int32_t>(proto_->instructions.size()) - 1;
        Emit(OpCode::JMP);
        proto_->instructions.back().bx32 = back_offset;
    }

    PatchJump(jmp_to_list_inc);

    {
        uint16_t regs_before = next_reg_;
        auto ag = AllocReg();
        Emit(OpCode::GETGLOBAL, ag, idx_var);
        auto ar = AllocReg();
        Emit(OpCode::LOADK, ar, AddConstant(Value::Number(1)));
        Emit(OpCode::ADD, ar, ag, ar);
        Emit(OpCode::SETGLOBAL, ar, idx_var);
        FreeRegs(next_reg_ - regs_before);

        auto cr = AllocReg();
        auto lg2 = AllocReg();
        Emit(OpCode::GETGLOBAL, cr, idx_var);
        Emit(OpCode::GETGLOBAL, lg2, len_var);
        Emit(OpCode::LT, cr, cr, lg2);
        Emit(OpCode::TEST, cr, 0);
        FreeRegs(2);

        size_t jmp_list_exit = proto_->instructions.size();
        Emit(OpCode::JMP, 0);

        int32_t back_offset = static_cast<int32_t>(loop_start) -
                              static_cast<int32_t>(proto_->instructions.size()) - 1;
        Emit(OpCode::JMP);
        proto_->instructions.back().bx32 = back_offset;

        PatchJump(jmp_list_exit);
    }

    for (size_t idx : coro_exit_jmps) {
        PatchJump(idx);
    }

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchContinueJump(patch.instr_idx, continue_target);
    }
    pending_continues_.clear();

    pending_breaks_ = std::move(saved_breaks);
    pending_continues_ = std::move(saved_continues);
}

void Compiler::CompileForDict(const ForStmt* stmt, uint32_t depth) {
    auto saved_breaks = std::move(pending_breaks_);
    auto saved_continues = std::move(pending_continues_);
    pending_breaks_.clear();
    pending_continues_.clear();

    const auto& var_name = stmt->var_name;
    bool use_locals = !is_top_level_;

    if (!use_locals) {
        std::string d = std::to_string(depth);
        auto dict_var = AddConstant(MakeString("__for_dict_" + d));
        auto keys_var = AddConstant(MakeString("__for_keys_" + d));
        auto len_var = AddConstant(MakeString("__for_len_" + d));
        auto idx_var = AddConstant(MakeString("__for_idx_" + d));
        auto elem_var = AddConstant(MakeString(var_name));

        auto dict_reg = CompileExpr(stmt->iterable);
        Emit(OpCode::SETGLOBAL, dict_reg, dict_var);
        FreeRegs(1);

        auto keys_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, keys_reg, dict_var);
        Emit(OpCode::GETATTR, keys_reg, keys_reg, AddConstant(MakeString("keys")));
        Emit(OpCode::CALL, keys_reg, 0, 1);
        Emit(OpCode::SETGLOBAL, keys_reg, keys_var);
        FreeRegs(1);

        auto len_func = AllocReg();
        Emit(OpCode::GETGLOBAL, len_func, AddConstant(MakeString("len")));
        auto len_arg = AllocReg();
        Emit(OpCode::GETGLOBAL, len_arg, keys_var);
        Emit(OpCode::MOVE, len_func + 1, len_arg);
        Emit(OpCode::CALL, len_func, 1, 1);
        Emit(OpCode::SETGLOBAL, len_func, len_var);
        FreeRegs(2);

        auto zero_c = AddConstant(Value::Number(0));
        auto zero_reg = AllocReg();
        Emit(OpCode::LOADK, zero_reg, zero_c);
        Emit(OpCode::SETGLOBAL, zero_reg, idx_var);
        FreeRegs(1);

        size_t loop_start = proto_->instructions.size();

        auto elem_reg = AllocReg();
        auto idx_get = AllocReg();
        auto keys_get = AllocReg();
        auto len_get = AllocReg();
        auto cond_reg = AllocReg();
        auto one_c = AddConstant(Value::Number(1));
        auto add_reg = AllocReg();

        Emit(OpCode::GETGLOBAL, cond_reg, idx_var);
        Emit(OpCode::GETGLOBAL, len_get, len_var);
        Emit(OpCode::LT, cond_reg, cond_reg, len_get);
        Emit(OpCode::TEST, cond_reg, 0);

        size_t jmp_out = proto_->instructions.size();
        Emit(OpCode::JMP, 0);

        Emit(OpCode::GETGLOBAL, keys_get, keys_var);
        Emit(OpCode::GETGLOBAL, idx_get, idx_var);
        Emit(OpCode::GETINDEX, elem_reg, keys_get, idx_get);
        Emit(OpCode::SETGLOBAL, elem_reg, elem_var);

        CompileBlock(stmt->body);

        size_t continue_target = proto_->instructions.size();

        Emit(OpCode::GETGLOBAL, idx_get, idx_var);
        Emit(OpCode::LOADK, add_reg, one_c);
        Emit(OpCode::ADD, add_reg, idx_get, add_reg);
        Emit(OpCode::SETGLOBAL, add_reg, idx_var);

        int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
        Emit(OpCode::JMP);
        proto_->instructions.back().bx32 = back_offset;

        PatchJump(jmp_out);

        for (auto& patch : pending_breaks_) {
            PatchJump(patch.instr_idx);
        }
        pending_breaks_.clear();

        for (auto& patch : pending_continues_) {
            PatchContinueJump(patch.instr_idx, continue_target);
        }
        pending_continues_.clear();

        pending_breaks_ = std::move(saved_breaks);
        pending_continues_ = std::move(saved_continues);
        return;
    }

    uint16_t dict_reg_local = AllocReg();
    auto iter_reg = CompileExpr(stmt->iterable);
    Emit(OpCode::MOVE, dict_reg_local, iter_reg);
    FreeRegs(next_reg_ - (dict_reg_local + 1));

    uint16_t keys_reg_local = AllocReg();
    Emit(OpCode::GETATTR, keys_reg_local, dict_reg_local, AddConstant(MakeString("keys")));
    Emit(OpCode::CALL, keys_reg_local, 0, 1);
    FreeRegs(next_reg_ - (keys_reg_local + 1));

    uint16_t len_reg_local = AllocReg();
    auto len_func = AllocReg();
    Emit(OpCode::GETGLOBAL, len_func, AddConstant(MakeString("len")));
    auto len_arg = AllocReg();
    Emit(OpCode::MOVE, len_arg, keys_reg_local);
    Emit(OpCode::CALL, len_func, 1, 1);
    Emit(OpCode::MOVE, len_reg_local, len_func);
    FreeRegs(next_reg_ - (len_reg_local + 1));

    uint16_t idx_reg_local = AllocReg();
    auto zero_c = AddConstant(Value::Number(0));
    auto zero_reg = AllocReg();
    Emit(OpCode::LOADK, zero_reg, zero_c);
    Emit(OpCode::MOVE, idx_reg_local, zero_reg);
    FreeRegs(next_reg_ - (idx_reg_local + 1));

    bool has_local_elem = locals_.find(var_name) != locals_.end();
    uint16_t elem_reg_local = has_local_elem ? locals_.at(var_name) : AllocReg();
    if (!has_local_elem) {
        locals_[var_name] = elem_reg_local;
    }

    auto one_c = AddConstant(Value::Number(1));
    size_t loop_start = proto_->instructions.size();

    uint16_t regs_before = next_reg_;
    auto elem_get = AllocReg();
    Emit(OpCode::GETINDEX, elem_get, keys_reg_local, idx_reg_local);
    Emit(OpCode::MOVE, elem_reg_local, elem_get);
    FreeRegs(next_reg_ - regs_before);

    CompileBlock(stmt->body);

    size_t continue_target = proto_->instructions.size();

    regs_before = next_reg_;
    auto add_reg = AllocReg();
    Emit(OpCode::LOADK, add_reg, one_c);
    Emit(OpCode::ADD, add_reg, idx_reg_local, add_reg);
    Emit(OpCode::MOVE, idx_reg_local, add_reg);
    FreeRegs(next_reg_ - regs_before);

    regs_before = next_reg_;
    auto cond_reg = AllocReg();
    Emit(OpCode::LT, cond_reg, idx_reg_local, len_reg_local);
    Emit(OpCode::TEST, cond_reg, 0);
    FreeRegs(next_reg_ - regs_before);

    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
    Emit(OpCode::JMP);
    proto_->instructions.back().bx32 = back_offset;

    PatchJump(jmp_out);

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchContinueJump(patch.instr_idx, continue_target);
    }
    pending_continues_.clear();

    pending_breaks_ = std::move(saved_breaks);
    pending_continues_ = std::move(saved_continues);
}

void Compiler::EmitDefaultsPrologue(const std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>>& params,
                                     uint16_t param_reg_base) {
    for (size_t i = 0; i < params.size(); ++i) {
        auto& def = params[i].second;
        if (!def) continue;

        uint16_t param_reg = static_cast<uint16_t>(param_reg_base + i);

        auto argc_reg = AllocReg();
        Emit(OpCode::ARGC, argc_reg);

        auto idx_const = AddConstant(Value::Number(static_cast<double>(i)));
        auto idx_reg = AllocReg();
        Emit(OpCode::LOADK, idx_reg, idx_const);

        auto cond_reg = AllocReg();
        Emit(OpCode::LE, cond_reg, argc_reg, idx_reg);
        FreeRegs(2);

        Emit(OpCode::TEST, cond_reg, 0);
        size_t jmp_have_arg = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        FreeRegs(1);

        uint16_t regs_before = next_reg_;
        auto val_reg = CompileExpr(def);
        Emit(OpCode::MOVE, param_reg, val_reg);
        FreeRegs(next_reg_ - regs_before);

        PatchJump(jmp_have_arg);
    }
}

void Compiler::CompileFunc(const FuncDef* func) {
    RejectMemberModifiersOutsideClass(func->is_static, func->is_private,
                                       ("func " + func->name).c_str(), func->line, func->col, source_name_);
    Compiler sub;
    sub.is_top_level_ = false;
    sub.in_async_func_ = func->is_async;
    sub.proto_ = std::make_shared<Proto>();
    sub.proto_->is_async = func->is_async;
    sub.source_name_ = source_name_;
    sub.proto_->source_name = source_name_;
    sub.proto_->debug_name = func->name;
    sub.proto_->num_params = static_cast<uint8_t>(func->params.size());
    sub.proto_->is_vararg = func->is_vararg;
    sub.compiled_classes_ = compiled_classes_;
    sub.class_field_types_ = class_field_types_;
    sub.class_dynamic_attrs_ = class_dynamic_attrs_;
    sub.class_method_returns_ = class_method_returns_;
    sub.class_method_params_ = class_method_params_;
    sub.known_funcs_ = known_funcs_;
    sub.known_func_returns_ = known_func_returns_;
    sub.known_top_level_globals_ = known_top_level_globals_;
    sub.extern_func_params_ = extern_func_params_;
    sub.extern_func_returns_ = extern_func_returns_;
    sub.has_wildcard_import_ = has_wildcard_import_;
    sub.current_base_class_ = current_base_class_;

    sub.next_reg_ = 1;
    sub.max_reg_ = sub.next_reg_;
    for (auto& [pname, def] : func->params) {
        sub.locals_[pname] = sub.next_reg_;
        sub.next_reg_++;
    }
    sub.max_reg_ = sub.next_reg_;

    for (auto& [name, reg] : locals_) {
        sub.parent_locals_.push_back({name, reg});
        sub.proto_->upvalue_descs.push_back({true, reg});
        sub.next_reg_++;
    }

    for (size_t i = 0; i < parent_locals_.size(); ++i) {
        auto& pname = parent_locals_[i].first;
        if (locals_.count(pname)) continue;
        sub.parent_locals_.push_back({pname, static_cast<uint16_t>(i)});
        sub.proto_->upvalue_descs.push_back({false, static_cast<uint16_t>(i)});
        sub.next_reg_++;
    }

    sub.EmitDefaultsPrologue(func->params, 1);
    sub.current_return_type_ = ResolveTypeName(func->return_type);

    sub.CompileChunk(func->body);
    uint8_t ret_a = sub.result_reg_ > 0 ? static_cast<uint8_t>(sub.result_reg_) : 0;
    uint8_t ret_b = sub.result_reg_ > 0 ? 1 : 0;
    sub.Emit(OpCode::RETURN, ret_a, static_cast<uint16_t>(ret_b), 0);
    sub.proto_->num_registers = std::max<uint16_t>(sub.max_reg_ + 1, sub.next_reg_);

    uint16_t child_idx = static_cast<uint16_t>(proto_->child_protos.size());
    proto_->child_protos.push_back(sub.proto_);

    auto reg = AllocReg();
    Emit(OpCode::CLOSURE, reg, child_idx);

    auto name_idx = AddConstant(MakeString(func->name));
    Emit(OpCode::SETGLOBAL, reg, name_idx);
}

void Compiler::CompileChunk(const std::vector<std::shared_ptr<StmtNode>>& stmts) {
    RejectDuplicateFuncDefs(stmts, source_name_);
    if (is_top_level_) {
        CollectFuncSignatures(stmts, known_funcs_, known_func_returns_, compiled_classes_, known_top_level_globals_);
    } else {
        std::unordered_set<std::string> discard;
        CollectFuncSignatures(stmts, known_funcs_, known_func_returns_, compiled_classes_, discard);
    }
    for (size_t i = 0; i < stmts.size(); i++) {
        if (i == stmts.size() - 1) {
            result_reg_ = CompileExprToReg(stmts[i]);
        } else {
            CompileStmt(stmts[i]);
        }
    }
}

void Compiler::MarkBlockScoped(const std::string& name) {
    if (!block_scoped_names_.empty()) {
        block_scoped_names_.back().insert(name);
    }
}

void Compiler::CompileBlock(const std::vector<std::shared_ptr<StmtNode>>& body) {
    auto saved_locals = locals_;
    auto saved_symbols = symbols_;
    uint16_t saved_result_reg = result_reg_;

    block_scoped_names_.emplace_back();
    CompileChunk(body);

    for (const auto& name : block_scoped_names_.back()) {
        auto local_it = saved_locals.find(name);
        if (local_it != saved_locals.end()) {
            locals_[name] = local_it->second;
        } else {
            locals_.erase(name);
        }
        auto symbol_it = saved_symbols.find(name);
        if (symbol_it != saved_symbols.end()) {
            symbols_[name] = symbol_it->second;
        } else {
            symbols_.erase(name);
        }
    }
    for (auto& [name, sym] : symbols_) {
        if (saved_symbols.count(name)) continue;
        sym = Symbol{};
        sym.name = name;
    }
    block_scoped_names_.pop_back();

    result_reg_ = saved_result_reg;
}

std::shared_ptr<Proto> Compiler::Compile(const std::shared_ptr<Chunk>& chunk,
                                          const std::string& source_name) {
    Reset();
    source_name_ = source_name;
    proto_->source_name = source_name_;
    CompileChunk(chunk->statements);
    if (result_reg_ > 0 || next_reg_ > 0) {
        Emit(OpCode::RETURN, result_reg_ > 0 ? result_reg_ : 0, result_reg_ > 0 ? 1 : 0);
    } else {
        Emit(OpCode::RETURN, 0, 0);
    }
    proto_->num_registers = max_reg_ + 1;
    return proto_;
}

uint16_t Compiler::CompileExprToReg(const std::shared_ptr<StmtNode>& stmt) {
    StampLine(stmt);

    if (auto* e = dynamic_cast<ExprStmt*>(stmt.get())) {
        return CompileExpr(e->expr);
    }
    if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
        RejectMemberModifiersOutsideClass(a->is_static, a->is_private, "asignacion", a->line, a->col, source_name_);
        if (!a->target) {
            FreeRegs(1);
            return 0;
        }

        if (!a->value) {
            TypeRef declared_type = ResolveTypeName(a->explicit_type);
            ValidateTypeAnnotation(a->explicit_type, declared_type, TypeRef{}, a->line, a->col, source_name_);
            if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
                DeclareSymbol(n->name, declared_type, TypeRef{});
            }
            return 0;
        }
        TypeRef declared_type = ResolveTypeName(a->explicit_type);
        TypeRef inferred_type = InferExprTypeRef(a->value);
        ValidateTypeAnnotation(a->explicit_type, declared_type, inferred_type, a->line, a->col, source_name_);
        if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
            bool in_method = locals_.find("this") != locals_.end();
            bool has_local = locals_.find(n->name) != locals_.end();
            bool is_known_attr = instance_attrs_.find(n->name) != instance_attrs_.end();
            uint16_t regs_before = next_reg_;

            if (in_method && (n->name != "this") && !has_local && is_known_attr) {
                auto val_reg = CompileExpr(a->value);
                auto attr_idx = AddConstant(MakeString(n->name));
                auto this_reg = locals_.at("this");
                Emit(OpCode::SETATTR, this_reg, attr_idx, val_reg);
                FreeRegs(next_reg_ - regs_before);
                return 0;
            }

            if (!is_top_level_ && (n->name != "this")) {
                if (a->is_local) {
                    uint16_t local_reg = AllocReg();
                    auto val_reg = CompileExpr(a->value);
                    Emit(OpCode::MOVE, local_reg, val_reg);
                    locals_[n->name] = local_reg;
                    MarkBlockScoped(n->name);
                    CheckReassignment(a, n->name, inferred_type);
                    DeclareSymbol(n->name, declared_type, inferred_type);
                    FreeRegs(next_reg_ - (local_reg + 1));
                    return 0;
                }
                if (has_local) {
                    uint16_t local_reg = locals_.at(n->name);
                    auto val_reg = CompileExpr(a->value);
                    Emit(OpCode::MOVE, local_reg, val_reg);
                    CheckReassignment(a, n->name, inferred_type);
                    DeclareSymbol(n->name, declared_type, inferred_type);
                    FreeRegs(next_reg_ - regs_before);
                    return 0;
                }

                int16_t upval_idx = FindUpvalue(n->name);
                if (upval_idx >= 0) {
                    auto val_reg = CompileExpr(a->value);
                    Emit(OpCode::SETUPVAL, val_reg, static_cast<uint16_t>(upval_idx));
                    FreeRegs(next_reg_ - regs_before);
                    return 0;
                }

                if (known_top_level_globals_.count(n->name)) {
                    auto val_reg = CompileExpr(a->value);
                    auto idx = AddConstant(MakeString(n->name));
                    Emit(OpCode::SETGLOBAL, val_reg, idx);
                    CheckReassignment(a, n->name, inferred_type);
                    DeclareSymbol(n->name, declared_type, inferred_type);
                    FreeRegs(next_reg_ - regs_before);
                    return 0;
                }

                uint16_t local_reg = AllocReg();
                auto val_reg = CompileExpr(a->value);
                Emit(OpCode::MOVE, local_reg, val_reg);
                locals_[n->name] = local_reg;
                CheckReassignment(a, n->name, inferred_type);
                DeclareSymbol(n->name, declared_type, inferred_type);
                FreeRegs(next_reg_ - (local_reg + 1));
                return 0;
            }

            auto val_reg = CompileExpr(a->value);
            auto idx = AddConstant(MakeString(n->name));
            Emit(OpCode::SETGLOBAL, val_reg, idx);
            CheckReassignment(a, n->name, inferred_type);
            DeclareSymbol(n->name, declared_type, inferred_type);
            FreeRegs(next_reg_ - regs_before);
            return 0;
        }
        if (auto* i = dynamic_cast<IndexExpr*>(a->target.get())) {
            uint16_t regs_before = next_reg_;
            auto val_reg = CompileExpr(a->value);
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            auto saved_idx = AllocReg();
            Emit(OpCode::MOVE, saved_idx, idx_reg);
            auto saved_obj = AllocReg();
            Emit(OpCode::MOVE, saved_obj, obj_reg);
            Emit(OpCode::SETINDEX, saved_obj, saved_idx, val_reg);
            FreeRegs(next_reg_ - regs_before);
            return 0;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_this = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if ((name->name == "this")) {
                    is_this = true;
                }
            }

            uint16_t regs_before = next_reg_;
            auto val_reg = CompileExpr(a->value);

            uint16_t obj_reg;
            if (is_this && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }

            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, val_reg);

            FreeRegs(next_reg_ - regs_before);
            return 0;
        }
        FreeRegs(1);
        return 0;
    }
    if (auto* a = dynamic_cast<AugAssignStmt*>(stmt.get())) {
        uint16_t regs_before = next_reg_;
        auto target_reg = CompileExpr(a->target);
        auto val_reg = CompileExpr(a->value);
        auto result_reg = AllocReg();
        Emit(BinOpToOpcode(a->op), result_reg, target_reg, val_reg);
        if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {

            if (!is_top_level_ && (n->name != "this") && locals_.find(n->name) != locals_.end()) {
                Emit(OpCode::MOVE, locals_.at(n->name), result_reg);
                FreeRegs(next_reg_ - regs_before);
                return 0;
            }
            auto idx = AddConstant(MakeString(n->name));
            Emit(OpCode::SETGLOBAL, result_reg, idx);
            FreeRegs(next_reg_ - regs_before);
            return 0;
        }
        if (auto* i = dynamic_cast<IndexExpr*>(a->target.get())) {
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            Emit(OpCode::SETINDEX, obj_reg, idx_reg, result_reg);
            FreeRegs(next_reg_ - regs_before);
            return 0;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_this = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if ((name->name == "this")) {
                    is_this = true;
                }
            }

            uint16_t obj_reg;
            if (is_this && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }

            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, result_reg);

            FreeRegs(next_reg_ - regs_before);
            return 0;
        }
        FreeRegs(next_reg_ - regs_before);
        return 0;
    }
    if (auto* r = dynamic_cast<ReturnStmt*>(stmt.get())) {
        CheckReturnType(r);
        if (r->value) {
            uint16_t regs_before = next_reg_;
            auto reg = CompileExpr(r->value);
			
            for (auto it = pending_finally_stack_.rbegin(); it != pending_finally_stack_.rend(); ++it) {
                CompileChunk(**it);
            }
            Emit(OpCode::RETURN, reg, 1);
            FreeRegs(next_reg_ - regs_before);
        } else {
            for (auto it = pending_finally_stack_.rbegin(); it != pending_finally_stack_.rend(); ++it) {
                CompileChunk(**it);
            }
            Emit(OpCode::RETURN, 0, 0);
        }
        return 0;
    }
    if (dynamic_cast<PassStmt*>(stmt.get())) {
        return 0;
    }
    if (auto* b = dynamic_cast<BreakStmt*>(stmt.get())) {
        (void)b;
        if (loop_depth_ == 0) {
            throw AvaError("'break' outside loop", current_line_, current_col_, source_name_);
        }
        pending_breaks_.push_back({proto_->instructions.size()});
        Emit(OpCode::JMP, 0);
        return 0;
    }
    if (auto* c = dynamic_cast<ContinueStmt*>(stmt.get())) {
        (void)c;
        if (loop_depth_ == 0) {
            throw AvaError("'continue' outside loop", current_line_, current_col_, source_name_);
        }
        pending_continues_.push_back({proto_->instructions.size()});
        Emit(OpCode::JMP, 0);
        return 0;
    }
    if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt.get())) {
        CompileIf(if_stmt);
        return 0;
    }
    if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt.get())) {
        CompileWhile(while_stmt);
        return 0;
    }
    if (auto* for_stmt = dynamic_cast<ForStmt*>(stmt.get())) {
        CompileFor(for_stmt);
        return 0;
    }
    if (auto* for_range_stmt = dynamic_cast<ForRangeStmt*>(stmt.get())) {
        CompileForRange(for_range_stmt);
        return 0;
    }
    if (auto* func_def = dynamic_cast<FuncDef*>(stmt.get())) {
        CompileFunc(func_def);
        return 0;
    }
    if (auto* class_def = dynamic_cast<ClassDef*>(stmt.get())) {
        CompileClass(class_def);
        return 0;
    }
    if (auto* import_stmt = dynamic_cast<ImportStmt*>(stmt.get())) {
        CompileImport(import_stmt);
        return 0;
    }
    if (auto* try_stmt = dynamic_cast<TryStmt*>(stmt.get())) {
        CompileTry(try_stmt);
        return 0;
    }
    if (auto* raise_stmt = dynamic_cast<RaiseStmt*>(stmt.get())) {
        CompileRaise(raise_stmt);
        return 0;
    }
    return 0;
}

void Compiler::CompileMultiAssign(const MultiAssignStmt* stmt) {
    size_t n = std::min(stmt->targets.size(), stmt->values.size());
    if (n == 0) return;

    std::vector<std::string> tmp_vars;
    for (size_t i = 0; i < n; ++i) {
        std::string var = "__ma_tmp_" + std::to_string(i);
        tmp_vars.push_back(var);
        uint16_t regs_before = next_reg_;
        auto vr = CompileExpr(stmt->values[i]);
        Emit(OpCode::SETGLOBAL, vr, AddConstant(MakeString(var)));
        FreeRegs(next_reg_ - regs_before);
    }
    for (size_t i = 0; i < n; ++i) {
        auto tmp_name = std::make_shared<NameExpr>("__ma_tmp_" + std::to_string(i));
        auto assign = std::make_shared<AssignStmt>(stmt->targets[i], tmp_name);
        assign->line = stmt->line;
        CompileStmt(assign);
    }
}

void Compiler::CompileClass(const ClassDef* cls) {
    auto* class_obj = new ClassObj();
    class_obj->name = cls->name;

    ClassObj* base_class = nullptr;
    if (cls->base_class) {
        auto* base_name = dynamic_cast<NameExpr*>(cls->base_class.get());
        if (!base_name) {
            throw AvaError("class '" + cls->name + "': base class must be a plain class name",
                            cls->line, cls->col, source_name_);
        }
        if (base_name->name == cls->name) {
            throw AvaError("class '" + cls->name + "' cannot inherit from itself",
                            cls->line, cls->col, source_name_);
        }
        auto it = compiled_classes_.find(base_name->name);
        if (it == compiled_classes_.end()) {
            throw AvaError("class '" + cls->name + "' extends unknown class '" + base_name->name +
                                "' -- '" + base_name->name +
                                "' must be defined earlier in the file, and the name must be spelled exactly right",
                            cls->line, cls->col, source_name_);
        }
        base_class = it->second;

        for (auto& [mname, mproto] : base_class->methods) {
            if (mname != "__init__" && mname != "__base__") {
                class_obj->methods[mname] = mproto;
            }
        }

        for (auto& [aname, aval] : base_class->instance_defaults) {
            class_obj->instance_defaults[aname] = aval;
        }

        for (auto& pname : base_class->private_members) {
            class_obj->private_members.insert(pname);
        }
    }

    {
        std::unordered_set<std::string> seen_methods;
        for (auto& stmt : cls->body) {
            auto* f = dynamic_cast<FuncDef*>(stmt.get());
            if (!f) continue;
            if (!seen_methods.insert(f->name).second) {
                bool is_ctor = f->name == cls->name;
                std::string msg = is_ctor
                    ? "class '" + cls->name + "' defines the constructor '" + f->name +
                          "' more than once -- AvaLang doesn't support constructor overloading, " +
                          "use default parameter values instead"
                    : "class '" + cls->name + "' defines method '" + f->name + "' more than once";
                throw AvaError(msg, f->line, f->col, source_name_);
            }
        }
    }

    if (cls->base_class) {
        auto bit = class_field_types_.find(base_class->name);
        if (bit != class_field_types_.end()) {
            class_field_types_[cls->name] = bit->second;
        }
    }

    if (cls->base_class) {
        auto dit = class_dynamic_attrs_.find(base_class->name);
        if (dit != class_dynamic_attrs_.end()) {
            class_dynamic_attrs_[cls->name] = dit->second;
        }
    }
    {
        std::unordered_set<std::string> own_dynamic_attrs;
        for (auto& stmt : cls->body) {
            auto* f = dynamic_cast<FuncDef*>(stmt.get());
            if (!f) continue;
            CollectDynamicThisAttrs(f->body, own_dynamic_attrs);
        }
        for (auto& name : own_dynamic_attrs) class_dynamic_attrs_[cls->name].insert(name);
    }

    for (auto& stmt : cls->body) {
        if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
            if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
                Value attr_val;
                if (auto* s = dynamic_cast<StringExpr*>(a->value.get())) {
                    attr_val = MakeString(s->value);
                } else if (auto* num = dynamic_cast<NumberExpr*>(a->value.get())) {
                    attr_val = Value::Number(num->value);
                } else if (dynamic_cast<NilExpr*>(a->value.get())) {
                    attr_val = Value::Nil();
                } else if (auto* b = dynamic_cast<BoolExpr*>(a->value.get())) {
                    attr_val = Value::Bool(b->value);
                } else {
                    attr_val = Value::Nil();
                }

                if (a->is_static) {
                    class_obj->attrs[n->name] = attr_val;
                } else {
                    class_obj->instance_defaults[n->name] = attr_val;
                }
                if (a->is_private) {
                    class_obj->private_members.insert(n->name);
                }

                TypeRef field_type = !a->explicit_type.empty()
                    ? ResolveTypeName(a->explicit_type)
                    : TypeRef{InferExprType(a->value), "", nullptr, nullptr};
                class_field_types_[cls->name][n->name] = field_type;
            }
        }
    }

    if (cls->base_class) {
        auto rit = class_method_returns_.find(base_class->name);
        if (rit != class_method_returns_.end()) {
            for (auto& [mname, mret] : rit->second) {
                if (mname != "__init__") class_method_returns_[cls->name][mname] = mret;
            }
        }
        auto pit = class_method_params_.find(base_class->name);
        if (pit != class_method_params_.end()) {
            for (auto& [mname, mparams] : pit->second) {
                if (mname != "__init__") class_method_params_[cls->name][mname] = mparams;
            }
        }
    }
    for (auto& stmt : cls->body) {
        auto* f = dynamic_cast<FuncDef*>(stmt.get());
        if (!f) continue;
        bool is_ctor = f->name == cls->name;
        std::string method_name = is_ctor ? "__init__" : f->name;

        bool explicit_self_param = !f->params.empty() && f->params[0].first == "this";
        std::vector<std::pair<std::string, Type>> sig;
        for (size_t i = explicit_self_param ? 1 : 0; i < f->params.size(); ++i) {
            Type t = (i < f->param_types.size()) ? TypeFromName(f->param_types[i]) : Type::Unknown;
            sig.push_back({f->params[i].first, t});
        }
        class_method_params_[cls->name][method_name] = std::move(sig);
        class_method_returns_[cls->name][method_name] = ResolveTypeName(f->return_type);
    }

    for (auto& stmt : cls->body) {
        if (auto* f = dynamic_cast<FuncDef*>(stmt.get())) {
            Compiler sub;
            sub.is_top_level_ = false;
            sub.in_async_func_ = f->is_async;
            sub.proto_ = std::make_shared<Proto>();
            sub.proto_->is_async = f->is_async;
            sub.source_name_ = source_name_;
            sub.proto_->source_name = source_name_;
            sub.proto_->debug_name = cls->name + "." + f->name;
            sub.current_base_class_ = base_class;
            sub.compiled_classes_ = compiled_classes_;
            sub.class_field_types_ = class_field_types_;
            sub.class_dynamic_attrs_ = class_dynamic_attrs_;
            sub.class_method_returns_ = class_method_returns_;
            sub.class_method_params_ = class_method_params_;
            sub.known_funcs_ = known_funcs_;
            sub.known_func_returns_ = known_func_returns_;
            sub.known_top_level_globals_ = known_top_level_globals_;
            sub.extern_func_params_ = extern_func_params_;
            sub.extern_func_returns_ = extern_func_returns_;
            sub.has_wildcard_import_ = has_wildcard_import_;

            if (!f->params.empty() && f->params[0].first == "self") {
                throw AvaError(
                    "'self' does not exist in AvaLang -- the instance reference is implicit: "
                    "don't declare it as a parameter. If you need to name it explicitly, use 'this'.",
                    f->line, f->col, source_name_);
            }
            bool explicit_self_param = !f->params.empty() && f->params[0].first == "this";
            size_t real_param_count = f->params.size() - (explicit_self_param ? 1 : 0);

            sub.proto_->num_params = static_cast<uint8_t>(real_param_count + 1);
            sub.proto_->is_vararg = f->is_vararg;
            sub.proto_->is_method = true;

            sub.next_reg_ = static_cast<uint16_t>(real_param_count + 1);
            sub.max_reg_ = sub.next_reg_;

            if (!f->is_static) {
                sub.locals_["this"] = 0;
                Symbol this_sym;
                this_sym.name = "this";
                this_sym.declaredType = Type::Object;
                this_sym.declaredClassName = cls->name;
                this_sym.inferredType = Type::Object;
                this_sym.inferredClassName = cls->name;
                this_sym.RefreshEffectiveType();
                sub.symbols_["this"] = this_sym;
            }

            for (auto& [attr_name, attr_val] : class_obj->instance_defaults) {
                sub.instance_attrs_.insert(attr_name);
            }

            std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> real_params;
            for (size_t i = explicit_self_param ? 1 : 0; i < f->params.size(); ++i) {
                auto& pname = f->params[i].first;
                sub.locals_[pname] = static_cast<uint16_t>(real_params.size() + 1);
                real_params.push_back(f->params[i]);
            }

            sub.EmitDefaultsPrologue(real_params, 1);

            sub.current_return_type_ = ResolveTypeName(f->return_type);

            sub.CompileChunk(f->body);
            sub.Emit(OpCode::RETURN);

            uint16_t min_registers = static_cast<uint16_t>(real_param_count + 1);
            sub.proto_->num_registers = std::max<uint16_t>(sub.max_reg_ + 1, min_registers);

            bool is_constructor = f->name == cls->name;
            std::string method_name = is_constructor ? "__init__" : f->name;
            class_obj->methods[method_name] = sub.proto_;
            if (f->is_private) {
                class_obj->private_members.insert(method_name);
            }
        }
    }

    if (cls->base_class) {
        Value base_val;
        base_val.type = ValueType::Class;
        base_val.obj = base_class;
        Retain(base_val);
        class_obj->attrs["__base__"] = base_val;
        compiled_classes_[cls->name + ".__base__"] = class_obj;
    }
    compiled_classes_[cls->name] = class_obj;

    Value class_val;
    class_val.type = ValueType::Class;
    class_val.obj = class_obj;

    auto class_idx = AddConstant(class_val);
    auto reg = AllocReg();
    Emit(OpCode::LOADK, reg, class_idx);

    auto name_idx = AddConstant(MakeString(cls->name));
    Emit(OpCode::SETGLOBAL, reg, name_idx);
}

void Compiler::CompileImport(const ImportStmt* stmt) {
    if (stmt->alias.empty() && stmt->module_path.size() == 1) {
        has_wildcard_import_ = true;
    }

    std::string full_path;
    for (size_t i = 0; i < stmt->module_path.size(); ++i) {
        if (i > 0) full_path += ".";
        full_path += stmt->module_path[i];
    }

    auto module_idx = AddConstant(MakeString(full_path));
    auto import_reg = AllocReg();
    Emit(OpCode::GETGLOBAL, import_reg, AddConstant(MakeString("__import__")));

    auto mod_arg_reg = AllocReg();
    Emit(OpCode::LOADK, mod_arg_reg, module_idx);

    auto alias_arg_reg = AllocReg();
    Emit(OpCode::LOADK, alias_arg_reg, AddConstant(MakeString(stmt->alias)));

    auto call_reg = AllocReg();
    Emit(OpCode::CALL, import_reg, 2, 1);

    FreeRegs(4);
}

void Compiler::CompileExtern(const ExternStmt* stmt) {

    auto* mod = new ModuleObj();
    mod->name = stmt->alias;
    mod->library = stmt->library;

    for (auto& fn : stmt->functions) {
        std::vector<std::pair<std::string, Type>> sig;
        sig.reserve(fn.params.size());
        for (size_t i = 0; i < fn.params.size(); ++i) {
            Type t = (i < fn.param_types.size()) ? TypeFromName(fn.param_types[i]) : Type::Unknown;
            sig.push_back({fn.params[i], t});
        }
        extern_func_params_[stmt->alias][fn.name] = std::move(sig);
        extern_func_returns_[stmt->alias][fn.name] = ResolveTypeName(fn.return_type);

        auto* meta = new ExternFuncMeta();
        meta->library = stmt->library;
        meta->alias = stmt->alias;
        meta->func_name = fn.name;
        meta->arity = fn.params.size();
        meta->is_vararg = fn.is_vararg;

        auto* native = new NativeObj();
        native->fn = ava_extern_call;
        native->user_data = meta;

        Value fn_val;
        fn_val.type = ValueType::Native;
        fn_val.obj = native;
        mod->attrs[fn.name] = fn_val;
    }

    Value mod_val;
    mod_val.type = ValueType::Module;
    mod_val.obj = mod;

    auto mod_idx = AddConstant(mod_val);
    auto reg = AllocReg();
    Emit(OpCode::LOADK, reg, mod_idx);

    auto alias_idx = AddConstant(MakeString(stmt->alias));
    Emit(OpCode::SETGLOBAL, reg, alias_idx);
    FreeRegs(1);
}

void Compiler::CompileTry(const TryStmt* stmt) {
    std::vector<size_t> except_end_jmps;

    size_t try_instr_idx = proto_->instructions.size();
    Emit(OpCode::TRY, 0);

    bool has_finally = !stmt->finally_body.empty();
    if (has_finally) {
        pending_finally_stack_.push_back(&stmt->finally_body);
    }

    CompileBlock(stmt->try_body);

    Emit(OpCode::TRY_END);

    size_t success_jmp = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    PatchJump(try_instr_idx);

    size_t land_catch = proto_->instructions.size();
    Emit(OpCode::CATCH, 0);

    if (stmt->except_exprs.size() > 0 && stmt->except_exprs[0]) {
        auto exc_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, exc_reg, AddConstant(MakeString("__exception__")));
        auto var_name = std::dynamic_pointer_cast<NameExpr>(stmt->except_exprs[0]);
        if (var_name) {
            auto var_idx = AddConstant(MakeString(var_name->name));
            Emit(OpCode::SETGLOBAL, exc_reg, var_idx);
        }
        FreeRegs(1);
    }

    if (stmt->except_bodies.size() > 0) {
        CompileBlock(stmt->except_bodies[0]);

        size_t end_jmp = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        except_end_jmps.push_back(end_jmp);
    }

    for (size_t i = 1; i < stmt->except_bodies.size(); ++i) {
        size_t catch_instr_idx = proto_->instructions.size();
        Emit(OpCode::CATCH, 0);

        if (stmt->except_exprs[i]) {
            auto exc_reg = AllocReg();
            Emit(OpCode::GETGLOBAL, exc_reg, AddConstant(MakeString("__exception__")));
            auto var_name = std::dynamic_pointer_cast<NameExpr>(stmt->except_exprs[i]);
            if (var_name) {
                auto var_idx = AddConstant(MakeString(var_name->name));
                Emit(OpCode::SETGLOBAL, exc_reg, var_idx);
            }
            FreeRegs(1);
        }

        CompileBlock(stmt->except_bodies[i]);

        size_t end_jmp = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        except_end_jmps.push_back(end_jmp);

        PatchJump(catch_instr_idx);
    }

    if (has_finally) {
        pending_finally_stack_.pop_back();
    }

    auto raise_reg = AllocReg();
    Emit(OpCode::GETGLOBAL, raise_reg, AddConstant(MakeString("__exception__")));
    size_t reraise_jmp = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    PatchJump(success_jmp);
    for (size_t jmp : except_end_jmps) {
        PatchJump(jmp);
    }

    if (has_finally) {
        CompileBlock(stmt->finally_body);
    }
    size_t done_jmp = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    PatchJump(reraise_jmp);
    if (has_finally) {
        CompileBlock(stmt->finally_body);
    }
    Emit(OpCode::RAISE, raise_reg);
    PatchJump(done_jmp);
    FreeRegs(1);
}

void Compiler::CompileRaise(const RaiseStmt* stmt) {
    if (stmt->value) {
        uint16_t regs_before = next_reg_;
        auto exc_reg = CompileExpr(stmt->value);
        Emit(OpCode::RAISE, exc_reg);
        FreeRegs(next_reg_ - regs_before);
    } else {
        uint16_t regs_before = next_reg_;
        auto raise_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, raise_reg, AddConstant(MakeString("__exception__")));
        Emit(OpCode::RAISE, raise_reg);
        FreeRegs(next_reg_ - regs_before);
    }
}

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool IsAlphaNum(char c) { return IsAlpha(c) || IsDigit(c); }

bool IsBinOp(const std::string& s) {
    return s == "+" || s == "-" || s == "*" || s == "/" || s == "%" || s == "//" || s == "**" ||
           s == "==" || s == "!=" || s == "<" || s == ">" || s == "<=" || s == ">=" ||
           s == "and" || s == "or";
}

BinOp ParseBinOp(const std::string& s) {
    if (s == "+") return BinOp::Add;
    if (s == "-") return BinOp::Sub;
    if (s == "*") return BinOp::Mul;
    if (s == "/") return BinOp::Div;
    if (s == "//") return BinOp::IDiv;
    if (s == "%") return BinOp::Mod;
    if (s == "**") return BinOp::Pow;
    if (s == "==") return BinOp::Eq;
    if (s == "!=") return BinOp::Ne;
    if (s == "<") return BinOp::Lt;
    if (s == ">") return BinOp::Gt;
    if (s == "<=") return BinOp::Le;
    if (s == ">=") return BinOp::Ge;
    if (s == "and") return BinOp::And;
    if (s == "or") return BinOp::Or;
    throw std::runtime_error("unknown binop: " + s);
}

size_t SkipWhitespace(const std::string& s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
    return pos;
}

bool StartsWith(const std::string& s, size_t pos, const std::string& prefix) {
    if (pos + prefix.size() > s.size()) return false;
    return s.compare(pos, prefix.size(), prefix) == 0;
}

std::string ParseIdent(const std::string& s, size_t& pos) {
    size_t start = pos;
    while (pos < s.size() && IsAlphaNum(s[pos])) pos++;
    return s.substr(start, pos - start);
}

double ParseNumber(const std::string& s, size_t& pos) {
    size_t start = pos;
    bool has_dot = false;
    while (pos < s.size() && (IsDigit(s[pos]) || s[pos] == '.')) {
        if (s[pos] == '.') {
            if (has_dot) break;
            has_dot = true;
        }
        pos++;
    }
    return std::stod(s.substr(start, pos - start));
}

std::string PeekNextToken(const std::string& s, size_t pos) {
    pos = SkipWhitespace(s, pos);
    if (pos >= s.size()) return "";
    if (IsAlpha(s[pos])) return "ident";
    if (IsDigit(s[pos])) return "number";
    if (s[pos] == '"' || s[pos] == '\'') return "string";
    if (s[pos] == '(') return "(";
    if (s[pos] == ')') return ")";
    if (s[pos] == '[') return "[";
    if (s[pos] == ']') return "]";
    if (s[pos] == '.') return ".";
    if (s[pos] == ',') return ",";
    if (s[pos] == '+' || s[pos] == '-' || s[pos] == '*' || s[pos] == '/' || s[pos] == '%') {
        if (pos + 1 < s.size() && s[pos + 1] == s[pos]) {
            if (s[pos] == '*') return "**";
            if (s[pos] == '/') return "//";
            return "";
        }
        return std::string(1, s[pos]);
    }
    if (s[pos] == '=' || s[pos] == '!' || s[pos] == '<' || s[pos] == '>') {
        if (pos + 1 < s.size() && s[pos + 1] == '=') return std::string(1, s[pos]) + "=";
        return std::string(1, s[pos]);
    }
    if (StartsWith(s, pos, "and")) return "and";
    if (StartsWith(s, pos, "or")) return "or";
    return std::string(1, s[pos]);
}

}

std::shared_ptr<ExprNode> Compiler::ParseFStringExpr(const std::string& expr_str) {
    size_t pos = 0;
    return ParseExpr(expr_str, pos);
}

std::shared_ptr<ExprNode> Compiler::ParseExpr(const std::string& s, size_t& pos) {
    return ParseOrExpr(s, pos);
}

std::shared_ptr<ExprNode> Compiler::ParseOrExpr(const std::string& s, size_t& pos) {
    auto left = ParseAndExpr(s, pos);

    while (true) {
        pos = SkipWhitespace(s, pos);
        std::string op = PeekNextToken(s, pos);
        if (op != "and" && op != "or") break;
        pos += op.size();
        auto right = ParseAndExpr(s, pos);
        left = std::make_shared<BinOpExpr>(ParseBinOp(op), left, right);
    }
    return left;
}

std::shared_ptr<ExprNode> Compiler::ParseAndExpr(const std::string& s, size_t& pos) {
    auto left = ParseComparison(s, pos);

    while (true) {
        pos = SkipWhitespace(s, pos);
        std::string op = PeekNextToken(s, pos);
        if (op != "and" && op != "or") break;
        pos += op.size();
        auto right = ParseComparison(s, pos);
        left = std::make_shared<BinOpExpr>(ParseBinOp(op), left, right);
    }
    return left;
}

std::shared_ptr<ExprNode> Compiler::ParseComparison(const std::string& s, size_t& pos) {
    auto left = ParseAddSub(s, pos);

    while (true) {
        pos = SkipWhitespace(s, pos);
        std::string op = PeekNextToken(s, pos);
        if (!IsBinOp(op)) break;
        pos += op.size();
        auto right = ParseAddSub(s, pos);
        left = std::make_shared<BinOpExpr>(ParseBinOp(op), left, right);
    }
    return left;
}

std::shared_ptr<ExprNode> Compiler::ParseAddSub(const std::string& s, size_t& pos) {
    auto left = ParseMulDiv(s, pos);

    while (true) {
        pos = SkipWhitespace(s, pos);
        std::string op = PeekNextToken(s, pos);
        if (op != "+" && op != "-") break;
        pos += op.size();
        auto right = ParseMulDiv(s, pos);
        left = std::make_shared<BinOpExpr>(ParseBinOp(op), left, right);
    }
    return left;
}

std::shared_ptr<ExprNode> Compiler::ParseMulDiv(const std::string& s, size_t& pos) {
    auto left = ParseUnary(s, pos);

    while (true) {
        pos = SkipWhitespace(s, pos);
        std::string op = PeekNextToken(s, pos);
        if (op != "*" && op != "/" && op != "%") break;
        pos += op.size();
        auto right = ParseUnary(s, pos);
        left = std::make_shared<BinOpExpr>(ParseBinOp(op), left, right);
    }
    return left;
}

std::shared_ptr<ExprNode> Compiler::ParseUnary(const std::string& s, size_t& pos) {
    pos = SkipWhitespace(s, pos);
    if (StartsWith(s, pos, "not ")) {
        pos += 4;
        auto operand = ParseUnary(s, pos);
        return std::make_shared<UnOpExpr>(UnOp::Not, operand);
    }
    if (StartsWith(s, pos, "++")) {
        pos += 2;
        auto operand = ParseUnary(s, pos);
        return std::make_shared<UnOpExpr>(UnOp::Inc, operand);
    }
    if (StartsWith(s, pos, "--")) {
        pos += 2;
        auto operand = ParseUnary(s, pos);
        return std::make_shared<UnOpExpr>(UnOp::Dec, operand);
    }
    if (StartsWith(s, pos, "-")) {
        pos++;
        auto operand = ParseUnary(s, pos);
        return std::make_shared<UnOpExpr>(UnOp::Neg, operand);
    }
    return ParsePower(s, pos);
}

std::shared_ptr<ExprNode> Compiler::ParsePower(const std::string& s, size_t& pos) {
    auto base = ParsePostfix(s, pos);

    pos = SkipWhitespace(s, pos);
    if (StartsWith(s, pos, "**")) {
        pos += 2;
        auto exp = ParseUnary(s, pos);
        return std::make_shared<BinOpExpr>(BinOp::Pow, base, exp);
    }
    return base;
}

std::shared_ptr<ExprNode> Compiler::ParsePostfix(const std::string& s, size_t& pos) {
    auto expr = ParsePrimary(s, pos);

    while (true) {
        pos = SkipWhitespace(s, pos);
        if (pos >= s.size()) break;

        if (s[pos] == '.') {
            pos++;
            std::string attr = ParseIdent(s, pos);
            expr = std::make_shared<AttrExpr>(expr, attr);
        } else if (s[pos] == '[') {
            pos++;
            pos = SkipWhitespace(s, pos);
            auto idx = ParseExpr(s, pos);
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ']') pos++;
            expr = std::make_shared<IndexExpr>(expr, idx);
        } else if (s[pos] == '(') {
            pos++;
            std::vector<std::shared_ptr<ExprNode>> args;
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] != ')') {
                args.push_back(ParseExpr(s, pos));
                while (pos < s.size() && s[pos] == ',') {
                    pos++;
                    pos = SkipWhitespace(s, pos);
                    args.push_back(ParseExpr(s, pos));
                }
            }
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ')') pos++;
            expr = std::make_shared<CallExpr>(expr, args);
        } else {
            break;
        }
    }
    return expr;
}

std::shared_ptr<ExprNode> Compiler::ParsePrimary(const std::string& s, size_t& pos) {
    pos = SkipWhitespace(s, pos);
    if (pos >= s.size()) {
        return std::make_shared<NilExpr>();
    }

    if (s[pos] == '$' && pos + 1 < s.size() && s[pos + 1] == '"') {
        pos += 2;
        size_t start = pos;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) pos += 2;
            else pos++;
        }
        std::string raw = s.substr(start, pos - start);
        if (pos < s.size() && s[pos] == '"') pos++;

        std::vector<std::pair<bool, std::string>> fragments;
        std::string current_literal;
        size_t j = 0;
        while (j < raw.size()) {
            if (raw[j] == '{') {
                if (j + 1 < raw.size() && raw[j + 1] == '{') {
                    current_literal += '{';
                    j += 2;
                } else {
                    if (!current_literal.empty()) {
                        fragments.push_back({false, current_literal});
                        current_literal.clear();
                    }
                    size_t fstart = j + 1;
                    size_t brace_count = 1;
                    j++;
                    while (j < raw.size() && brace_count > 0) {
                        if (raw[j] == '{') brace_count++;
                        else if (raw[j] == '}') brace_count--;
                        j++;
                    }
                    if (brace_count == 0) {
                        std::string expr_str = raw.substr(fstart, j - fstart - 1);
                        if (!expr_str.empty()) {
                            fragments.push_back({true, expr_str});
                        }
                    }
                }
            } else if (raw[j] == '}') {
                if (j + 1 < raw.size() && raw[j + 1] == '}') {
                    current_literal += '}';
                    j += 2;
                } else {
                    current_literal += '}';
                    j++;
                }
            } else if (raw[j] == '\\' && j + 1 < raw.size()) {
                char next = raw[j + 1];
                switch (next) {
                    case 'n': current_literal += '\n'; break;
                    case 't': current_literal += '\t'; break;
                    case 'r': current_literal += '\r'; break;
                    case 'b': current_literal += '\b'; break;
                    case '"': current_literal += '"'; break;
                    case '\'': current_literal += '\''; break;
                    case '\\': current_literal += '\\'; break;
                    default: current_literal += raw[j]; current_literal += raw[j + 1]; break;
                }
                j += 2;
            } else {
                current_literal += raw[j];
                j++;
            }
        }
        if (!current_literal.empty()) {
            fragments.push_back({false, current_literal});
        }
        return std::make_shared<FStringExpr>(fragments);
    }

    if (s[pos] == '(') {
        pos++;
        pos = SkipWhitespace(s, pos);
        auto expr = ParseExpr(s, pos);
        pos = SkipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ')') pos++;
        return expr;
    }

    if (s[pos] == '[') {
        pos++;
        std::vector<std::shared_ptr<ExprNode>> items;
        pos = SkipWhitespace(s, pos);
        if (pos < s.size() && s[pos] != ']') {
            items.push_back(ParseExpr(s, pos));
            pos = SkipWhitespace(s, pos);
            while (pos < s.size() && s[pos] == ',') {
                pos++;
                pos = SkipWhitespace(s, pos);
                if (pos < s.size() && s[pos] == ']') break; // trailing comma
                items.push_back(ParseExpr(s, pos));
                pos = SkipWhitespace(s, pos);
            }
        }
        if (pos < s.size() && s[pos] == ']') pos++;
        return std::make_shared<ListExpr>(items);
    }

    if (s[pos] == '{') {
        pos++;
        std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> entries;
        pos = SkipWhitespace(s, pos);
        while (pos < s.size() && s[pos] != '}') {
            std::string key;
            if (s[pos] == '"' || s[pos] == '\'') {
                char delim = s[pos];
                pos++;
                while (pos < s.size() && s[pos] != delim) { key += s[pos]; pos++; }
                if (pos < s.size() && s[pos] == delim) pos++;
            } else {
                key = ParseIdent(s, pos);
            }
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ':') pos++;
            pos = SkipWhitespace(s, pos);
            auto val = ParseExpr(s, pos);
            entries.push_back({key, val});
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') {
                pos++;
                pos = SkipWhitespace(s, pos);
            }
        }
        if (pos < s.size() && s[pos] == '}') pos++;
        return std::make_shared<DictExpr>(entries);
    }

    if (IsDigit(s[pos])) {
        double val = ParseNumber(s, pos);
        return std::make_shared<NumberExpr>(val);
    }

    if (s[pos] == '"' || s[pos] == '\'') {
        char delim = s[pos];
        pos++;
        std::string val;
        while (pos < s.size() && s[pos] != delim) {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                switch (s[pos]) {
                    case 'n': val += '\n'; break;
                    case 't': val += '\t'; break;
                    case 'r': val += '\r'; break;
                    case 'b': val += '\b'; break;
                    default: val += s[pos]; break;
                }
            } else {
                val += s[pos];
            }
            pos++;
        }
        if (pos < s.size() && s[pos] == delim) pos++;
        return std::make_shared<StringExpr>(val);
    }

    if (s[pos] == 't' && StartsWith(s, pos, "true")) {
        pos += 4;
        return std::make_shared<BoolExpr>(true);
    }
    if (s[pos] == 'f' && StartsWith(s, pos, "false")) {
        pos += 5;
        return std::make_shared<BoolExpr>(false);
    }
    if (s[pos] == 'n' && StartsWith(s, pos, "nil")) {
        pos += 3;
        return std::make_shared<NilExpr>();
    }

    std::string name = ParseIdent(s, pos);
    return std::make_shared<NameExpr>(name);
}

uint16_t Compiler::CompileFStringExpression(const std::string& expr_str) {
    auto expr = ParseFStringExpr(expr_str);
    return CompileExpr(expr);
}

}
