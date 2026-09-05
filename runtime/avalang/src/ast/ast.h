#ifndef AVA_AST_AST_H
#define AVA_AST_AST_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ava {

enum class BinOp {
    Add, Sub, Mul, Div, IDiv, Mod, Pow,
    Eq, Ne, Lt, Le, Gt, Ge,
    And, Or,
    // Fase 2 del plan break/continue/operadores: bitwise, operan sobre
    // Number truncado a entero en runtime (ver vm_arith.cpp OpBand/
    // OpBor/OpBxor/OpShl/OpShr).
    BAnd, BOr, BXor, Shl, Shr
};

enum class UnOp {
    Neg, Not, Inc, Dec,
    // Fase 2: complemento bitwise (`~x`), ver vm_arith.cpp OpBnot.
    BNot
};

struct AstNode {
    virtual ~AstNode() = default;

    // 1-based source line/col this node came from; 0 = unknown/not stamped.
    // Only statements get these stamped today (see AstBuilder::visitStatement),
    // since that's the granularity Compiler::CompileStmt needs to keep
    // Proto::debug_lines in sync with emitted instructions.
    int line = 0;
    int col = 0;
};

struct ExprNode : AstNode {};

struct NumberExpr : ExprNode {
    double value;
    // Phase 5 ("Inferencia"): whether the
    // literal was written with a decimal point (`10.0`) vs. without one
    // (`10`). NUMBER's grammar rule is `DIGIT+ ('.' DIGIT+)? EXPONENT?`
    // (no hex), so this is a plain textual check in
    // AstBuilder::visitNumberAtom
    // -- it does NOT look at `value` itself, because `10.0` and `10` parse to
    // the same double but must infer as Float and Int respectively. Only
    // Compiler::InferExprType reads this; codegen (CompileExpr) still just
    // uses `value`, since the VM has a single numeric ValueType::Number.
    bool is_float = false;
    explicit NumberExpr(double v, bool is_float = false) : value(v), is_float(is_float) {}
};

struct StringExpr : ExprNode {
    std::string value;
    explicit StringExpr(std::string v) : value(std::move(v)) {}
};

struct FStringExpr : ExprNode {
    std::vector<std::pair<bool, std::string>> fragments;
    explicit FStringExpr(std::vector<std::pair<bool, std::string>> f) : fragments(std::move(f)) {}
};

struct BoolExpr : ExprNode {
    bool value;
    explicit BoolExpr(bool v) : value(v) {}
};

struct NilExpr : ExprNode {};

struct NameExpr : ExprNode {
    std::string name;
    explicit NameExpr(std::string n) : name(std::move(n)) {}
};

struct BinOpExpr : ExprNode {
    BinOp op;
    std::shared_ptr<ExprNode> left;
    std::shared_ptr<ExprNode> right;
    BinOpExpr(BinOp o, std::shared_ptr<ExprNode> l, std::shared_ptr<ExprNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
};

struct UnOpExpr : ExprNode {
    UnOp op;
    std::shared_ptr<ExprNode> operand;
    UnOpExpr(UnOp o, std::shared_ptr<ExprNode> e) : op(o), operand(std::move(e)) {}
};

// Fase 3 del plan break/continue/operadores: `cond ? then_expr : else_expr`.
// Azucar sintactico -- Compiler::CompileExpr lo compila con el mismo
// patron TEST+JMP que ya usa CompileIf, sin opcode nuevo en la VM.
struct TernaryExpr : ExprNode {
    std::shared_ptr<ExprNode> condition;
    std::shared_ptr<ExprNode> then_expr;
    std::shared_ptr<ExprNode> else_expr;
    TernaryExpr(std::shared_ptr<ExprNode> c, std::shared_ptr<ExprNode> t, std::shared_ptr<ExprNode> e)
        : condition(std::move(c)), then_expr(std::move(t)), else_expr(std::move(e)) {}
};

struct CallExpr : ExprNode {
    std::shared_ptr<ExprNode> callee;
    std::vector<std::shared_ptr<ExprNode>> args;
    CallExpr(std::shared_ptr<ExprNode> c, std::vector<std::shared_ptr<ExprNode>> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct BaseExpr : ExprNode {
    std::vector<std::shared_ptr<ExprNode>> args;
    std::string method_name; // defaults to "__init__" when base.NAME(...) is not used
    explicit BaseExpr(std::vector<std::shared_ptr<ExprNode>> a = {}, std::string m = "__init__")
        : args(std::move(a)), method_name(std::move(m)) {}
};

struct IndexExpr : ExprNode {
    std::shared_ptr<ExprNode> obj;
    std::shared_ptr<ExprNode> index;
    IndexExpr(std::shared_ptr<ExprNode> o, std::shared_ptr<ExprNode> i)
        : obj(std::move(o)), index(std::move(i)) {}
};

struct SliceExpr : ExprNode {
    std::shared_ptr<ExprNode> obj;
    std::shared_ptr<ExprNode> start;
    std::shared_ptr<ExprNode> end;
    std::shared_ptr<ExprNode> step;
    SliceExpr(std::shared_ptr<ExprNode> o, std::shared_ptr<ExprNode> s, 
              std::shared_ptr<ExprNode> e, std::shared_ptr<ExprNode> st = nullptr)
        : obj(std::move(o)), start(std::move(s)), end(std::move(e)), step(std::move(st)) {}
};

struct AttrExpr : ExprNode {
    std::shared_ptr<ExprNode> obj;
    std::string attr;
    AttrExpr(std::shared_ptr<ExprNode> o, std::string a)
        : obj(std::move(o)), attr(std::move(a)) {}
};

struct ListExpr : ExprNode {
    std::vector<std::shared_ptr<ExprNode>> items;
    explicit ListExpr(std::vector<std::shared_ptr<ExprNode>> i) : items(std::move(i)) {}
};

struct DictExpr : ExprNode {
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> entries;
    explicit DictExpr(std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> e)
        : entries(std::move(e)) {}
};

struct StmtNode : AstNode {};

struct ExprStmt : StmtNode {
    std::shared_ptr<ExprNode> expr;
    explicit ExprStmt(std::shared_ptr<ExprNode> e) : expr(std::move(e)) {}
};

struct AssignStmt : StmtNode {
    std::shared_ptr<ExprNode> target;
    // nullptr only for a type declaration without an initializer (Phase 3
    //  -- `age as int` with no `= value`,
    // see AstBuilder::visitTypedDeclStatement). Every other AssignStmt
    // (plain `x = 1`, typed `x as int = 1`, `static`/`private` attrs)
    // still always has a value. The compiler does not yet handle a null
    // value (that's Phase 4/7 -- symbol table + "declaracion sin
    // inicializar" semantics); this node only carries the information.
    std::shared_ptr<ExprNode> value;
    // Solo tienen sentido cuando esta asignación es en realidad la
    // declaración de un atributo de clase (`static x = 0` / `private x =
    // 0` dentro de un cuerpo de clase). Para cualquier otra asignación
    // (variable local, top-level, etc.) quedan en false y se ignoran --
    // ver DISENO_visibilidad_clases_avalang.md, Fase A/B: la gramática
    // permite estos modificadores en cualquier asignación, así que es acá,
    // en el compilador (Fase C), donde se valida que solo aparezcan dentro
    // de una clase.
    bool is_static = false;
    bool is_private = false;
    // Phase 3: raw source spelling of an explicit `as Type` annotation
    // (e.g. "int"), or empty when there is none (plain `x = expr`). Set by
    // AstBuilder::visitTypedAssignStatement / visitTypedDeclStatement.
    // Resolving this string to an ava::Type (src/common/type.h, Phase 1)
    // happens once the symbol table exists (Phase 4+) -- this field only
    // carries the annotation through the AST, unresolved and unvalidated.
    std::string explicit_type;
    // Phase 7 ("Asignaciones posteriores", plan section 11, "Scope y
    // shadowing con `local`"): true for `local x = expr` / `local x as
    // Type = expr` / `local x as Type`, set by
    // AstBuilder::visitLocalStatement. Consulted by
    // Compiler::CompileStmt/CompileExprToReg (a) to skip validating this
    // assignment's value against a same-named symbol already in scope and
    // (b) to allocate a FRESH function-local register that shadows any
    // same-named module global / upvalue / outer local, instead of
    // falling through to SETGLOBAL/SETUPVAL -- see CompileStmt. NOTE: the
    // shadow lives for the whole function (there is still no block-level
    // scope stack, so it does not un-shadow itself when the enclosing
    // block ends); a second `local x` later in the same function binds to
    // a new register and onwards references use it.
    bool is_local = false;
    AssignStmt(std::shared_ptr<ExprNode> t, std::shared_ptr<ExprNode> v)
        : target(std::move(t)), value(std::move(v)) {}
};

struct AugAssignStmt : StmtNode {
    std::shared_ptr<ExprNode> target;
    BinOp op;
    std::shared_ptr<ExprNode> value;
    AugAssignStmt(std::shared_ptr<ExprNode> t, BinOp o, std::shared_ptr<ExprNode> v)
        : target(std::move(t)), op(o), value(std::move(v)) {}
};

struct MultiAssignStmt : StmtNode {
    std::vector<std::shared_ptr<ExprNode>> targets;
    std::vector<std::shared_ptr<ExprNode>> values;
    MultiAssignStmt(std::vector<std::shared_ptr<ExprNode>> t, std::vector<std::shared_ptr<ExprNode>> v)
        : targets(std::move(t)), values(std::move(v)) {}
};

struct ReturnStmt : StmtNode {
    std::shared_ptr<ExprNode> value;
    explicit ReturnStmt(std::shared_ptr<ExprNode> v = nullptr) : value(std::move(v)) {}
};

struct PassStmt : StmtNode {};

struct BreakStmt : StmtNode {};

struct ContinueStmt : StmtNode {};

struct TryStmt : StmtNode {
    std::vector<std::shared_ptr<StmtNode>> try_body;
    std::vector<std::vector<std::shared_ptr<StmtNode>>> except_bodies;
    std::vector<std::shared_ptr<ExprNode>> except_exprs;
    std::vector<std::shared_ptr<StmtNode>> finally_body;
    TryStmt(std::vector<std::shared_ptr<StmtNode>> t,
            std::vector<std::vector<std::shared_ptr<StmtNode>>> eb,
            std::vector<std::shared_ptr<ExprNode>> ee,
            std::vector<std::shared_ptr<StmtNode>> f = {})
        : try_body(std::move(t)), except_bodies(std::move(eb)),
          except_exprs(std::move(ee)), finally_body(std::move(f)) {}
};

struct IfStmt : StmtNode {
    std::shared_ptr<ExprNode> condition;
    std::vector<std::shared_ptr<StmtNode>> then_body;
    std::vector<std::pair<std::shared_ptr<ExprNode>, std::vector<std::shared_ptr<StmtNode>>>> elif_clauses;
    std::vector<std::shared_ptr<StmtNode>> else_body;
    IfStmt(std::shared_ptr<ExprNode> c, std::vector<std::shared_ptr<StmtNode>> t,
           std::vector<std::pair<std::shared_ptr<ExprNode>, std::vector<std::shared_ptr<StmtNode>>>> e,
           std::vector<std::shared_ptr<StmtNode>> b)
        : condition(std::move(c)), then_body(std::move(t)),
          elif_clauses(std::move(e)), else_body(std::move(b)) {}
};

struct WhileStmt : StmtNode {
    std::shared_ptr<ExprNode> condition;
    std::vector<std::shared_ptr<StmtNode>> body;
    WhileStmt(std::shared_ptr<ExprNode> c, std::vector<std::shared_ptr<StmtNode>> b)
        : condition(std::move(c)), body(std::move(b)) {}
};

struct ForStmt : StmtNode {
    std::string var_name;
    std::shared_ptr<ExprNode> iterable;
    std::vector<std::shared_ptr<StmtNode>> body;
    ForStmt(std::string v, std::shared_ptr<ExprNode> i, std::vector<std::shared_ptr<StmtNode>> b)
        : var_name(std::move(v)), iterable(std::move(i)), body(std::move(b)) {}
};

// Fase 4 del plan break/continue/operadores: `for i = start to stop
// [step s] then ... end`, azucar estilo VB6. Deliberadamente un nodo AST
// separado de ForStmt (no desazucarado a range() en AstBuilder) para que
// `step` con signo negativo se pueda evaluar en runtime -- ver
// Compiler::CompileForRange.
struct ForRangeStmt : StmtNode {
    std::string var_name;
    std::shared_ptr<ExprNode> start;
    std::shared_ptr<ExprNode> stop;
    std::shared_ptr<ExprNode> step; // nullptr = sin 'step' explicito (paso 1)
    std::vector<std::shared_ptr<StmtNode>> body;
    ForRangeStmt(std::string v, std::shared_ptr<ExprNode> a, std::shared_ptr<ExprNode> b,
                 std::shared_ptr<ExprNode> s, std::vector<std::shared_ptr<StmtNode>> bd)
        : var_name(std::move(v)), start(std::move(a)), stop(std::move(b)),
          step(std::move(s)), body(std::move(bd)) {}
};

struct FuncDef : StmtNode {
    std::string name;
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> params;
    // Phase 8 ("Funciones"): raw
    // spelling of each parameter's `as Type` annotation, parallel to
    // `params` (same index, same length once set -- empty if this FuncDef
    // predates Phase 8 or was built some other way) -- "" = no annotation
    // for that parameter, same convention as AssignStmt::explicit_type
    // below. Grammar support (`param: NAME typeAnnotation? ('=' expr)?`)
    // has existed since Phase 2; AstBuilder::visitFuncDeclaration silently
    // discarded it until this phase. Resolving these strings to ava::Type
    // (TypeFromName) and actually validating them (unknown type name,
    // argument compatibility) is deferred to Phase 9 ("Validación de
    // parámetros en llamadas") -- this struct only stops throwing the
    // information away. Set directly after construction (like is_static/
    // is_private/is_async below), not via the constructor, to avoid
    // touching the one existing call site's positional-arg shape.
    std::vector<std::string> param_types;
    // Raw spelling of the `as Type` after the parameter list, e.g.
    // `func add(a as int) as int` -> "int"; "" = no return annotation.
    // Same deferred-resolution note as param_types -- consumed starting
    // Phase 10 ("Validación de retornos").
    std::string return_type;
    bool is_vararg = false;
    std::vector<std::shared_ptr<StmtNode>> body;
    // Mismo caveat que en AssignStmt: solo tienen significado real cuando
    // este FuncDef es un método dentro de un ClassDef. Un `static func`/
    // `private func` a nivel de módulo es sintácticamente válido (la
    // gramática no distingue contexto) pero semánticamente sin sentido;
    // el compilador debe rechazarlo (Fase C).
    bool is_static = false;
    bool is_private = false;
    // `async func x() ... end` (ver grammar/AvaLang.g4, asyncFuncDeclaration).
    // Habilita `await` en el cuerpo léxico directo de esta función -- ver
    // Compiler::in_async_func_. No anidado: un `func` normal declarado
    // dentro de una `async func` no hereda esto (mismo criterio que
    // JS/Python/C#).
    bool is_async = false;
    FuncDef(std::string n, std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> p,
            bool v, std::vector<std::shared_ptr<StmtNode>> b)
        : name(std::move(n)), params(std::move(p)), is_vararg(v), body(std::move(b)) {}
};

struct ClassDef : StmtNode {
    std::string name;
    std::shared_ptr<ExprNode> base_class;
    std::vector<std::shared_ptr<StmtNode>> body;
    ClassDef(std::string n, std::shared_ptr<ExprNode> b,
             std::vector<std::shared_ptr<StmtNode>> s)
        : name(std::move(n)), base_class(std::move(b)), body(std::move(s)) {}
};

struct ImportStmt : StmtNode {
    std::vector<std::string> module_path;
    std::string alias;
    explicit ImportStmt(std::vector<std::string> mp, std::string a = {})
        : module_path(std::move(mp)), alias(std::move(a)) {}
};

struct RaiseStmt : StmtNode {
    std::shared_ptr<ExprNode> value;
    explicit RaiseStmt(std::shared_ptr<ExprNode> v) : value(std::move(v)) {}
};

struct IncDecStmt : StmtNode {
    std::shared_ptr<ExprNode> target;
    UnOp op;
    IncDecStmt(std::shared_ptr<ExprNode> t, UnOp o) : target(std::move(t)), op(o) {}
};

struct LambdaExpr : ExprNode {
    std::string name;
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> defaults;
    std::vector<std::shared_ptr<StmtNode>> body;
    bool is_vararg = false;
    // Phase 14 ("Lambdas y funciones
    // como valores"). Same convention as FuncDef::param_types/return_type
    // (Phase 8): raw `as Type` spelling, parallel to `defaults` (same
    // index, "" = no annotation for that parameter), and the raw
    // return-type spelling from the (now optional) `returnType` after the
    // parameter list. Set directly after construction, not via the
    // constructor, for the same reason as FuncDef's: three existing call
    // sites (visitShortLambdaExprAlt, visitLambdaExprAlt, and the new
    // visitSingleParamLambdaExprAlt) stay untouched positionally.
    // `singleParamLambdaExpr` (`x => x * 2`) never populates param_types
    // (there is no `as Type` slot in that grammar rule at all -- see the
    // grammar comment) and never populates return_type either (no
    // `returnType` slot there); both stay at their default-constructed
    // empty state for that form, same "" = no annotation meaning as
    // everywhere else.
    std::vector<std::string> param_types;
    std::string return_type;
    LambdaExpr(std::string n, std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> d,
               std::vector<std::shared_ptr<StmtNode>> b, bool v)
        : name(std::move(n)), defaults(std::move(d)), body(std::move(b)), is_vararg(v) {}
};

// Declaración de una función dentro de un bloque `extern` (sin cuerpo,
// resuelta contra la librería nativa por el runtime).
//
// Phase 16 ("extern"): param_types/
// return_type are the exact same parallel-array + raw-spelling pattern as
// FuncDef's (this file, Phase 8) and LambdaExpr's (Phase 14) -- one string
// per entry in `params`, index-aligned, "" = no annotation for that
// position; return_type is the raw spelling after the parameter list, ""
// = no annotation. Populated by AstBuilder::visitExternFuncDeclaration
// from the grammar's `externParam: NAME typeAnnotation?` and
// `externFuncDeclaration: ... ')' returnType? ...` (both reusing
// `typeAnnotation`/`returnType`, not a new rule -- see the grammar's own
// comment on why that's unambiguous against `extern "lib" as Alias`'s
// `as`). Consumed by Compiler::CompileExtern to fill
// extern_func_params_/extern_func_returns_, the Alias.Func(...) analogue
// of known_funcs_/known_func_returns_ (free functions) and
// class_method_params_/class_method_returns_ (obj.method()).
struct ExternFuncDecl {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> param_types;
    std::string return_type;
    bool is_vararg = false;
};

struct ExternStmt : StmtNode {
    std::string library;   // p.ej. "kernel32" (nombre lógico/logico de archivo)
    std::string alias;     // p.ej. "Kernel" -- namespace obligatorio
    std::vector<ExternFuncDecl> functions;
    ExternStmt(std::string lib, std::string a, std::vector<ExternFuncDecl> f)
        : library(std::move(lib)), alias(std::move(a)), functions(std::move(f)) {}
};

struct YieldExpr : ExprNode {
    std::vector<std::shared_ptr<ExprNode>> values;
    explicit YieldExpr(std::vector<std::shared_ptr<ExprNode>> v = {}) : values(std::move(v)) {}
};

// `await expr` -- solo valido (ver Compiler::in_async_func_) dentro del
// cuerpo lexico directo de un `async func`. Por ahora, mientras no exista
// el scheduler real de Task (auto-resume de "esperadores"), compila igual
// que YieldExpr: pausa la coroutine y devuelve lo que sea que la resuma.
// Es la fase intermedia usable/testeable antes de la semantica completa
// de Task (.wait(), .result(), auto-resume).
struct AwaitExpr : ExprNode {
    std::shared_ptr<ExprNode> value;
    explicit AwaitExpr(std::shared_ptr<ExprNode> v) : value(std::move(v)) {}
};

struct Chunk {
    std::vector<std::shared_ptr<StmtNode>> statements;
};

} // namespace ava

#endif // AVA_AST_AST_H