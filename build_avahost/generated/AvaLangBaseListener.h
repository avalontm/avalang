
// Generated from D:/_CODE_/avalang/grammar/AvaLang.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "AvaLangListener.h"


/**
 * This class provides an empty implementation of AvaLangListener,
 * which can be extended to create a listener which only needs to handle a subset
 * of the available methods.
 */
class  AvaLangBaseListener : public AvaLangListener {
public:

  virtual void enterChunk(AvaLangParser::ChunkContext * /*ctx*/) override { }
  virtual void exitChunk(AvaLangParser::ChunkContext * /*ctx*/) override { }

  virtual void enterBlock(AvaLangParser::BlockContext * /*ctx*/) override { }
  virtual void exitBlock(AvaLangParser::BlockContext * /*ctx*/) override { }

  virtual void enterStatement(AvaLangParser::StatementContext * /*ctx*/) override { }
  virtual void exitStatement(AvaLangParser::StatementContext * /*ctx*/) override { }

  virtual void enterSimpleStatement(AvaLangParser::SimpleStatementContext * /*ctx*/) override { }
  virtual void exitSimpleStatement(AvaLangParser::SimpleStatementContext * /*ctx*/) override { }

  virtual void enterSmallStatement(AvaLangParser::SmallStatementContext * /*ctx*/) override { }
  virtual void exitSmallStatement(AvaLangParser::SmallStatementContext * /*ctx*/) override { }

  virtual void enterModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext * /*ctx*/) override { }
  virtual void exitModifiedAssignStatement(AvaLangParser::ModifiedAssignStatementContext * /*ctx*/) override { }

  virtual void enterIncDecStatement(AvaLangParser::IncDecStatementContext * /*ctx*/) override { }
  virtual void exitIncDecStatement(AvaLangParser::IncDecStatementContext * /*ctx*/) override { }

  virtual void enterCompoundStatement(AvaLangParser::CompoundStatementContext * /*ctx*/) override { }
  virtual void exitCompoundStatement(AvaLangParser::CompoundStatementContext * /*ctx*/) override { }

  virtual void enterMemberModifier(AvaLangParser::MemberModifierContext * /*ctx*/) override { }
  virtual void exitMemberModifier(AvaLangParser::MemberModifierContext * /*ctx*/) override { }

  virtual void enterModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext * /*ctx*/) override { }
  virtual void exitModifiedFuncDeclaration(AvaLangParser::ModifiedFuncDeclarationContext * /*ctx*/) override { }

  virtual void enterTryStatement(AvaLangParser::TryStatementContext * /*ctx*/) override { }
  virtual void exitTryStatement(AvaLangParser::TryStatementContext * /*ctx*/) override { }

  virtual void enterExceptClause(AvaLangParser::ExceptClauseContext * /*ctx*/) override { }
  virtual void exitExceptClause(AvaLangParser::ExceptClauseContext * /*ctx*/) override { }

  virtual void enterFinallyClause(AvaLangParser::FinallyClauseContext * /*ctx*/) override { }
  virtual void exitFinallyClause(AvaLangParser::FinallyClauseContext * /*ctx*/) override { }

  virtual void enterMultiAssignStatement(AvaLangParser::MultiAssignStatementContext * /*ctx*/) override { }
  virtual void exitMultiAssignStatement(AvaLangParser::MultiAssignStatementContext * /*ctx*/) override { }

  virtual void enterAssignStatement(AvaLangParser::AssignStatementContext * /*ctx*/) override { }
  virtual void exitAssignStatement(AvaLangParser::AssignStatementContext * /*ctx*/) override { }

  virtual void enterAugAssignStatement(AvaLangParser::AugAssignStatementContext * /*ctx*/) override { }
  virtual void exitAugAssignStatement(AvaLangParser::AugAssignStatementContext * /*ctx*/) override { }

  virtual void enterExprStatement(AvaLangParser::ExprStatementContext * /*ctx*/) override { }
  virtual void exitExprStatement(AvaLangParser::ExprStatementContext * /*ctx*/) override { }

  virtual void enterReturnStatement(AvaLangParser::ReturnStatementContext * /*ctx*/) override { }
  virtual void exitReturnStatement(AvaLangParser::ReturnStatementContext * /*ctx*/) override { }

  virtual void enterBreakStatement(AvaLangParser::BreakStatementContext * /*ctx*/) override { }
  virtual void exitBreakStatement(AvaLangParser::BreakStatementContext * /*ctx*/) override { }

  virtual void enterContinueStatement(AvaLangParser::ContinueStatementContext * /*ctx*/) override { }
  virtual void exitContinueStatement(AvaLangParser::ContinueStatementContext * /*ctx*/) override { }

  virtual void enterPassStatement(AvaLangParser::PassStatementContext * /*ctx*/) override { }
  virtual void exitPassStatement(AvaLangParser::PassStatementContext * /*ctx*/) override { }

  virtual void enterImportStatement(AvaLangParser::ImportStatementContext * /*ctx*/) override { }
  virtual void exitImportStatement(AvaLangParser::ImportStatementContext * /*ctx*/) override { }

  virtual void enterLocalStatement(AvaLangParser::LocalStatementContext * /*ctx*/) override { }
  virtual void exitLocalStatement(AvaLangParser::LocalStatementContext * /*ctx*/) override { }

  virtual void enterRaiseStatement(AvaLangParser::RaiseStatementContext * /*ctx*/) override { }
  virtual void exitRaiseStatement(AvaLangParser::RaiseStatementContext * /*ctx*/) override { }

  virtual void enterYieldStatement(AvaLangParser::YieldStatementContext * /*ctx*/) override { }
  virtual void exitYieldStatement(AvaLangParser::YieldStatementContext * /*ctx*/) override { }

  virtual void enterIfStatement(AvaLangParser::IfStatementContext * /*ctx*/) override { }
  virtual void exitIfStatement(AvaLangParser::IfStatementContext * /*ctx*/) override { }

  virtual void enterElifClause(AvaLangParser::ElifClauseContext * /*ctx*/) override { }
  virtual void exitElifClause(AvaLangParser::ElifClauseContext * /*ctx*/) override { }

  virtual void enterElseClause(AvaLangParser::ElseClauseContext * /*ctx*/) override { }
  virtual void exitElseClause(AvaLangParser::ElseClauseContext * /*ctx*/) override { }

  virtual void enterWhileStatement(AvaLangParser::WhileStatementContext * /*ctx*/) override { }
  virtual void exitWhileStatement(AvaLangParser::WhileStatementContext * /*ctx*/) override { }

  virtual void enterForStatement(AvaLangParser::ForStatementContext * /*ctx*/) override { }
  virtual void exitForStatement(AvaLangParser::ForStatementContext * /*ctx*/) override { }

  virtual void enterFuncDeclaration(AvaLangParser::FuncDeclarationContext * /*ctx*/) override { }
  virtual void exitFuncDeclaration(AvaLangParser::FuncDeclarationContext * /*ctx*/) override { }

  virtual void enterClassDeclaration(AvaLangParser::ClassDeclarationContext * /*ctx*/) override { }
  virtual void exitClassDeclaration(AvaLangParser::ClassDeclarationContext * /*ctx*/) override { }

  virtual void enterClassHeritage(AvaLangParser::ClassHeritageContext * /*ctx*/) override { }
  virtual void exitClassHeritage(AvaLangParser::ClassHeritageContext * /*ctx*/) override { }

  virtual void enterExternStatement(AvaLangParser::ExternStatementContext * /*ctx*/) override { }
  virtual void exitExternStatement(AvaLangParser::ExternStatementContext * /*ctx*/) override { }

  virtual void enterExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext * /*ctx*/) override { }
  virtual void exitExternFuncDeclaration(AvaLangParser::ExternFuncDeclarationContext * /*ctx*/) override { }

  virtual void enterExternParamList(AvaLangParser::ExternParamListContext * /*ctx*/) override { }
  virtual void exitExternParamList(AvaLangParser::ExternParamListContext * /*ctx*/) override { }

  virtual void enterExternParam(AvaLangParser::ExternParamContext * /*ctx*/) override { }
  virtual void exitExternParam(AvaLangParser::ExternParamContext * /*ctx*/) override { }

  virtual void enterParamList(AvaLangParser::ParamListContext * /*ctx*/) override { }
  virtual void exitParamList(AvaLangParser::ParamListContext * /*ctx*/) override { }

  virtual void enterParam(AvaLangParser::ParamContext * /*ctx*/) override { }
  virtual void exitParam(AvaLangParser::ParamContext * /*ctx*/) override { }

  virtual void enterTargetList(AvaLangParser::TargetListContext * /*ctx*/) override { }
  virtual void exitTargetList(AvaLangParser::TargetListContext * /*ctx*/) override { }

  virtual void enterTarget(AvaLangParser::TargetContext * /*ctx*/) override { }
  virtual void exitTarget(AvaLangParser::TargetContext * /*ctx*/) override { }

  virtual void enterExprList(AvaLangParser::ExprListContext * /*ctx*/) override { }
  virtual void exitExprList(AvaLangParser::ExprListContext * /*ctx*/) override { }

  virtual void enterShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext * /*ctx*/) override { }
  virtual void exitShortLambdaExprAlt(AvaLangParser::ShortLambdaExprAltContext * /*ctx*/) override { }

  virtual void enterLambdaExprAlt(AvaLangParser::LambdaExprAltContext * /*ctx*/) override { }
  virtual void exitLambdaExprAlt(AvaLangParser::LambdaExprAltContext * /*ctx*/) override { }

  virtual void enterOrExprAlt(AvaLangParser::OrExprAltContext * /*ctx*/) override { }
  virtual void exitOrExprAlt(AvaLangParser::OrExprAltContext * /*ctx*/) override { }

  virtual void enterShortLambdaExpr(AvaLangParser::ShortLambdaExprContext * /*ctx*/) override { }
  virtual void exitShortLambdaExpr(AvaLangParser::ShortLambdaExprContext * /*ctx*/) override { }

  virtual void enterLambdaExpr(AvaLangParser::LambdaExprContext * /*ctx*/) override { }
  virtual void exitLambdaExpr(AvaLangParser::LambdaExprContext * /*ctx*/) override { }

  virtual void enterOrExpr(AvaLangParser::OrExprContext * /*ctx*/) override { }
  virtual void exitOrExpr(AvaLangParser::OrExprContext * /*ctx*/) override { }

  virtual void enterAndExpr(AvaLangParser::AndExprContext * /*ctx*/) override { }
  virtual void exitAndExpr(AvaLangParser::AndExprContext * /*ctx*/) override { }

  virtual void enterNotExpr(AvaLangParser::NotExprContext * /*ctx*/) override { }
  virtual void exitNotExpr(AvaLangParser::NotExprContext * /*ctx*/) override { }

  virtual void enterComparison(AvaLangParser::ComparisonContext * /*ctx*/) override { }
  virtual void exitComparison(AvaLangParser::ComparisonContext * /*ctx*/) override { }

  virtual void enterCompOp(AvaLangParser::CompOpContext * /*ctx*/) override { }
  virtual void exitCompOp(AvaLangParser::CompOpContext * /*ctx*/) override { }

  virtual void enterAdditive(AvaLangParser::AdditiveContext * /*ctx*/) override { }
  virtual void exitAdditive(AvaLangParser::AdditiveContext * /*ctx*/) override { }

  virtual void enterMultiplicative(AvaLangParser::MultiplicativeContext * /*ctx*/) override { }
  virtual void exitMultiplicative(AvaLangParser::MultiplicativeContext * /*ctx*/) override { }

  virtual void enterUnary(AvaLangParser::UnaryContext * /*ctx*/) override { }
  virtual void exitUnary(AvaLangParser::UnaryContext * /*ctx*/) override { }

  virtual void enterPower(AvaLangParser::PowerContext * /*ctx*/) override { }
  virtual void exitPower(AvaLangParser::PowerContext * /*ctx*/) override { }

  virtual void enterPostfix(AvaLangParser::PostfixContext * /*ctx*/) override { }
  virtual void exitPostfix(AvaLangParser::PostfixContext * /*ctx*/) override { }

  virtual void enterAttrTrailer(AvaLangParser::AttrTrailerContext * /*ctx*/) override { }
  virtual void exitAttrTrailer(AvaLangParser::AttrTrailerContext * /*ctx*/) override { }

  virtual void enterIndexTrailer(AvaLangParser::IndexTrailerContext * /*ctx*/) override { }
  virtual void exitIndexTrailer(AvaLangParser::IndexTrailerContext * /*ctx*/) override { }

  virtual void enterSliceTrailer(AvaLangParser::SliceTrailerContext * /*ctx*/) override { }
  virtual void exitSliceTrailer(AvaLangParser::SliceTrailerContext * /*ctx*/) override { }

  virtual void enterCallTrailer(AvaLangParser::CallTrailerContext * /*ctx*/) override { }
  virtual void exitCallTrailer(AvaLangParser::CallTrailerContext * /*ctx*/) override { }

  virtual void enterIncTrailer(AvaLangParser::IncTrailerContext * /*ctx*/) override { }
  virtual void exitIncTrailer(AvaLangParser::IncTrailerContext * /*ctx*/) override { }

  virtual void enterDecTrailer(AvaLangParser::DecTrailerContext * /*ctx*/) override { }
  virtual void exitDecTrailer(AvaLangParser::DecTrailerContext * /*ctx*/) override { }

  virtual void enterSliceRange(AvaLangParser::SliceRangeContext * /*ctx*/) override { }
  virtual void exitSliceRange(AvaLangParser::SliceRangeContext * /*ctx*/) override { }

  virtual void enterArgList(AvaLangParser::ArgListContext * /*ctx*/) override { }
  virtual void exitArgList(AvaLangParser::ArgListContext * /*ctx*/) override { }

  virtual void enterNamedArg(AvaLangParser::NamedArgContext * /*ctx*/) override { }
  virtual void exitNamedArg(AvaLangParser::NamedArgContext * /*ctx*/) override { }

  virtual void enterPositionalArg(AvaLangParser::PositionalArgContext * /*ctx*/) override { }
  virtual void exitPositionalArg(AvaLangParser::PositionalArgContext * /*ctx*/) override { }

  virtual void enterNameAtom(AvaLangParser::NameAtomContext * /*ctx*/) override { }
  virtual void exitNameAtom(AvaLangParser::NameAtomContext * /*ctx*/) override { }

  virtual void enterNumberAtom(AvaLangParser::NumberAtomContext * /*ctx*/) override { }
  virtual void exitNumberAtom(AvaLangParser::NumberAtomContext * /*ctx*/) override { }

  virtual void enterStringAtom(AvaLangParser::StringAtomContext * /*ctx*/) override { }
  virtual void exitStringAtom(AvaLangParser::StringAtomContext * /*ctx*/) override { }

  virtual void enterFstringAtom(AvaLangParser::FstringAtomContext * /*ctx*/) override { }
  virtual void exitFstringAtom(AvaLangParser::FstringAtomContext * /*ctx*/) override { }

  virtual void enterTrueAtom(AvaLangParser::TrueAtomContext * /*ctx*/) override { }
  virtual void exitTrueAtom(AvaLangParser::TrueAtomContext * /*ctx*/) override { }

  virtual void enterFalseAtom(AvaLangParser::FalseAtomContext * /*ctx*/) override { }
  virtual void exitFalseAtom(AvaLangParser::FalseAtomContext * /*ctx*/) override { }

  virtual void enterNilAtom(AvaLangParser::NilAtomContext * /*ctx*/) override { }
  virtual void exitNilAtom(AvaLangParser::NilAtomContext * /*ctx*/) override { }

  virtual void enterListAtom(AvaLangParser::ListAtomContext * /*ctx*/) override { }
  virtual void exitListAtom(AvaLangParser::ListAtomContext * /*ctx*/) override { }

  virtual void enterDictAtom(AvaLangParser::DictAtomContext * /*ctx*/) override { }
  virtual void exitDictAtom(AvaLangParser::DictAtomContext * /*ctx*/) override { }

  virtual void enterGroupAtom(AvaLangParser::GroupAtomContext * /*ctx*/) override { }
  virtual void exitGroupAtom(AvaLangParser::GroupAtomContext * /*ctx*/) override { }

  virtual void enterBaseAtom(AvaLangParser::BaseAtomContext * /*ctx*/) override { }
  virtual void exitBaseAtom(AvaLangParser::BaseAtomContext * /*ctx*/) override { }

  virtual void enterListLiteral(AvaLangParser::ListLiteralContext * /*ctx*/) override { }
  virtual void exitListLiteral(AvaLangParser::ListLiteralContext * /*ctx*/) override { }

  virtual void enterDictLiteral(AvaLangParser::DictLiteralContext * /*ctx*/) override { }
  virtual void exitDictLiteral(AvaLangParser::DictLiteralContext * /*ctx*/) override { }

  virtual void enterDictEntry(AvaLangParser::DictEntryContext * /*ctx*/) override { }
  virtual void exitDictEntry(AvaLangParser::DictEntryContext * /*ctx*/) override { }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

};

