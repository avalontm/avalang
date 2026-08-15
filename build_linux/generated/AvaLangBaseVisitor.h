
// Generated from /mnt/d/_CODE_/avalang/runtime/avalang/grammar/AvaLang.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "AvaLangVisitor.h"


/**
 * This class provides an empty implementation of AvaLangVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  AvaLangBaseVisitor : public AvaLangVisitor {
public:

  virtual std::any visitChunk(AvaLangParser::ChunkContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(AvaLangParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(AvaLangParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSimpleStatement(AvaLangParser::SimpleStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSmallStatement(AvaLangParser::SmallStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIncDecStatement(AvaLangParser::IncDecStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundStatement(AvaLangParser::CompoundStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemberModifier(AvaLangParser::MemberModifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTryStatement(AvaLangParser::TryStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExceptClause(AvaLangParser::ExceptClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFinallyClause(AvaLangParser::FinallyClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMultiAssignStatement(AvaLangParser::MultiAssignStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignStatement(AvaLangParser::AssignStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAugAssignStatement(AvaLangParser::AugAssignStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStatement(AvaLangParser::ExprStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStatement(AvaLangParser::ReturnStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStatement(AvaLangParser::BreakStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStatement(AvaLangParser::ContinueStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPassStatement(AvaLangParser::PassStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitImportStatement(AvaLangParser::ImportStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLocalStatement(AvaLangParser::LocalStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRaiseStatement(AvaLangParser::RaiseStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitYieldStatement(AvaLangParser::YieldStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStatement(AvaLangParser::IfStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElifClause(AvaLangParser::ElifClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElseClause(AvaLangParser::ElseClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStatement(AvaLangParser::WhileStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForStatement(AvaLangParser::ForStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncDeclaration(AvaLangParser::FuncDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassDeclaration(AvaLangParser::ClassDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassHeritage(AvaLangParser::ClassHeritageContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternStatement(AvaLangParser::ExternStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternParamList(AvaLangParser::ExternParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternParam(AvaLangParser::ExternParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParamList(AvaLangParser::ParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(AvaLangParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTargetList(AvaLangParser::TargetListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTarget(AvaLangParser::TargetContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprList(AvaLangParser::ExprListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLambdaExprAlt(AvaLangParser::LambdaExprAltContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOrExprAlt(AvaLangParser::OrExprAltContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitShortLambdaExpr(AvaLangParser::ShortLambdaExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLambdaExpr(AvaLangParser::LambdaExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOrExpr(AvaLangParser::OrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAndExpr(AvaLangParser::AndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNotExpr(AvaLangParser::NotExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitComparison(AvaLangParser::ComparisonContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompOp(AvaLangParser::CompOpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAdditive(AvaLangParser::AdditiveContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMultiplicative(AvaLangParser::MultiplicativeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnary(AvaLangParser::UnaryContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPower(AvaLangParser::PowerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostfix(AvaLangParser::PostfixContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttrTrailer(AvaLangParser::AttrTrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexTrailer(AvaLangParser::IndexTrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSliceTrailer(AvaLangParser::SliceTrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallTrailer(AvaLangParser::CallTrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIncTrailer(AvaLangParser::IncTrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDecTrailer(AvaLangParser::DecTrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSliceRange(AvaLangParser::SliceRangeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgList(AvaLangParser::ArgListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNamedArg(AvaLangParser::NamedArgContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPositionalArg(AvaLangParser::PositionalArgContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNameAtom(AvaLangParser::NameAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNumberAtom(AvaLangParser::NumberAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStringAtom(AvaLangParser::StringAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFstringAtom(AvaLangParser::FstringAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTrueAtom(AvaLangParser::TrueAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFalseAtom(AvaLangParser::FalseAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNilAtom(AvaLangParser::NilAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitListAtom(AvaLangParser::ListAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDictAtom(AvaLangParser::DictAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGroupAtom(AvaLangParser::GroupAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBaseAtom(AvaLangParser::BaseAtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitListLiteral(AvaLangParser::ListLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDictLiteral(AvaLangParser::DictLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDictEntry(AvaLangParser::DictEntryContext *ctx) override {
    return visitChildren(ctx);
  }


};

