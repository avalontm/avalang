#include "ast_builder.h"
#include "../common/ava_error.h"
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

// Strips the surrounding quotes from a raw STRING token AND resolves the
// escape sequences the grammar's ESCAPE_SEQ fragment allows inside one
// (\b \t \n \r \" \' \\). stripQuotes() alone leaves those as literal
// backslash-letter pairs, which is wrong for actual string content -- use
// this instead wherever a STRING token's *value* (not just its raw text)
// is needed.
static std::string unescapeString(const std::string& raw_token) {
    std::string s = stripQuotes(raw_token);
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            switch (next) {
                case 'b': out += '\b'; break;
                case 't': out += '\t'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case '"': out += '"'; break;
                case '\'': out += '\''; break;
                case '\\': out += '\\'; break;
                default: out += '\\'; out += next; break;
            }
            i++;
        } else {
            out += s[i];
        }
    }
    return out;
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
    if (txt == "//") return BinOp::IDiv;
    if (txt == "%")  return BinOp::Mod;
    if (txt == "**") return BinOp::Pow;
    // Fase 2 del plan break/continue/operadores.
    if (txt == "&")  return BinOp::BAnd;
    if (txt == "|")  return BinOp::BOr;
    if (txt == "^")  return BinOp::BXor;
    if (txt == "<<") return BinOp::Shl;
    if (txt == ">>") return BinOp::Shr;
    throw std::runtime_error("unknown binop: " + txt);
}

static BinOp augOpToBinOp(const std::string& op) {
    if (op == "+=")  return BinOp::Add;
    if (op == "-=")  return BinOp::Sub;
    if (op == "*=")  return BinOp::Mul;
    if (op == "/=")  return BinOp::Div;
    if (op == "%=")  return BinOp::Mod;
    if (op == "//=") return BinOp::IDiv;
    throw std::runtime_error("unknown augop: " + op);
}

// Resuelve `memberModifier+` (ver grammar/AvaLang.g4, Fase A) a los dos
// flags booleanos que terminan viviendo en AssignStmt/FuncDef. Rechaza el
// mismo modificador repetido dos veces (`private private x = 1`) -- es
// una validación semántica simple, no algo que la gramática necesite
// resolver por sí sola.
static void ResolveMemberModifiers(const std::vector<AvaLangParser::MemberModifierContext*>& mods,
                                    bool& is_static, bool& is_private) {
    is_static = false;
    is_private = false;
    for (auto* mod : mods) {
        auto text = mod->getText();
        if (text == "static") {
            if (is_static) throw std::runtime_error("modificador 'static' repetido en la misma declaracion");
            is_static = true;
        } else if (text == "private") {
            if (is_private) throw std::runtime_error("modificador 'private' repetido en la misma declaracion");
            is_private = true;
        }
    }
}

std::shared_ptr<ExprNode> AstBuilder::makeString(const std::string& s) {
    return std::make_shared<StringExpr>(unescapeString(s));
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
    if (a.type() == typeid(std::shared_ptr<YieldExpr>)) {
        return std::any_cast<std::shared_ptr<YieldExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<AwaitExpr>)) {
        return std::any_cast<std::shared_ptr<AwaitExpr>>(a);
    }
    if (a.type() == typeid(std::shared_ptr<TernaryExpr>)) {
        return std::any_cast<std::shared_ptr<TernaryExpr>>(a);
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
        auto multi_assign = std::any_cast<std::shared_ptr<MultiAssignStmt>>(a);
        return multi_assign;
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
        auto for_range_stmt = std::any_cast<std::shared_ptr<ForRangeStmt>>(a);
        return for_range_stmt;
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
        auto extern_stmt = std::any_cast<std::shared_ptr<ExternStmt>>(a);
        return extern_stmt;
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
    auto targets = ctx->targetList()->target();
    auto exprs = ctx->exprList()->expr();
    if (targets.size() > 1 || exprs.size() > 1) {
        throw AvaError(
            "asignacion multiple requiere 'x = v, y = w' (cada asignacion con su propio '='); "
            "la forma 'x, y = v, w' no esta soportada",
            static_cast<int>(ctx->getStart()->getLine()),
            static_cast<int>(ctx->getStart()->getCharPositionInLine()) + 1
        );
    }
    auto target = exprFromTarget(targets[0]);
    auto value = exprFromAny(exprs[0]->accept(this));
    return std::make_shared<AssignStmt>(target, value);
}

// Phase 3. Grammar: `NAME typeAnnotation
// '=' expr` (grammar/AvaLang.g4, typedAssignStatement, Phase 2) -- target is
// always a bare NAME here (unlike visitAssignStatement's target, which can
// carry index/attr trailers), so build the NameExpr directly instead of
// going through exprFromTarget(TargetContext*), which expects a different
// context type.
std::any AstBuilder::visitTypedAssignStatement(AvaLangParser::TypedAssignStatementContext* ctx) {
    auto target = std::make_shared<NameExpr>(ctx->NAME()->getText());
    auto value = exprFromAny(ctx->expr()->accept(this));
    auto assign = std::make_shared<AssignStmt>(target, value);
    assign->explicit_type = ctx->typeAnnotation()->NAME()->getText();
    return assign;
}

// `NAME typeAnnotation` with no `= expr` (grammar/AvaLang.g4,
// typedDeclStatement). value stays nullptr -- see ast.h's AssignStmt
// comment: the compiler does not yet do anything sensible with a null
// value (that lands in Phase 4/7); this only builds the AST node.
std::any AstBuilder::visitTypedDeclStatement(AvaLangParser::TypedDeclStatementContext* ctx) {
    auto target = std::make_shared<NameExpr>(ctx->NAME()->getText());
    auto assign = std::make_shared<AssignStmt>(target, nullptr);
    assign->explicit_type = ctx->typeAnnotation()->NAME()->getText();
    return assign;
}

std::any AstBuilder::visitMultiAssignStatement(AvaLangParser::MultiAssignStatementContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> targets;
    std::vector<std::shared_ptr<ExprNode>> values;
    for (auto* assign : ctx->assignStatement()) {
        auto assign_targets = assign->targetList()->target();
        auto assign_exprs = assign->exprList()->expr();
        if (assign_targets.size() > 1 || assign_exprs.size() > 1) {
            throw AvaError(
                "asignacion multiple requiere 'x = v, y = w' (cada asignacion con su propio '='); "
                "la forma 'x, y = v, w' no esta soportada",
                static_cast<int>(assign->getStart()->getLine()),
                static_cast<int>(assign->getStart()->getCharPositionInLine()) + 1
            );
        }
        auto target = exprFromTarget(assign_targets[0]);
        auto value = exprFromAny(assign_exprs[0]->accept(this));
        targets.push_back(target);
        values.push_back(value);
    }
    return std::make_shared<MultiAssignStmt>(targets, values);
}

std::any AstBuilder::visitChunk(AvaLangParser::ChunkContext* ctx) {
    auto chunk = std::make_shared<Chunk>();
    std::vector<std::shared_ptr<StmtNode>> stmts;
    for (auto* s : ctx->statement()) {
        auto any_result = s->accept(this);
        auto single_stmt = stmtFromAny(any_result);
        if (single_stmt) {
            stmts.push_back(single_stmt);
        } else {
            auto more_stmts = stmtsFromAny(any_result);
            stmts.insert(stmts.end(), more_stmts.begin(), more_stmts.end());
        }
    }
    chunk->statements = stmts;
    return chunk;
}

std::any AstBuilder::visitBlock(AvaLangParser::BlockContext* ctx) {
    std::vector<std::shared_ptr<StmtNode>> stmts;
    for (auto* s : ctx->statement()) {
        auto any_result = s->accept(this);
        auto single_stmt = stmtFromAny(any_result);
        if (single_stmt) {
            stmts.push_back(single_stmt);
        } else {
            auto more_stmts = stmtsFromAny(any_result);
            stmts.insert(stmts.end(), more_stmts.begin(), more_stmts.end());
        }
    }
    return stmts;
}

std::any AstBuilder::visitStatement(AvaLangParser::StatementContext* ctx) {
    std::any result = ctx->simpleStatement()
        ? visitSimpleStatement(ctx->simpleStatement())
        : visitCompoundStatement(ctx->compoundStatement());

    // Every statement passes through here regardless of its concrete kind,
    // so this is the single point where source lines get stamped onto the
    // AST (see AstNode::line). stmtFromAny already knows how to pull a
    // shared_ptr<StmtNode> out of any concrete statement type's std::any --
    // reuse it here instead of duplicating that dispatch, then re-wrap as
    // the base type so downstream stmtFromAny/stmtsFromAny calls (which
    // try the base type first) still find it.
    auto stmt = stmtFromAny(result);
    if (stmt) {
        stmt->line = static_cast<int>(ctx->getStart()->getLine());
        stmt->col = static_cast<int>(ctx->getStart()->getCharPositionInLine()) + 1;
        return std::any(stmt);
    }
    return result;
}

std::any AstBuilder::visitSimpleStatement(AvaLangParser::SimpleStatementContext* ctx) {
    return visitSmallStatement(ctx->smallStatement());
}

std::any AstBuilder::visitSmallStatement(AvaLangParser::SmallStatementContext* ctx) {
    if (ctx->assignStatement())    return visitAssignStatement(ctx->assignStatement());
    if (ctx->multiAssignStatement()) return visitMultiAssignStatement(ctx->multiAssignStatement());
    if (ctx->augAssignStatement()) return visitAugAssignStatement(ctx->augAssignStatement());
    if (ctx->exprStatement())     return visitExprStatement(ctx->exprStatement());
    if (ctx->returnStatement())   return visitReturnStatement(ctx->returnStatement());
    if (ctx->breakStatement())    return std::make_shared<BreakStmt>();
    if (ctx->continueStatement()) return std::make_shared<ContinueStmt>();
    if (ctx->passStatement())     return std::make_shared<PassStmt>();
    if (ctx->localStatement())    return visitLocalStatement(ctx->localStatement());
    if (ctx->importStatement())   return visitImportStatement(ctx->importStatement());
    if (ctx->raiseStatement())    return visitRaiseStatement(ctx->raiseStatement());
    if (ctx->incDecStatement())  return visitIncDecStatement(ctx->incDecStatement());
    if (ctx->modifiedAssignStatement()) return visitModifiedAssignStatement(ctx->modifiedAssignStatement());
    if (ctx->typedAssignStatement()) return visitTypedAssignStatement(ctx->typedAssignStatement());
    if (ctx->typedDeclStatement())   return visitTypedDeclStatement(ctx->typedDeclStatement());
    throw std::runtime_error("unsupported small statement");
}

std::any AstBuilder::visitIncDecStatement(AvaLangParser::IncDecStatementContext* ctx) {
    auto target = exprFromTarget(ctx->target());
    auto op_text = ctx->children[0]->getText();
    UnOp op = (op_text == "++") ? UnOp::Inc : UnOp::Dec;
    return std::make_shared<ExprStmt>(std::make_shared<UnOpExpr>(op, target));
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
    if (ctx->modifiedFuncDeclaration()) return visitModifiedFuncDeclaration(ctx->modifiedFuncDeclaration());
    if (ctx->asyncFuncDeclaration()) return visitAsyncFuncDeclaration(ctx->asyncFuncDeclaration());
    if (ctx->externStatement()) return visitExternStatement(ctx->externStatement());
    if (ctx->selectStatement()) return visitSelectStatement(ctx->selectStatement());
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
        auto exprs = ctx->exprList()->expr();
        if (exprs.size() == 1) {
            return std::make_shared<ReturnStmt>(exprFromAny(exprs[0]->accept(this)));
        }
        // Bug critico: 'return a, b, c' descartaba silenciosamente todos los
        // valores salvo el primero (solo se tomaba exprList()->expr(0)).
        // AvaLang no soporta destructuring de asignacion (x, y = v, w esta
        // explicitamente rechazado en visitAssignStatement), asi que en vez
        // de inventar un tipo tupla nuevo o tocar la VM, empaquetamos los
        // valores en una lista real -- no se pierde nada y el caller puede
        // indexar/iterar el resultado como cualquier otra lista.
        std::vector<std::shared_ptr<ExprNode>> items;
        items.reserve(exprs.size());
        for (auto* e : exprs) {
            items.push_back(exprFromAny(e->accept(this)));
        }
        return std::make_shared<ReturnStmt>(std::make_shared<ListExpr>(items));
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

// `local x = 1` / `local x as int = 1` / `local x as int` (grammar/
// AvaLang.g4, localStatement, Phase 2 adds the last two alternatives).
// Phase 7: marks the resulting AssignStmt with is_local = true (see ast.h)
// so the compiler knows this line always starts a fresh binding on the
// type side instead of validating against a same-named symbol already in
// scope -- same std::any_cast-and-tag pattern as
// visitModifiedAssignStatement does for `static`/`private`.
std::any AstBuilder::visitLocalStatement(AvaLangParser::LocalStatementContext* ctx) {
    std::shared_ptr<AssignStmt> assign;
    if (ctx->assignStatement()) {
        assign = std::any_cast<std::shared_ptr<AssignStmt>>(visitAssignStatement(ctx->assignStatement()));
    } else if (ctx->typedAssignStatement()) {
        assign = std::any_cast<std::shared_ptr<AssignStmt>>(visitTypedAssignStatement(ctx->typedAssignStatement()));
    } else if (ctx->typedDeclStatement()) {
        assign = std::any_cast<std::shared_ptr<AssignStmt>>(visitTypedDeclStatement(ctx->typedDeclStatement()));
    } else {
        throw std::runtime_error("unsupported local statement");
    }
    assign->is_local = true;
    return assign;
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

// Desazucara `select` a un IfStmt equivalente en vez de agregar un nodo
// de AST y soporte de compilador/VM nuevos:
//
//   select expr
//       case a, b then BODY1
//       case c to d then BODY2
//       case is >= e then BODY3
//       else BODY4
//   end
//
// se vuelve
//
//   __select$N = expr
//   if __select$N == a or __select$N == b then BODY1
//   elif __select$N >= c and __select$N <= d then BODY2
//   elif __select$N >= e then BODY3
//   else BODY4
//   end
//
// El discriminante se evalua una sola vez (en __select$N) para no
// duplicar side effects si `expr` es, por ejemplo, una llamada a
// funcion. Como el resultado es un IfStmt de verdad, se apoya en
// CompileIf tal cual existe hoy -- cero cambios en compiler.cpp/vm.
//
// visitChunk/visitBlock ya saben expandir un compound statement que
// devuelve varios StmtNode (ver stmtsFromAny arriba), asi que basta con
// devolver el vector {asignacion, if} desde aca.
std::any AstBuilder::visitSelectStatement(AvaLangParser::SelectStatementContext* ctx) {
    int line = static_cast<int>(ctx->getStart()->getLine());

    auto discriminant = exprFromAny(ctx->expr()->accept(this));

    std::string tmp_name = "__select$" + std::to_string(select_counter_++);
    auto tmp_ref = [&]() { return std::make_shared<NameExpr>(tmp_name); };

    auto assign_tmp = std::make_shared<AssignStmt>(tmp_ref(), discriminant);
    assign_tmp->line = line;

    // Construye la condicion booleana de un caseItem contra __select$N.
    auto conditionForItem = [&](AvaLangParser::CaseItemContext* item) -> std::shared_ptr<ExprNode> {
        if (auto* range = dynamic_cast<AvaLangParser::CaseItemRangeContext*>(item)) {
            auto kw = range->NAME()->getText();
            if (kw != "to") {
                auto line = static_cast<int>(range->NAME()->getSymbol()->getLine());
                auto col = static_cast<int>(range->NAME()->getSymbol()->getCharPositionInLine());
                throw AvaError("expected 'to' in case range, got '" + kw + "'", line, col);
            }
            auto lo = exprFromAny(range->expr(0)->accept(this));
            auto hi = exprFromAny(range->expr(1)->accept(this));
            auto ge = std::make_shared<BinOpExpr>(BinOp::Ge, tmp_ref(), lo);
            auto le = std::make_shared<BinOpExpr>(BinOp::Le, tmp_ref(), hi);
            return std::make_shared<BinOpExpr>(BinOp::And, ge, le);
        }
        if (auto* rel = dynamic_cast<AvaLangParser::CaseItemRelationalContext*>(item)) {
            auto rhs = exprFromAny(rel->expr()->accept(this));
            BinOp op = compOpToBinOp(rel->compOp()->getText());
            return std::make_shared<BinOpExpr>(op, tmp_ref(), rhs);
        }
        auto* eq = dynamic_cast<AvaLangParser::CaseItemEqualsContext*>(item);
        auto rhs = exprFromAny(eq->expr()->accept(this));
        return std::make_shared<BinOpExpr>(BinOp::Eq, tmp_ref(), rhs);
    };

    // Cada caseClause tiene una lista de caseItem separados por coma
    // (equivalente a "or" entre ellos, como los `Case a, b, c` de VB6).
    auto conditionForClause = [&](AvaLangParser::CaseClauseContext* clause) -> std::shared_ptr<ExprNode> {
        std::shared_ptr<ExprNode> cond = conditionForItem(clause->caseItem(0));
        for (size_t i = 1; i < clause->caseItem().size(); ++i) {
            cond = std::make_shared<BinOpExpr>(BinOp::Or, cond, conditionForItem(clause->caseItem(i)));
        }
        return cond;
    };

    auto clauses = ctx->caseClause();
    auto if_condition = conditionForClause(clauses[0]);
    auto if_body = stmtsFromAny(visitBlock(clauses[0]->block()));

    std::vector<std::pair<std::shared_ptr<ExprNode>, std::vector<std::shared_ptr<StmtNode>>>> elif_clauses;
    for (size_t i = 1; i < clauses.size(); ++i) {
        auto cond = conditionForClause(clauses[i]);
        auto body = stmtsFromAny(visitBlock(clauses[i]->block()));
        elif_clauses.push_back({cond, body});
    }

    std::vector<std::shared_ptr<StmtNode>> else_body;
    if (ctx->elseClause()) {
        else_body = stmtsFromAny(visitBlock(ctx->elseClause()->block()));
    }

    auto if_stmt = std::make_shared<IfStmt>(if_condition, if_body, elif_clauses, else_body);
    if_stmt->line = line;

    std::vector<std::shared_ptr<StmtNode>> result;
    result.push_back(assign_tmp);
    result.push_back(if_stmt);
    return result;
}

std::any AstBuilder::visitWhileStatement(AvaLangParser::WhileStatementContext* ctx) {
    auto condition = exprFromAny(ctx->expr()->accept(this));
    auto body = stmtsFromAny(visitBlock(ctx->block()));
    return std::make_shared<WhileStmt>(condition, body);
}

std::any AstBuilder::visitForStatement(AvaLangParser::ForStatementContext* ctx) {
    // Fase 4 del plan break/continue/operadores: la tercera alternativa
    // de forStatement (`for i = a to b step s`) es la unica de las tres
    // que no pasa por targetList -- ver el comentario de la gramatica.
    if (!ctx->targetList()) {
        auto var_name = ctx->NAME(0)->getText();
        auto to_kw = ctx->NAME(1)->getText();
        if (to_kw != "to") {
            auto line = static_cast<int>(ctx->NAME(1)->getSymbol()->getLine());
            auto col = static_cast<int>(ctx->NAME(1)->getSymbol()->getCharPositionInLine());
            throw AvaError("expected 'to' in for-range loop, got '" + to_kw + "'", line, col);
        }
        auto branches = ctx->expr();
        auto start_expr = exprFromAny(branches[0]->accept(this));
        auto stop_expr = exprFromAny(branches[1]->accept(this));
        std::shared_ptr<ExprNode> step_expr = nullptr;
        if (branches.size() == 3) {
            auto step_kw = ctx->NAME(2)->getText();
            if (step_kw != "step") {
                auto line = static_cast<int>(ctx->NAME(2)->getSymbol()->getLine());
                auto col = static_cast<int>(ctx->NAME(2)->getSymbol()->getCharPositionInLine());
                throw AvaError("expected 'step' in for-range loop, got '" + step_kw + "'", line, col);
            }
            step_expr = exprFromAny(branches[2]->accept(this));
        }
        auto body = stmtsFromAny(visitBlock(ctx->block()));
        return std::make_shared<ForRangeStmt>(var_name, start_expr, stop_expr, step_expr, body);
    }

    auto* targets = ctx->targetList();
    auto var_name = targets->target(0)->NAME()->getText();
    auto iter_expr = exprFromAny(ctx->exprList()->expr(0)->accept(this));
    auto body = stmtsFromAny(visitBlock(ctx->block()));
    return std::make_shared<ForStmt>(var_name, iter_expr, body);
}

std::any AstBuilder::visitFuncDeclaration(AvaLangParser::FuncDeclarationContext* ctx) {
    auto name = ctx->NAME()->getText();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> params;
    // Phase 8 -- parallel to `params`
    // below, built in the same loop so the indices always line up.
    std::vector<std::string> param_types;
    bool is_vararg = false;

    if (ctx->paramList()) {
        for (auto* p : ctx->paramList()->param()) {
            auto pname = p->NAME()->getText();
            std::shared_ptr<ExprNode> def = nullptr;
            if (p->expr()) def = exprFromAny(p->expr()->accept(this));
            params.push_back({pname, def});
            param_types.push_back(p->typeAnnotation() ? p->typeAnnotation()->NAME()->getText() : "");
        }
        if (ctx->paramList()->NAME()) {
            is_vararg = true;
        }
    }

    auto body = stmtsFromAny(visitBlock(ctx->block()));
    auto func = std::make_shared<FuncDef>(name, params, is_vararg, body);
    // Phase 8 ("Funciones"): stop
    // discarding what the grammar has parsed since Phase 2
    // (`param: NAME typeAnnotation? (...)`, `funcDeclaration: ...
    // returnType? block 'end'`). See ast.h's FuncDef::param_types/
    // return_type for what happens with these next (nothing yet --
    // Phase 9/10).
    func->param_types = param_types;
    func->return_type = ctx->returnType() ? ctx->returnType()->typeAnnotation()->NAME()->getText() : "";
    return func;
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

std::any AstBuilder::visitModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext* ctx) {
    bool is_static = false;
    bool is_private = false;
    ResolveMemberModifiers(ctx->memberModifier(), is_static, is_private);

    auto func = std::any_cast<std::shared_ptr<FuncDef>>(visitFuncDeclaration(ctx->funcDeclaration()));
    func->is_static = is_static;
    func->is_private = is_private;
    return func;
}

std::any AstBuilder::visitAsyncFuncDeclaration(AvaLangParser::AsyncFuncDeclarationContext* ctx) {
    auto func = std::any_cast<std::shared_ptr<FuncDef>>(visitFuncDeclaration(ctx->funcDeclaration()));
    func->is_async = true;
    return func;
}

std::any AstBuilder::visitExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext* ctx) {
    ExternFuncDecl decl;
    decl.name = ctx->NAME()->getText();
    if (ctx->externParamList()) {
        for (auto* p : ctx->externParamList()->externParam()) {
            decl.params.push_back(p->NAME()->getText());
            // Phase 16 -- same
            // "" = no annotation convention as visitFuncDeclaration's
            // identical param_types loop above.
            decl.param_types.push_back(p->typeAnnotation() ? p->typeAnnotation()->NAME()->getText() : "");
        }
        if (ctx->externParamList()->NAME()) {
            decl.is_vararg = true;
        }
    }
    // Phase 16: mirrors visitFuncDeclaration's identical return_type line.
    decl.return_type = ctx->returnType() ? ctx->returnType()->typeAnnotation()->NAME()->getText() : "";
    return decl;
}

std::any AstBuilder::visitExternStatement(AvaLangParser::ExternStatementContext* ctx) {
    // unescapeString espera el texto crudo del token STRING (con comillas);
    // ver definición arriba, usada igual para importStatement/StringExpr.
    auto library = unescapeString(ctx->STRING()->getText());
    auto alias = ctx->NAME()->getText();

    std::vector<ExternFuncDecl> functions;
    for (auto* fd : ctx->externFuncDeclaration()) {
        functions.push_back(std::any_cast<ExternFuncDecl>(visitExternFuncDeclaration(fd)));
    }

    return std::make_shared<ExternStmt>(library, alias, functions);
}

std::any AstBuilder::visitModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext* ctx) {
    bool is_static = false;
    bool is_private = false;
    ResolveMemberModifiers(ctx->memberModifier(), is_static, is_private);

    auto assign = std::any_cast<std::shared_ptr<AssignStmt>>(visitAssignStatement(ctx->assignStatement()));
    assign->is_static = is_static;
    assign->is_private = is_private;
    return assign;
}

std::any AstBuilder::visitTryStatement(AvaLangParser::TryStatementContext* ctx) {
    auto try_body = stmtsFromAny(visitBlock(ctx->block()));
    
    std::vector<std::vector<std::shared_ptr<StmtNode>>> except_bodies;
    std::vector<std::shared_ptr<ExprNode>> except_exprs;
    
    for (auto* exc : ctx->exceptClause()) {
        auto exc_body = stmtsFromAny(visitBlock(exc->block()));
        except_bodies.push_back(exc_body);
        
        std::shared_ptr<ExprNode> exc_expr = nullptr;
        
        if (exc->expr()) {
            exc_expr = exprFromAny(exc->expr()->accept(this));
        }
        
        except_exprs.push_back(exc_expr);
    }
    
    std::vector<std::shared_ptr<StmtNode>> finally_body;
    auto* fin = ctx->finallyClause();
    if (fin) {
        finally_body = stmtsFromAny(visitBlock(fin->block()));
    }
    
    return std::make_shared<TryStmt>(try_body, except_bodies, except_exprs, finally_body);
}

std::any AstBuilder::visitExprList(AvaLangParser::ExprListContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> exprs;
    for (auto* e : ctx->expr()) {
        exprs.push_back(exprFromAny(e->accept(this)));
    }
    return exprs;
}

std::any AstBuilder::visitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext* ctx) {
    if (!ctx->shortLambdaExpr()) return visitSingleParamLambdaExprAlt(nullptr);
    auto* lambda = ctx->shortLambdaExpr();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> defaults;
    // Phase 14 -- parallel to
    // `defaults` below, same convention as visitFuncDeclaration's
    // param_types (Phase 8): "" = no annotation for that parameter.
    std::vector<std::string> param_types;
    std::string name = "<lambda>";

    if (lambda->paramList()) {
        for (auto* p : lambda->paramList()->param()) {
            auto pname = p->NAME()->getText();
            std::shared_ptr<ExprNode> def = nullptr;
            if (p->expr()) def = exprFromAny(p->expr()->accept(this));
            defaults.push_back({pname, def});
            param_types.push_back(p->typeAnnotation() ? p->typeAnnotation()->NAME()->getText() : "");
        }
    }

    auto body = std::make_shared<ReturnStmt>(exprFromAny(lambda->expr()->accept(this)));
    std::vector<std::shared_ptr<StmtNode>> body_stmts = {body};
    auto result = std::make_shared<LambdaExpr>(name, defaults, body_stmts, false);
    result->param_types = param_types;
    // Phase 14: the optional `as Type` after the parameter list, before
    // `=>` -- see grammar/AvaLang.g4's shortLambdaExpr and returnType.
    result->return_type = lambda->returnType() ? lambda->returnType()->typeAnnotation()->NAME()->getText() : "";
    return result;
}

// Phase 14 ("Lambdas y funciones como
// valores"): the new bare, paren-less, untyped single-parameter form
// (`callback = x => x * 2`, grammar/AvaLang.g4's singleParamLambdaExpr).
// Mirrors visitShortLambdaExprAlt's body-building (implicit `return` of
// the single expression) but there is no paramList/typeAnnotation/
// returnType to read at all -- the grammar rule itself has no slot for
// any of those (see the grammar comment on why that's deliberate, not a
// gap), so param_types/return_type are left at their default-constructed
// empty state on the resulting LambdaExpr.
std::any AstBuilder::visitSingleParamLambdaExprAlt(AvaLangParser::SingleParamLambdaExprAltContext* ctx) {
    if (!ctx->singleParamLambdaExpr()) return visitLambdaExprAlt(nullptr);
    auto* lambda = ctx->singleParamLambdaExpr();
    auto pname = lambda->NAME()->getText();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> defaults = {{pname, nullptr}};

    auto body = std::make_shared<ReturnStmt>(exprFromAny(lambda->expr()->accept(this)));
    std::vector<std::shared_ptr<StmtNode>> body_stmts = {body};
    return std::make_shared<LambdaExpr>("<lambda>", defaults, body_stmts, false);
}

std::any AstBuilder::visitLambdaExprAlt(AvaLangParser::LambdaExprAltContext* ctx) {
    if (!ctx->lambdaExpr()) return visitOrExprAlt(nullptr);
    auto* lambda = ctx->lambdaExpr();
    std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>> defaults;
    std::vector<std::string> param_types;
    bool is_vararg = false;
    std::string name = "<lambda>";

    if (lambda->paramList()) {
        for (auto* p : lambda->paramList()->param()) {
            auto pname = p->NAME()->getText();
            std::shared_ptr<ExprNode> def = nullptr;
            if (p->expr()) def = exprFromAny(p->expr()->accept(this));
            defaults.push_back({pname, def});
            param_types.push_back(p->typeAnnotation() ? p->typeAnnotation()->NAME()->getText() : "");
        }
        if (lambda->paramList()->NAME()) is_vararg = true;
    }

    auto body = stmtsFromAny(visitBlock(lambda->block()));
    auto result = std::make_shared<LambdaExpr>(name, defaults, body, is_vararg);
    result->param_types = param_types;
    // Phase 14: same returnType? slot as shortLambdaExpr above, added to
    // the `func(...) end` anonymous lambda form in symmetry with
    // funcDeclaration (see grammar/AvaLang.g4's lambdaExpr).
    result->return_type = lambda->returnType() ? lambda->returnType()->typeAnnotation()->NAME()->getText() : "";
    return result;
}

std::any AstBuilder::visitOrExprAlt(AvaLangParser::OrExprAltContext* ctx) {
    if (!ctx) return nullptr;
    return visitTernaryExpr(ctx->ternaryExpr());
}

// Fase 3 del plan break/continue/operadores: `cond ? then : else`. Sin
// '?', se reduce a un simple pass-through al chain de orExpr (misma
// forma que antes tenia visitOrExprAlt, movida aca porque ahora orExpr
// se llega vía ternaryExpr en vez de directo desde el alt de expr).
std::any AstBuilder::visitTernaryExpr(AvaLangParser::TernaryExprContext* ctx) {
    if (!ctx) return nullptr;
    auto* orCtx = ctx->orExpr();
    if (!orCtx) return nullptr;
    auto cond = exprFromAny(visitOrExpr(orCtx));

    auto branches = ctx->expr();
    if (branches.size() == 2) {
        auto then_e = exprFromAny(branches[0]->accept(this));
        auto else_e = exprFromAny(branches[1]->accept(this));
        return std::make_shared<TernaryExpr>(cond, then_e, else_e);
    }
    return cond;
}

std::any AstBuilder::visitOrExpr(AvaLangParser::OrExprContext* ctx) {
    auto exprs = ctx->andExpr();
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
    auto exprs = ctx->bitOr();
    auto ops = ctx->compOp();

    if (ops.empty()) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 0; i < ops.size(); ++i) {
        auto right = exprFromAny(exprs[i + 1]->accept(this));
        left = std::make_shared<BinOpExpr>(compOpToBinOp(ops[i]->getText()), left, right);
    }
    return left;
}

// Fase 2 del plan break/continue/operadores: bitwise OR/XOR/AND, un solo
// operador por nivel (igual que visitAndExpr), y shift (`<<`/`>>`), que
// como additive/multiplicative necesita buscar el token operador entre
// los hijos porque hay dos posibles en ese nivel.
std::any AstBuilder::visitBitOr(AvaLangParser::BitOrContext* ctx) {
    auto exprs = ctx->bitXor();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        left = std::make_shared<BinOpExpr>(BinOp::BOr, left, right);
    }
    return left;
}

std::any AstBuilder::visitBitXor(AvaLangParser::BitXorContext* ctx) {
    auto exprs = ctx->bitAnd();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        left = std::make_shared<BinOpExpr>(BinOp::BXor, left, right);
    }
    return left;
}

std::any AstBuilder::visitBitAnd(AvaLangParser::BitAndContext* ctx) {
    auto exprs = ctx->shift();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto right = exprFromAny(exprs[i]->accept(this));
        left = std::make_shared<BinOpExpr>(BinOp::BAnd, left, right);
    }
    return left;
}

std::any AstBuilder::visitShift(AvaLangParser::ShiftContext* ctx) {
    auto exprs = ctx->additive();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    std::vector<antlr4::tree::TerminalNode*> ops;
    for (auto* child : ctx->children) {
        if (auto* t = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            auto txt = t->getText();
            if (txt == "<<" || txt == ">>") ops.push_back(t);
        }
    }

    for (size_t i = 0; i < ops.size(); ++i) {
        auto right = exprFromAny(exprs[i + 1]->accept(this));
        left = std::make_shared<BinOpExpr>(textToBinOp(ops[i]->getText()), left, right);
    }
    return left;
}

std::any AstBuilder::visitAdditive(AvaLangParser::AdditiveContext* ctx) {
    auto exprs = ctx->multiplicative();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    std::vector<antlr4::tree::TerminalNode*> ops;
    for (auto* child : ctx->children) {
        if (auto* t = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            auto txt = t->getText();
            if (txt == "+" || txt == "-") ops.push_back(t);
        }
    }

    for (size_t i = 0; i < ops.size(); ++i) {
        auto right = exprFromAny(exprs[i + 1]->accept(this));
        left = std::make_shared<BinOpExpr>(textToBinOp(ops[i]->getText()), left, right);
    }
    return left;
}

std::any AstBuilder::visitMultiplicative(AvaLangParser::MultiplicativeContext* ctx) {
    auto exprs = ctx->unary();
    if (exprs.size() == 1) return exprFromAny(exprs[0]->accept(this));

    auto left = exprFromAny(exprs[0]->accept(this));
    std::vector<antlr4::tree::TerminalNode*> ops;
    for (auto* child : ctx->children) {
        if (auto* t = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            auto txt = t->getText();
            if (txt == "*" || txt == "/" || txt == "%" || txt == "//") ops.push_back(t);
        }
    }

    for (size_t i = 0; i < ops.size(); ++i) {
        auto right = exprFromAny(exprs[i + 1]->accept(this));
        left = std::make_shared<BinOpExpr>(textToBinOp(ops[i]->getText()), left, right);
    }
    return left;
}

std::any AstBuilder::visitUnary(AvaLangParser::UnaryContext* ctx) {
    if (ctx->unary()) {
        auto operand = exprFromAny(ctx->unary()->accept(this));
        auto txt = ctx->children[0]->getText();
        if (txt == "-") return std::make_shared<UnOpExpr>(UnOp::Neg, operand);
        if (txt == "++") return std::make_shared<UnOpExpr>(UnOp::Inc, operand);
        if (txt == "--") return std::make_shared<UnOpExpr>(UnOp::Dec, operand);
        if (txt == "~") return std::make_shared<UnOpExpr>(UnOp::BNot, operand);
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
        } else if (dynamic_cast<AvaLangParser::IncTrailerContext*>(t)) {
            expr = std::make_shared<UnOpExpr>(UnOp::Inc, expr);
        } else if (dynamic_cast<AvaLangParser::DecTrailerContext*>(t)) {
            expr = std::make_shared<UnOpExpr>(UnOp::Dec, expr);
        }
    }
    return expr;
}

std::any AstBuilder::visitNameAtom(AvaLangParser::NameAtomContext* ctx) {
    return std::make_shared<NameExpr>(ctx->NAME()->getText());
}

std::any AstBuilder::visitNumberAtom(AvaLangParser::NumberAtomContext* ctx) {
    std::string text = ctx->NUMBER()->getText();
    double val = std::stod(text);
    // Phase 5: `10` -> Int, `10.0` ->
    // Float (see NumberExpr::is_float in ast.h). Textual check, not a check
    // on `val`, so `10.0` doesn't get misread as an int-valued double.
    bool is_float = text.find('.') != std::string::npos ||
                    text.find('e') != std::string::npos ||
                    text.find('E') != std::string::npos;
    return std::make_shared<NumberExpr>(val, is_float);
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
        else if (de->STRING()) key = unescapeString(de->STRING()->getText());
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
    // 'base.NAME(args)' calls the parent's NAME method; plain 'base(args)'
    // keeps calling the parent constructor (__init__), same as before.
    std::string method_name = ctx->NAME() ? ctx->NAME()->getText() : "__init__";
    std::shared_ptr<ExprNode> expr = std::make_shared<BaseExpr>(args, method_name);

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
            std::vector<std::shared_ptr<ExprNode>> call_args;
            if (call->argList()) {
                for (auto* a : call->argList()->arg()) {
                    if (auto* named = dynamic_cast<AvaLangParser::NamedArgContext*>(a)) {
                        call_args.push_back(exprFromAny(named->expr()->accept(this)));
                    } else if (auto* pos = dynamic_cast<AvaLangParser::PositionalArgContext*>(a)) {
                        call_args.push_back(exprFromAny(pos->expr()->accept(this)));
                    }
                }
            }
            expr = std::make_shared<CallExpr>(expr, call_args);
        } else if (dynamic_cast<AvaLangParser::IncTrailerContext*>(t)) {
            expr = std::make_shared<UnOpExpr>(UnOp::Inc, expr);
        } else if (dynamic_cast<AvaLangParser::DecTrailerContext*>(t)) {
            expr = std::make_shared<UnOpExpr>(UnOp::Dec, expr);
        }
    }
    return expr;
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

std::any AstBuilder::visitYieldAtom(AvaLangParser::YieldAtomContext* ctx) {
    std::vector<std::shared_ptr<ExprNode>> values;
    if (ctx->exprList()) {
        for (auto* e : ctx->exprList()->expr()) {
            values.push_back(exprFromAny(e->accept(this)));
        }
    }
    return std::make_shared<YieldExpr>(values);
}

std::any AstBuilder::visitAwaitAtom(AvaLangParser::AwaitAtomContext* ctx) {
    auto value = exprFromAny(ctx->expr()->accept(this));
    return std::make_shared<AwaitExpr>(value);
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