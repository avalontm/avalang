#include "ast_builder.h"
#include <stdexcept>

namespace ava {

static std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2) {
        if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static BinOp compOpToBinOp(const std::string& txt) {
    if (txt == "==") return BinOp::Eq;
    if (txt == "!=") return BinOp::Ne;
    if (txt == "<")  return BinOp::Lt;
    if (txt == ">")  return BinOp::Gt;
    if (txt == "<=") return BinOp::Le;
    if (txt == ">=") return BinOp::Ge;
    throw std::runtime_error("unknown comp op: " + txt);
}

static BinOp textToBinOp(const std::string& txt) {
    if (txt == "+")  return BinOp::Add;
    if (txt == "-")  return BinOp::Sub;
    if (txt == "*")  return BinOp::Mul;
    if (txt == "/")  return BinOp::Div;
    if (txt == "%")  return BinOp::Mod;
    if (txt == "**") return BinOp::Pow;
    throw std::runtime_error("unknown binop: " + txt);
}

static BinOp augOpToBinOp(const std::string& op) {
    if (op == "+=")  return BinOp::Add;
    if (op == "-=")  return BinOp::Sub;
    if (op == "*=")  return BinOp::Mul;
    if (op == "/=")  return BinOp::Div;
    if (op == "%=")  return BinOp::Mod;
    throw std::runtime_error("unknown augop: " + op);
}

std::shared_ptr<ExprNode> AstBuilder::makeString(const std::string& s) {
    return std::make_shared<StringExpr>(stripQuotes(s));
}

std::shared_ptr<ExprNode> AstBuilder::exprFromAny(const std::any& a) {
    if (!a.has_value()) return nullptr;
    if (a.type() == typeid(std::shared_ptr<ExprNode>)) {
        return std::any_cast<std::shared_ptr<ExprNode>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<NameExpr>)) {
        return std::any_cast<std::shared_ptr<NameExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<NumberExpr>)) {
        return std::any_cast<std::shared_ptr<NumberExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<StringExpr>)) {
        return std::any_cast<std::shared_ptr<StringExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<BoolExpr>)) {
        return std::any_cast<std::shared_ptr<BoolExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<NilExpr>)) {
        return std::any_cast<std::shared_ptr<NilExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<BinOpExpr>)) {
        return std::any_cast<std::shared_ptr<BinOpExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<UnOpExpr>)) {
        return std::any_cast<std::shared_ptr<UnOpExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<CallExpr>)) {
        return std::any_cast<std::shared_ptr<CallExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<ListExpr>)) {
        return std::any_cast<std::shared_ptr<ListExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<DictExpr>)) {
        return std::any_cast<std::shared_ptr<DictExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<IndexExpr>)) {
        return std::any_cast<std::shared_ptr<IndexExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<SliceExpr>)) {
        return std::any_cast<std::shared_ptr<SliceExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<AttrExpr>)) {
        return std::any_cast<std::shared_ptr<AttrExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<LambdaExpr>)) {
        return std::any_cast<std::shared_ptr<LambdaExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<BaseExpr>)) {
        return std::any_cast<std::shared_ptr<BaseExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<FStringExpr>)) {
        return std::any_cast<std::shared_ptr<FStringExpr>>(a);
    }
    return nullptr;
}

std::shared_ptr<StmtNode> AstBuilder::stmtFromAny(const std::any& a) {
    if (!a.has_value()) return nullptr;
    try {
        return std::any_cast<std::shared_ptr<StmtNode>>(a);
    } catch (...) {}
    try {
        auto expr_stmt = std::any_cast<std::shared_ptr<ExprStmt>>(a);
        return expr_stmt;
    } catch (...) {}
    try {
        auto assign_stmt = std::any_cast<std::shared_ptr<AssignStmt>>(a);
        return assign_stmt;
    } catch (...) {}
    try {
        auto aug_assign = std::any_cast<std::shared_ptr<AugAssignStmt>>(a);
        return aug_assign;
    } catch (...) {}
    try {
        auto ret_stmt = std::any_cast<std::shared_ptr<ReturnStmt>>(a);
        return ret_stmt;
    } catch (...) {}
    try {
        auto pass_stmt = std::any_cast<std::shared_ptr<PassStmt>>(a);
        return pass_stmt;
    } catch (...) {}
    try {
        auto break_stmt = std::any_cast<std::shared_ptr<BreakStmt>>(a);
        return break_stmt;
    } catch (...) {}
    try {
        auto cont_stmt = std::any_cast<std::shared_ptr<ContinueStmt>>(a);
        return cont_stmt;
    } catch (...) {}
    try {
        auto if_stmt = std::any_cast<std::shared_ptr<IfStmt>>(a);
        return if_stmt;
    } catch (...) {}
    try {
        auto while_stmt = std::any_cast<std::shared_ptr<WhileStmt>>(a);
        return while_stmt;
    } catch (...) {}
    try {
        auto for_stmt = std::any_cast<std::shared_ptr<ForStmt>>(a);
        return for_stmt;
    } catch (...) {}
    try {
        auto func_def = std::any_cast<std::shared_ptr<FuncDef>>(a);
        return func_def;
    } catch (...) {}
    try {
        auto class_def = std::any_cast<std::shared_ptr<ClassDef>>(a);
        return class_def;
    } catch (...) {}
    try {
        auto import_stmt = std::any_cast<std::shared_ptr<ImportStmt>>(a);
        return import_stmt;
    } catch (...) {}
    try {
        auto raise_stmt = std::any_cast<std::shared_ptr<RaiseStmt>>(a);
        return raise_stmt;
    } catch (...) {}
    try {
        auto try_stmt = std::any_cast<std::shared_ptr<TryStmt>>(a);
        return try_stmt;
    } catch (...) {}
    try {
        auto yield_stmt = std::any_cast<std::shared_ptr<YieldStmt>>(a);
        return yield_stmt;
    } catch (...) {}
    return nullptr;
}

std::vector<std::shared_ptr<StmtNode>> AstBuilder::stmtsFromAny(const std::any& a) {
    if (!a.has_value()) return {};
    try {
        return std::any_cast<std::vector<std::shared_ptr<StmtNode>>>(a);
    } catch (...) {
        return {};
    }
}

std::shared_ptr<ExprNode> AstBuilder::exprFromTarget(AvaLangParser::TargetContext* t) {
    if (!t) return nullptr;
    auto base = std::make_shared<NameExpr>(t->NAME()->getText());
    std::shared_ptr<ExprNode> expr = base;
    for (auto* tr : t->trailer()) {
        if (auto* attr = dynamic_cast<AvaLangParser::AttrTrailerContext*>(tr)) {
            expr = std::make_shared<AttrExpr>(expr, attr->NAME()->getText());
        } else if (auto* idx = dynamic_cast<AvaLangParser::IndexTrailerContext*>(tr)) {
            auto idx_expr = exprFromAny(idx->expr()->accept(this));
            expr = std::make_shared<IndexExpr>(expr, idx_expr);
        } else if (auto* call = dynamic_cast<AvaLangParser::CallTrailerContext*>(tr)) {
            std::vector<std::shared_ptr<ExprNode>> args;
            if (call->argList()) {
                for (auto* a : call->argList()->arg()) {
                    if (auto* named = dynamic_cast<AvaLangParser::NamedArgContext*>(a)) {
                        args.push_back(exprFromAny(named->expr()->accept(this)));
                    } else if (auto* pos = dynamic_cast<AvaLangParser::PositionalArgContext*>(a)) {
                        args.push_back(exprFromAny(pos->expr()->accept(this)));
                    }
                }
            }
            expr = std::make_shared<CallExpr>(expr, args);
        }
    }
    return expr;
}

std::any AstBuilder::visitAssignStatement(AvaLangParser::AssignStatementContext* ctx) {
    auto target = exprFromTarget(ctx->targetList()->target(0));
    auto value = exprFromAny(ctx->exprList()->expr(0)->accept(this));
    return std::make_shared<AssignStmt>(target, value);
}

std::any AstBuilder::visitChunk(AvaLangParser::ChunkContext* ctx) {
    auto chunk = std::make_shared<Chunk>();
    std::vector<std::shared_ptr<StmtNode>> stmts;
    for (auto* s : ctx->statement()) {
        auto any_result = s->accept(this);
        auto stmt = stmtFromAny(any_result);
        if (stmt) {
            stmts.push_back(stmt);
        }
    }
    chunk->statements = stmts;
    return chunk;
}

std::any AstBuilder::visitBlock(AvaLangParser::BlockContext* ctx) {
    std::vector<std::shared_ptr<StmtNode>> stmts;
    for (auto* s : ctx->statement()) {
        auto any_result = s->accept(this);
        auto stmt = stmtFromAny(any_result);
        if (stmt) {
            stmts.push_back(stmt);
        }
    }
    return stmts;
}

std::any AstBuilder::visitStatement(AvaLangParser::StatementContext* ctx) {
    if (ctx->simpleStatement()) return visitSimpleStatement(ctx->simpleStatement());
    return visitCompoundStatement(ctx->compoundStatement());
}

std::any AstBuilder::visitSimpleStatement(AvaLangParser::SimpleStatementContext* ctx) {
    return visitSmallStatement(ctx->smallStatement());
}

std::any AstBuilder::visitSmallStatement(AvaLangParser::SmallStatementContext* ctx) {
    if (ctx->assignStatement())    return visitAssignStatement(ctx->assignStatement());
    if (ctx->augAssignStatement()) return visitAugAssignStatement(ctx->augAssignStatement());
    if (ctx->exprStatement())     return visitExprStatement(ctx->exprStatement());
    if (ctx->returnStatement())   return visitReturnStatement(ctx->returnStatement());
    if (ctx->breakStatement())    return std::make_shared<BreakStmt>();
    if (ctx->continueStatement()) return std::make_shared<ContinueStmt>();
    if (ctx->passStatement())     return std::make_shared<PassStmt>();
    if (ctx->localStatement())    return visitLocalStatement(ctx->localStatement());
    if (ctx->importStatement())   return visitImportStatement(ctx->importStatement());
    if (ctx->raiseStatement())    return visitRaiseStatement(ctx->raiseStatement());
    if (ctx->yieldStatement())   return visitYieldStatement(ctx->yieldStatement());
    throw std::runtime_error("unsupported small statement");
}

std::any AstBuilder::visitExprStatement(AvaLangParser::ExprStatementContext* ctx) {
    auto* exprs = ctx->exprList();
    if (!exprs || exprs->expr().empty()) {
        return std::make_shared<ExprStmt>(std::make_shared<NilExpr>());
    }
    auto* lastExpr = exprs->expr(exprs->expr().size() - 1);
    auto result = lastExpr->accept(this);
    auto expr = exprFromAny(result);
    return std::make_shared<ExprStmt>(expr);
}

std::any AstBuilder::visitCompoundStatement(AvaLangParser::CompoundStatementContext* ctx) {
    if (ctx->ifStatement())     return visitIfStatement(ctx->ifStatement());
    if (ctx->whileStatement())  return visitWhileStatement(ctx->whileStatement());
    if (ctx->forStatement())    return visitForStatement(ctx->forStatement());
    if (ctx->funcDeclaration()) return visitFuncDeclaration(ctx->funcDeclaration());
    if (ctx->classDeclaration()) return visitClassDeclaration(ctx->classDeclaration());
    if (ctx->tryStatement())   return visitTryStatement(ctx->tryStatement());
    throw std::runtime_error("unsupported compound statement");
}

std::any AstBuilder::visitAugAssignStatement(AvaLangParser::AugAssignStatementContext* ctx) {
    auto target = exprFromTarget(ctx->target());
    auto value = exprFromAny(ctx->expr()->accept(this));
    auto op = augOpToBinOp(ctx->op->getText());
    return std::make_shared<AugAssignStmt>(target, op, value);
}

std::any AstBuilder::visitReturnStatement(AvaLangParser::ReturnStatementContext* ctx) {
    if (ctx->exprList() && !ctx->exprList()->expr().empty()) {
        return std::make_shared<ReturnStmt>(exprFromAny(ctx->exprList()->expr(0)->accept(this)));
    }
    return std::make_shared<ReturnStmt>();
}

std::any AstBuilder::visitBreakStatement(AvaLangParser::BreakStatementContext*) {
    return std::make_shared<BreakStmt>();
}

std::any AstBuilder::visitContinueStatement(AvaLangParser::ContinueStatementContext*) {
    return std::make_shared<ContinueStmt>();
}

std::any AstBuilder::visitPassStatement(AvaLangParser::PassStatementContext*) {
    return std::make_shared<PassStmt>();
}

std::any AstBuilder::visitLocalStatement(AvaLangParser::LocalStatementContext* ctx) {
    return visitAssignStatement(ctx->assignStatement());
}

std::any AstBuilder::visitIfStatement(AvaLangParser::IfStatementContext* ctx) {
    auto condition = exprFromAny(ctx->expr()->accept(this));
    auto then_body = stmtsFromAny(visitBlock(ctx->block()));

    std::vector<std::pair<std::shared_ptr<ExprNode>, std::vector<std::shared_ptr<StmtNode>>>> elif_clauses;
    for (auto* ec : ctx->elifClause()) {
        auto elif_cond = exprFromAny(ec->expr()->accept(this));
        std::vector<std::shared_ptr<StmtNode>> elif_body = stmtsFromAny(visitBlock(ec->block()));
        elif_clauses.push_back({elif_cond, elif_body});
    }

    std::vector<std::shared_ptr<StmtNode>> else_body;
    if (ctx->elseClause()) {
        else_body = stmtsFromAny(visitBlock(ctx->elseClause()->block()));
    }

    return std::make_shared<IfStmt>(condition, then_body, elif_clauses, else_body);
}

std::any AstBuilder::visitWhileStatement(AvaLangParser::WhileStatementContext* ctx) {
    auto condition = exprFromAny(ctx->expr()->accept(this));
    auto body = stmtsFromAny(visitBlock(ctx->block()));
    return std::make_shared<WhileStmt>(condition, body);
}

std::any AstBuilder::visitForStatement(AvaLangParser::ForStatementContext* ctx) {
    auto* targets = ctx->targetList();
    auto var_name = targets->target(0)->NAME()->getText();
    auto iter_expr = exprFromAny(ctx->exprList()->expr(0)->accept(this));
    auto body = stmtsFromAny(visitBlock(ctx->block()));
    return std::make_shared<ForStmt>(var_name, iter_expr, body);
}

std::any AstBuilder::visitFuncDeclaration(AvaLangParser::FuncDeclarationContext* ctx) {
    auto name = ctx->NAME()->getText();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> params;
    bool is_vararg = false;

    if (ctx->paramList()) {
        for (auto* p : ctx->paramList()->param()) {
            auto pname = p->NAME()->getText();
            std::shared_ptr<ExprNode> def = nullptr;
            if (p->expr()) def = exprFromAny(p->expr()->accept(this));
            params.push_back({pname, def});
        }
        if (ctx->paramList()->NAME()) {
            is_vararg = true;
        }
    }

    auto body = stmtsFromAny(visitBlock(ctx->block()));
    return std::make_shared<FuncDef>(name, params, is_vararg, body);
}

std::any AstBuilder::visitClassDeclaration(AvaLangParser::ClassDeclarationContext* ctx) {
    auto name = ctx->NAME()->getText();

    std::shared_ptr<ExprNode> base_class = nullptr;
    if (ctx->classHeritage()) {
        auto base_name = ctx->classHeritage()->NAME()->getText();
        base_class = std::make_shared<NameExpr>(base_name);
    }

    auto body = stmtsFromAny(visitBlock(ctx->block()));
    return std::make_shared<ClassDef>(name, base_class, body);
}

std::any AstBuilder::visitTryStatement(AvaLangParser::TryStatementContext* ctx) {
    auto try_body = stmtsFromAny(visitBlock(ctx->block()));
    
    std::vector<std::vector<std::shared_ptr<StmtNode>>> except_bodies;
    std::vector<std::pair<std::string, std::string>> except_types;
    std::vector<std::string> except_vars;
    
    for (auto* exc : ctx->exceptClause()) {
        auto exc_body = stmtsFromAny(visitBlock(exc->block()));
        except_bodies.push_back(exc_body);
        
        std::string type_name;
        std::string var_name;
        
        if (exc->NAME().size() == 1) {
            type_name = exc->NAME(0)->getText();
            var_name = exc->NAME(0)->getText();
        } else if (exc->NAME().size() == 2) {
            type_name = exc->NAME(0)->getText();
            var_name = exc->NAME(1)->getText();
        } else {
            type_name = "Exception";
            var_name = "";
        }
        
        except_types.push_back({type_name, ""});
        except_vars.push_back(var_name);
    }
    
    std::vector<std::shared_ptr<StmtNode>> finally_body;
    auto* fin = ctx->finallyClause();
    if (fin) {
        finally_body = stmtsFromAny(visitBlock(fin->block()));
    }
    
    return std::make_shared<TryStmt>(try_body, except_bodies, except_types, except_vars, finally_body);
}

std::any AstBuilder::visitExprList(AvaLangParser::ExprListContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> exprs;
    for (auto* e : ctx->expr()) {
        exprs.push_back(exprFromAny(e->accept(this)));
    }
    return exprs;
}

std::any AstBuilder::visitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext* ctx) {
    if (!ctx->shortLambdaExpr()) return visitLambdaExprAlt(nullptr);
    auto* lambda = ctx->shortLambdaExpr();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> defaults;
    std::string name = "<lambda>";

    if (lambda->paramList()) {
        for (auto* p : lambda->paramList()->param()) {
            auto pname = p->NAME()->getText();
            std::shared_ptr<ExprNode> def = nullptr;
            if (p->expr()) def = exprFromAny(p->expr()->accept(this));
            defaults.push_back({pname, def});
        }
    }

    auto body = std::make_shared<ReturnStmt>(exprFromAny(lambda->expr()->accept(this)));
    std::vector<std::shared_ptr<StmtNode>> body_stmts = {body};
    return std::make_shared<LambdaExpr>(name, defaults, body_stmts, false);
}

std::any AstBuilder::visitLambdaExprAlt(AvaLangParser::LambdaExprAltContext* ctx) {
    if (!ctx->lambdaExpr()) return visitOrExprAlt(nullptr);
    auto* lambda = ctx->lambdaExpr();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> defaults;
    bool is_vararg = false;
    std::string name = "<lambda>";

    if (lambda->paramList()) {
        for (auto* p : lambda->paramList()->param()) {
            auto pname = p->NAME()->getText();
            std::shared_ptr<ExprNode> def = nullptr;
            if (p->expr()) def = exprFromAny(p->expr()->accept(this));
            defaults.push_back({pname, def});
        }
        if (lambda->paramList()->NAME()) is_vararg = true;
    }

    auto body = stmtsFromAny(visitBlock(lambda->block()));
    return std::make_shared<LambdaExpr>(name, defaults, body, is_vararg);
}

std::any AstBuilder::visitOrExprAlt(AvaLangParser::OrExprAltContext* ctx) {
    if (!ctx) return nullptr;
    auto* orCtx = ctx->orExpr();
    if (!orCtx) return nullptr;
    auto exprs = orCtx->andExpr();
    if (exprs.size() == 1) return visitAndExpr(exprs[0]);

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        left = std::make_shared<BinOpExpr>(BinOp::Or, left, right);
    }
    return left;
}

std::any AstBuilder::visitAndExpr(AvaLangParser::AndExprContext* ctx) {
    auto exprs = ctx->notExpr();
    if (exprs.size() == 1) return visitNotExpr(exprs[0]);

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        left = std::make_shared<BinOpExpr>(BinOp::And, left, right);
    }
    return left;
}

std::any AstBuilder::visitNotExpr(AvaLangParser::NotExprContext* ctx) {
    if (ctx->notExpr()) {
        auto operand = exprFromAny(ctx->notExpr()->accept(this));
        return std::make_shared<UnOpExpr>(UnOp::Not, operand);
    }
    return visitComparison(ctx->comparison());
}

std::any AstBuilder::visitComparison(AvaLangParser::ComparisonContext* ctx) {
    auto exprs = ctx->additive();
    auto ops = ctx->compOp();

    if (ops.empty()) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 0; i < ops.size(); ++i) {
        auto right = exprFromAny(exprs[i + 1]->accept(this));
        left = std::make_shared<BinOpExpr>(compOpToBinOp(ops[i]->getText()), left, right);
    }
    return left;
}

std::any AstBuilder::visitAdditive(AvaLangParser::AdditiveContext* ctx) {
    auto exprs = ctx->multiplicative();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    auto ops = ctx->children;
    size_t op_idx = 1;
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        auto txt = ops[op_idx++]->getText();
        left = std::make_shared<BinOpExpr>(textToBinOp(txt), left, right);
    }
    return left;
}

std::any AstBuilder::visitMultiplicative(AvaLangParser::MultiplicativeContext* ctx) {
    auto exprs = ctx->unary();
    auto ops = ctx->children;

    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    size_t op_idx = 1;
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        auto txt = ops[op_idx++]->getText();
        left = std::make_shared<BinOpExpr>(textToBinOp(txt), left, right);
    }
    return left;
}

std::any AstBuilder::visitUnary(AvaLangParser::UnaryContext* ctx) {
    if (ctx->unary()) {
        auto operand = exprFromAny(ctx->unary()->accept(this));
        auto txt = ctx->children[0]->getText();
        if (txt == "-") return std::make_shared<UnOpExpr>(UnOp::Neg, operand);
        return std::make_shared<UnOpExpr>(UnOp::Not, operand);
    }
    return visitPower(ctx->power());
}

std::any AstBuilder::visitPower(AvaLangParser::PowerContext* ctx) {
    auto base = exprFromAny(ctx->postfix()->accept(this));
    if (ctx->unary()) {
        auto exp = exprFromAny(ctx->unary()->accept(this));
        return std::make_shared<BinOpExpr>(BinOp::Pow, base, exp);
    }
    return base;
}

std::any AstBuilder::visitPostfix(AvaLangParser::PostfixContext* ctx) {
    auto result = ctx->primary()->accept(this);
    auto expr = exprFromAny(result);
    if (!expr) return nullptr;

    for (auto* t : ctx->trailer()) {
        if (auto* attr = dynamic_cast<AvaLangParser::AttrTrailerContext*>(t)) {
            expr = std::make_shared<AttrExpr>(expr, attr->NAME()->getText());
        } else if (auto* idx = dynamic_cast<AvaLangParser::IndexTrailerContext*>(t)) {
            auto idx_expr = exprFromAny(idx->expr()->accept(this));
            expr = std::make_shared<IndexExpr>(expr, idx_expr);
        } else if (auto* slice = dynamic_cast<AvaLangParser::SliceTrailerContext*>(t)) {
            auto slice_result = slice->accept(this);
            auto slice_expr = std::any_cast<std::shared_ptr<SliceExpr>>(slice_result);
            slice_expr->obj = expr;
            expr = slice_expr;
        } else if (auto* call = dynamic_cast<AvaLangParser::CallTrailerContext*>(t)) {
            std::vector<std::shared_ptr<ExprNode>> args;
            if (call->argList()) {
                for (auto* a : call->argList()->arg()) {
                    if (auto* named = dynamic_cast<AvaLangParser::NamedArgContext*>(a)) {
                        args.push_back(exprFromAny(named->expr()->accept(this)));
                    } else if (auto* pos = dynamic_cast<AvaLangParser::PositionalArgContext*>(a)) {
                        args.push_back(exprFromAny(pos->expr()->accept(this)));
                    }
                }
            }
            expr = std::make_shared<CallExpr>(expr, args);
        }
    }
    return expr;
}

std::any AstBuilder::visitNameAtom(AvaLangParser::NameAtomContext* ctx) {
    return std::make_shared<NameExpr>(ctx->NAME()->getText());
}

std::any AstBuilder::visitNumberAtom(AvaLangParser::NumberAtomContext* ctx) {
    double val = std::stod(ctx->NUMBER()->getText());
    return std::make_shared<NumberExpr>(val);
}

std::any AstBuilder::visitStringAtom(AvaLangParser::StringAtomContext* ctx) {
    return makeString(ctx->STRING()->getText());
}

std::any AstBuilder::visitFstringAtom(AvaLangParser::FstringAtomContext* ctx) {
    std::string raw = ctx->FSTRING()->getText();
    raw = raw.substr(2, raw.size() - 3);
    
    std::vector<std::pair<bool, std::string>> fragments;
    std::string current_literal;
    size_t i = 0;
    
    while (i < raw.size()) {
        if (raw[i] == '{') {
            if (i + 1 < raw.size() && raw[i + 1] == '{') {
                current_literal += '{';
                i += 2;
            } else {
                if (!current_literal.empty()) {
                    fragments.push_back({false, current_literal});
                    current_literal.clear();
                }
                
                size_t start = i + 1;
                size_t brace_count = 1;
                i++;
                while (i < raw.size() && brace_count > 0) {
                    if (raw[i] == '{') brace_count++;
                    else if (raw[i] == '}') brace_count--;
                    i++;
                }
                
                if (brace_count == 0) {
                    std::string expr_str = raw.substr(start, i - start - 1);
                    if (!expr_str.empty()) {
                        fragments.push_back({true, expr_str});
                    }
                }
            }
        } else if (raw[i] == '}') {
            if (i + 1 < raw.size() && raw[i + 1] == '}') {
                current_literal += '}';
                i += 2;
            } else {
                current_literal += '}';
                i++;
            }
        } else if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            switch (next) {
                case 'n': current_literal += '\n'; break;
                case 't': current_literal += '\t'; break;
                case 'r': current_literal += '\r'; break;
                case 'b': current_literal += '\b'; break;
                case '"': current_literal += '"'; break;
                case '\'': current_literal += '\''; break;
                case '\\': current_literal += '\\'; break;
                default: current_literal += raw[i]; current_literal += raw[i + 1]; break;
            }
            i += 2;
        } else {
            current_literal += raw[i];
            i++;
        }
    }
    
    if (!current_literal.empty()) {
        fragments.push_back({false, current_literal});
    }
    
    return std::make_shared<FStringExpr>(fragments);
}

std::any AstBuilder::visitTrueAtom(AvaLangParser::TrueAtomContext*) {
    return std::make_shared<BoolExpr>(true);
}

std::any AstBuilder::visitFalseAtom(AvaLangParser::FalseAtomContext*) {
    return std::make_shared<BoolExpr>(false);
}

std::any AstBuilder::visitNilAtom(AvaLangParser::NilAtomContext*) {
    return std::make_shared<NilExpr>();
}

std::any AstBuilder::visitListLiteral(AvaLangParser::ListLiteralContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> items;
    for (auto* e : ctx->expr()) {
        items.push_back(exprFromAny(e->accept(this)));
    }
    return std::make_shared<ListExpr>(items);
}

std::any AstBuilder::visitDictLiteral(AvaLangParser::DictLiteralContext* ctx) {
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> entries;
    for (auto* de : ctx->dictEntry()) {
        std::string key;
        if (de->NAME()) key = de->NAME()->getText();
        else if (de->STRING()) key = stripQuotes(de->STRING()->getText());
        entries.push_back({key, exprFromAny(de->expr()->accept(this))});
    }
    return std::make_shared<DictExpr>(entries);
}

std::any AstBuilder::visitGroupAtom(AvaLangParser::GroupAtomContext* ctx) {
    return exprFromAny(ctx->expr()->accept(this));
}

std::any AstBuilder::visitBaseAtom(AvaLangParser::BaseAtomContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> args;
    if (ctx->argList()) {
        for (auto* a : ctx->argList()->arg()) {
            if (auto* named = dynamic_cast<AvaLangParser::NamedArgContext*>(a)) {
                args.push_back(exprFromAny(named->expr()->accept(this)));
            } else if (auto* pos = dynamic_cast<AvaLangParser::PositionalArgContext*>(a)) {
                args.push_back(exprFromAny(pos->expr()->accept(this)));
            }
        }
    }
    return std::make_shared<BaseExpr>(args);
}

std::any AstBuilder::visitImportStatement(AvaLangParser::ImportStatementContext* ctx) {
    std::vector<std::string> module_path;
    size_t name_count = ctx->NAME().size();
    
    if (ctx->as) {
        for (size_t i = 0; i < name_count - 1; ++i) {
            module_path.push_back(ctx->NAME(i)->getText());
        }
        std::string alias = ctx->NAME(name_count - 1)->getText();
        return std::make_shared<ImportStmt>(module_path, alias);
    } else {
        for (size_t i = 0; i < name_count; ++i) {
            module_path.push_back(ctx->NAME(i)->getText());
        }
        return std::make_shared<ImportStmt>(module_path, "");
    }
}

std::any AstBuilder::visitRaiseStatement(AvaLangParser::RaiseStatementContext* ctx) {
    auto value = exprFromAny(ctx->expr()->accept(this));
    return std::make_shared<RaiseStmt>(value);
}

std::any AstBuilder::visitYieldStatement(AvaLangParser::YieldStatementContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> values;
    if (ctx->exprList()) {
        for (auto* e : ctx->exprList()->expr()) {
            values.push_back(exprFromAny(e->accept(this)));
        }
    }
    return std::make_shared<YieldStmt>(values);
}

std::any AstBuilder::visitSliceTrailer(AvaLangParser::SliceTrailerContext* ctx) {
    auto* slice_range = ctx->sliceRange();

    std::shared_ptr<ExprNode> start = nullptr;
    std::shared_ptr<ExprNode> end = nullptr;
    std::shared_ptr<ExprNode> step = nullptr;

    auto exprs = slice_range->expr();

    // NOTE: the grammar rule `sliceRange: expr? ':' expr? (':' expr?)?;`
    // has no labeled subrules, so `slice_range->start`/`->stop` are just
    // ANTLR's built-in ParserRuleContext::start/stop token pointers (the
    // first/last token of the whole rule) -- NOT indicators of which colon
    // slots are present. Those are always non-null, which previously made
    // colon_count always evaluate to 2 and every parsed slice collapse its
    // single expr into `step` while leaving start/end null (e.g. arr[:3]
    // was parsed as slice(arr, nil, nil, 3) instead of slice(arr, nil, 3,
    // nil)).
    //
    // ANTLR also flattens all the optional expr() matches from every slot
    // into a single list, so we can't tell from exprs.size() alone which
    // slot a given expr came from. Instead walk the actual children in
    // source order, using real ':' terminal tokens to track which slot
    // (start/end/step) we're currently in.
    size_t slot = 0; // 0 = start, 1 = end, 2 = step
    size_t expr_idx = 0;
    for (auto* child : slice_range->children) {
        if (child->getText() == ":") {
            slot++;
        } else if (expr_idx < exprs.size() &&
                   static_cast<antlr4::tree::ParseTree*>(exprs[expr_idx]) == child) {
            auto val = exprFromAny(exprs[expr_idx]->accept(this));
            if (slot == 0) start = val;
            else if (slot == 1) end = val;
            else step = val;
            expr_idx++;
        }
    }

    return std::make_shared<SliceExpr>(nullptr, start, end, step);
}

} // namespace ava