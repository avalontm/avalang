#include "compiler.h"
#include <stdexcept>

namespace ava {

Value Compiler::MakeString(const std::string& s) {
    Value v;
    v.type = ValueType::String;
    v.obj = new StringObj(s);
    return v;
}

void Compiler::Reset() {
    proto_ = std::make_shared<Proto>();
    next_reg_ = 0;
    max_reg_ = 0;
    locals_.clear();
    pending_breaks_.clear();
    pending_continues_.clear();
    parent_locals_.clear();
    parent_ = nullptr;
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
    if (next_reg_ > max_reg_) max_reg_ = next_reg_;
}

void Compiler::PatchJump(size_t instr_idx) {
    auto& instr = proto_->instructions[instr_idx];
    int32_t offset = static_cast<int32_t>(proto_->instructions.size()) -
                     static_cast<int32_t>(instr_idx) - 1;
    instr.b = static_cast<uint16_t>(static_cast<int16_t>(offset));
}

OpCode Compiler::BinOpToOpcode(BinOp op) {
    switch (op) {
        case BinOp::Add: return OpCode::ADD;
        case BinOp::Sub: return OpCode::SUB;
        case BinOp::Mul: return OpCode::MUL;
        case BinOp::Div: return OpCode::DIV;
        case BinOp::Mod: return OpCode::MOD;
        case BinOp::Pow: return OpCode::POW;
        case BinOp::Eq:  return OpCode::EQ;
        case BinOp::Ne:  return OpCode::NE;
        case BinOp::Lt:  return OpCode::LT;
        case BinOp::Le:  return OpCode::LE;
        case BinOp::Gt:  return OpCode::GT;
        case BinOp::Ge:  return OpCode::GE;
        default: break;
    }
    throw std::runtime_error("BinOpToOpcode: and/or not handled here");
}

bool Compiler::IsShortCircuit(BinOp op) {
    return op == BinOp::And || op == BinOp::Or;
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
        if (n->name == "this" || n->name == "self") {
            if (locals_.find("this") != locals_.end()) {
                return locals_.at("this");
            }
        }

        for (size_t upval_idx = 0; upval_idx < proto_->upvalue_descs.size(); ++upval_idx) {
            auto& uvd = proto_->upvalue_descs[upval_idx];
            if (uvd.from_parent_local) {
                for (auto& [pname, preg] : parent_locals_) {
                    if (pname == n->name && preg == uvd.index) {
                        auto reg = AllocReg();
                        Emit(OpCode::GETUPVAL, reg, static_cast<uint16_t>(upval_idx));
                        return reg;
                    }
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
        if (IsShortCircuit(b->op)) {
            auto result_reg = AllocReg();
            if (b->op == BinOp::And) {
                auto left_reg = CompileExpr(b->left);
                Emit(OpCode::TEST, left_reg, 0);
                size_t jmp_idx = proto_->instructions.size();
                Emit(OpCode::JMP, 0);
                auto right_reg = CompileExpr(b->right);
                Emit(OpCode::MOVE, result_reg, right_reg);
                PatchJump(jmp_idx);
                Emit(OpCode::MOVE, result_reg, left_reg);
            } else {
                auto left_reg = CompileExpr(b->left);
                Emit(OpCode::TEST, left_reg, 1);
                size_t jmp_idx = proto_->instructions.size();
                Emit(OpCode::JMP, 0);
                auto right_reg = CompileExpr(b->right);
                Emit(OpCode::MOVE, result_reg, right_reg);
                PatchJump(jmp_idx);
                Emit(OpCode::MOVE, result_reg, left_reg);
            }
            return result_reg;
        }

        auto left_reg = CompileExpr(b->left);
        auto right_reg = CompileExpr(b->right);
        auto result_reg = AllocReg();
        Emit(BinOpToOpcode(b->op), result_reg, left_reg, right_reg);
        return result_reg;
    }

    if (auto* u = dynamic_cast<UnOpExpr*>(expr.get())) {
        auto reg = CompileExpr(u->operand);
        if (u->op == UnOp::Neg) {
            auto result = AllocReg();
            Emit(OpCode::NEG, result, reg);
            return result;
        }
        auto result = AllocReg();
        Emit(OpCode::NOT, result, reg);
        return result;
    }

    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        auto callee_reg = CompileExpr(c->callee);
        std::vector<uint16_t> arg_regs;
        for (auto& arg : c->args) {
            arg_regs.push_back(CompileExpr(arg));
        }
        uint8_t argc = static_cast<uint8_t>(arg_regs.size());
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            Emit(OpCode::MOVE, callee_reg + 1 + i, arg_regs[i]);
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
            auto val_reg = CompileExpr(val);
            auto key_idx = AddConstant(MakeString(key));
            auto saved_idx = AllocReg();
            Emit(OpCode::LOADK, saved_idx, key_idx);
            Emit(OpCode::SETINDEX, reg, saved_idx, val_reg);
            FreeRegs(2);
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

        // Reserve the 4 fixed argument slots (obj, start, end, step) up
        // front. If we only call AllocReg() for slots that are actually
        // present, a skipped slot (e.g. missing `start` in arr[:3], which
        // is filled with a bare LOADNIL) never advances next_reg_, so the
        // *next* CompileExpr call reuses that same register and clobbers
        // the value we just wrote there.
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
        if (locals_.find("this") == locals_.end()) {
            throw std::runtime_error("base() can only be used inside a method");
        }

        auto method_idx = AddConstant(MakeString("__init__"));
        uint8_t argc = static_cast<uint8_t>(s->args.size());
        std::vector<uint16_t> arg_regs;
        for (auto& arg : s->args) {
            arg_regs.push_back(CompileExpr(arg));
        }
        for (size_t i = 0; i < arg_regs.size(); ++i) {
            Emit(OpCode::MOVE, 1 + i, arg_regs[i]);
        }
        auto result_reg = AllocReg();
        Emit(OpCode::BASECALL, result_reg, method_idx, argc);
        FreeRegs(arg_regs.size());
        return result_reg;
    }

    if (auto* l = dynamic_cast<LambdaExpr*>(expr.get())) {
        Compiler sub;
        sub.proto_ = std::make_shared<Proto>();
        sub.proto_->debug_name = l->name;
        sub.proto_->num_params = static_cast<uint8_t>(l->defaults.size());
        sub.proto_->is_vararg = l->is_vararg;

        sub.next_reg_ = 1;
        sub.max_reg_ = sub.next_reg_;
        for (auto& [pname, def] : l->defaults) {
            sub.locals_[pname] = sub.next_reg_;
            sub.next_reg_++;
        }

        for (auto& [name, reg] : locals_) {
            if (name != "this") {
                sub.parent_locals_.push_back({name, reg});
                sub.proto_->upvalue_descs.push_back({true, reg});
                sub.next_reg_++;
            }
        }

        sub.CompileChunk(l->body);
        sub.proto_->instructions.push_back({OpCode::RETURN, 0, 0});
        sub.proto_->num_registers = std::max<uint16_t>(sub.max_reg_ + 1, sub.next_reg_);

        uint16_t child_idx = static_cast<uint16_t>(proto_->child_protos.size());
        proto_->child_protos.push_back(sub.proto_);

        auto reg = AllocReg();
        Emit(OpCode::CLOSURE, reg, child_idx);
        return reg;
    }

    throw std::runtime_error("unknown expr type in compiler");
}

void Compiler::CompileStmt(const std::shared_ptr<StmtNode>& stmt) {
    if (auto* e = dynamic_cast<ExprStmt*>(stmt.get())) {
        CompileExpr(e->expr);
        return;
    }

    if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
        if (!a->target) {
            FreeRegs(1);
            return;
        }
        if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
            bool in_method = locals_.find("this") != locals_.end();
            bool has_local = locals_.find(n->name) != locals_.end();
            bool is_known_attr = instance_attrs_.find(n->name) != instance_attrs_.end();
            auto val_reg = CompileExpr(a->value);
            if (in_method && n->name != "this" && n->name != "self" && !has_local && is_known_attr) {
                auto attr_idx = AddConstant(MakeString(n->name));
                auto this_reg = locals_.at("this");
                Emit(OpCode::SETATTR, this_reg, attr_idx, val_reg);
                FreeRegs(1);
                return;
            }
            auto idx = AddConstant(MakeString(n->name));
            Emit(OpCode::SETGLOBAL, val_reg, idx);
            FreeRegs(1);
            return;
        }
        if (auto* i = dynamic_cast<IndexExpr*>(a->target.get())) {
            auto val_reg = CompileExpr(a->value);
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            auto saved_idx = AllocReg();
            Emit(OpCode::MOVE, saved_idx, idx_reg);
            auto saved_obj = AllocReg();
            Emit(OpCode::MOVE, saved_obj, obj_reg);
            Emit(OpCode::SETINDEX, saved_obj, saved_idx, val_reg);
            FreeRegs(5);
            return;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_self = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if (name->name == "this" || name->name == "self") {
                    is_self = true;
                }
            }
            
            auto val_reg = CompileExpr(a->value);
            
            uint16_t obj_reg;
            if (is_self && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }
            
            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, val_reg);
            
            if (!is_self) {
                FreeRegs(1);
            }
            FreeRegs(1);
            return;
        }
        FreeRegs(1);
        return;
    }

    if (auto* a = dynamic_cast<AugAssignStmt*>(stmt.get())) {
        auto target_reg = CompileExpr(a->target);
        auto val_reg = CompileExpr(a->value);
        auto result_reg = AllocReg();
        Emit(BinOpToOpcode(a->op), result_reg, target_reg, val_reg);
        if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
            auto idx = AddConstant(MakeString(n->name));
            Emit(OpCode::SETGLOBAL, result_reg, idx);
            FreeRegs(2);
            return;
        }
        if (auto* i = dynamic_cast<IndexExpr*>(a->target.get())) {
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            Emit(OpCode::SETINDEX, obj_reg, idx_reg, result_reg);
            FreeRegs(3);
            return;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_self = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if (name->name == "this" || name->name == "self") {
                    is_self = true;
                }
            }
            
            uint16_t obj_reg;
            if (is_self && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }
            
            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, result_reg);
            
            if (!is_self) {
                FreeRegs(1);
            }
            return;
        }
        FreeRegs(1);
        return;
    }

    if (auto* r = dynamic_cast<ReturnStmt*>(stmt.get())) {
        if (r->value) {
            auto reg = CompileExpr(r->value);
            Emit(OpCode::RETURN, reg, 1);
            FreeRegs(1);
        } else {
            Emit(OpCode::RETURN, 0, 0);
        }
        return;
    }

    if (auto* b = dynamic_cast<BreakStmt*>(stmt.get())) {
        (void)b;
        pending_breaks_.push_back({proto_->instructions.size()});
        Emit(OpCode::JMP, 0);
        return;
    }

    if (auto* c = dynamic_cast<ContinueStmt*>(stmt.get())) {
        (void)c;
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

    (void)stmt;
}

void Compiler::CompileIf(const IfStmt* stmt) {
    auto cond_reg = CompileExpr(stmt->condition);
    Emit(OpCode::TEST, cond_reg, 1);
    size_t jmp_to_else_or_next = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    CompileChunk(stmt->then_body);

    std::vector<size_t> exit_jmps;
    size_t jmp_after_if = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    exit_jmps.push_back(jmp_after_if);

    PatchJump(jmp_to_else_or_next);

    for (auto& [elif_cond, elif_body] : stmt->elif_clauses) {
        auto ec_reg = CompileExpr(elif_cond);
        Emit(OpCode::TEST, ec_reg, 1);
        size_t jmp_to_next = proto_->instructions.size();
        Emit(OpCode::JMP, 0);

        CompileChunk(elif_body);

        size_t jmp_out = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        exit_jmps.push_back(jmp_out);

        PatchJump(jmp_to_next);
    }

    if (!stmt->else_body.empty()) {
        CompileChunk(stmt->else_body);
    }

    for (size_t idx : exit_jmps) {
        PatchJump(idx);
    }

    FreeRegs(1);
}

void Compiler::CompileWhile(const WhileStmt* stmt) {
    size_t loop_start = proto_->instructions.size();
    auto cond_reg = CompileExpr(stmt->condition);
    Emit(OpCode::TEST, cond_reg, 0);
    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    CompileChunk(stmt->body);

    size_t jmp_back = proto_->instructions.size();
    int32_t offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(jmp_back) - 1;
    Emit(OpCode::JMP, 0, static_cast<uint16_t>(static_cast<int16_t>(offset)));

    PatchJump(jmp_out);

    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchJump(patch.instr_idx);
    }
    pending_continues_.clear();
}

void Compiler::CompileFor(const ForStmt* stmt) {
    auto list_var = AddConstant(MakeString("__for_list"));
    auto len_var = AddConstant(MakeString("__for_len"));
    auto idx_var = AddConstant(MakeString("__for_idx"));
    auto elem_var = AddConstant(MakeString(stmt->var_name));
    
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
    
    Emit(OpCode::LOADNIL, 1);
    Emit(OpCode::SETGLOBAL, 1, idx_var);
    
    size_t loop_start = proto_->instructions.size();
    
    auto elem_reg = AllocReg();
    auto idx_get = AllocReg();
    auto list_get = AllocReg();
    auto len_get = AllocReg();
    auto cond_reg = AllocReg();
    auto one_c = AddConstant(Value::Number(1));
    auto add_reg = AllocReg();
    
    max_reg_ = std::max(max_reg_, static_cast<uint16_t>(206));
    
    Emit(OpCode::GETGLOBAL, list_get, list_var);
    Emit(OpCode::GETGLOBAL, idx_get, idx_var);
    Emit(OpCode::GETINDEX, elem_reg, list_get, idx_get);
    Emit(OpCode::SETGLOBAL, elem_reg, elem_var);
    
    CompileChunk(stmt->body);
    
    Emit(OpCode::GETGLOBAL, idx_get, idx_var);
    Emit(OpCode::LOADK, add_reg, one_c);
    Emit(OpCode::ADD, add_reg, idx_get, add_reg);
    Emit(OpCode::SETGLOBAL, add_reg, idx_var);
    
    Emit(OpCode::GETGLOBAL, cond_reg, idx_var);
    Emit(OpCode::GETGLOBAL, len_get, len_var);
    Emit(OpCode::LT, cond_reg, cond_reg, len_get);
    Emit(OpCode::TEST, cond_reg, 0);
    
    size_t jmp_out = proto_->instructions.size();
    Emit(OpCode::JMP, 0);
    
    int32_t back_offset = static_cast<int32_t>(loop_start) - static_cast<int32_t>(proto_->instructions.size()) - 1;
    Emit(OpCode::JMP, 0, static_cast<uint16_t>(static_cast<int16_t>(back_offset)));
    
    PatchJump(jmp_out);
    
    for (auto& patch : pending_breaks_) {
        PatchJump(patch.instr_idx);
    }
    pending_breaks_.clear();

    for (auto& patch : pending_continues_) {
        PatchJump(patch.instr_idx);
    }
    pending_continues_.clear();
}

void Compiler::CompileFunc(const FuncDef* func) {
    Compiler sub;
    sub.proto_ = std::make_shared<Proto>();
    sub.proto_->debug_name = func->name;
    sub.proto_->num_params = static_cast<uint8_t>(func->params.size());
    sub.proto_->is_vararg = func->is_vararg;

    sub.next_reg_ = 1;
    sub.max_reg_ = sub.next_reg_;
    for (auto& [pname, def] : func->params) {
        sub.locals_[pname] = sub.next_reg_;
        sub.next_reg_++;
    }

    sub.CompileChunk(func->body);
    sub.proto_->instructions.push_back({OpCode::RETURN, 0, 0});
    sub.proto_->num_registers = sub.max_reg_ + 1;

    uint16_t child_idx = static_cast<uint16_t>(proto_->child_protos.size());
    proto_->child_protos.push_back(sub.proto_);

    auto reg = AllocReg();
    Emit(OpCode::CLOSURE, reg, child_idx);

    auto name_idx = AddConstant(MakeString(func->name));
    Emit(OpCode::SETGLOBAL, reg, name_idx);
}

void Compiler::CompileChunk(const std::vector<std::shared_ptr<StmtNode>>& stmts) {
    for (auto& stmt : stmts) {
        CompileStmt(stmt);
    }
}

std::shared_ptr<Proto> Compiler::Compile(const std::shared_ptr<Chunk>& chunk) {
    Reset();
    CompileChunk(chunk->statements);
    Emit(OpCode::RETURN, 0, 0);
    proto_->num_registers = max_reg_ + 1;
    return proto_;
}

void Compiler::CompileClass(const ClassDef* cls) {
    auto* class_obj = new ClassObj();
    class_obj->name = cls->name;
    
    if (cls->base_class) {
        if (auto* base_name = dynamic_cast<NameExpr*>(cls->base_class.get())) {
            auto it = compiled_classes_.find(base_name->name);
            if (it != compiled_classes_.end()) {
                auto* base_class = it->second;
                for (auto& [mname, mproto] : base_class->methods) {
                    if (mname != "__init__" && mname != "__base__") {
                        class_obj->methods[mname] = mproto;
                    }
                }
                for (auto& [aname, aval] : base_class->attrs) {
                    if (aname != "__base__") {
                        class_obj->attrs[aname] = aval;
                    }
                }
            }
        }
    }
    
    for (auto& stmt : cls->body) {
        if (auto* a = dynamic_cast<AssignStmt*>(stmt.get())) {
            if (auto* n = dynamic_cast<NameExpr*>(a->target.get())) {
                if (auto* s = dynamic_cast<StringExpr*>(a->value.get())) {
                    class_obj->attrs[n->name] = MakeString(s->value);
                } else if (auto* num = dynamic_cast<NumberExpr*>(a->value.get())) {
                    class_obj->attrs[n->name] = Value::Number(num->value);
                } else if (dynamic_cast<NilExpr*>(a->value.get())) {
                    class_obj->attrs[n->name] = Value::Nil();
                } else if (auto* b = dynamic_cast<BoolExpr*>(a->value.get())) {
                    class_obj->attrs[n->name] = Value::Bool(b->value);
                } else {
                    class_obj->attrs[n->name] = Value::Nil();
                }
            }
        }
    }
        
    for (auto& stmt : cls->body) {
        if (auto* f = dynamic_cast<FuncDef*>(stmt.get())) {
            Compiler sub;
            sub.proto_ = std::make_shared<Proto>();
            sub.proto_->debug_name = cls->name + "." + f->name;
            sub.proto_->num_params = static_cast<uint8_t>(f->params.size() + 1);
            sub.proto_->is_vararg = f->is_vararg;
            sub.proto_->is_method = true;
            
            sub.next_reg_ = static_cast<uint16_t>(f->params.size() + 1);
            sub.max_reg_ = sub.next_reg_;
            sub.locals_["this"] = 0;
            for (auto& [attr_name, attr_val] : class_obj->attrs) {
                sub.instance_attrs_.insert(attr_name);
            }
            
            for (size_t i = 0; i < f->params.size(); ++i) {
                auto& pname = f->params[i].first;
                sub.locals_[pname] = static_cast<uint16_t>(i + 1);
            }
            
            sub.CompileChunk(f->body);
            sub.proto_->instructions.push_back({OpCode::RETURN, 0, 0});
            
            uint16_t min_registers = static_cast<uint16_t>(f->params.size() + 1);
            sub.proto_->num_registers = std::max<uint16_t>(sub.max_reg_ + 1, min_registers);
            
            std::string method_name = f->name == "init" ? "__init__" : f->name;
            class_obj->methods[method_name] = sub.proto_;
            if (f->name == "init") {
                class_obj->methods["init"] = sub.proto_;
            }
        }
    }
    
    if (cls->base_class) {
        auto base_name_expr = dynamic_cast<NameExpr*>(cls->base_class.get());
        if (base_name_expr) {
            auto it = compiled_classes_.find(base_name_expr->name);
            if (it != compiled_classes_.end()) {
                Value base_val;
                base_val.type = ValueType::Class;
                base_val.obj = it->second;
                class_obj->attrs["__base__"] = base_val;
            }
        }
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

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool IsAlphaNum(char c) { return IsAlpha(c) || IsDigit(c); }

bool IsBinOp(const std::string& s) {
    return s == "+" || s == "-" || s == "*" || s == "/" || s == "%" || s == "**" ||
           s == "==" || s == "!=" || s == "<" || s == ">" || s == "<=" || s == ">=" ||
           s == "and" || s == "or";
}

BinOp ParseBinOp(const std::string& s) {
    if (s == "+") return BinOp::Add;
    if (s == "-") return BinOp::Sub;
    if (s == "*") return BinOp::Mul;
    if (s == "/") return BinOp::Div;
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

} // namespace

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
    
    if (s[pos] == '(') {
        pos++;
        pos = SkipWhitespace(s, pos);
        auto expr = ParseExpr(s, pos);
        pos = SkipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ')') pos++;
        return expr;
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

} // namespace ava