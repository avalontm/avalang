#include "compiler.h"
#include "../vm/vm_extern.h"
#include "../common/ava_error.h"
#include <stdexcept>
#include <cstdio>

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
        if (uvd.from_parent_local) {
            for (auto& [pname, preg] : parent_locals_) {
                if (pname == name && preg == uvd.index) {
                    return static_cast<int16_t>(upval_idx);
                }
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

// Phase 9 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de parámetros
// en llamadas"). Pre-pass over a chunk's OWN statement list -- same
// granularity as RejectDuplicateFuncDefs right above (called from the same
// place, CompileChunk, on every invocation including nested if/while/for
// bodies that share their enclosing function's Compiler instance -- see
// compiler.h's comment on symbols_ for why that sharing is the existing,
// documented architecture, not something new introduced here). Records
// each FuncDef's parameter names + resolved types (Phase 8's `param_types`
// strings, turned into `ava::Type` via TypeFromName) into `out`, keyed by
// function name, so CompileExpr(CallExpr) can check a call's arguments
// against them without caring whether the call textually precedes or
// follows the def in the same chunk (this runs before any statement in
// `stmts` is compiled). A parameter with no `as Type` annotation records
// Type::Unknown for that position -- Compiler::CheckCallArgs treats that
// as "nothing to check", same convention as the rest of the type system.
// Does NOT recurse into nested blocks (a `func` declared inside an `if`
// inside this chunk is still found, since RejectDuplicateFuncDefs's own
// non-recursion into ITS `stmts` doesn't apply here -- this is a shallow
// scan of exactly the same `stmts` list); does NOT touch class bodies or
// externs (methods are called via AttrExpr, not a plain NameExpr, so
// Compiler::CheckCallArgs never looks them up here regardless).
//
// Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de retornos")
// extends this same pre-pass to also fill `out_returns`, keyed by the same
// function name, with the resolved Type of FuncDef::return_type (Phase 8)
// -- Type::Unknown for no annotation or an unrecognized type name, same
// convention as `out`'s per-parameter Type::Unknown. Consumed by
// InferExprType's CallExpr case, not by CheckReturnType (which only cares
// about the CURRENTLY-COMPILING function's own return_type, tracked
// separately via current_return_type_ -- this map is for inferring the
// type of a *call expression*, e.g. `y = add(1, 2)`).
// Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md (\"Clases y objetos\").
// Free-function counterpart to Compiler::ResolveTypeName -- exists as its
// own static function (instead of just calling the member function)
// because CollectFuncSignatures below is itself a static free function,
// called before any Compiler instance-specific work happens for a chunk,
// so it only gets handed the one map (compiled_classes) it actually
// needs rather than a whole Compiler&. Compiler::ResolveTypeName
// (compiler.h) delegates to this for the exact same logic.
static TypeRef ResolveTypeNameAgainst(const std::string& name,
                                       const std::unordered_map<std::string, ClassObj*>& compiled_classes) {
    Type prim = TypeFromName(name);
    if (prim != Type::Unknown) return TypeRef{prim, "", nullptr, nullptr};
    if (compiled_classes.count(name)) return TypeRef{Type::Object, name, nullptr, nullptr};
    return TypeRef{Type::Unknown, "", nullptr, nullptr};
}

// Phase 13: out_returns is now TypeRef-keyed (was plain Type) since a
// free function's declared return type can itself be a class name
// (`func make() as User ... end`) -- see known_func_returns_'s comment
// in compiler.h. `compiled_classes` is threaded through so this static
// function can resolve those class-typed return annotations the same
// way Compiler::ResolveTypeName does, without needing a full Compiler
// instance. Parameter types (`out`) are deliberately left as plain Type,
// unchanged -- see class_method_params_'s comment in compiler.h for why
// parameter class-awareness isn't extended by this phase.
static void CollectFuncSignatures(
        const std::vector<std::shared_ptr<StmtNode>>& stmts,
        std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>& out,
        std::unordered_map<std::string, TypeRef>& out_returns,
        const std::unordered_map<std::string, ClassObj*>& compiled_classes) {
    for (auto& stmt : stmts) {
        auto* f = dynamic_cast<FuncDef*>(stmt.get());
        if (!f) continue;
        std::vector<std::pair<std::string, Type>> sig;
        sig.reserve(f->params.size());
        for (size_t i = 0; i < f->params.size(); ++i) {
            Type t = (i < f->param_types.size()) ? TypeFromName(f->param_types[i]) : Type::Unknown;
            sig.push_back({f->params[i].first, t});
        }
        out[f->name] = std::move(sig);
        out_returns[f->name] = ResolveTypeNameAgainst(f->return_type, compiled_classes);
    }
}

// Phase 6 of AvaLang_Plan_Sistema_de_Tipos.md ("Validacion de anotaciones").
// Called from both AssignStmt paths (CompileStmt and its CompileExprToReg
// mirror) right after resolving `declared_type`/`inferred_type` for a
// statement that carries an explicit `as Type` (explicit_type non-empty).
// A no-op when there's no annotation at all -- ordinary `x = expr` is never
// touched by this. Two things can go wrong:
//   1. `explicit_type` doesn't name a real primitive (e.g. a typo, `age as
//      itn`) -- TypeFromName already returned Type::Unknown for it, which
//      is reported here as "unknown type" rather than silently doing
//      nothing (that would leave a typo'd annotation with no effect at
//      all, which is worse than an error).
//   2. Both types resolved to something real and they don't match (e.g.
//      `age as int = "hello"`). `inferred_type == Type::Unknown` (the
//      value has no initializer yet, per Phase 4/7, or its expression form
//      InferExprType doesn't resolve -- calls, indexing, collections, etc,
//      Phases 8/12/13/14/15) is NOT an error: there's nothing to compare
//      against yet, so the annotation is taken on faith until a later
//      phase can actually check it.
// Deliberately exact-match only: an int value against a `float`
// annotation is also flagged, since numeric coercion is explicitly left
// as an open decision (plan, section 24) rather than assumed safe here.
// Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos"). Error-
// message spelling for a TypeRef: the class name when it's an Object,
// TypeName() otherwise. Used by ValidateTypeAnnotation/ValidateReassignment/
// CheckReturnType below instead of the plain TypeName() so a mismatch
// against a class-typed annotation names the actual class ("expected class
// User") rather than the generic "object" TypeName() falls back to when it
// has no class name available.
// Phase 15 of AvaLang_Plan_Sistema_de_Tipos.md ("Colecciones"): extended
// with List/Dict cases, recursing into element_type/key_type when they're
// resolved (nullptr -- "unknown element type", see TypeRef's comment in
// type.h -- falls back to the bare "list"/"dict" TypeName()). E.g. `list
// of int`, `dict of string to bool`, or a bare `list` for `[]` / a
// mixed-element literal.
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
    // Phase 15: mismatch computed via TypeRefEquals (type.h) instead of a
    // hand-rolled Object-only comparison, so a declared List/Dict
    // annotation (once a later phase gives them declaration syntax, plan
    // section 19) also gets its element/key type cross-checked the same
    // way a class annotation's name already was -- see TypeRefEquals'
    // comment for exactly when it does/doesn't flag a mismatch.
    if (!TypeRefEquals(declared_type, inferred_type)) {
        std::string msg = "type mismatch: expected " + DisplayType(declared_type) +
                           ", received " + DisplayType(inferred_type);
        throw AvaError(msg, line, col, source_name);
    }
}

// Phase 7 of AvaLang_Plan_Sistema_de_Tipos.md ("Asignaciones posteriores"),
// plan section 11, point 2: a plain `identifier = expr` with no `as` on
// this line, where `identifier` already has a symbol recorded in this
// scope, is a *reassignment* -- its value must be compatible with the type
// already fixed for that name (its `effectiveType`: whichever of
// declaredType/inferredType is authoritative, see symbol.h), not treated
// as a brand-new inferred declaration. Same shape/message as
// ValidateTypeAnnotation, just keyed off a pre-existing Symbol instead of
// an `as Type` written on this line.
//
// `existing` is nullptr when the name has no prior symbol at all (first
// assignment ever -- ordinary inference applies, nothing to check) or when
// its effectiveType is still Type::Unknown (nothing resolved yet either).
// `inferred_type == Type::Unknown` (this value's expression form isn't
// resolved by InferExprType yet) also skips the check, same reasoning as
// ValidateTypeAnnotation: no false positives on something the type system
// can't evaluate. Callers skip calling this entirely for `local`
// declarations (AssignStmt::is_local) and for lines that carry their own
// `as Type` (ValidateTypeAnnotation covers those against THIS line's
// annotation instead -- whether that annotation is itself allowed to
// override a different previously-fixed type for the same name is left
// open, same as the "conflict between two explicit annotations" note on
// DeclareSymbol).
// Phase 13: `inferred_type` is now a TypeRef (was plain Type), so a
// reassignment to a class-typed name (`user = OtherClass(...)`) is caught
// the same way a primitive mismatch already was -- same DisplayType
// spelling ValidateTypeAnnotation/CheckReturnType use, so the error names
// the actual class instead of falling back to Type::Object's generic
// "object" TypeName().
static void ValidateReassignment(const Symbol* existing, const TypeRef& inferred_type,
                                  int line, int col, const std::string& source_name) {
    if (!existing || existing->effectiveType == Type::Unknown) return;
    if (inferred_type.type == Type::Unknown) return;
    // Phase 15: existing_ref now also carries effectiveElementType/
    // effectiveKeyType (symbol.h), so reassigning a name that was first
    // inferred as `list of int` (e.g. `items = [1, 2, 3]`) to `list of
    // string` later (`items = ["a", "b"]`) is caught the same way a
    // primitive/class mismatch already was -- see TypeRefEquals in type.h.
    TypeRef existing_ref{existing->effectiveType, existing->effectiveClassName,
                          existing->effectiveElementType, existing->effectiveKeyType};
    if (!TypeRefEquals(existing_ref, inferred_type)) {
        std::string msg = "type mismatch: expected " + DisplayType(existing_ref) +
                           ", received " + DisplayType(inferred_type);
        throw AvaError(msg, line, col, source_name);
    }
}

// Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md ("Operadores"). Canonical
// source-level spelling of each BinOp/UnOp, purely for error messages --
// matches the exact tokens ParseBinOp (below, in the f-string mini-parser)
// accepts, which in turn match the grammar's operator tokens.
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
    }
    return "?";
}

static const char* UnOpSymbol(UnOp op) {
    switch (op) {
        case UnOp::Neg: return "-";
        case UnOp::Not: return "not";
        case UnOp::Inc: return "++";
        case UnOp::Dec: return "--";
    }
    return "?";
}

// Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md ("Operadores"). Plan section
// 15 asks for "una tabla de compatibilidad" for the operators, with an
// illustrative table -- but that same section is explicit that "la tabla
// definitiva debe coincidir con la semántica real de AvaLang", so this
// table is built from what the VM (runtime/avalang/src/vm/vm_arith.cpp,
// vm_compare.cpp) actually does with each operator, not from the plan's
// example rows verbatim. Two consequences worth calling out up front,
// since both diverge from a literal reading of the plan:
//
//   1. Plan section 22 lists `true + "hello"` as an example "operación
//      incompatible" that should error. At runtime it does NOT error --
//      vm_arith.cpp's OpAdd special-cases '+' so that if EITHER operand is
//      a String, it concatenates (stringifying the other side) instead of
//      doing numeric addition, so `true + "hello"` evaluates to the string
//      "truehello". Flagging that as a compile error would be a false
//      positive against real, working AvaLang code. What DOES throw at
//      runtime for '+' is `true + 5` or `true + true` -- neither operand is
//      a String, so OpAdd falls through to CoerceToNumber(bool), which
//      always throws (vm_helpers.cpp). That's the case this table flags.
//   2. `==`/`!=`/`and`/`or` are NOT validated at all here, even though the
//      plan's table lists `==` needing "tipos compatibles". vm_compare.cpp's
//      OpEq/OpNe never throw for any type combination -- matching types
//      compare structurally, mismatched types fall through to a
//      well-defined `false`/`true` (pointer-identity) result. `and`/`or`
//      compile to a truthiness test against the constant 0 (see
//      CompileExpr's short-circuit branch) which likewise never throws for
//      any ValueType. Since nothing here can actually fail at runtime,
//      this system's established rule (every other phase: only flag what
//      would genuinely break) says don't invent an error for it.
//
// Only the four primitive Types this system tracks (Int, Float, Bool,
// String) are relevant -- Type::Unknown (an expression form InferExprType
// doesn't resolve yet, e.g. a call, an index, a collection) always skips
// the check, same "no false positives on what we can't evaluate" rule as
// ValidateTypeAnnotation/CheckCallArgs/CheckReturnType.
//
// Arithmetic (+, -, *, /, //, %, **): vm_arith.cpp routes every operand
// that isn't handled by '+'s String-concat/List-concat special cases
// through CoerceToNumber (vm_helpers.cpp), which:
//   - passes a Number through as-is;
//   - parses a String if (and only if) its trimmed content looks like a
//     number, otherwise throws;
//   - always throws for Bool (Bool is neither Number nor String).
// A String operand is deliberately NOT flagged here for -, *, /, //, %, **
// (unlike '+', these have no concat exception) -- whether a given String
// actually parses as a number is a fact about its runtime *value*
// ("10" vs "hello"), and Type::String on its own carries no such value
// information at this level (see InferExprType), so flagging it would risk
// false positives on perfectly valid code like `"10" - 1`. A Bool operand,
// on the other hand, ALWAYS throws regardless of value, so it's always
// safe to flag.
//
// Comparisons (<, <=, >, >=): vm_compare.cpp compares lexicographically
// when BOTH operands are String, and otherwise routes both operands through
// the same CoerceToNumber as above. Bool can never take the String-String
// branch (it isn't a String), so a Bool operand on either side always
// throws here too, by the same reasoning as arithmetic above. A String
// operand mixed with a non-String is left unchecked for the same
// numeric-string-value-ambiguity reason as arithmetic.
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
            // '+' concatenates (never throws) whenever either side is a
            // String, Bool included -- see point 1 in the comment above.
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
            // Deliberately not validated -- see point 2 in the comment above.
            return;
    }
}

// Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md. Companion to
// ValidateBinOpTypes above, for the three unary operators that touch a
// value's type: vm_arith.cpp's OpNeg/OpInc/OpDec all route their single
// operand through the exact same CoerceToNumber as the binary arithmetic
// operators above, so the same "Bool always throws, String is
// value-ambiguous so left unchecked" reasoning applies here verbatim.
// `not` (OpNot) is excluded on purpose: it calls Value::IsTruthy(), which
// is defined for every ValueType and never throws, so `not` is universally
// valid at runtime regardless of the operand's type -- same "don't flag
// what can't actually fail" rule as Eq/Ne/And/Or above.
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
        // Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md ("Operadores"). Runs
        // before any bytecode for this BinOpExpr is emitted -- same "check
        // first, compile after" pattern as CheckCallArgs (Phase 9). No-op
        // for every combination this phase doesn't flag; see CheckBinOpTypes/
        // ValidateBinOpTypes below for the actual table.
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
        // Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md ("Operadores"). Same
        // "check first" placement as the BinOpExpr case above.
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
        auto result = AllocReg();
        Emit(OpCode::NOT, result, reg);
        return result;
    }

    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        if (auto* callee_name = dynamic_cast<NameExpr*>(c->callee.get())) {
            CheckCallArgs(callee_name->name, c);
        } else if (auto* callee_attr = dynamic_cast<AttrExpr*>(c->callee.get())) {
            // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md: obj.method(...)
            // now gets its arguments checked too -- see CheckMethodCallArgs.
            CheckMethodCallArgs(callee_attr, c);
        }
        auto callee_val_reg = CompileExpr(c->callee);
        // CompileExpr(c->callee) may return a "borrowed" register -- e.g. a
        // local variable's own storage slot (NameExpr for a local returns
        // it->second directly, no copy) -- rather than a freshly allocated
        // temporary. CALL below writes the call's return value into the "a"
        // register of the call frame, which is callee_reg itself; if we used
        // the borrowed register directly as callee_reg, a call would
        // overwrite the local variable holding the callee with the return
        // value, corrupting it for any later use (e.g. calling the same
        // local function-valued variable/parameter a second time). Always
        // copy into a fresh call-frame base register first so locals/upvalues
        // survive the call.
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
        if (locals_.find("this") == locals_.end()) {
            throw AvaError("base() can only be used inside a method", current_line_, current_col_, source_name_);
        }
        if (!current_base_class_) {
            throw AvaError("base() can only be used inside a class that extends another class "
                            "(this class has no ': ParentClass')",
                            current_line_, current_col_, source_name_);
        }

        auto method_idx = AddConstant(MakeString(s->method_name));
        uint8_t argc = static_cast<uint8_t>(s->args.size());

        uint16_t call_frame_end = 1 + argc;
        if (next_reg_ < call_frame_end) {
            next_reg_ = call_frame_end;
            if (next_reg_ > max_reg_) max_reg_ = next_reg_;
        }
        for (size_t i = 0; i < s->args.size(); ++i) {
            uint16_t regs_before_arg = next_reg_;
            auto arg_reg = CompileExpr(s->args[i]);
            Emit(OpCode::MOVE, static_cast<uint16_t>(1 + i), arg_reg);
            FreeRegs(next_reg_ - regs_before_arg);
        }
        auto result_reg = AllocReg();
        Emit(OpCode::BASECALL, result_reg, method_idx, argc);
        FreeRegs(argc);
        return result_reg;
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
        // Phase 14 of AvaLang_Plan_Sistema_de_Tipos.md ("Lambdas y
        // funciones como valores"): copied down exactly like CompileFunc
        // does (Phase 13) -- without this, ResolveTypeName below (and any
        // class-typed reference inside the lambda's own body) would see
        // empty maps, since a brand-new `Compiler sub` otherwise starts
        // with none of the enclosing scope's compiled-class knowledge. A
        // plain copy, never written back into by the lambda, same
        // reasoning as CompileFunc's copy.
        sub.compiled_classes_ = compiled_classes_;
        sub.class_field_types_ = class_field_types_;
        sub.class_method_returns_ = class_method_returns_;
        sub.class_method_params_ = class_method_params_;

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

        sub.EmitDefaultsPrologue(l->defaults, 1);

        // Phase 14: resolve this lambda's own `as Type` return annotation
        // (LambdaExpr::return_type, Phase 14's addition to the AST) once,
        // before compiling its body, exactly like CompileFunc does with
        // FuncDef::return_type -- so every ReturnStmt inside is checked by
        // CheckReturnType. Type::Unknown (no annotation -- including the
        // bare `x => expr` form, which has no return-type syntax at all)
        // makes that check a no-op, same convention as everywhere else.
        sub.current_return_type_ = ResolveTypeName(l->return_type);

        sub.CompileChunk(l->body);
        // Same convention as named functions (see CompileFunctionDecl):
        // if the body's last statement was a bare expression, CompileChunk
        // leaves its register in sub.result_reg_ so it becomes the implicit
        // return value. Previously this was `sub.Emit(OpCode::RETURN);`
        // with no operands, which always fell into the a=0/b=0 -> Nil
        // branch and silently discarded the last expression's value for
        // any lambda that didn't end in an explicit `return`.
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
        // Base register doubles as the expression's result: OpYield packs
        // the yielded values (or nil, if none) back into registers[base]
        // once it runs, and -- once the coroutine is resumed with a value --
        // that same register is where the resume value lands too, so
        // whatever calls CompileExpr on a YieldExpr gets `yield`'s result
        // like any other expression (e.g. `x = yield a, b`, `f(yield v)`).
        //
        // Unlike the old statement form, base is a freshly allocated
        // register (not hardcoded to 0) so `yield` composes safely as a
        // sub-expression without clobbering whatever else is live in reg 0
        // (e.g. `this` in a method).
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
            // AwaitExpr (like most ExprNode kinds) never gets its own ->line
            // stamped -- only StmtNode gets that in visitStatement -- so we
            // use current_line_, the line of the statement this expr lives
            // in (kept up to date in CompileStmt), same source of truth the
            // other AvaError throws in this file rely on.
            std::string msg = "'await' can only be used inside an 'async func'";
            if (current_line_ > 0) {
                msg += " (line " + std::to_string(current_line_) + ")";
            }
            throw AvaError(msg, current_line_, current_col_, source_name_);
        }

        // Fase 2: `await expr` now emits a real AWAIT, not a YIELD alias.
        // expr is evaluated into base_reg; OpAwait either resolves it
        // immediately (expr is a settled/non-Task value) or suspends this
        // call's coroutine and lets VM::SettleTask overwrite base_reg with
        // the Task's result once it completes (see vm/vm_task.cpp).
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

// Phase 13: declared.class_name/inferred.class_name, parallel to Symbol's
// own declaredClassName/inferredClassName (symbol.h), only meaningful when
// the corresponding TypeRef.type is Type::Object. A class name is only
// ever written into the symbol when its Type actually changes (same
// "Type::Unknown never downgrades an already-known value" rule this
// function already followed for declaredType/inferredType) -- see the two
// `if` blocks below, each now sets its Type, class name, element type, and
// key type together so none of them drift out of sync with each other.
// Phase 15: declared/inferred were plain (Type, class_name) pairs before
// this phase; now full TypeRef, so element_type/key_type (meaningful only
// for Type::List/Type::Dict) move alongside type/class_name under the same
// "only when this call's type isn't Unknown" rule.
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
        // Scoped to this Compiler's own symbols_ only, same as DeclareSymbol
        // -- a name that lives in an enclosing function's scope (upvalue)
        // isn't looked up here yet, it just infers Unknown for now.
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
        }
        return Type::Unknown;
    }
    if (auto* b = dynamic_cast<BinOpExpr*>(expr.get())) {
        switch (b->op) {
            case BinOp::Eq: case BinOp::Ne:
            case BinOp::Lt: case BinOp::Le:
            case BinOp::Gt: case BinOp::Ge:
            case BinOp::And: case BinOp::Or:
                // Whether the operand types are actually compatible with
                // each other for this operator is Phase 11's job; AvaLang's
                // own semantics say these always yield a bool.
                return Type::Bool;
            case BinOp::Add: {
                // Phase 12 of AvaLang_Plan_Sistema_de_Tipos.md
                // ("Compatibilidad con expresiones compuestas"). '+' is
                // special-cased ahead of the generic numeric-promotion
                // logic below because vm_arith.cpp's OpAdd is: if EITHER
                // operand is a String, concatenate (the other side gets
                // stringified, whatever its type) -- see the identical
                // reasoning already spelled out for ValidateBinOpTypes in
                // Phase 11. That means the result is String as soon as one
                // side is *known* to be String, regardless of whether the
                // OTHER side resolves at all -- e.g. `"Total: " +
                // items[0]` is String even though `items[0]` itself infers
                // Unknown (IndexExpr, still unresolved -- see the bottom of
                // this function). Checking this before the shared
                // Unknown-short-circuit below (which the other arithmetic
                // ops still rely on) is what lets a composed expression
                // like `greeting = "Hi " + name` correctly propagate
                // greeting -> String into the symbol table, so a later
                // `greeting = 5` is caught by Phase 7's reassignment check
                // -- the concrete "composición de expresiones" improvement
                // this phase asks for.
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
        // Anything else (e.g. int + string) is a mismatch for Phase 11 to
        // flag -- this function only infers, it never reports errors.
        return Type::Unknown;
    }

    // Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de
    // retornos"): a call whose callee is a plain NameExpr with a known
    // signature in this chunk (known_func_returns_, filled by
    // CollectFuncSignatures) now infers as that function's declared
    // return_type -- e.g. `y = add(1, 2)` infers y -> int when `add`
    // declares `as int`. Type::Unknown either way (callee not a NameExpr,
    // not found in this chunk, or found with no return annotation) falls
    // through with the same "can't evaluate yet" meaning as everything
    // else in this function. This does NOT re-run CheckCallArgs or
    // otherwise validate the call itself -- that already happened at the
    // call's own CompileExpr site (Compiler::CheckCallArgs), independent
    // of whether anything here reads the result.
    if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        if (auto* callee_name = dynamic_cast<NameExpr*>(c->callee.get())) {
            // Phase 13: known_func_returns_ is now TypeRef-keyed (a
            // function's return can itself be a class); this function only
            // ever hands back a plain Type, so an Object-typed return still
            // infers as Type::Object here (correct as far as it goes -- just
            // without WHICH class) -- callers that need the class name use
            // InferExprTypeRef instead. Class instantiation (`User(...)`,
            // callee_name naming a compiled class rather than a known free
            // function) also resolves here as Type::Object for the same
            // reason -- see InferExprTypeRef for the class-aware version.
            auto it = known_func_returns_.find(callee_name->name);
            if (it != known_func_returns_.end()) return it->second.type;
            if (compiled_classes_.count(callee_name->name)) return Type::Object;
        }
        return Type::Unknown;
    }

    // Phase 15 of AvaLang_Plan_Sistema_de_Tipos.md ("Colecciones") closes
    // the two gaps the Phase 12 audit comment (below) explicitly left for
    // this phase: ListExpr/DictExpr literals now infer as Type::List/
    // Type::Dict (regardless of whether their elements agree on one type
    // -- it's unambiguously a list/dict either way; WHICH element type, if
    // any, is InferExprTypeRef's job, not this plain-Type function's), and
    // IndexExpr/SliceExpr now delegate to InferExprTypeRef to read the
    // element/value type back off whatever the indexed/sliced expression
    // resolves to (Unknown if that isn't itself a resolved List/Dict, same
    // "can't evaluate yet" fallback as always). Delegating to
    // InferExprTypeRef here -- instead of duplicating its ListExpr/DictExpr/
    // IndexExpr/SliceExpr logic -- is safe: those four cases are handled
    // directly there (see its own comment) before it ever falls back to
    // wrapping InferExprType, so there's no risk of infinite recursion.
    if (dynamic_cast<ListExpr*>(expr.get())) return Type::List;
    if (dynamic_cast<DictExpr*>(expr.get())) return Type::Dict;
    if (dynamic_cast<IndexExpr*>(expr.get())) return InferExprTypeRef(expr).type;
    if (dynamic_cast<SliceExpr*>(expr.get())) return InferExprTypeRef(expr).type;

    // Phase 12 of AvaLang_Plan_Sistema_de_Tipos.md ("Compatibilidad con
    // expresiones compuestas") audited every remaining ExprNode kind --
    // IndexExpr (`items[0]`), SliceExpr (`items[1:5]`), AttrExpr
    // (`object.property`), ListExpr (`[1, 2, 3]`), DictExpr, LambdaExpr,
    // BaseExpr, AwaitExpr, YieldExpr, NilExpr -- against every caller of
    // InferExprType (this function's own recursion for BinOpExpr/UnOpExpr
    // operands above, CheckCallArgs's per-argument loop, CheckReturnType,
    // ValidateTypeAnnotation/CheckReassignment via their call sites in
    // CompileStmt/CompileExprToReg). Of that list, IndexExpr/SliceExpr/
    // ListExpr/DictExpr are now handled above (Phase 15, see that comment);
    // AttrExpr (`object.property`) is UNCHANGED by this phase on purpose --
    // it needs class/instance member types (Phase 13's InferExprTypeRef
    // AttrExpr branch already resolves this for callers that use THAT
    // function, but this plain-Type InferExprType still doesn't reach for
    // it, matching Phase 12's original scope decision, not a new gap this
    // phase introduced). LambdaExpr/BaseExpr/AwaitExpr/YieldExpr/NilExpr
    // still fall through to Type::Unknown below, same as before. Every
    // consumer of this function already treats Type::Unknown as "nothing
    // to check, don't emit a false positive" (that convention dates back
    // to Phase 6), which is what still makes `result = items[0].value +
    // 10` safe: `.value` (AttrExpr) infers Unknown, so the outer `+`
    // (Phase 11's ValidateBinOpTypes, and this function's own BinOp::Add
    // case above) silently skips validation instead of guessing, even
    // though `items[0]` itself (IndexExpr) now resolves correctly.
    return Type::Unknown;
}

TypeRef Compiler::ResolveTypeName(const std::string& name) {
    return ResolveTypeNameAgainst(name, compiled_classes_);
}

// Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos").
// TypeRef-returning sibling of InferExprType above -- same walk, but only
// three ExprNode kinds actually need their OWN branch here, because those
// are the only forms that can resolve to Type::Object with a specific
// class attached:
//   - NameExpr: reads effectiveClassName alongside effectiveType from
//     symbols_ (InferExprType's NameExpr branch only reads effectiveType).
//   - CallExpr: either instantiating a compiled class directly
//     (`User(...)` -- AvaLang has no `new`, see class_method_params_'s
//     comment in compiler.h for why), calling a known free function whose
//     declared return type is a class (known_func_returns_, Phase 10,
//     made TypeRef-aware this phase), or calling a method on an
//     already-resolved object (`obj.method()` -- recurses into this same
//     function for `obj`, then looks the method's return type up in
//     class_method_returns_).
//   - AttrExpr: a field read (`obj.field`) on an already-resolved object,
//     looked up in class_field_types_.
// Phase 15 of AvaLang_Plan_Sistema_de_Tipos.md ("Colecciones") adds four
// more branches below for the same reason: ListExpr/DictExpr (literals)
// and IndexExpr/SliceExpr (reading an element/value back out) are the
// forms that can resolve to a List/Dict WITH its element/key type
// attached, which -- like Object's class_name -- InferExprType's plain
// Type can't carry.
// Every other expression form (literals, unary/binary operators, lambdas,
// base/await/yield, nil) can never itself evaluate to an instance of a
// user-defined class or a list/dict with element-type information -- see
// InferExprType's own Phase 12/15 audit comments for why those forms don't
// carry that identity even in principle -- so they're delegated to
// InferExprType and wrapped with an empty class_name, which is meaningless
// for anything other than Type::Object (see TypeRef's comment in type.h)
// and therefore harmless here regardless of what Type comes back.
TypeRef Compiler::InferExprTypeRef(const std::shared_ptr<ExprNode>& expr) {
    if (!expr) return TypeRef{};

    if (auto* n = dynamic_cast<NameExpr*>(expr.get())) {
        // Same scoping caveat as InferExprType's own NameExpr branch: this
        // Compiler's own symbols_ only, no upvalue/parent lookup.
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
            // Phase 16 of AvaLang_Plan_Sistema_de_Tipos.md ("extern").
            // Same "matched directly against the alias name, not via
            // InferExprTypeRef" reasoning as CheckMethodCallArgs's own
            // extern branch above -- checked first, same order.
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

    // Phase 15 of AvaLang_Plan_Sistema_de_Tipos.md ("Colecciones").
    //   - ListExpr (`[1, 2, 3]`): Type::List, with element_type set to the
    //     first item's own TypeRef IF every item agrees with it
    //     (TypeRefEquals, type.h) -- an empty list or one with mixed
    //     element types still resolves as Type::List, just with a null
    //     element_type ("it's a list, element type not resolved" -- see
    //     TypeRef's comment in type.h for why that's different from
    //     Type::Unknown). Recursing through InferExprTypeRef (not
    //     InferExprType) for each item is what lets a list of objects
    //     (`[user1, user2]`) or a list of lists work: element_type itself
    //     ends up carrying a class_name or a nested element_type.
    //   - DictExpr: same shape, Type::Dict, with element_type as the value
    //     type (same agreement rule as ListExpr's items) and key_type
    //     always `{Type::String, ""}` when there's at least one entry --
    //     DictExpr::entries (ast.h) stores each key as a plain source-level
    //     std::string (`{name: "Ada"}`), never a general expression, so the
    //     key's type isn't actually ambiguous the way a list's elements or
    //     a dict's values are; there's nothing to disagree about.
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

    // IndexExpr (`items[0]`, `d["key"]`): reads the element/value type back
    // off whatever `obj` itself resolves to -- List's element_type or
    // Dict's element_type (the value type; see the field's own comment in
    // type.h for why List/Dict share that member). A null element_type
    // (unresolved, see above) or an `obj` that isn't a List/Dict at all
    // both fall through to TypeRef{} ("not resolved"), same meaning as
    // everywhere else in this function.
    if (auto* idx = dynamic_cast<IndexExpr*>(expr.get())) {
        TypeRef obj_type = InferExprTypeRef(idx->obj);
        if ((obj_type.type == Type::List || obj_type.type == Type::Dict) && obj_type.element_type) {
            return *obj_type.element_type;
        }
        return TypeRef{};
    }
    // SliceExpr (`items[1:5]`): slicing a list yields another list of the
    // SAME element type -- `obj_type` itself is already the right answer
    // when it's a List (dicts aren't sliceable in AvaLang's grammar, so
    // that case is left as "not resolved" rather than guessed at).
    if (auto* sl = dynamic_cast<SliceExpr*>(expr.get())) {
        TypeRef obj_type = InferExprTypeRef(sl->obj);
        if (obj_type.type == Type::List) return obj_type;
        return TypeRef{};
    }

    return TypeRef{InferExprType(expr), "", nullptr, nullptr};
}

// Phase 9 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de parámetros
// en llamadas"). Called from CompileExpr's CallExpr case, before any
// bytecode for the call is emitted, only when c->callee is a plain
// NameExpr (a->attr(...), a lambda stored in a variable, etc. have no
// static signature to check against -- out of scope here). No-op if
// `func_name` isn't in known_funcs_ (unknown callee -- could be a global
// builtin, an imported name, or simply a typo the runtime will catch;
// none of that is this phase's job) -- see known_funcs_'s comment in
// compiler.h for the scoping this lookup is limited to.
// For each argument position present in BOTH the call and the recorded
// signature: skipped when the parameter has no annotation (Type::Unknown)
// or when InferExprType(arg) can't resolve the argument's own type
// (Type::Unknown) -- same "no false positives on what the type system
// can't evaluate yet" rule as ValidateTypeAnnotation/ValidateReassignment.
// Otherwise mismatched types throw AvaError, naming the function,
// parameter, and both types.
// Deliberately NOT checked here, left open for a later pass:
//   - argument count vs. parameter count (too few/many arguments) --
//     interacts with default values and `*args` varargs, which need their
//     own pass rather than falling out of a type check as a side effect;
//   - the call's own return value (Phase 10, see InferExprType above).
// Shared per-argument loop between CheckCallArgs (free functions AND, as
// of Phase 13, class instantiation) and CheckMethodCallArgs
// (obj.method(...)) below -- `label` is only used for the error message,
// so each caller supplies its own wording (quoted function name, "class
// 'X' constructor", "method 'X.y'") without this loop needing to know
// which kind of callable it's checking.
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

void Compiler::CheckCallArgs(const std::string& func_name, const CallExpr* c) {
    auto it = known_funcs_.find(func_name);
    if (it != known_funcs_.end()) {
        CheckCallArgsAgainst(it->second, "'" + func_name + "'", c);
        return;
    }
    // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos"). A
    // NameExpr callee that isn't a known free function might instead be a
    // class being instantiated (`User(...)` -- AvaLang's grammar has no
    // `new`, see class_method_params_'s comment in compiler.h for why
    // instantiation is a plain call on the class name). Check its
    // constructor's declared parameters ("__init__", same key class_obj-
    // >methods already uses) the same way a free function's would be.
    if (!compiled_classes_.count(func_name)) return;
    auto cit = class_method_params_.find(func_name);
    if (cit == class_method_params_.end()) return;
    auto init_it = cit->second.find("__init__");
    if (init_it == cit->second.end()) return;
    CheckCallArgsAgainst(init_it->second, "class '" + func_name + "' constructor", c);
}

// Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos"). The
// obj.method(...) companion to CheckCallArgs above -- closes the gap
// Phase 9's own comment documented (\"Callees que no son NameExpr\": an
// AttrExpr callee was never checked at all). Resolves `callee->obj`'s
// class via InferExprTypeRef (so this only fires when the receiver itself
// is a known class instance -- an unresolved receiver, e.g. a parameter
// with no `as Type`, silently skips the check, same \"no false positives
// on what we can't evaluate\" rule as everywhere else in this file) and
// looks the method up in class_method_params_.
void Compiler::CheckMethodCallArgs(const AttrExpr* callee, const CallExpr* c) {
    // Phase 16 of AvaLang_Plan_Sistema_de_Tipos.md ("extern"). Checked
    // BEFORE the class-instance path below: `Alias.Func(...)`'s `Alias`
    // is a plain module alias (a NameExpr matched directly against
    // extern_func_params_'s keys), never something InferExprTypeRef
    // resolves to a TypeRef -- AvaLang's type system has no "module" case
    // in Type/TypeRef (see type.h), so this can't reuse that resolution
    // path the way obj.method() does. `return`s either way (found or
    // not) once obj is a known alias, same "an extern alias is never
    // also a class name" assumption CompileExtern's SETGLOBAL/
    // compiled_classes_ split already relies on elsewhere in this file.
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
    if (cit == class_method_params_.end()) return;
    auto mit = cit->second.find(callee->attr);
    if (mit == cit->second.end()) return;
    CheckCallArgsAgainst(mit->second, "method '" + obj_type.class_name + "." + callee->attr + "'", c);
}

// Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de retornos").
// Called from both ReturnStmt sites (CompileStmt and its CompileExprToReg
// mirror) before any bytecode for the return is emitted. No-op whenever
// current_return_type_ is Type::Unknown -- the currently-compiling function
// (if any) has no `as Type` after its parameter list, same "nothing to
// check" convention as CheckCallArgs/ValidateTypeAnnotation for an
// unannotated parameter/variable.
//
// Two things can go wrong once current_return_type_ IS known:
//   1. `return` with no value at all (r->value == nullptr) inside a
//      function that declares a non-empty return type. The plan (section
//      14) leaves this open ("También debe decidirse cómo se comportan los
//      `return` vacíos..."); decided now, in the same spirit as Phase 9's
//      strictness and consistent with `null`/optional types being an
//      explicitly UNRESOLVED feature (plan section 24) -- there is no
//      default/null value this phase is allowed to invent to satisfy a
//      declared `as int` return type, so this is an error, not a silent
//      zero-value/nil. Same reasoning C#'s non-nullable-by-default typing
//      uses, as opposed to Go's zero values.
//   2. `return expr` where InferExprType(expr) resolves to something other
//      than current_return_type_. Same "skip if inferred is Unknown" rule
//      as CheckCallArgs/ValidateTypeAnnotation -- no false positives on an
//      expression form this phase can't evaluate yet (calls to other
//      functions, indexing, collections, ...).
//
// Deliberately NOT checked here, left open for a later pass (same
// enumeration style as CheckCallArgs):
//   - a function whose body never hits an explicit `return` at all but
//     falls through to the end. AvaLang implicitly returns the last
//     expression statement's value in that case (see CompileFunc's
//     trailing RETURN using sub.result_reg_) -- whether THAT implicit
//     value must also match current_return_type_ is a full "does every
//     path return the declared type" analysis (definite-return checking),
//     a bigger feature than a single ReturnStmt check and not what the
//     plan's section 14 examples ask for;
//   - an unrecognized return type name (`as itn`) is silently treated as
//     "no annotation" (TypeFromName returns Type::Unknown for it, exactly
//     like Phase 9 currently treats an unrecognized PARAMETER type name --
//     see CollectFuncSignatures/CheckCallArgs's own note on this).
void Compiler::CheckReturnType(const ReturnStmt* r) {
    // Phase 13: current_return_type_ is now a TypeRef (was plain Type), so
    // a method/function declared `as SomeClass` gets its returns checked
    // the same way a primitive-typed one already was -- DisplayType names
    // the actual class in the error instead of falling back to
    // Type::Object's generic "object" TypeName().
    if (current_return_type_.type == Type::Unknown) return;
    if (!r->value) {
        std::string msg = "return type mismatch: '" + proto_->debug_name + "' expects " +
                           DisplayType(current_return_type_) + ", received nothing (empty 'return')";
        throw AvaError(msg, current_line_, current_col_, source_name_);
    }
    TypeRef actual = InferExprTypeRef(r->value);
    if (actual.type == Type::Unknown) return;
    // Phase 15: TypeRefEquals (type.h) replaces the Object-only comparison
    // this used before, so a return type declared as a list/dict (once a
    // later phase gives that a declaration syntax) would also get its
    // element/key type checked -- see TypeRefEquals' own comment.
    if (!TypeRefEquals(actual, current_return_type_)) {
        std::string msg = "return type mismatch: '" + proto_->debug_name + "' expects " +
                           DisplayType(current_return_type_) + ", received " + DisplayType(actual);
        throw AvaError(msg, current_line_, current_col_, source_name_);
    }
}

// Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md ("Operadores"). Called from
// CompileExpr's BinOpExpr case, before any bytecode for the operator (or
// its operands) is emitted -- same "check first" placement as
// CheckCallArgs (Phase 9). Just gathers both operands' InferExprType and
// defers to the static ValidateBinOpTypes table above; see that function's
// comment for the actual compatibility rules and why they diverge from a
// literal reading of the plan's illustrative table.
void Compiler::CheckBinOpTypes(const BinOpExpr* b) {
    Type lt = InferExprType(b->left);
    Type rt = InferExprType(b->right);
    ValidateBinOpTypes(b->op, lt, rt, current_line_, current_col_, source_name_);
}

// Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md. Companion to
// CheckBinOpTypes above, for UnOpExpr (Neg/Not/Inc/Dec). Same placement
// (CompileExpr, before compiling the operand) and same delegation pattern.
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
        // Phase 4 of AvaLang_Plan_Sistema_de_Tipos.md: a type declaration
        // without an initializer (`age as int`, AssignStmt::value ==
        // nullptr, see ast.h Phase 3) has no value to generate code for --
        // what happens before the first real assignment (error vs. a
        // zero value) is an open question left to Phase 7. For now, just
        // record the declared type in the symbol table and emit nothing.
        if (!a->value) {
            // Phase 13: ResolveTypeName (primitive-or-known-class) replaces
            // the plain TypeFromName here, so `user as User` (no
            // initializer) also records the right declared class.
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

    CompileChunk(stmt->then_body);

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

    CompileChunk(stmt->body);

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

void Compiler::CompileForIterator(const ForStmt* stmt) {
    uint32_t my_depth = for_depth_++;
    IteratorKind kind = DetectIteratorKind(stmt->iterable);

    if (kind == IteratorKind::Coroutine) {
        CompileForCoroutine(stmt, my_depth);
        for_depth_--;
        return;
    }
    if (dynamic_cast<DictExpr*>(stmt->iterable.get())) {
        CompileForDict(stmt, my_depth);
        for_depth_--;
        return;
    }
    if (dynamic_cast<ListExpr*>(stmt->iterable.get()) ||
        dynamic_cast<StringExpr*>(stmt->iterable.get()) ||
        dynamic_cast<CallExpr*>(stmt->iterable.get())) {
        CompileForList(stmt, my_depth);
        for_depth_--;
        return;
    }

    CompileForDynamic(stmt, my_depth);
    for_depth_--;
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

        CompileChunk(stmt->body);

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

    CompileChunk(stmt->body);

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

        CompileChunk(stmt->body);

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

    CompileChunk(stmt->body);

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

    size_t continue_target = proto_->instructions.size();

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

    CompileChunk(stmt->body);

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

        CompileChunk(stmt->body);

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

    CompileChunk(stmt->body);

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

        auto val_reg = CompileExpr(def);
        Emit(OpCode::MOVE, param_reg, val_reg);
        FreeRegs(1);

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
    // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos"):
    // copied down from the enclosing Compiler (instead of the fresh, empty
    // maps a brand-new `Compiler sub` would otherwise start with) so this
    // function's body can resolve/check class-typed annotations, calls,
    // and returns (`func make() as User ... end`, `user.getName()`, a
    // parameter later used as `user.name`) against every class already
    // compiled by the time this func is reached -- same "must be defined
    // earlier in the file" ordering CompileClass already enforces for base
    // classes. A plain copy (not a reference/pointer) is fine here: these
    // maps are only ever read during this func's own body compilation,
    // never written back into by it, and classes are cheap to copy at this
    // scale.
    sub.compiled_classes_ = compiled_classes_;
    sub.class_field_types_ = class_field_types_;
    sub.class_method_returns_ = class_method_returns_;
    sub.class_method_params_ = class_method_params_;

    sub.next_reg_ = 1;
    sub.max_reg_ = sub.next_reg_;
    for (auto& [pname, def] : func->params) {
        sub.locals_[pname] = sub.next_reg_;
        sub.next_reg_++;
    }
    sub.max_reg_ = sub.next_reg_;

    // Named nested functions (func inc() ... end declared inside another
    // function body) must be able to close over the enclosing function's
    // locals, same as anonymous lambdas do (see the LambdaExpr case above).
    // Without this, any reference inside the nested function to a name that
    // isn't one of its own params/locals silently falls through to the
    // "auto-declare a fresh local" branch in AssignStmt (or a global lookup
    // on read), so the outer variable is never actually captured -- each
    // call gets its own throwaway copy instead of sharing the parent's.
    // Top-level functions have no enclosing locals_ to capture (is_top_level_
    // compiler instance has none for real locals), so this is a no-op there.
    for (auto& [name, reg] : locals_) {
        if (name != "this") {
            sub.parent_locals_.push_back({name, reg});
            sub.proto_->upvalue_descs.push_back({true, reg});
            sub.next_reg_++;
        }
    }

    sub.EmitDefaultsPrologue(func->params, 1);

    // Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md: resolve this function's
    // own `as Type` (Phase 8's return_type string) once, before compiling
    // its body, so every ReturnStmt inside -- including ones nested in
    // if/while/for blocks, which share this same `sub` instance -- can be
    // checked by CheckReturnType. Type::Unknown (no annotation) makes that
    // check a no-op, same convention as everywhere else in this file.
    // Phase 13: ResolveTypeName (via `this`, so it resolves against
    // classes already compiled at this point) replaces TypeFromName here,
    // so `func make() as User ... end` sets current_return_type_ to
    // {Object, "User"} instead of leaving the class name unresolved.
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
    CollectFuncSignatures(stmts, known_funcs_, known_func_returns_, compiled_classes_);
    for (size_t i = 0; i < stmts.size(); i++) {
        if (i == stmts.size() - 1) {
            result_reg_ = CompileExprToReg(stmts[i]);
        } else {
            CompileStmt(stmts[i]);
        }
    }
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
    // Ver StampLine() en compiler.h -- CompileChunk manda la ULTIMA statement
    // del chunk por aca en vez de por CompileStmt (implicit-return del ultimo
    // valor), asi que necesita el mismo stamping o los errores dentro de la
    // ultima statement de un chunk se reportan en la statement previa.
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
        // Phase 4 of AvaLang_Plan_Sistema_de_Tipos.md -- see the matching
        // comment in CompileStmt above; same rationale applies here since
        // this is the "last statement of a chunk" mirror of that code path.
        if (!a->value) {
            // Phase 13 -- see the matching comment in CompileStmt above.
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
            auto val_reg = CompileExpr(a->value);
            auto obj_reg = CompileExpr(i->obj);
            auto idx_reg = CompileExpr(i->index);
            auto saved_idx = AllocReg();
            Emit(OpCode::MOVE, saved_idx, idx_reg);
            auto saved_obj = AllocReg();
            Emit(OpCode::MOVE, saved_obj, obj_reg);
            Emit(OpCode::SETINDEX, saved_obj, saved_idx, val_reg);
            FreeRegs(5);
            return 0;
        }
        if (auto* a_expr = dynamic_cast<AttrExpr*>(a->target.get())) {
            bool is_this = false;
            if (auto* name = dynamic_cast<NameExpr*>(a_expr->obj.get())) {
                if ((name->name == "this")) {
                    is_this = true;
                }
            }

            auto val_reg = CompileExpr(a->value);

            uint16_t obj_reg;
            if (is_this && locals_.find("this") != locals_.end()) {
                obj_reg = locals_.at("this");
            } else {
                obj_reg = CompileExpr(a_expr->obj);
            }

            auto attr_idx = AddConstant(MakeString(a_expr->attr));
            Emit(OpCode::SETATTR, obj_reg, attr_idx, val_reg);

            if (!is_this) {
                FreeRegs(1);
            }
            FreeRegs(1);
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
            // Bug #42: antes, un 'return' dentro de un try saltaba directo
            // a OpCode::RETURN y el/los 'finally' que lo envolvian nunca se
            // ejecutaban (cierres de archivos, locks, cleanup, etc. se
            // perdian en silencio). Ahora compilamos los finally's
            // pendientes -- del mas anidado al mas externo -- justo antes
            // de emitir el RETURN. 'reg' queda a salvo: todavia no se hizo
            // FreeRegs, asi que el bloque finally solo usa registros por
            // encima de next_reg_ actual y no lo pisa.
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
        pending_breaks_.push_back({proto_->instructions.size()});
        Emit(OpCode::JMP, 0);
        return 0;
    }
    if (auto* c = dynamic_cast<ContinueStmt*>(stmt.get())) {
        (void)c;
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

    // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos").
    // Inherited field types merge the same way instance_defaults/
    // private_members did above -- copied from the base class's own
    // already-resolved class_field_types_ entry (compiled earlier in the
    // file, same ordering rule the base-class lookup above already
    // enforces), before this class's own fields (below) can override them.
    if (cls->base_class) {
        auto bit = class_field_types_.find(base_class->name);
        if (bit != class_field_types_.end()) {
            class_field_types_[cls->name] = bit->second;
        }
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

                // Phase 13: record this field's resolved type -- from its
                // own `as Type` annotation when it has one (ResolveTypeName,
                // primitive-or-known-class), else from InferExprType on its
                // literal default (the same literal kinds the attr_val
                // switch above already recognizes; a default that isn't one
                // of those -- e.g. a call expression -- infers Unknown here
                // exactly like attr_val itself falls back to Value::Nil()
                // for it). Overwrites any inherited entry for the same
                // name, same "subclass wins" rule instance_defaults/
                // private_members already follow above.
                TypeRef field_type = !a->explicit_type.empty()
                    ? ResolveTypeName(a->explicit_type)
                    : TypeRef{InferExprType(a->value), "", nullptr, nullptr};
                class_field_types_[cls->name][n->name] = field_type;
            }
        }
    }

    // Phase 13: pre-populate class_method_returns_/class_method_params_
    // for THIS class before compiling any of its method bodies below --
    // otherwise a method calling a sibling method declared later in the
    // same class body (or `this.attr` on a field declared earlier, via
    // class_field_types_ above) would see an incomplete signature table.
    // Inherited signatures merge first (same "__init__ is never copied
    // from a base class" rule the `methods` merge above already follows),
    // then this class's own methods overwrite/add to them.
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
        // Same "'this' as an explicit first parameter is skipped when
        // computing the real parameter list" rule the method-compiling
        // loop below applies -- kept in sync deliberately rather than
        // shared, matching this file's existing style (e.g. CompileStmt/
        // CompileExprToReg's identical AssignStmt handling).
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
            // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y
            // objetos") -- see the identical copy in CompileFunc for why:
            // a method's own body needs to resolve/check class-typed
            // annotations, field access, and calls just like a free
            // function's body does, including this class's OWN just-
            // populated field/method maps above (so `this.attr`/
            // `this.other()` resolve inside its own methods).
            sub.compiled_classes_ = compiled_classes_;
            sub.class_field_types_ = class_field_types_;
            sub.class_method_returns_ = class_method_returns_;
            sub.class_method_params_ = class_method_params_;

            // AvaLang's canonical method convention binds `this` a
            // register 0 IMPLICITLY (ver libraries/mysql/index.ava: `func
            // connect(host, user, ...)`, sin `this` en la firma, usando
            // `this.attr` adentro) -- estilo C#, no Python. `self` NO es
            // un alias de `this`: si el autor lo escribe como parámetro
            // explícito (`func incrementar(self)`, costumbre de Python),
            // es un error de compilación a propósito, para no dejar
            // colar la confusión Python/C# en la firma del método.
            // Escribir `this` como primer parámetro explícito sigue
            // aceptado (algunos estilos lo prefieren) y se pisa por el
            // binding implícito de abajo sin correr el resto de
            // parámetros de registro.
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
                // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y
                // objetos"): gives `this` a real symbol-table entry, the
                // same way any other class-typed variable would get one
                // via DeclareSymbol -- this is what lets
                // InferExprTypeRef's NameExpr branch resolve `this` to
                // {Object, cls->name} inside a method body, which in turn
                // is what makes `this.attr`/`this.method(...)` resolvable
                // (InferExprTypeRef's AttrExpr/CallExpr branches both
                // start by recursing into their `obj`/`callee->obj`).
                // Declared AND inferred are both set to the class itself
                // (not just one) since `this` is never actually assigned
                // to inside a method -- there's no separate "declared via
                // annotation" vs "inferred from a value" moment for it the
                // way a normal `x as Type = expr` has.
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

            // Phase 10/13 of AvaLang_Plan_Sistema_de_Tipos.md: same
            // treatment as CompileFunc for free functions -- see that
            // function's comment. Applies uniformly to every method
            // including the constructor (f->name == cls->name); AvaLang's
            // grammar doesn't forbid a constructor from carrying `as
            // Type`, and this phase isn't the place to start special-
            // casing that. ResolveTypeName (via `this`, the class-
            // compiling Compiler, not `sub`) so `as SomeClass` resolves
            // the same way it does for a free function's return type.
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
        // Phase 16 of AvaLang_Plan_Sistema_de_Tipos.md ("extern"). Same
        // per-signature loop CompileClass's method-compiling loop already
        // runs for class_method_params_/class_method_returns_ (see that
        // function), just keyed by (stmt->alias, fn.name) instead of
        // (cls->name, method_name) -- there is no "this" parameter to skip
        // here (extern functions are plain, never methods on an
        // instance), so unlike CompileClass's loop every entry in
        // fn.params/fn.param_types is used as-is.
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
    std::vector<size_t> except_jmps;
    std::vector<size_t> except_end_jmps;

    size_t try_instr_idx = proto_->instructions.size();
    Emit(OpCode::TRY, 0);

    // Bug #42 (finally con return): mientras compilamos el cuerpo del try
    // y los except's, este TryStmt cuenta como "finally pendiente" -- si
    // CompileStmt encuentra un ReturnStmt en ese cuerpo, inyecta este
    // finally_body antes del RETURN. Se saca de la pila ANTES de compilar
    // el propio finally_body (mas abajo) para que un return dentro del
    // finally no se re-dispare a si mismo.
    bool has_finally = !stmt->finally_body.empty();
    if (has_finally) {
        pending_finally_stack_.push_back(&stmt->finally_body);
    }

    CompileChunk(stmt->try_body);

    Emit(OpCode::TRY_END);

    size_t success_jmp = proto_->instructions.size();
    Emit(OpCode::JMP, 0);

    PatchJump(try_instr_idx);

    for (size_t i = 0; i < stmt->except_bodies.size(); ++i) {
        size_t catch_instr_idx = proto_->instructions.size();
        Emit(OpCode::CATCH, 0);
        except_jmps.push_back(catch_instr_idx);

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

        CompileChunk(stmt->except_bodies[i]);

        size_t end_jmp = proto_->instructions.size();
        Emit(OpCode::JMP, 0);
        except_end_jmps.push_back(end_jmp);

        PatchJump(catch_instr_idx);
    }

    if (has_finally) {
        pending_finally_stack_.pop_back();
    }

    {
        auto raise_reg = AllocReg();
        Emit(OpCode::GETGLOBAL, raise_reg, AddConstant(MakeString("__exception__")));
        Emit(OpCode::RAISE, raise_reg);
        FreeRegs(1);
    }

    PatchJump(success_jmp);
    for (size_t jmp : except_end_jmps) {
        PatchJump(jmp);
    }

    if (!stmt->finally_body.empty()) {
        CompileChunk(stmt->finally_body);
    }
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

    // Nested f-string, e.g. {$"inner{x}"} used inside an outer f-string
    // interpolation. Reuses the same {..}-counting fragment split as the
    // top-level FSTRING token (see AstBuilder::visitFstringAtom) so nested
    // braces/quotes inside it are handled the same way.
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