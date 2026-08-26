#ifndef AVA_AST_AST_BUILDER_H
#define AVA_AST_AST_BUILDER_H

#include "AvaLangParser.h"
#include "AvaLangBaseVisitor.h"
#include "ast.h"

namespace ava {

class AstBuilder : public AvaLangBaseVisitor {
public:
    std::any visitChunk(AvaLangParser::ChunkContext* ctx) override;
    std::any visitBlock(AvaLangParser::BlockContext* ctx) override;
    std::any visitStatement(AvaLangParser::StatementContext* ctx) override;
    std::any visitSimpleStatement(AvaLangParser::SimpleStatementContext* ctx) override;
    std::any visitSmallStatement(AvaLangParser::SmallStatementContext* ctx) override;
    std::any visitCompoundStatement(AvaLangParser::CompoundStatementContext* ctx) override;

    std::any visitAssignStatement(AvaLangParser::AssignStatementContext* ctx) override;
    std::any visitMultiAssignStatement(AvaLangParser::MultiAssignStatementContext* ctx) override;
    std::any visitAugAssignStatement(AvaLangParser::AugAssignStatementContext* ctx) override;
    // Phase 3 of AvaLang_Plan_Sistema_de_Tipos.md: `age as int = 25` and
    // `age as int` (no initializer). Both build an AssignStmt with
    // explicit_type set (see ast.h) -- kept as separate visitors instead
    // of folding into visitAssignStatement because their grammar contexts
    // are distinct types (TypedAssignStatementContext /
    // TypedDeclStatementContext, not AssignStatementContext), same as the
    // grammar itself keeps them as separate productions (see
    // grammar/AvaLang.g4, Phase 2).
    std::any visitTypedAssignStatement(AvaLangParser::TypedAssignStatementContext* ctx) override;
    std::any visitTypedDeclStatement(AvaLangParser::TypedDeclStatementContext* ctx) override;
    std::any visitExprStatement(AvaLangParser::ExprStatementContext* ctx) override;
    std::any visitReturnStatement(AvaLangParser::ReturnStatementContext* ctx) override;
    std::any visitBreakStatement(AvaLangParser::BreakStatementContext* ctx) override;
    std::any visitContinueStatement(AvaLangParser::ContinueStatementContext* ctx) override;
    std::any visitPassStatement(AvaLangParser::PassStatementContext* ctx) override;
    std::any visitLocalStatement(AvaLangParser::LocalStatementContext* ctx) override;

    std::any visitIfStatement(AvaLangParser::IfStatementContext* ctx) override;
    std::any visitSelectStatement(AvaLangParser::SelectStatementContext* ctx) override;
    std::any visitWhileStatement(AvaLangParser::WhileStatementContext* ctx) override;
    std::any visitForStatement(AvaLangParser::ForStatementContext* ctx) override;
    std::any visitFuncDeclaration(AvaLangParser::FuncDeclarationContext* ctx) override;
    std::any visitClassDeclaration(AvaLangParser::ClassDeclarationContext* ctx) override;
    std::any visitImportStatement(AvaLangParser::ImportStatementContext* ctx) override;
    std::any visitRaiseStatement(AvaLangParser::RaiseStatementContext* ctx) override;
    std::any visitYieldAtom(AvaLangParser::YieldAtomContext* ctx) override;
    std::any visitTryStatement(AvaLangParser::TryStatementContext* ctx) override;
    std::any visitIncDecStatement(AvaLangParser::IncDecStatementContext* ctx) override;
    std::any visitModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext* ctx) override;
    std::any visitAsyncFuncDeclaration(AvaLangParser::AsyncFuncDeclarationContext* ctx) override;
    std::any visitAwaitAtom(AvaLangParser::AwaitAtomContext* ctx) override;
    std::any visitModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext* ctx) override;
    std::any visitExternStatement(AvaLangParser::ExternStatementContext* ctx) override;
    std::any visitExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext* ctx) override;

    std::any visitExprList(AvaLangParser::ExprListContext* ctx) override;
    std::any visitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext* ctx) override;
    // Phase 14 of AvaLang_Plan_Sistema_de_Tipos.md: the new bare
    // `x => expr` form (grammar/AvaLang.g4's singleParamLambdaExpr).
    std::any visitSingleParamLambdaExprAlt(AvaLangParser::SingleParamLambdaExprAltContext* ctx) override;
    std::any visitLambdaExprAlt(AvaLangParser::LambdaExprAltContext* ctx) override;
    std::any visitOrExprAlt(AvaLangParser::OrExprAltContext* ctx) override;
    std::any visitAndExpr(AvaLangParser::AndExprContext* ctx) override;
    std::any visitNotExpr(AvaLangParser::NotExprContext* ctx) override;
    std::any visitComparison(AvaLangParser::ComparisonContext* ctx) override;
    std::any visitAdditive(AvaLangParser::AdditiveContext* ctx) override;
    std::any visitMultiplicative(AvaLangParser::MultiplicativeContext* ctx) override;
    std::any visitUnary(AvaLangParser::UnaryContext* ctx) override;
    std::any visitPower(AvaLangParser::PowerContext* ctx) override;
    std::any visitPostfix(AvaLangParser::PostfixContext* ctx) override;
    std::any visitNameAtom(AvaLangParser::NameAtomContext* ctx) override;
    std::any visitNumberAtom(AvaLangParser::NumberAtomContext* ctx) override;
    std::any visitStringAtom(AvaLangParser::StringAtomContext* ctx) override;
    std::any visitFstringAtom(AvaLangParser::FstringAtomContext* ctx) override;
    std::any visitTrueAtom(AvaLangParser::TrueAtomContext* ctx) override;
    std::any visitFalseAtom(AvaLangParser::FalseAtomContext* ctx) override;
    std::any visitNilAtom(AvaLangParser::NilAtomContext* ctx) override;
    std::any visitListLiteral(AvaLangParser::ListLiteralContext* ctx) override;
    std::any visitDictLiteral(AvaLangParser::DictLiteralContext* ctx) override;
    std::any visitGroupAtom(AvaLangParser::GroupAtomContext* ctx) override;
    std::any visitBaseAtom(AvaLangParser::BaseAtomContext* ctx) override;
    std::any visitSliceTrailer(AvaLangParser::SliceTrailerContext* ctx) override;

private:
    std::shared_ptr<ExprNode> exprFromAny(const std::any& a);
    std::shared_ptr<StmtNode> stmtFromAny(const std::any& a);
    std::vector<std::shared_ptr<StmtNode>> stmtsFromAny(const std::any& a);

    std::shared_ptr<ExprNode> makeString(const std::string& s);
    std::shared_ptr<ExprNode> exprFromTarget(AvaLangParser::TargetContext* t);

    // Nombre unico para la variable temporal que guarda el valor del
    // `select` (ver visitSelectStatement). Un contador simple alcanza:
    // el builder recorre el AST de forma secuencial y de un solo hilo,
    // asi que no hace falta nada mas elaborado para evitar colisiones
    // entre `select`s hermanos o anidados.
    int select_counter_ = 0;
};

} // namespace ava

#endif // AVA_AST_AST_BUILDER_H