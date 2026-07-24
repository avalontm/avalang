#ifndef AVA_AST_AST_H
#define AVA_AST_AST_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ava {

enum class BinOp {
    Add, Sub, Mul, Div, Mod, Pow,
    Eq, Ne, Lt, Le, Gt, Ge,
    And, Or
};

enum class UnOp {
    Neg, Not, Inc, Dec
};

struct AstNode {
    virtual ~AstNode() = default;
};

struct ExprNode : AstNode {};

struct NumberExpr : ExprNode {
    double value;
    explicit NumberExpr(double v) : value(v) {}
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

struct CallExpr : ExprNode {
    std::shared_ptr<ExprNode> callee;
    std::vector<std::shared_ptr<ExprNode>> args;
    CallExpr(std::shared_ptr<ExprNode> c, std::vector<std::shared_ptr<ExprNode>> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct BaseExpr : ExprNode {
    std::vector<std::shared_ptr<ExprNode>> args;
    explicit BaseExpr(std::vector<std::shared_ptr<ExprNode>> a = {}) : args(std::move(a)) {}
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
    std::shared_ptr<ExprNode> value;
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

struct FuncDef : StmtNode {
    std::string name;
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> params;
    bool is_vararg = false;
    std::vector<std::shared_ptr<StmtNode>> body;
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
    LambdaExpr(std::string n, std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> d,
               std::vector<std::shared_ptr<StmtNode>> b, bool v)
        : name(std::move(n)), defaults(std::move(d)), body(std::move(b)), is_vararg(v) {}
};

struct YieldStmt : StmtNode {
    std::vector<std::shared_ptr<ExprNode>> values;
    explicit YieldStmt(std::vector<std::shared_ptr<ExprNode>> v = {}) : values(std::move(v)) {}
};

struct Chunk {
    std::vector<std::shared_ptr<StmtNode>> statements;
};

} // namespace ava

#endif // AVA_AST_AST_H