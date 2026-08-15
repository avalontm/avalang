
// Generated from /mnt/d/_CODE_/avalang/runtime/avalang/grammar/AvaLang.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "AvaLangParser.h"


/**
 * This interface defines an abstract listener for a parse tree produced by AvaLangParser.
 */
class  AvaLangListener : public antlr4::tree::ParseTreeListener {
public:

  virtual void enterChunk(AvaLangParser::ChunkContext *ctx) = 0;
  virtual void exitChunk(AvaLangParser::ChunkContext *ctx) = 0;

  virtual void enterBlock(AvaLangParser::BlockContext *ctx) = 0;
  virtual void exitBlock(AvaLangParser::BlockContext *ctx) = 0;

  virtual void enterStatement(AvaLangParser::StatementContext *ctx) = 0;
  virtual void exitStatement(AvaLangParser::StatementContext *ctx) = 0;

  virtual void enterSimpleStatement(AvaLangParser::SimpleStatementContext *ctx) = 0;
  virtual void exitSimpleStatement(AvaLangParser::SimpleStatementContext *ctx) = 0;

  virtual void enterSmallStatement(AvaLangParser::SmallStatementContext *ctx) = 0;
  virtual void exitSmallStatement(AvaLangParser::SmallStatementContext *ctx) = 0;

  virtual void enterModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext *ctx) = 0;
  virtual void exitModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext *ctx) = 0;

  virtual void enterIncDecStatement(AvaLangParser::IncDecStatementContext *ctx) = 0;
  virtual void exitIncDecStatement(AvaLangParser::IncDecStatementContext *ctx) = 0;

  virtual void enterCompoundStatement(AvaLangParser::CompoundStatementContext *ctx) = 0;
  virtual void exitCompoundStatement(AvaLangParser::CompoundStatementContext *ctx) = 0;

  virtual void enterMemberModifier(AvaLangParser::MemberModifierContext *ctx) = 0;
  virtual void exitMemberModifier(AvaLangParser::MemberModifierContext *ctx) = 0;

  virtual void enterModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext *ctx) = 0;
  virtual void exitModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext *ctx) = 0;

  virtual void enterTryStatement(AvaLangParser::TryStatementContext *ctx) = 0;
  virtual void exitTryStatement(AvaLangParser::TryStatementContext *ctx) = 0;

  virtual void enterExceptClause(AvaLangParser::ExceptClauseContext *ctx) = 0;
  virtual void exitExceptClause(AvaLangParser::ExceptClauseContext *ctx) = 0;

  virtual void enterFinallyClause(AvaLangParser::FinallyClauseContext *ctx) = 0;
  virtual void exitFinallyClause(AvaLangParser::FinallyClauseContext *ctx) = 0;

  virtual void enterMultiAssignStatement(AvaLangParser::MultiAssignStatementContext *ctx) = 0;
  virtual void exitMultiAssignStatement(AvaLangParser::MultiAssignStatementContext *ctx) = 0;

  virtual void enterAssignStatement(AvaLangParser::AssignStatementContext *ctx) = 0;
  virtual void exitAssignStatement(AvaLangParser::AssignStatementContext *ctx) = 0;

  virtual void enterAugAssignStatement(AvaLangParser::AugAssignStatementContext *ctx) = 0;
  virtual void exitAugAssignStatement(AvaLangParser::AugAssignStatementContext *ctx) = 0;

  virtual void enterExprStatement(AvaLangParser::ExprStatementContext *ctx) = 0;
  virtual void exitExprStatement(AvaLangParser::ExprStatementContext *ctx) = 0;

  virtual void enterReturnStatement(AvaLangParser::ReturnStatementContext *ctx) = 0;
  virtual void exitReturnStatement(AvaLangParser::ReturnStatementContext *ctx) = 0;

  virtual void enterBreakStatement(AvaLangParser::BreakStatementContext *ctx) = 0;
  virtual void exitBreakStatement(AvaLangParser::BreakStatementContext *ctx) = 0;

  virtual void enterContinueStatement(AvaLangParser::ContinueStatementContext *ctx) = 0;
  virtual void exitContinueStatement(AvaLangParser::ContinueStatementContext *ctx) = 0;

  virtual void enterPassStatement(AvaLangParser::PassStatementContext *ctx) = 0;
  virtual void exitPassStatement(AvaLangParser::PassStatementContext *ctx) = 0;

  virtual void enterImportStatement(AvaLangParser::ImportStatementContext *ctx) = 0;
  virtual void exitImportStatement(AvaLangParser::ImportStatementContext *ctx) = 0;

  virtual void enterLocalStatement(AvaLangParser::LocalStatementContext *ctx) = 0;
  virtual void exitLocalStatement(AvaLangParser::LocalStatementContext *ctx) = 0;

  virtual void enterRaiseStatement(AvaLangParser::RaiseStatementContext *ctx) = 0;
  virtual void exitRaiseStatement(AvaLangParser::RaiseStatementContext *ctx) = 0;

  virtual void enterYieldStatement(AvaLangParser::YieldStatementContext *ctx) = 0;
  virtual void exitYieldStatement(AvaLangParser::YieldStatementContext *ctx) = 0;

  virtual void enterIfStatement(AvaLangParser::IfStatementContext *ctx) = 0;
  virtual void exitIfStatement(AvaLangParser::IfStatementContext *ctx) = 0;

  virtual void enterElifClause(AvaLangParser::ElifClauseContext *ctx) = 0;
  virtual void exitElifClause(AvaLangParser::ElifClauseContext *ctx) = 0;

  virtual void enterElseClause(AvaLangParser::ElseClauseContext *ctx) = 0;
  virtual void exitElseClause(AvaLangParser::ElseClauseContext *ctx) = 0;

  virtual void enterWhileStatement(AvaLangParser::WhileStatementContext *ctx) = 0;
  virtual void exitWhileStatement(AvaLangParser::WhileStatementContext *ctx) = 0;

  virtual void enterForStatement(AvaLangParser::ForStatementContext *ctx) = 0;
  virtual void exitForStatement(AvaLangParser::ForStatementContext *ctx) = 0;

  virtual void enterFuncDeclaration(AvaLangParser::FuncDeclarationContext *ctx) = 0;
  virtual void exitFuncDeclaration(AvaLangParser::FuncDeclarationContext *ctx) = 0;

  virtual void enterClassDeclaration(AvaLangParser::ClassDeclarationContext *ctx) = 0;
  virtual void exitClassDeclaration(AvaLangParser::ClassDeclarationContext *ctx) = 0;

  virtual void enterClassHeritage(AvaLangParser::ClassHeritageContext *ctx) = 0;
  virtual void exitClassHeritage(AvaLangParser::ClassHeritageContext *ctx) = 0;

  virtual void enterExternStatement(AvaLangParser::ExternStatementContext *ctx) = 0;
  virtual void exitExternStatement(AvaLangParser::ExternStatementContext *ctx) = 0;

  virtual void enterExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext *ctx) = 0;
  virtual void exitExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext *ctx) = 0;

  virtual void enterExternParamList(AvaLangParser::ExternParamListContext *ctx) = 0;
  virtual void exitExternParamList(AvaLangParser::ExternParamListContext *ctx) = 0;

  virtual void enterExternParam(AvaLangParser::ExternParamContext *ctx) = 0;
  virtual void exitExternParam(AvaLangParser::ExternParamContext *ctx) = 0;

  virtual void enterParamList(AvaLangParser::ParamListContext *ctx) = 0;
  virtual void exitParamList(AvaLangParser::ParamListContext *ctx) = 0;

  virtual void enterParam(AvaLangParser::ParamContext *ctx) = 0;
  virtual void exitParam(AvaLangParser::ParamContext *ctx) = 0;

  virtual void enterTargetList(AvaLangParser::TargetListContext *ctx) = 0;
  virtual void exitTargetList(AvaLangParser::TargetListContext *ctx) = 0;

  virtual void enterTarget(AvaLangParser::TargetContext *ctx) = 0;
  virtual void exitTarget(AvaLangParser::TargetContext *ctx) = 0;

  virtual void enterExprList(AvaLangParser::ExprListContext *ctx) = 0;
  virtual void exitExprList(AvaLangParser::ExprListContext *ctx) = 0;

  virtual void enterShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext *ctx) = 0;
  virtual void exitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext *ctx) = 0;

  virtual void enterLambdaExprAlt(AvaLangParser::LambdaExprAltContext *ctx) = 0;
  virtual void exitLambdaExprAlt(AvaLangParser::LambdaExprAltContext *ctx) = 0;

  virtual void enterOrExprAlt(AvaLangParser::OrExprAltContext *ctx) = 0;
  virtual void exitOrExprAlt(AvaLangParser::OrExprAltContext *ctx) = 0;

  virtual void enterShortLambdaExpr(AvaLangParser::ShortLambdaExprContext *ctx) = 0;
  virtual void exitShortLambdaExpr(AvaLangParser::ShortLambdaExprContext *ctx) = 0;

  virtual void enterLambdaExpr(AvaLangParser::LambdaExprContext *ctx) = 0;
  virtual void exitLambdaExpr(AvaLangParser::LambdaExprContext *ctx) = 0;

  virtual void enterOrExpr(AvaLangParser::OrExprContext *ctx) = 0;
  virtual void exitOrExpr(AvaLangParser::OrExprContext *ctx) = 0;

  virtual void enterAndExpr(AvaLangParser::AndExprContext *ctx) = 0;
  virtual void exitAndExpr(AvaLangParser::AndExprContext *ctx) = 0;

  virtual void enterNotExpr(AvaLangParser::NotExprContext *ctx) = 0;
  virtual void exitNotExpr(AvaLangParser::NotExprContext *ctx) = 0;

  virtual void enterComparison(AvaLangParser::ComparisonContext *ctx) = 0;
  virtual void exitComparison(AvaLangParser::ComparisonContext *ctx) = 0;

  virtual void enterCompOp(AvaLangParser::CompOpContext *ctx) = 0;
  virtual void exitCompOp(AvaLangParser::CompOpContext *ctx) = 0;

  virtual void enterAdditive(AvaLangParser::AdditiveContext *ctx) = 0;
  virtual void exitAdditive(AvaLangParser::AdditiveContext *ctx) = 0;

  virtual void enterMultiplicative(AvaLangParser::MultiplicativeContext *ctx) = 0;
  virtual void exitMultiplicative(AvaLangParser::MultiplicativeContext *ctx) = 0;

  virtual void enterUnary(AvaLangParser::UnaryContext *ctx) = 0;
  virtual void exitUnary(AvaLangParser::UnaryContext *ctx) = 0;

  virtual void enterPower(AvaLangParser::PowerContext *ctx) = 0;
  virtual void exitPower(AvaLangParser::PowerContext *ctx) = 0;

  virtual void enterPostfix(AvaLangParser::PostfixContext *ctx) = 0;
  virtual void exitPostfix(AvaLangParser::PostfixContext *ctx) = 0;

  virtual void enterAttrTrailer(AvaLangParser::AttrTrailerContext *ctx) = 0;
  virtual void exitAttrTrailer(AvaLangParser::AttrTrailerContext *ctx) = 0;

  virtual void enterIndexTrailer(AvaLangParser::IndexTrailerContext *ctx) = 0;
  virtual void exitIndexTrailer(AvaLangParser::IndexTrailerContext *ctx) = 0;

  virtual void enterSliceTrailer(AvaLangParser::SliceTrailerContext *ctx) = 0;
  virtual void exitSliceTrailer(AvaLangParser::SliceTrailerContext *ctx) = 0;

  virtual void enterCallTrailer(AvaLangParser::CallTrailerContext *ctx) = 0;
  virtual void exitCallTrailer(AvaLangParser::CallTrailerContext *ctx) = 0;

  virtual void enterIncTrailer(AvaLangParser::IncTrailerContext *ctx) = 0;
  virtual void exitIncTrailer(AvaLangParser::IncTrailerContext *ctx) = 0;

  virtual void enterDecTrailer(AvaLangParser::DecTrailerContext *ctx) = 0;
  virtual void exitDecTrailer(AvaLangParser::DecTrailerContext *ctx) = 0;

  virtual void enterSliceRange(AvaLangParser::SliceRangeContext *ctx) = 0;
  virtual void exitSliceRange(AvaLangParser::SliceRangeContext *ctx) = 0;

  virtual void enterArgList(AvaLangParser::ArgListContext *ctx) = 0;
  virtual void exitArgList(AvaLangParser::ArgListContext *ctx) = 0;

  virtual void enterNamedArg(AvaLangParser::NamedArgContext *ctx) = 0;
  virtual void exitNamedArg(AvaLangParser::NamedArgContext *ctx) = 0;

  virtual void enterPositionalArg(AvaLangParser::PositionalArgContext *ctx) = 0;
  virtual void exitPositionalArg(AvaLangParser::PositionalArgContext *ctx) = 0;

  virtual void enterNameAtom(AvaLangParser::NameAtomContext *ctx) = 0;
  virtual void exitNameAtom(AvaLangParser::NameAtomContext *ctx) = 0;

  virtual void enterNumberAtom(AvaLangParser::NumberAtomContext *ctx) = 0;
  virtual void exitNumberAtom(AvaLangParser::NumberAtomContext *ctx) = 0;

  virtual void enterStringAtom(AvaLangParser::StringAtomContext *ctx) = 0;
  virtual void exitStringAtom(AvaLangParser::StringAtomContext *ctx) = 0;

  virtual void enterFstringAtom(AvaLangParser::FstringAtomContext *ctx) = 0;
  virtual void exitFstringAtom(AvaLangParser::FstringAtomContext *ctx) = 0;

  virtual void enterTrueAtom(AvaLangParser::TrueAtomContext *ctx) = 0;
  virtual void exitTrueAtom(AvaLangParser::TrueAtomContext *ctx) = 0;

  virtual void enterFalseAtom(AvaLangParser::FalseAtomContext *ctx) = 0;
  virtual void exitFalseAtom(AvaLangParser::FalseAtomContext *ctx) = 0;

  virtual void enterNilAtom(AvaLangParser::NilAtomContext *ctx) = 0;
  virtual void exitNilAtom(AvaLangParser::NilAtomContext *ctx) = 0;

  virtual void enterListAtom(AvaLangParser::ListAtomContext *ctx) = 0;
  virtual void exitListAtom(AvaLangParser::ListAtomContext *ctx) = 0;

  virtual void enterDictAtom(AvaLangParser::DictAtomContext *ctx) = 0;
  virtual void exitDictAtom(AvaLangParser::DictAtomContext *ctx) = 0;

  virtual void enterGroupAtom(AvaLangParser::GroupAtomContext *ctx) = 0;
  virtual void exitGroupAtom(AvaLangParser::GroupAtomContext *ctx) = 0;

  virtual void enterBaseAtom(AvaLangParser::BaseAtomContext *ctx) = 0;
  virtual void exitBaseAtom(AvaLangParser::BaseAtomContext *ctx) = 0;

  virtual void enterListLiteral(AvaLangParser::ListLiteralContext *ctx) = 0;
  virtual void exitListLiteral(AvaLangParser::ListLiteralContext *ctx) = 0;

  virtual void enterDictLiteral(AvaLangParser::DictLiteralContext *ctx) = 0;
  virtual void exitDictLiteral(AvaLangParser::DictLiteralContext *ctx) = 0;

  virtual void enterDictEntry(AvaLangParser::DictEntryContext *ctx) = 0;
  virtual void exitDictEntry(AvaLangParser::DictEntryContext *ctx) = 0;


};

