// Mini DSL para construir AST de AvaLang a mano (bypassa el frontend
// ANTLR, mismo patron que test_proto_io_obfuscate.cpp) para poder
// compilar y correr scripts .ava minimos sin depender de vcpkg/antlr4
// en este entorno de verificacion.
#pragma once
#include "ast/ast.h"
#include <memory>
#include <vector>
#include <string>

namespace dsl {
using namespace ava;

inline std::shared_ptr<ExprNode> Name(const std::string& n) { return std::make_shared<NameExpr>(n); }
inline std::shared_ptr<ExprNode> Num(double v) { return std::make_shared<NumberExpr>(v); }
inline std::shared_ptr<ExprNode> Str(const std::string& v) { return std::make_shared<StringExpr>(v); }
inline std::shared_ptr<ExprNode> Bin(BinOp op, std::shared_ptr<ExprNode> l, std::shared_ptr<ExprNode> r) {
    return std::make_shared<BinOpExpr>(op, l, r);
}
inline std::shared_ptr<ExprNode> Call(std::shared_ptr<ExprNode> callee, std::vector<std::shared_ptr<ExprNode>> args) {
    return std::make_shared<CallExpr>(callee, args);
}
inline std::shared_ptr<ExprNode> Call(const std::string& name, std::vector<std::shared_ptr<ExprNode>> args) {
    return Call(Name(name), args);
}
inline std::shared_ptr<ExprNode> List(std::vector<std::shared_ptr<ExprNode>> items) {
    return std::make_shared<ListExpr>(items);
}
inline std::shared_ptr<ExprNode> Idx(std::shared_ptr<ExprNode> obj, std::shared_ptr<ExprNode> i) {
    return std::make_shared<IndexExpr>(obj, i);
}
inline std::shared_ptr<ExprNode> Slice(std::shared_ptr<ExprNode> obj, std::shared_ptr<ExprNode> start, std::shared_ptr<ExprNode> end) {
    return std::make_shared<SliceExpr>(obj, start, end);
}

inline std::shared_ptr<StmtNode> Expr(std::shared_ptr<ExprNode> e) { return std::make_shared<ExprStmt>(e); }
inline std::shared_ptr<StmtNode> Assign(const std::string& name, std::shared_ptr<ExprNode> v) {
    return std::make_shared<AssignStmt>(Name(name), v);
}
inline std::shared_ptr<StmtNode> Assign(std::shared_ptr<ExprNode> target, std::shared_ptr<ExprNode> v) {
    return std::make_shared<AssignStmt>(target, v);
}
inline std::shared_ptr<StmtNode> Return(std::shared_ptr<ExprNode> v = nullptr) { return std::make_shared<ReturnStmt>(v); }
inline std::shared_ptr<StmtNode> If(std::shared_ptr<ExprNode> cond,
                                     std::vector<std::shared_ptr<StmtNode>> then_body,
                                     std::vector<std::shared_ptr<StmtNode>> else_body = {}) {
    return std::make_shared<IfStmt>(cond, then_body,
        std::vector<std::pair<std::shared_ptr<ExprNode>, std::vector<std::shared_ptr<StmtNode>>>>{},
        else_body);
}
inline std::shared_ptr<StmtNode> For(const std::string& var, std::shared_ptr<ExprNode> iterable,
                                      std::vector<std::shared_ptr<StmtNode>> body) {
    return std::make_shared<ForStmt>(var, iterable, body);
}
inline std::shared_ptr<StmtNode> Yield(std::vector<std::shared_ptr<ExprNode>> values) {
    return std::make_shared<YieldStmt>(values);
}
inline std::shared_ptr<StmtNode> Func(const std::string& name, std::vector<std::string> params,
                                       std::vector<std::shared_ptr<StmtNode>> body) {
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> p;
    for (auto& pn : params) p.push_back({pn, nullptr});
    return std::make_shared<FuncDef>(name, p, false, body);
}

} // namespace dsl
