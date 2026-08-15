
// Generated from /mnt/d/_CODE_/avalang/runtime/avalang/grammar/AvaLang.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "AvaLangParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by AvaLangParser.
 */
class  AvaLangVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by AvaLangParser.
   */
    virtual std::any visitChunk(AvaLangParser::ChunkContext *context) = 0;

    virtual std::any visitBlock(AvaLangParser::BlockContext *context) = 0;

    virtual std::any visitStatement(AvaLangParser::StatementContext *context) = 0;

    virtual std::any visitSimpleStatement(AvaLangParser::SimpleStatementContext *context) = 0;

    virtual std::any visitSmallStatement(AvaLangParser::SmallStatementContext *context) = 0;

    virtual std::any visitModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext *context) = 0;

    virtual std::any visitIncDecStatement(AvaLangParser::IncDecStatementContext *context) = 0;

    virtual std::any visitCompoundStatement(AvaLangParser::CompoundStatementContext *context) = 0;

    virtual std::any visitMemberModifier(AvaLangParser::MemberModifierContext *context) = 0;

    virtual std::any visitModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext *context) = 0;

    virtual std::any visitTryStatement(AvaLangParser::TryStatementContext *context) = 0;

    virtual std::any visitExceptClause(AvaLangParser::ExceptClauseContext *context) = 0;

    virtual std::any visitFinallyClause(AvaLangParser::FinallyClauseContext *context) = 0;

    virtual std::any visitMultiAssignStatement(AvaLangParser::MultiAssignStatementContext *context) = 0;

    virtual std::any visitAssignStatement(AvaLangParser::AssignStatementContext *context) = 0;

    virtual std::any visitAugAssignStatement(AvaLangParser::AugAssignStatementContext *context) = 0;

    virtual std::any visitExprStatement(AvaLangParser::ExprStatementContext *context) = 0;

    virtual std::any visitReturnStatement(AvaLangParser::ReturnStatementContext *context) = 0;

    virtual std::any visitBreakStatement(AvaLangParser::BreakStatementContext *context) = 0;

    virtual std::any visitContinueStatement(AvaLangParser::ContinueStatementContext *context) = 0;

    virtual std::any visitPassStatement(AvaLangParser::PassStatementContext *context) = 0;

    virtual std::any visitImportStatement(AvaLangParser::ImportStatementContext *context) = 0;

    virtual std::any visitLocalStatement(AvaLangParser::LocalStatementContext *context) = 0;

    virtual std::any visitRaiseStatement(AvaLangParser::RaiseStatementContext *context) = 0;

    virtual std::any visitYieldStatement(AvaLangParser::YieldStatementContext *context) = 0;

    virtual std::any visitIfStatement(AvaLangParser::IfStatementContext *context) = 0;

    virtual std::any visitElifClause(AvaLangParser::ElifClauseContext *context) = 0;

    virtual std::any visitElseClause(AvaLangParser::ElseClauseContext *context) = 0;

    virtual std::any visitWhileStatement(AvaLangParser::WhileStatementContext *context) = 0;

    virtual std::any visitForStatement(AvaLangParser::ForStatementContext *context) = 0;

    virtual std::any visitFuncDeclaration(AvaLangParser::FuncDeclarationContext *context) = 0;

    virtual std::any visitClassDeclaration(AvaLangParser::ClassDeclarationContext *context) = 0;

    virtual std::any visitClassHeritage(AvaLangParser::ClassHeritageContext *context) = 0;

    virtual std::any visitExternStatement(AvaLangParser::ExternStatementContext *context) = 0;

    virtual std::any visitExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext *context) = 0;

    virtual std::any visitExternParamList(AvaLangParser::ExternParamListContext *context) = 0;

    virtual std::any visitExternParam(AvaLangParser::ExternParamContext *context) = 0;

    virtual std::any visitParamList(AvaLangParser::ParamListContext *context) = 0;

    virtual std::any visitParam(AvaLangParser::ParamContext *context) = 0;

    virtual std::any visitTargetList(AvaLangParser::TargetListContext *context) = 0;

    virtual std::any visitTarget(AvaLangParser::TargetContext *context) = 0;

    virtual std::any visitExprList(AvaLangParser::ExprListContext *context) = 0;

    virtual std::any visitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext *context) = 0;

    virtual std::any visitLambdaExprAlt(AvaLangParser::LambdaExprAltContext *context) = 0;

    virtual std::any visitOrExprAlt(AvaLangParser::OrExprAltContext *context) = 0;

    virtual std::any visitShortLambdaExpr(AvaLangParser::ShortLambdaExprContext *context) = 0;

    virtual std::any visitLambdaExpr(AvaLangParser::LambdaExprContext *context) = 0;

    virtual std::any visitOrExpr(AvaLangParser::OrExprContext *context) = 0;

    virtual std::any visitAndExpr(AvaLangParser::AndExprContext *context) = 0;

    virtual std::any visitNotExpr(AvaLangParser::NotExprContext *context) = 0;

    virtual std::any visitComparison(AvaLangParser::ComparisonContext *context) = 0;

    virtual std::any visitCompOp(AvaLangParser::CompOpContext *context) = 0;

    virtual std::any visitAdditive(AvaLangParser::AdditiveContext *context) = 0;

    virtual std::any visitMultiplicative(AvaLangParser::MultiplicativeContext *context) = 0;

    virtual std::any visitUnary(AvaLangParser::UnaryContext *context) = 0;

    virtual std::any visitPower(AvaLangParser::PowerContext *context) = 0;

    virtual std::any visitPostfix(AvaLangParser::PostfixContext *context) = 0;

    virtual std::any visitAttrTrailer(AvaLangParser::AttrTrailerContext *context) = 0;

    virtual std::any visitIndexTrailer(AvaLangParser::IndexTrailerContext *context) = 0;

    virtual std::any visitSliceTrailer(AvaLangParser::SliceTrailerContext *context) = 0;

    virtual std::any visitCallTrailer(AvaLangParser::CallTrailerContext *context) = 0;

    virtual std::any visitIncTrailer(AvaLangParser::IncTrailerContext *context) = 0;

    virtual std::any visitDecTrailer(AvaLangParser::DecTrailerContext *context) = 0;

    virtual std::any visitSliceRange(AvaLangParser::SliceRangeContext *context) = 0;

    virtual std::any visitArgList(AvaLangParser::ArgListContext *context) = 0;

    virtual std::any visitNamedArg(AvaLangParser::NamedArgContext *context) = 0;

    virtual std::any visitPositionalArg(AvaLangParser::PositionalArgContext *context) = 0;

    virtual std::any visitNameAtom(AvaLangParser::NameAtomContext *context) = 0;

    virtual std::any visitNumberAtom(AvaLangParser::NumberAtomContext *context) = 0;

    virtual std::any visitStringAtom(AvaLangParser::StringAtomContext *context) = 0;

    virtual std::any visitFstringAtom(AvaLangParser::FstringAtomContext *context) = 0;

    virtual std::any visitTrueAtom(AvaLangParser::TrueAtomContext *context) = 0;

    virtual std::any visitFalseAtom(AvaLangParser::FalseAtomContext *context) = 0;

    virtual std::any visitNilAtom(AvaLangParser::NilAtomContext *context) = 0;

    virtual std::any visitListAtom(AvaLangParser::ListAtomContext *context) = 0;

    virtual std::any visitDictAtom(AvaLangParser::DictAtomContext *context) = 0;

    virtual std::any visitGroupAtom(AvaLangParser::GroupAtomContext *context) = 0;

    virtual std::any visitBaseAtom(AvaLangParser::BaseAtomContext *context) = 0;

    virtual std::any visitListLiteral(AvaLangParser::ListLiteralContext *context) = 0;

    virtual std::any visitDictLiteral(AvaLangParser::DictLiteralContext *context) = 0;

    virtual std::any visitDictEntry(AvaLangParser::DictEntryContext *context) = 0;


};

