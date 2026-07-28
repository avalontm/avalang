
// Generated from D:/_CODE_/avalang/grammar/AvaLang.g4 by ANTLR 4.13.2


#include "AvaLangListener.h"
#include "AvaLangVisitor.h"

#include "AvaLangParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct AvaLangParserStaticData final {
  AvaLangParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  AvaLangParserStaticData(const AvaLangParserStaticData&) = delete;
  AvaLangParserStaticData(AvaLangParserStaticData&&) = delete;
  AvaLangParserStaticData& operator=(const AvaLangParserStaticData&) = delete;
  AvaLangParserStaticData& operator=(AvaLangParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag avalangParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<AvaLangParserStaticData> avalangParserStaticData = nullptr;

void avalangParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (avalangParserStaticData != nullptr) {
    return;
  }
#else
  assert(avalangParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<AvaLangParserStaticData>(
    std::vector<std::string>{
      "chunk", "block", "statement", "simpleStatement", "smallStatement", 
      "modifiedAssignStatement", "incDecStatement", "compoundStatement", 
      "memberModifier", "modifiedFuncDeclaration", "tryStatement", "exceptClause", 
      "finallyClause", "multiAssignStatement", "assignStatement", "augAssignStatement", 
      "exprStatement", "returnStatement", "breakStatement", "continueStatement", 
      "passStatement", "importStatement", "localStatement", "raiseStatement", 
      "yieldStatement", "ifStatement", "elifClause", "elseClause", "whileStatement", 
      "forStatement", "funcDeclaration", "classDeclaration", "classHeritage", 
      "paramList", "param", "targetList", "target", "exprList", "expr", 
      "shortLambdaExpr", "lambdaExpr", "orExpr", "andExpr", "notExpr", "comparison", 
      "compOp", "additive", "multiplicative", "unary", "power", "postfix", 
      "trailer", "sliceRange", "argList", "arg", "primary", "listLiteral", 
      "dictLiteral", "dictEntry"
    },
    std::vector<std::string>{
      "", "';'", "'static'", "'private'", "'try'", "'end'", "'catch'", "'('", 
      "')'", "'finally'", "','", "'='", "'+='", "'-='", "'*='", "'/='", 
      "'%='", "'//='", "'return'", "'break'", "'continue'", "'pass'", "'import'", 
      "'.'", "'as'", "'local'", "'raise'", "'yield'", "'if'", "'then'", 
      "'elif'", "'else'", "'while'", "'for'", "'in'", "'func'", "'class'", 
      "':'", "'*'", "'=>'", "'or'", "'and'", "'not'", "'=='", "'!='", "'<'", 
      "'>'", "'<='", "'>='", "'+'", "'-'", "'/'", "'%'", "'**'", "'['", 
      "']'", "'true'", "'false'", "'nil'", "'base'", "'{'", "'}'", "'ava'", 
      "'++'", "'--'", "'//'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "AVA_LANG", "INC", "DEC", 
      "IDIV", "NAME", "NUMBER", "STRING", "FSTRING", "NEWLINE", "COMMENT", 
      "WS", "LINE_JOIN"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,73,582,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
  	21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
  	28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
  	35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
  	42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
  	49,2,50,7,50,2,51,7,51,2,52,7,52,2,53,7,53,2,54,7,54,2,55,7,55,2,56,7,
  	56,2,57,7,57,2,58,7,58,1,0,1,0,5,0,121,8,0,10,0,12,0,124,9,0,1,1,1,1,
  	5,1,128,8,1,10,1,12,1,131,9,1,1,2,1,2,3,2,135,8,2,1,3,1,3,1,3,1,3,4,3,
  	141,8,3,11,3,12,3,142,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,
  	1,4,1,4,3,4,159,8,4,1,5,4,5,162,8,5,11,5,12,5,163,1,5,1,5,1,6,1,6,1,6,
  	1,7,1,7,1,7,1,7,1,7,1,7,1,7,3,7,178,8,7,1,8,1,8,1,9,4,9,183,8,9,11,9,
  	12,9,184,1,9,1,9,1,10,1,10,1,10,4,10,192,8,10,11,10,12,10,193,1,10,3,
  	10,197,8,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,206,8,10,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,218,8,11,1,12,1,12,1,12,
  	1,13,1,13,1,13,4,13,226,8,13,11,13,12,13,227,1,14,1,14,1,14,1,14,1,15,
  	1,15,1,15,1,15,1,16,1,16,1,17,1,17,3,17,242,8,17,1,18,1,18,1,19,1,19,
  	1,20,1,20,1,21,1,21,1,21,1,21,5,21,254,8,21,10,21,12,21,257,9,21,1,21,
  	1,21,3,21,261,8,21,1,22,1,22,1,22,1,23,1,23,1,23,1,24,1,24,3,24,271,8,
  	24,1,25,1,25,1,25,1,25,1,25,5,25,278,8,25,10,25,12,25,281,9,25,1,25,3,
  	25,284,8,25,1,25,1,25,1,26,1,26,1,26,1,26,1,26,1,27,1,27,1,27,1,28,1,
  	28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,3,28,308,8,28,1,
  	29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,
  	29,1,29,1,29,1,29,3,29,328,8,29,1,30,1,30,1,30,1,30,3,30,334,8,30,1,30,
  	1,30,1,30,1,30,1,31,1,31,1,31,3,31,343,8,31,1,31,1,31,1,31,1,32,1,32,
  	1,32,1,33,1,33,1,33,5,33,354,8,33,10,33,12,33,357,9,33,1,33,1,33,1,33,
  	3,33,362,8,33,1,34,1,34,1,34,3,34,367,8,34,1,35,1,35,1,35,5,35,372,8,
  	35,10,35,12,35,375,9,35,1,36,1,36,5,36,379,8,36,10,36,12,36,382,9,36,
  	1,37,1,37,1,37,5,37,387,8,37,10,37,12,37,390,9,37,1,38,1,38,1,38,3,38,
  	395,8,38,1,39,1,39,3,39,399,8,39,1,39,1,39,1,39,1,39,1,40,1,40,1,40,3,
  	40,408,8,40,1,40,1,40,1,40,1,40,1,41,1,41,1,41,5,41,417,8,41,10,41,12,
  	41,420,9,41,1,42,1,42,1,42,5,42,425,8,42,10,42,12,42,428,9,42,1,43,1,
  	43,1,43,3,43,433,8,43,1,44,1,44,1,44,1,44,5,44,439,8,44,10,44,12,44,442,
  	9,44,1,45,1,45,1,46,1,46,1,46,5,46,449,8,46,10,46,12,46,452,9,46,1,47,
  	1,47,1,47,5,47,457,8,47,10,47,12,47,460,9,47,1,48,1,48,1,48,3,48,465,
  	8,48,1,49,1,49,1,49,3,49,470,8,49,1,50,1,50,5,50,474,8,50,10,50,12,50,
  	477,9,50,1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,1,51,
  	3,51,491,8,51,1,51,1,51,1,51,3,51,496,8,51,1,52,3,52,499,8,52,1,52,1,
  	52,3,52,503,8,52,1,52,1,52,3,52,507,8,52,3,52,509,8,52,1,53,1,53,1,53,
  	5,53,514,8,53,10,53,12,53,517,9,53,1,54,1,54,1,54,1,54,3,54,523,8,54,
  	1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,
  	1,55,1,55,3,55,541,8,55,1,55,3,55,544,8,55,1,56,1,56,1,56,1,56,5,56,550,
  	8,56,10,56,12,56,553,9,56,1,56,3,56,556,8,56,3,56,558,8,56,1,56,1,56,
  	1,57,1,57,1,57,1,57,5,57,566,8,57,10,57,12,57,569,9,57,1,57,3,57,572,
  	8,57,3,57,574,8,57,1,57,1,57,1,58,1,58,1,58,1,58,1,58,0,0,59,0,2,4,6,
  	8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,
  	56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,
  	102,104,106,108,110,112,114,116,0,8,1,0,63,64,1,0,2,3,1,0,12,17,1,0,43,
  	48,1,0,49,50,3,0,38,38,51,52,65,65,3,0,42,42,50,50,63,64,2,0,66,66,68,
  	68,613,0,122,1,0,0,0,2,129,1,0,0,0,4,134,1,0,0,0,6,136,1,0,0,0,8,158,
  	1,0,0,0,10,161,1,0,0,0,12,167,1,0,0,0,14,177,1,0,0,0,16,179,1,0,0,0,18,
  	182,1,0,0,0,20,205,1,0,0,0,22,217,1,0,0,0,24,219,1,0,0,0,26,222,1,0,0,
  	0,28,229,1,0,0,0,30,233,1,0,0,0,32,237,1,0,0,0,34,239,1,0,0,0,36,243,
  	1,0,0,0,38,245,1,0,0,0,40,247,1,0,0,0,42,249,1,0,0,0,44,262,1,0,0,0,46,
  	265,1,0,0,0,48,268,1,0,0,0,50,272,1,0,0,0,52,287,1,0,0,0,54,292,1,0,0,
  	0,56,307,1,0,0,0,58,327,1,0,0,0,60,329,1,0,0,0,62,339,1,0,0,0,64,347,
  	1,0,0,0,66,350,1,0,0,0,68,363,1,0,0,0,70,368,1,0,0,0,72,376,1,0,0,0,74,
  	383,1,0,0,0,76,394,1,0,0,0,78,396,1,0,0,0,80,404,1,0,0,0,82,413,1,0,0,
  	0,84,421,1,0,0,0,86,432,1,0,0,0,88,434,1,0,0,0,90,443,1,0,0,0,92,445,
  	1,0,0,0,94,453,1,0,0,0,96,464,1,0,0,0,98,466,1,0,0,0,100,471,1,0,0,0,
  	102,495,1,0,0,0,104,498,1,0,0,0,106,510,1,0,0,0,108,522,1,0,0,0,110,543,
  	1,0,0,0,112,545,1,0,0,0,114,561,1,0,0,0,116,577,1,0,0,0,118,121,3,4,2,
  	0,119,121,5,70,0,0,120,118,1,0,0,0,120,119,1,0,0,0,121,124,1,0,0,0,122,
  	120,1,0,0,0,122,123,1,0,0,0,123,1,1,0,0,0,124,122,1,0,0,0,125,128,3,4,
  	2,0,126,128,5,70,0,0,127,125,1,0,0,0,127,126,1,0,0,0,128,131,1,0,0,0,
  	129,127,1,0,0,0,129,130,1,0,0,0,130,3,1,0,0,0,131,129,1,0,0,0,132,135,
  	3,6,3,0,133,135,3,14,7,0,134,132,1,0,0,0,134,133,1,0,0,0,135,5,1,0,0,
  	0,136,140,3,8,4,0,137,141,5,70,0,0,138,139,5,1,0,0,139,141,5,70,0,0,140,
  	137,1,0,0,0,140,138,1,0,0,0,141,142,1,0,0,0,142,140,1,0,0,0,142,143,1,
  	0,0,0,143,7,1,0,0,0,144,159,3,28,14,0,145,159,3,26,13,0,146,159,3,30,
  	15,0,147,159,3,32,16,0,148,159,3,34,17,0,149,159,3,36,18,0,150,159,3,
  	38,19,0,151,159,3,40,20,0,152,159,3,42,21,0,153,159,3,44,22,0,154,159,
  	3,46,23,0,155,159,3,48,24,0,156,159,3,12,6,0,157,159,3,10,5,0,158,144,
  	1,0,0,0,158,145,1,0,0,0,158,146,1,0,0,0,158,147,1,0,0,0,158,148,1,0,0,
  	0,158,149,1,0,0,0,158,150,1,0,0,0,158,151,1,0,0,0,158,152,1,0,0,0,158,
  	153,1,0,0,0,158,154,1,0,0,0,158,155,1,0,0,0,158,156,1,0,0,0,158,157,1,
  	0,0,0,159,9,1,0,0,0,160,162,3,16,8,0,161,160,1,0,0,0,162,163,1,0,0,0,
  	163,161,1,0,0,0,163,164,1,0,0,0,164,165,1,0,0,0,165,166,3,28,14,0,166,
  	11,1,0,0,0,167,168,7,0,0,0,168,169,3,72,36,0,169,13,1,0,0,0,170,178,3,
  	50,25,0,171,178,3,56,28,0,172,178,3,58,29,0,173,178,3,60,30,0,174,178,
  	3,62,31,0,175,178,3,20,10,0,176,178,3,18,9,0,177,170,1,0,0,0,177,171,
  	1,0,0,0,177,172,1,0,0,0,177,173,1,0,0,0,177,174,1,0,0,0,177,175,1,0,0,
  	0,177,176,1,0,0,0,178,15,1,0,0,0,179,180,7,1,0,0,180,17,1,0,0,0,181,183,
  	3,16,8,0,182,181,1,0,0,0,183,184,1,0,0,0,184,182,1,0,0,0,184,185,1,0,
  	0,0,185,186,1,0,0,0,186,187,3,60,30,0,187,19,1,0,0,0,188,189,5,4,0,0,
  	189,191,3,2,1,0,190,192,3,22,11,0,191,190,1,0,0,0,192,193,1,0,0,0,193,
  	191,1,0,0,0,193,194,1,0,0,0,194,196,1,0,0,0,195,197,3,24,12,0,196,195,
  	1,0,0,0,196,197,1,0,0,0,197,198,1,0,0,0,198,199,5,5,0,0,199,206,1,0,0,
  	0,200,201,5,4,0,0,201,202,3,2,1,0,202,203,3,24,12,0,203,204,5,5,0,0,204,
  	206,1,0,0,0,205,188,1,0,0,0,205,200,1,0,0,0,206,21,1,0,0,0,207,208,5,
  	6,0,0,208,209,5,7,0,0,209,210,3,76,38,0,210,211,5,8,0,0,211,212,3,2,1,
  	0,212,218,1,0,0,0,213,214,5,6,0,0,214,215,3,76,38,0,215,216,3,2,1,0,216,
  	218,1,0,0,0,217,207,1,0,0,0,217,213,1,0,0,0,218,23,1,0,0,0,219,220,5,
  	9,0,0,220,221,3,2,1,0,221,25,1,0,0,0,222,225,3,28,14,0,223,224,5,10,0,
  	0,224,226,3,28,14,0,225,223,1,0,0,0,226,227,1,0,0,0,227,225,1,0,0,0,227,
  	228,1,0,0,0,228,27,1,0,0,0,229,230,3,70,35,0,230,231,5,11,0,0,231,232,
  	3,74,37,0,232,29,1,0,0,0,233,234,3,72,36,0,234,235,7,2,0,0,235,236,3,
  	76,38,0,236,31,1,0,0,0,237,238,3,74,37,0,238,33,1,0,0,0,239,241,5,18,
  	0,0,240,242,3,74,37,0,241,240,1,0,0,0,241,242,1,0,0,0,242,35,1,0,0,0,
  	243,244,5,19,0,0,244,37,1,0,0,0,245,246,5,20,0,0,246,39,1,0,0,0,247,248,
  	5,21,0,0,248,41,1,0,0,0,249,250,5,22,0,0,250,255,5,66,0,0,251,252,5,23,
  	0,0,252,254,5,66,0,0,253,251,1,0,0,0,254,257,1,0,0,0,255,253,1,0,0,0,
  	255,256,1,0,0,0,256,260,1,0,0,0,257,255,1,0,0,0,258,259,5,24,0,0,259,
  	261,5,66,0,0,260,258,1,0,0,0,260,261,1,0,0,0,261,43,1,0,0,0,262,263,5,
  	25,0,0,263,264,3,28,14,0,264,45,1,0,0,0,265,266,5,26,0,0,266,267,3,76,
  	38,0,267,47,1,0,0,0,268,270,5,27,0,0,269,271,3,74,37,0,270,269,1,0,0,
  	0,270,271,1,0,0,0,271,49,1,0,0,0,272,273,5,28,0,0,273,274,3,76,38,0,274,
  	275,5,29,0,0,275,279,3,2,1,0,276,278,3,52,26,0,277,276,1,0,0,0,278,281,
  	1,0,0,0,279,277,1,0,0,0,279,280,1,0,0,0,280,283,1,0,0,0,281,279,1,0,0,
  	0,282,284,3,54,27,0,283,282,1,0,0,0,283,284,1,0,0,0,284,285,1,0,0,0,285,
  	286,5,5,0,0,286,51,1,0,0,0,287,288,5,30,0,0,288,289,3,76,38,0,289,290,
  	5,29,0,0,290,291,3,2,1,0,291,53,1,0,0,0,292,293,5,31,0,0,293,294,3,2,
  	1,0,294,55,1,0,0,0,295,296,5,32,0,0,296,297,5,7,0,0,297,298,3,76,38,0,
  	298,299,5,8,0,0,299,300,3,2,1,0,300,301,5,5,0,0,301,308,1,0,0,0,302,303,
  	5,32,0,0,303,304,3,76,38,0,304,305,3,2,1,0,305,306,5,5,0,0,306,308,1,
  	0,0,0,307,295,1,0,0,0,307,302,1,0,0,0,308,57,1,0,0,0,309,310,5,33,0,0,
  	310,311,3,70,35,0,311,312,5,34,0,0,312,313,3,74,37,0,313,314,5,29,0,0,
  	314,315,3,2,1,0,315,316,5,5,0,0,316,328,1,0,0,0,317,318,5,33,0,0,318,
  	319,3,70,35,0,319,320,5,34,0,0,320,321,5,7,0,0,321,322,3,74,37,0,322,
  	323,5,8,0,0,323,324,5,29,0,0,324,325,3,2,1,0,325,326,5,5,0,0,326,328,
  	1,0,0,0,327,309,1,0,0,0,327,317,1,0,0,0,328,59,1,0,0,0,329,330,5,35,0,
  	0,330,331,5,66,0,0,331,333,5,7,0,0,332,334,3,66,33,0,333,332,1,0,0,0,
  	333,334,1,0,0,0,334,335,1,0,0,0,335,336,5,8,0,0,336,337,3,2,1,0,337,338,
  	5,5,0,0,338,61,1,0,0,0,339,340,5,36,0,0,340,342,5,66,0,0,341,343,3,64,
  	32,0,342,341,1,0,0,0,342,343,1,0,0,0,343,344,1,0,0,0,344,345,3,2,1,0,
  	345,346,5,5,0,0,346,63,1,0,0,0,347,348,5,37,0,0,348,349,5,66,0,0,349,
  	65,1,0,0,0,350,355,3,68,34,0,351,352,5,10,0,0,352,354,3,68,34,0,353,351,
  	1,0,0,0,354,357,1,0,0,0,355,353,1,0,0,0,355,356,1,0,0,0,356,361,1,0,0,
  	0,357,355,1,0,0,0,358,359,5,10,0,0,359,360,5,38,0,0,360,362,5,66,0,0,
  	361,358,1,0,0,0,361,362,1,0,0,0,362,67,1,0,0,0,363,366,5,66,0,0,364,365,
  	5,11,0,0,365,367,3,76,38,0,366,364,1,0,0,0,366,367,1,0,0,0,367,69,1,0,
  	0,0,368,373,3,72,36,0,369,370,5,10,0,0,370,372,3,72,36,0,371,369,1,0,
  	0,0,372,375,1,0,0,0,373,371,1,0,0,0,373,374,1,0,0,0,374,71,1,0,0,0,375,
  	373,1,0,0,0,376,380,5,66,0,0,377,379,3,102,51,0,378,377,1,0,0,0,379,382,
  	1,0,0,0,380,378,1,0,0,0,380,381,1,0,0,0,381,73,1,0,0,0,382,380,1,0,0,
  	0,383,388,3,76,38,0,384,385,5,10,0,0,385,387,3,76,38,0,386,384,1,0,0,
  	0,387,390,1,0,0,0,388,386,1,0,0,0,388,389,1,0,0,0,389,75,1,0,0,0,390,
  	388,1,0,0,0,391,395,3,78,39,0,392,395,3,80,40,0,393,395,3,82,41,0,394,
  	391,1,0,0,0,394,392,1,0,0,0,394,393,1,0,0,0,395,77,1,0,0,0,396,398,5,
  	7,0,0,397,399,3,66,33,0,398,397,1,0,0,0,398,399,1,0,0,0,399,400,1,0,0,
  	0,400,401,5,8,0,0,401,402,5,39,0,0,402,403,3,76,38,0,403,79,1,0,0,0,404,
  	405,5,35,0,0,405,407,5,7,0,0,406,408,3,66,33,0,407,406,1,0,0,0,407,408,
  	1,0,0,0,408,409,1,0,0,0,409,410,5,8,0,0,410,411,3,2,1,0,411,412,5,5,0,
  	0,412,81,1,0,0,0,413,418,3,84,42,0,414,415,5,40,0,0,415,417,3,84,42,0,
  	416,414,1,0,0,0,417,420,1,0,0,0,418,416,1,0,0,0,418,419,1,0,0,0,419,83,
  	1,0,0,0,420,418,1,0,0,0,421,426,3,86,43,0,422,423,5,41,0,0,423,425,3,
  	86,43,0,424,422,1,0,0,0,425,428,1,0,0,0,426,424,1,0,0,0,426,427,1,0,0,
  	0,427,85,1,0,0,0,428,426,1,0,0,0,429,430,5,42,0,0,430,433,3,86,43,0,431,
  	433,3,88,44,0,432,429,1,0,0,0,432,431,1,0,0,0,433,87,1,0,0,0,434,440,
  	3,92,46,0,435,436,3,90,45,0,436,437,3,92,46,0,437,439,1,0,0,0,438,435,
  	1,0,0,0,439,442,1,0,0,0,440,438,1,0,0,0,440,441,1,0,0,0,441,89,1,0,0,
  	0,442,440,1,0,0,0,443,444,7,3,0,0,444,91,1,0,0,0,445,450,3,94,47,0,446,
  	447,7,4,0,0,447,449,3,94,47,0,448,446,1,0,0,0,449,452,1,0,0,0,450,448,
  	1,0,0,0,450,451,1,0,0,0,451,93,1,0,0,0,452,450,1,0,0,0,453,458,3,96,48,
  	0,454,455,7,5,0,0,455,457,3,96,48,0,456,454,1,0,0,0,457,460,1,0,0,0,458,
  	456,1,0,0,0,458,459,1,0,0,0,459,95,1,0,0,0,460,458,1,0,0,0,461,462,7,
  	6,0,0,462,465,3,96,48,0,463,465,3,98,49,0,464,461,1,0,0,0,464,463,1,0,
  	0,0,465,97,1,0,0,0,466,469,3,100,50,0,467,468,5,53,0,0,468,470,3,96,48,
  	0,469,467,1,0,0,0,469,470,1,0,0,0,470,99,1,0,0,0,471,475,3,110,55,0,472,
  	474,3,102,51,0,473,472,1,0,0,0,474,477,1,0,0,0,475,473,1,0,0,0,475,476,
  	1,0,0,0,476,101,1,0,0,0,477,475,1,0,0,0,478,479,5,23,0,0,479,496,5,66,
  	0,0,480,481,5,54,0,0,481,482,3,76,38,0,482,483,5,55,0,0,483,496,1,0,0,
  	0,484,485,5,54,0,0,485,486,3,104,52,0,486,487,5,55,0,0,487,496,1,0,0,
  	0,488,490,5,7,0,0,489,491,3,106,53,0,490,489,1,0,0,0,490,491,1,0,0,0,
  	491,492,1,0,0,0,492,496,5,8,0,0,493,496,5,63,0,0,494,496,5,64,0,0,495,
  	478,1,0,0,0,495,480,1,0,0,0,495,484,1,0,0,0,495,488,1,0,0,0,495,493,1,
  	0,0,0,495,494,1,0,0,0,496,103,1,0,0,0,497,499,3,76,38,0,498,497,1,0,0,
  	0,498,499,1,0,0,0,499,500,1,0,0,0,500,502,5,37,0,0,501,503,3,76,38,0,
  	502,501,1,0,0,0,502,503,1,0,0,0,503,508,1,0,0,0,504,506,5,37,0,0,505,
  	507,3,76,38,0,506,505,1,0,0,0,506,507,1,0,0,0,507,509,1,0,0,0,508,504,
  	1,0,0,0,508,509,1,0,0,0,509,105,1,0,0,0,510,515,3,108,54,0,511,512,5,
  	10,0,0,512,514,3,108,54,0,513,511,1,0,0,0,514,517,1,0,0,0,515,513,1,0,
  	0,0,515,516,1,0,0,0,516,107,1,0,0,0,517,515,1,0,0,0,518,519,5,66,0,0,
  	519,520,5,11,0,0,520,523,3,76,38,0,521,523,3,76,38,0,522,518,1,0,0,0,
  	522,521,1,0,0,0,523,109,1,0,0,0,524,544,5,66,0,0,525,544,5,67,0,0,526,
  	544,5,68,0,0,527,544,5,69,0,0,528,544,5,56,0,0,529,544,5,57,0,0,530,544,
  	5,58,0,0,531,544,3,112,56,0,532,544,3,114,57,0,533,534,5,7,0,0,534,535,
  	3,76,38,0,535,536,5,8,0,0,536,544,1,0,0,0,537,538,5,59,0,0,538,540,5,
  	7,0,0,539,541,3,106,53,0,540,539,1,0,0,0,540,541,1,0,0,0,541,542,1,0,
  	0,0,542,544,5,8,0,0,543,524,1,0,0,0,543,525,1,0,0,0,543,526,1,0,0,0,543,
  	527,1,0,0,0,543,528,1,0,0,0,543,529,1,0,0,0,543,530,1,0,0,0,543,531,1,
  	0,0,0,543,532,1,0,0,0,543,533,1,0,0,0,543,537,1,0,0,0,544,111,1,0,0,0,
  	545,557,5,54,0,0,546,551,3,76,38,0,547,548,5,10,0,0,548,550,3,76,38,0,
  	549,547,1,0,0,0,550,553,1,0,0,0,551,549,1,0,0,0,551,552,1,0,0,0,552,555,
  	1,0,0,0,553,551,1,0,0,0,554,556,5,10,0,0,555,554,1,0,0,0,555,556,1,0,
  	0,0,556,558,1,0,0,0,557,546,1,0,0,0,557,558,1,0,0,0,558,559,1,0,0,0,559,
  	560,5,55,0,0,560,113,1,0,0,0,561,573,5,60,0,0,562,567,3,116,58,0,563,
  	564,5,10,0,0,564,566,3,116,58,0,565,563,1,0,0,0,566,569,1,0,0,0,567,565,
  	1,0,0,0,567,568,1,0,0,0,568,571,1,0,0,0,569,567,1,0,0,0,570,572,5,10,
  	0,0,571,570,1,0,0,0,571,572,1,0,0,0,572,574,1,0,0,0,573,562,1,0,0,0,573,
  	574,1,0,0,0,574,575,1,0,0,0,575,576,5,61,0,0,576,115,1,0,0,0,577,578,
  	7,7,0,0,578,579,5,37,0,0,579,580,3,76,38,0,580,117,1,0,0,0,60,120,122,
  	127,129,134,140,142,158,163,177,184,193,196,205,217,227,241,255,260,270,
  	279,283,307,327,333,342,355,361,366,373,380,388,394,398,407,418,426,432,
  	440,450,458,464,469,475,490,495,498,502,506,508,515,522,540,543,551,555,
  	557,567,571,573
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  avalangParserStaticData = std::move(staticData);
}

}

AvaLangParser::AvaLangParser(TokenStream *input) : AvaLangParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

AvaLangParser::AvaLangParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  AvaLangParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *avalangParserStaticData->atn, avalangParserStaticData->decisionToDFA, avalangParserStaticData->sharedContextCache, options);
}

AvaLangParser::~AvaLangParser() {
  delete _interpreter;
}

const atn::ATN& AvaLangParser::getATN() const {
  return *avalangParserStaticData->atn;
}

std::string AvaLangParser::getGrammarFileName() const {
  return "AvaLang.g4";
}

const std::vector<std::string>& AvaLangParser::getRuleNames() const {
  return avalangParserStaticData->ruleNames;
}

const dfa::Vocabulary& AvaLangParser::getVocabulary() const {
  return avalangParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView AvaLangParser::getSerializedATN() const {
  return avalangParserStaticData->serializedATN;
}


//----------------- ChunkContext ------------------------------------------------------------------

AvaLangParser::ChunkContext::ChunkContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::StatementContext *> AvaLangParser::ChunkContext::statement() {
  return getRuleContexts<AvaLangParser::StatementContext>();
}

AvaLangParser::StatementContext* AvaLangParser::ChunkContext::statement(size_t i) {
  return getRuleContext<AvaLangParser::StatementContext>(i);
}

std::vector<tree::TerminalNode *> AvaLangParser::ChunkContext::NEWLINE() {
  return getTokens(AvaLangParser::NEWLINE);
}

tree::TerminalNode* AvaLangParser::ChunkContext::NEWLINE(size_t i) {
  return getToken(AvaLangParser::NEWLINE, i);
}


size_t AvaLangParser::ChunkContext::getRuleIndex() const {
  return AvaLangParser::RuleChunk;
}

void AvaLangParser::ChunkContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterChunk(this);
}

void AvaLangParser::ChunkContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitChunk(this);
}


std::any AvaLangParser::ChunkContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitChunk(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ChunkContext* AvaLangParser::chunk() {
  ChunkContext *_localctx = _tracker.createInstance<ChunkContext>(_ctx, getState());
  enterRule(_localctx, 0, AvaLangParser::RuleChunk);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(122);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -6970441808740613988) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 125) != 0)) {
      setState(120);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case AvaLangParser::T__1:
        case AvaLangParser::T__2:
        case AvaLangParser::T__3:
        case AvaLangParser::T__6:
        case AvaLangParser::T__17:
        case AvaLangParser::T__18:
        case AvaLangParser::T__19:
        case AvaLangParser::T__20:
        case AvaLangParser::T__21:
        case AvaLangParser::T__24:
        case AvaLangParser::T__25:
        case AvaLangParser::T__26:
        case AvaLangParser::T__27:
        case AvaLangParser::T__31:
        case AvaLangParser::T__32:
        case AvaLangParser::T__34:
        case AvaLangParser::T__35:
        case AvaLangParser::T__41:
        case AvaLangParser::T__49:
        case AvaLangParser::T__53:
        case AvaLangParser::T__55:
        case AvaLangParser::T__56:
        case AvaLangParser::T__57:
        case AvaLangParser::T__58:
        case AvaLangParser::T__59:
        case AvaLangParser::INC:
        case AvaLangParser::DEC:
        case AvaLangParser::NAME:
        case AvaLangParser::NUMBER:
        case AvaLangParser::STRING:
        case AvaLangParser::FSTRING: {
          setState(118);
          statement();
          break;
        }

        case AvaLangParser::NEWLINE: {
          setState(119);
          match(AvaLangParser::NEWLINE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(124);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BlockContext ------------------------------------------------------------------

AvaLangParser::BlockContext::BlockContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::StatementContext *> AvaLangParser::BlockContext::statement() {
  return getRuleContexts<AvaLangParser::StatementContext>();
}

AvaLangParser::StatementContext* AvaLangParser::BlockContext::statement(size_t i) {
  return getRuleContext<AvaLangParser::StatementContext>(i);
}

std::vector<tree::TerminalNode *> AvaLangParser::BlockContext::NEWLINE() {
  return getTokens(AvaLangParser::NEWLINE);
}

tree::TerminalNode* AvaLangParser::BlockContext::NEWLINE(size_t i) {
  return getToken(AvaLangParser::NEWLINE, i);
}


size_t AvaLangParser::BlockContext::getRuleIndex() const {
  return AvaLangParser::RuleBlock;
}

void AvaLangParser::BlockContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBlock(this);
}

void AvaLangParser::BlockContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBlock(this);
}


std::any AvaLangParser::BlockContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitBlock(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::BlockContext* AvaLangParser::block() {
  BlockContext *_localctx = _tracker.createInstance<BlockContext>(_ctx, getState());
  enterRule(_localctx, 2, AvaLangParser::RuleBlock);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(129);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -6970441808740613988) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 125) != 0)) {
      setState(127);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case AvaLangParser::T__1:
        case AvaLangParser::T__2:
        case AvaLangParser::T__3:
        case AvaLangParser::T__6:
        case AvaLangParser::T__17:
        case AvaLangParser::T__18:
        case AvaLangParser::T__19:
        case AvaLangParser::T__20:
        case AvaLangParser::T__21:
        case AvaLangParser::T__24:
        case AvaLangParser::T__25:
        case AvaLangParser::T__26:
        case AvaLangParser::T__27:
        case AvaLangParser::T__31:
        case AvaLangParser::T__32:
        case AvaLangParser::T__34:
        case AvaLangParser::T__35:
        case AvaLangParser::T__41:
        case AvaLangParser::T__49:
        case AvaLangParser::T__53:
        case AvaLangParser::T__55:
        case AvaLangParser::T__56:
        case AvaLangParser::T__57:
        case AvaLangParser::T__58:
        case AvaLangParser::T__59:
        case AvaLangParser::INC:
        case AvaLangParser::DEC:
        case AvaLangParser::NAME:
        case AvaLangParser::NUMBER:
        case AvaLangParser::STRING:
        case AvaLangParser::FSTRING: {
          setState(125);
          statement();
          break;
        }

        case AvaLangParser::NEWLINE: {
          setState(126);
          match(AvaLangParser::NEWLINE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(131);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementContext ------------------------------------------------------------------

AvaLangParser::StatementContext::StatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::SimpleStatementContext* AvaLangParser::StatementContext::simpleStatement() {
  return getRuleContext<AvaLangParser::SimpleStatementContext>(0);
}

AvaLangParser::CompoundStatementContext* AvaLangParser::StatementContext::compoundStatement() {
  return getRuleContext<AvaLangParser::CompoundStatementContext>(0);
}


size_t AvaLangParser::StatementContext::getRuleIndex() const {
  return AvaLangParser::RuleStatement;
}

void AvaLangParser::StatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterStatement(this);
}

void AvaLangParser::StatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitStatement(this);
}


std::any AvaLangParser::StatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::StatementContext* AvaLangParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 4, AvaLangParser::RuleStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(134);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 4, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(132);
      simpleStatement();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(133);
      compoundStatement();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SimpleStatementContext ------------------------------------------------------------------

AvaLangParser::SimpleStatementContext::SimpleStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::SmallStatementContext* AvaLangParser::SimpleStatementContext::smallStatement() {
  return getRuleContext<AvaLangParser::SmallStatementContext>(0);
}

std::vector<tree::TerminalNode *> AvaLangParser::SimpleStatementContext::NEWLINE() {
  return getTokens(AvaLangParser::NEWLINE);
}

tree::TerminalNode* AvaLangParser::SimpleStatementContext::NEWLINE(size_t i) {
  return getToken(AvaLangParser::NEWLINE, i);
}


size_t AvaLangParser::SimpleStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleSimpleStatement;
}

void AvaLangParser::SimpleStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterSimpleStatement(this);
}

void AvaLangParser::SimpleStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitSimpleStatement(this);
}


std::any AvaLangParser::SimpleStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitSimpleStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::SimpleStatementContext* AvaLangParser::simpleStatement() {
  SimpleStatementContext *_localctx = _tracker.createInstance<SimpleStatementContext>(_ctx, getState());
  enterRule(_localctx, 6, AvaLangParser::RuleSimpleStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(136);
    smallStatement();
    setState(140); 
    _errHandler->sync(this);
    alt = 1;
    do {
      switch (alt) {
        case 1: {
              setState(140);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case AvaLangParser::NEWLINE: {
                  setState(137);
                  match(AvaLangParser::NEWLINE);
                  break;
                }

                case AvaLangParser::T__0: {
                  setState(138);
                  match(AvaLangParser::T__0);
                  setState(139);
                  match(AvaLangParser::NEWLINE);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              break;
            }

      default:
        throw NoViableAltException(this);
      }
      setState(142); 
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 6, _ctx);
    } while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SmallStatementContext ------------------------------------------------------------------

AvaLangParser::SmallStatementContext::SmallStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::AssignStatementContext* AvaLangParser::SmallStatementContext::assignStatement() {
  return getRuleContext<AvaLangParser::AssignStatementContext>(0);
}

AvaLangParser::MultiAssignStatementContext* AvaLangParser::SmallStatementContext::multiAssignStatement() {
  return getRuleContext<AvaLangParser::MultiAssignStatementContext>(0);
}

AvaLangParser::AugAssignStatementContext* AvaLangParser::SmallStatementContext::augAssignStatement() {
  return getRuleContext<AvaLangParser::AugAssignStatementContext>(0);
}

AvaLangParser::ExprStatementContext* AvaLangParser::SmallStatementContext::exprStatement() {
  return getRuleContext<AvaLangParser::ExprStatementContext>(0);
}

AvaLangParser::ReturnStatementContext* AvaLangParser::SmallStatementContext::returnStatement() {
  return getRuleContext<AvaLangParser::ReturnStatementContext>(0);
}

AvaLangParser::BreakStatementContext* AvaLangParser::SmallStatementContext::breakStatement() {
  return getRuleContext<AvaLangParser::BreakStatementContext>(0);
}

AvaLangParser::ContinueStatementContext* AvaLangParser::SmallStatementContext::continueStatement() {
  return getRuleContext<AvaLangParser::ContinueStatementContext>(0);
}

AvaLangParser::PassStatementContext* AvaLangParser::SmallStatementContext::passStatement() {
  return getRuleContext<AvaLangParser::PassStatementContext>(0);
}

AvaLangParser::ImportStatementContext* AvaLangParser::SmallStatementContext::importStatement() {
  return getRuleContext<AvaLangParser::ImportStatementContext>(0);
}

AvaLangParser::LocalStatementContext* AvaLangParser::SmallStatementContext::localStatement() {
  return getRuleContext<AvaLangParser::LocalStatementContext>(0);
}

AvaLangParser::RaiseStatementContext* AvaLangParser::SmallStatementContext::raiseStatement() {
  return getRuleContext<AvaLangParser::RaiseStatementContext>(0);
}

AvaLangParser::YieldStatementContext* AvaLangParser::SmallStatementContext::yieldStatement() {
  return getRuleContext<AvaLangParser::YieldStatementContext>(0);
}

AvaLangParser::IncDecStatementContext* AvaLangParser::SmallStatementContext::incDecStatement() {
  return getRuleContext<AvaLangParser::IncDecStatementContext>(0);
}

AvaLangParser::ModifiedAssignStatementContext* AvaLangParser::SmallStatementContext::modifiedAssignStatement() {
  return getRuleContext<AvaLangParser::ModifiedAssignStatementContext>(0);
}


size_t AvaLangParser::SmallStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleSmallStatement;
}

void AvaLangParser::SmallStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterSmallStatement(this);
}

void AvaLangParser::SmallStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitSmallStatement(this);
}


std::any AvaLangParser::SmallStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitSmallStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::SmallStatementContext* AvaLangParser::smallStatement() {
  SmallStatementContext *_localctx = _tracker.createInstance<SmallStatementContext>(_ctx, getState());
  enterRule(_localctx, 8, AvaLangParser::RuleSmallStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(158);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(144);
      assignStatement();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(145);
      multiAssignStatement();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(146);
      augAssignStatement();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(147);
      exprStatement();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(148);
      returnStatement();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(149);
      breakStatement();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(150);
      continueStatement();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(151);
      passStatement();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(152);
      importStatement();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(153);
      localStatement();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(154);
      raiseStatement();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(155);
      yieldStatement();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(156);
      incDecStatement();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(157);
      modifiedAssignStatement();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModifiedAssignStatementContext ------------------------------------------------------------------

AvaLangParser::ModifiedAssignStatementContext::ModifiedAssignStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::AssignStatementContext* AvaLangParser::ModifiedAssignStatementContext::assignStatement() {
  return getRuleContext<AvaLangParser::AssignStatementContext>(0);
}

std::vector<AvaLangParser::MemberModifierContext *> AvaLangParser::ModifiedAssignStatementContext::memberModifier() {
  return getRuleContexts<AvaLangParser::MemberModifierContext>();
}

AvaLangParser::MemberModifierContext* AvaLangParser::ModifiedAssignStatementContext::memberModifier(size_t i) {
  return getRuleContext<AvaLangParser::MemberModifierContext>(i);
}


size_t AvaLangParser::ModifiedAssignStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleModifiedAssignStatement;
}

void AvaLangParser::ModifiedAssignStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterModifiedAssignStatement(this);
}

void AvaLangParser::ModifiedAssignStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitModifiedAssignStatement(this);
}


std::any AvaLangParser::ModifiedAssignStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitModifiedAssignStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ModifiedAssignStatementContext* AvaLangParser::modifiedAssignStatement() {
  ModifiedAssignStatementContext *_localctx = _tracker.createInstance<ModifiedAssignStatementContext>(_ctx, getState());
  enterRule(_localctx, 10, AvaLangParser::RuleModifiedAssignStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(161); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(160);
      memberModifier();
      setState(163); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == AvaLangParser::T__1

    || _la == AvaLangParser::T__2);
    setState(165);
    assignStatement();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- IncDecStatementContext ------------------------------------------------------------------

AvaLangParser::IncDecStatementContext::IncDecStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::TargetContext* AvaLangParser::IncDecStatementContext::target() {
  return getRuleContext<AvaLangParser::TargetContext>(0);
}

tree::TerminalNode* AvaLangParser::IncDecStatementContext::INC() {
  return getToken(AvaLangParser::INC, 0);
}

tree::TerminalNode* AvaLangParser::IncDecStatementContext::DEC() {
  return getToken(AvaLangParser::DEC, 0);
}


size_t AvaLangParser::IncDecStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleIncDecStatement;
}

void AvaLangParser::IncDecStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterIncDecStatement(this);
}

void AvaLangParser::IncDecStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitIncDecStatement(this);
}


std::any AvaLangParser::IncDecStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitIncDecStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::IncDecStatementContext* AvaLangParser::incDecStatement() {
  IncDecStatementContext *_localctx = _tracker.createInstance<IncDecStatementContext>(_ctx, getState());
  enterRule(_localctx, 12, AvaLangParser::RuleIncDecStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(167);
    _la = _input->LA(1);
    if (!(_la == AvaLangParser::INC

    || _la == AvaLangParser::DEC)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(168);
    target();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CompoundStatementContext ------------------------------------------------------------------

AvaLangParser::CompoundStatementContext::CompoundStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::IfStatementContext* AvaLangParser::CompoundStatementContext::ifStatement() {
  return getRuleContext<AvaLangParser::IfStatementContext>(0);
}

AvaLangParser::WhileStatementContext* AvaLangParser::CompoundStatementContext::whileStatement() {
  return getRuleContext<AvaLangParser::WhileStatementContext>(0);
}

AvaLangParser::ForStatementContext* AvaLangParser::CompoundStatementContext::forStatement() {
  return getRuleContext<AvaLangParser::ForStatementContext>(0);
}

AvaLangParser::FuncDeclarationContext* AvaLangParser::CompoundStatementContext::funcDeclaration() {
  return getRuleContext<AvaLangParser::FuncDeclarationContext>(0);
}

AvaLangParser::ClassDeclarationContext* AvaLangParser::CompoundStatementContext::classDeclaration() {
  return getRuleContext<AvaLangParser::ClassDeclarationContext>(0);
}

AvaLangParser::TryStatementContext* AvaLangParser::CompoundStatementContext::tryStatement() {
  return getRuleContext<AvaLangParser::TryStatementContext>(0);
}

AvaLangParser::ModifiedFuncDeclarationContext* AvaLangParser::CompoundStatementContext::modifiedFuncDeclaration() {
  return getRuleContext<AvaLangParser::ModifiedFuncDeclarationContext>(0);
}


size_t AvaLangParser::CompoundStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleCompoundStatement;
}

void AvaLangParser::CompoundStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterCompoundStatement(this);
}

void AvaLangParser::CompoundStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitCompoundStatement(this);
}


std::any AvaLangParser::CompoundStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitCompoundStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::CompoundStatementContext* AvaLangParser::compoundStatement() {
  CompoundStatementContext *_localctx = _tracker.createInstance<CompoundStatementContext>(_ctx, getState());
  enterRule(_localctx, 14, AvaLangParser::RuleCompoundStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(177);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case AvaLangParser::T__27: {
        enterOuterAlt(_localctx, 1);
        setState(170);
        ifStatement();
        break;
      }

      case AvaLangParser::T__31: {
        enterOuterAlt(_localctx, 2);
        setState(171);
        whileStatement();
        break;
      }

      case AvaLangParser::T__32: {
        enterOuterAlt(_localctx, 3);
        setState(172);
        forStatement();
        break;
      }

      case AvaLangParser::T__34: {
        enterOuterAlt(_localctx, 4);
        setState(173);
        funcDeclaration();
        break;
      }

      case AvaLangParser::T__35: {
        enterOuterAlt(_localctx, 5);
        setState(174);
        classDeclaration();
        break;
      }

      case AvaLangParser::T__3: {
        enterOuterAlt(_localctx, 6);
        setState(175);
        tryStatement();
        break;
      }

      case AvaLangParser::T__1:
      case AvaLangParser::T__2: {
        enterOuterAlt(_localctx, 7);
        setState(176);
        modifiedFuncDeclaration();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- MemberModifierContext ------------------------------------------------------------------

AvaLangParser::MemberModifierContext::MemberModifierContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::MemberModifierContext::getRuleIndex() const {
  return AvaLangParser::RuleMemberModifier;
}

void AvaLangParser::MemberModifierContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterMemberModifier(this);
}

void AvaLangParser::MemberModifierContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitMemberModifier(this);
}


std::any AvaLangParser::MemberModifierContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitMemberModifier(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::MemberModifierContext* AvaLangParser::memberModifier() {
  MemberModifierContext *_localctx = _tracker.createInstance<MemberModifierContext>(_ctx, getState());
  enterRule(_localctx, 16, AvaLangParser::RuleMemberModifier);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(179);
    _la = _input->LA(1);
    if (!(_la == AvaLangParser::T__1

    || _la == AvaLangParser::T__2)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModifiedFuncDeclarationContext ------------------------------------------------------------------

AvaLangParser::ModifiedFuncDeclarationContext::ModifiedFuncDeclarationContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::FuncDeclarationContext* AvaLangParser::ModifiedFuncDeclarationContext::funcDeclaration() {
  return getRuleContext<AvaLangParser::FuncDeclarationContext>(0);
}

std::vector<AvaLangParser::MemberModifierContext *> AvaLangParser::ModifiedFuncDeclarationContext::memberModifier() {
  return getRuleContexts<AvaLangParser::MemberModifierContext>();
}

AvaLangParser::MemberModifierContext* AvaLangParser::ModifiedFuncDeclarationContext::memberModifier(size_t i) {
  return getRuleContext<AvaLangParser::MemberModifierContext>(i);
}


size_t AvaLangParser::ModifiedFuncDeclarationContext::getRuleIndex() const {
  return AvaLangParser::RuleModifiedFuncDeclaration;
}

void AvaLangParser::ModifiedFuncDeclarationContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterModifiedFuncDeclaration(this);
}

void AvaLangParser::ModifiedFuncDeclarationContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitModifiedFuncDeclaration(this);
}


std::any AvaLangParser::ModifiedFuncDeclarationContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitModifiedFuncDeclaration(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ModifiedFuncDeclarationContext* AvaLangParser::modifiedFuncDeclaration() {
  ModifiedFuncDeclarationContext *_localctx = _tracker.createInstance<ModifiedFuncDeclarationContext>(_ctx, getState());
  enterRule(_localctx, 18, AvaLangParser::RuleModifiedFuncDeclaration);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(182); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(181);
      memberModifier();
      setState(184); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == AvaLangParser::T__1

    || _la == AvaLangParser::T__2);
    setState(186);
    funcDeclaration();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TryStatementContext ------------------------------------------------------------------

AvaLangParser::TryStatementContext::TryStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::BlockContext* AvaLangParser::TryStatementContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}

std::vector<AvaLangParser::ExceptClauseContext *> AvaLangParser::TryStatementContext::exceptClause() {
  return getRuleContexts<AvaLangParser::ExceptClauseContext>();
}

AvaLangParser::ExceptClauseContext* AvaLangParser::TryStatementContext::exceptClause(size_t i) {
  return getRuleContext<AvaLangParser::ExceptClauseContext>(i);
}

AvaLangParser::FinallyClauseContext* AvaLangParser::TryStatementContext::finallyClause() {
  return getRuleContext<AvaLangParser::FinallyClauseContext>(0);
}


size_t AvaLangParser::TryStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleTryStatement;
}

void AvaLangParser::TryStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTryStatement(this);
}

void AvaLangParser::TryStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTryStatement(this);
}


std::any AvaLangParser::TryStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitTryStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::TryStatementContext* AvaLangParser::tryStatement() {
  TryStatementContext *_localctx = _tracker.createInstance<TryStatementContext>(_ctx, getState());
  enterRule(_localctx, 20, AvaLangParser::RuleTryStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(205);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 13, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(188);
      match(AvaLangParser::T__3);
      setState(189);
      block();
      setState(191); 
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(190);
        exceptClause();
        setState(193); 
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while (_la == AvaLangParser::T__5);
      setState(196);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == AvaLangParser::T__8) {
        setState(195);
        finallyClause();
      }
      setState(198);
      match(AvaLangParser::T__4);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(200);
      match(AvaLangParser::T__3);
      setState(201);
      block();

      setState(202);
      finallyClause();
      setState(203);
      match(AvaLangParser::T__4);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExceptClauseContext ------------------------------------------------------------------

AvaLangParser::ExceptClauseContext::ExceptClauseContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::ExceptClauseContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::BlockContext* AvaLangParser::ExceptClauseContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}


size_t AvaLangParser::ExceptClauseContext::getRuleIndex() const {
  return AvaLangParser::RuleExceptClause;
}

void AvaLangParser::ExceptClauseContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExceptClause(this);
}

void AvaLangParser::ExceptClauseContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExceptClause(this);
}


std::any AvaLangParser::ExceptClauseContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExceptClause(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExceptClauseContext* AvaLangParser::exceptClause() {
  ExceptClauseContext *_localctx = _tracker.createInstance<ExceptClauseContext>(_ctx, getState());
  enterRule(_localctx, 22, AvaLangParser::RuleExceptClause);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(217);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 14, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(207);
      match(AvaLangParser::T__5);
      setState(208);
      match(AvaLangParser::T__6);
      setState(209);
      expr();
      setState(210);
      match(AvaLangParser::T__7);
      setState(211);
      block();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(213);
      match(AvaLangParser::T__5);
      setState(214);
      expr();
      setState(215);
      block();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FinallyClauseContext ------------------------------------------------------------------

AvaLangParser::FinallyClauseContext::FinallyClauseContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::BlockContext* AvaLangParser::FinallyClauseContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}


size_t AvaLangParser::FinallyClauseContext::getRuleIndex() const {
  return AvaLangParser::RuleFinallyClause;
}

void AvaLangParser::FinallyClauseContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFinallyClause(this);
}

void AvaLangParser::FinallyClauseContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFinallyClause(this);
}


std::any AvaLangParser::FinallyClauseContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitFinallyClause(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::FinallyClauseContext* AvaLangParser::finallyClause() {
  FinallyClauseContext *_localctx = _tracker.createInstance<FinallyClauseContext>(_ctx, getState());
  enterRule(_localctx, 24, AvaLangParser::RuleFinallyClause);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(219);
    match(AvaLangParser::T__8);
    setState(220);
    block();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- MultiAssignStatementContext ------------------------------------------------------------------

AvaLangParser::MultiAssignStatementContext::MultiAssignStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::AssignStatementContext *> AvaLangParser::MultiAssignStatementContext::assignStatement() {
  return getRuleContexts<AvaLangParser::AssignStatementContext>();
}

AvaLangParser::AssignStatementContext* AvaLangParser::MultiAssignStatementContext::assignStatement(size_t i) {
  return getRuleContext<AvaLangParser::AssignStatementContext>(i);
}


size_t AvaLangParser::MultiAssignStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleMultiAssignStatement;
}

void AvaLangParser::MultiAssignStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterMultiAssignStatement(this);
}

void AvaLangParser::MultiAssignStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitMultiAssignStatement(this);
}


std::any AvaLangParser::MultiAssignStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitMultiAssignStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::MultiAssignStatementContext* AvaLangParser::multiAssignStatement() {
  MultiAssignStatementContext *_localctx = _tracker.createInstance<MultiAssignStatementContext>(_ctx, getState());
  enterRule(_localctx, 26, AvaLangParser::RuleMultiAssignStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(222);
    assignStatement();
    setState(225); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(223);
      match(AvaLangParser::T__9);
      setState(224);
      assignStatement();
      setState(227); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == AvaLangParser::T__9);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AssignStatementContext ------------------------------------------------------------------

AvaLangParser::AssignStatementContext::AssignStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::TargetListContext* AvaLangParser::AssignStatementContext::targetList() {
  return getRuleContext<AvaLangParser::TargetListContext>(0);
}

AvaLangParser::ExprListContext* AvaLangParser::AssignStatementContext::exprList() {
  return getRuleContext<AvaLangParser::ExprListContext>(0);
}


size_t AvaLangParser::AssignStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleAssignStatement;
}

void AvaLangParser::AssignStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterAssignStatement(this);
}

void AvaLangParser::AssignStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitAssignStatement(this);
}


std::any AvaLangParser::AssignStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitAssignStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::AssignStatementContext* AvaLangParser::assignStatement() {
  AssignStatementContext *_localctx = _tracker.createInstance<AssignStatementContext>(_ctx, getState());
  enterRule(_localctx, 28, AvaLangParser::RuleAssignStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(229);
    targetList();
    setState(230);
    match(AvaLangParser::T__10);
    setState(231);
    exprList();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AugAssignStatementContext ------------------------------------------------------------------

AvaLangParser::AugAssignStatementContext::AugAssignStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::TargetContext* AvaLangParser::AugAssignStatementContext::target() {
  return getRuleContext<AvaLangParser::TargetContext>(0);
}

AvaLangParser::ExprContext* AvaLangParser::AugAssignStatementContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}


size_t AvaLangParser::AugAssignStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleAugAssignStatement;
}

void AvaLangParser::AugAssignStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterAugAssignStatement(this);
}

void AvaLangParser::AugAssignStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitAugAssignStatement(this);
}


std::any AvaLangParser::AugAssignStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitAugAssignStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::AugAssignStatementContext* AvaLangParser::augAssignStatement() {
  AugAssignStatementContext *_localctx = _tracker.createInstance<AugAssignStatementContext>(_ctx, getState());
  enterRule(_localctx, 30, AvaLangParser::RuleAugAssignStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(233);
    target();
    setState(234);
    antlrcpp::downCast<AugAssignStatementContext *>(_localctx)->op = _input->LT(1);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 258048) != 0))) {
      antlrcpp::downCast<AugAssignStatementContext *>(_localctx)->op = _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(235);
    expr();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprStatementContext ------------------------------------------------------------------

AvaLangParser::ExprStatementContext::ExprStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprListContext* AvaLangParser::ExprStatementContext::exprList() {
  return getRuleContext<AvaLangParser::ExprListContext>(0);
}


size_t AvaLangParser::ExprStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleExprStatement;
}

void AvaLangParser::ExprStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExprStatement(this);
}

void AvaLangParser::ExprStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExprStatement(this);
}


std::any AvaLangParser::ExprStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExprStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExprStatementContext* AvaLangParser::exprStatement() {
  ExprStatementContext *_localctx = _tracker.createInstance<ExprStatementContext>(_ctx, getState());
  enterRule(_localctx, 32, AvaLangParser::RuleExprStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(237);
    exprList();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ReturnStatementContext ------------------------------------------------------------------

AvaLangParser::ReturnStatementContext::ReturnStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprListContext* AvaLangParser::ReturnStatementContext::exprList() {
  return getRuleContext<AvaLangParser::ExprListContext>(0);
}


size_t AvaLangParser::ReturnStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleReturnStatement;
}

void AvaLangParser::ReturnStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterReturnStatement(this);
}

void AvaLangParser::ReturnStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitReturnStatement(this);
}


std::any AvaLangParser::ReturnStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitReturnStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ReturnStatementContext* AvaLangParser::returnStatement() {
  ReturnStatementContext *_localctx = _tracker.createInstance<ReturnStatementContext>(_ctx, getState());
  enterRule(_localctx, 34, AvaLangParser::RuleReturnStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(239);
    match(AvaLangParser::T__17);
    setState(241);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
      setState(240);
      exprList();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BreakStatementContext ------------------------------------------------------------------

AvaLangParser::BreakStatementContext::BreakStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::BreakStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleBreakStatement;
}

void AvaLangParser::BreakStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBreakStatement(this);
}

void AvaLangParser::BreakStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBreakStatement(this);
}


std::any AvaLangParser::BreakStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitBreakStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::BreakStatementContext* AvaLangParser::breakStatement() {
  BreakStatementContext *_localctx = _tracker.createInstance<BreakStatementContext>(_ctx, getState());
  enterRule(_localctx, 36, AvaLangParser::RuleBreakStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(243);
    match(AvaLangParser::T__18);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ContinueStatementContext ------------------------------------------------------------------

AvaLangParser::ContinueStatementContext::ContinueStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::ContinueStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleContinueStatement;
}

void AvaLangParser::ContinueStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterContinueStatement(this);
}

void AvaLangParser::ContinueStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitContinueStatement(this);
}


std::any AvaLangParser::ContinueStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitContinueStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ContinueStatementContext* AvaLangParser::continueStatement() {
  ContinueStatementContext *_localctx = _tracker.createInstance<ContinueStatementContext>(_ctx, getState());
  enterRule(_localctx, 38, AvaLangParser::RuleContinueStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(245);
    match(AvaLangParser::T__19);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PassStatementContext ------------------------------------------------------------------

AvaLangParser::PassStatementContext::PassStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::PassStatementContext::getRuleIndex() const {
  return AvaLangParser::RulePassStatement;
}

void AvaLangParser::PassStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterPassStatement(this);
}

void AvaLangParser::PassStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitPassStatement(this);
}


std::any AvaLangParser::PassStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitPassStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::PassStatementContext* AvaLangParser::passStatement() {
  PassStatementContext *_localctx = _tracker.createInstance<PassStatementContext>(_ctx, getState());
  enterRule(_localctx, 40, AvaLangParser::RulePassStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(247);
    match(AvaLangParser::T__20);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ImportStatementContext ------------------------------------------------------------------

AvaLangParser::ImportStatementContext::ImportStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> AvaLangParser::ImportStatementContext::NAME() {
  return getTokens(AvaLangParser::NAME);
}

tree::TerminalNode* AvaLangParser::ImportStatementContext::NAME(size_t i) {
  return getToken(AvaLangParser::NAME, i);
}


size_t AvaLangParser::ImportStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleImportStatement;
}

void AvaLangParser::ImportStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterImportStatement(this);
}

void AvaLangParser::ImportStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitImportStatement(this);
}


std::any AvaLangParser::ImportStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitImportStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ImportStatementContext* AvaLangParser::importStatement() {
  ImportStatementContext *_localctx = _tracker.createInstance<ImportStatementContext>(_ctx, getState());
  enterRule(_localctx, 42, AvaLangParser::RuleImportStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(249);
    match(AvaLangParser::T__21);
    setState(250);
    match(AvaLangParser::NAME);
    setState(255);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__22) {
      setState(251);
      match(AvaLangParser::T__22);
      setState(252);
      match(AvaLangParser::NAME);
      setState(257);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(260);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__23) {
      setState(258);
      antlrcpp::downCast<ImportStatementContext *>(_localctx)->as = match(AvaLangParser::T__23);
      setState(259);
      match(AvaLangParser::NAME);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LocalStatementContext ------------------------------------------------------------------

AvaLangParser::LocalStatementContext::LocalStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::AssignStatementContext* AvaLangParser::LocalStatementContext::assignStatement() {
  return getRuleContext<AvaLangParser::AssignStatementContext>(0);
}


size_t AvaLangParser::LocalStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleLocalStatement;
}

void AvaLangParser::LocalStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLocalStatement(this);
}

void AvaLangParser::LocalStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLocalStatement(this);
}


std::any AvaLangParser::LocalStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitLocalStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::LocalStatementContext* AvaLangParser::localStatement() {
  LocalStatementContext *_localctx = _tracker.createInstance<LocalStatementContext>(_ctx, getState());
  enterRule(_localctx, 44, AvaLangParser::RuleLocalStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(262);
    match(AvaLangParser::T__24);
    setState(263);
    assignStatement();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- RaiseStatementContext ------------------------------------------------------------------

AvaLangParser::RaiseStatementContext::RaiseStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::RaiseStatementContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}


size_t AvaLangParser::RaiseStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleRaiseStatement;
}

void AvaLangParser::RaiseStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterRaiseStatement(this);
}

void AvaLangParser::RaiseStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitRaiseStatement(this);
}


std::any AvaLangParser::RaiseStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitRaiseStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::RaiseStatementContext* AvaLangParser::raiseStatement() {
  RaiseStatementContext *_localctx = _tracker.createInstance<RaiseStatementContext>(_ctx, getState());
  enterRule(_localctx, 46, AvaLangParser::RuleRaiseStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(265);
    match(AvaLangParser::T__25);
    setState(266);
    expr();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- YieldStatementContext ------------------------------------------------------------------

AvaLangParser::YieldStatementContext::YieldStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprListContext* AvaLangParser::YieldStatementContext::exprList() {
  return getRuleContext<AvaLangParser::ExprListContext>(0);
}


size_t AvaLangParser::YieldStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleYieldStatement;
}

void AvaLangParser::YieldStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterYieldStatement(this);
}

void AvaLangParser::YieldStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitYieldStatement(this);
}


std::any AvaLangParser::YieldStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitYieldStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::YieldStatementContext* AvaLangParser::yieldStatement() {
  YieldStatementContext *_localctx = _tracker.createInstance<YieldStatementContext>(_ctx, getState());
  enterRule(_localctx, 48, AvaLangParser::RuleYieldStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(268);
    match(AvaLangParser::T__26);
    setState(270);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
      setState(269);
      exprList();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- IfStatementContext ------------------------------------------------------------------

AvaLangParser::IfStatementContext::IfStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::IfStatementContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::BlockContext* AvaLangParser::IfStatementContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}

std::vector<AvaLangParser::ElifClauseContext *> AvaLangParser::IfStatementContext::elifClause() {
  return getRuleContexts<AvaLangParser::ElifClauseContext>();
}

AvaLangParser::ElifClauseContext* AvaLangParser::IfStatementContext::elifClause(size_t i) {
  return getRuleContext<AvaLangParser::ElifClauseContext>(i);
}

AvaLangParser::ElseClauseContext* AvaLangParser::IfStatementContext::elseClause() {
  return getRuleContext<AvaLangParser::ElseClauseContext>(0);
}


size_t AvaLangParser::IfStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleIfStatement;
}

void AvaLangParser::IfStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterIfStatement(this);
}

void AvaLangParser::IfStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitIfStatement(this);
}


std::any AvaLangParser::IfStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitIfStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::IfStatementContext* AvaLangParser::ifStatement() {
  IfStatementContext *_localctx = _tracker.createInstance<IfStatementContext>(_ctx, getState());
  enterRule(_localctx, 50, AvaLangParser::RuleIfStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(272);
    match(AvaLangParser::T__27);
    setState(273);
    expr();
    setState(274);
    match(AvaLangParser::T__28);
    setState(275);
    block();
    setState(279);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__29) {
      setState(276);
      elifClause();
      setState(281);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(283);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__30) {
      setState(282);
      elseClause();
    }
    setState(285);
    match(AvaLangParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ElifClauseContext ------------------------------------------------------------------

AvaLangParser::ElifClauseContext::ElifClauseContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::ElifClauseContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::BlockContext* AvaLangParser::ElifClauseContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}


size_t AvaLangParser::ElifClauseContext::getRuleIndex() const {
  return AvaLangParser::RuleElifClause;
}

void AvaLangParser::ElifClauseContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterElifClause(this);
}

void AvaLangParser::ElifClauseContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitElifClause(this);
}


std::any AvaLangParser::ElifClauseContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitElifClause(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ElifClauseContext* AvaLangParser::elifClause() {
  ElifClauseContext *_localctx = _tracker.createInstance<ElifClauseContext>(_ctx, getState());
  enterRule(_localctx, 52, AvaLangParser::RuleElifClause);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(287);
    match(AvaLangParser::T__29);
    setState(288);
    expr();
    setState(289);
    match(AvaLangParser::T__28);
    setState(290);
    block();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ElseClauseContext ------------------------------------------------------------------

AvaLangParser::ElseClauseContext::ElseClauseContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::BlockContext* AvaLangParser::ElseClauseContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}


size_t AvaLangParser::ElseClauseContext::getRuleIndex() const {
  return AvaLangParser::RuleElseClause;
}

void AvaLangParser::ElseClauseContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterElseClause(this);
}

void AvaLangParser::ElseClauseContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitElseClause(this);
}


std::any AvaLangParser::ElseClauseContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitElseClause(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ElseClauseContext* AvaLangParser::elseClause() {
  ElseClauseContext *_localctx = _tracker.createInstance<ElseClauseContext>(_ctx, getState());
  enterRule(_localctx, 54, AvaLangParser::RuleElseClause);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(292);
    match(AvaLangParser::T__30);
    setState(293);
    block();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- WhileStatementContext ------------------------------------------------------------------

AvaLangParser::WhileStatementContext::WhileStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::WhileStatementContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::BlockContext* AvaLangParser::WhileStatementContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}


size_t AvaLangParser::WhileStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleWhileStatement;
}

void AvaLangParser::WhileStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterWhileStatement(this);
}

void AvaLangParser::WhileStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitWhileStatement(this);
}


std::any AvaLangParser::WhileStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitWhileStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::WhileStatementContext* AvaLangParser::whileStatement() {
  WhileStatementContext *_localctx = _tracker.createInstance<WhileStatementContext>(_ctx, getState());
  enterRule(_localctx, 56, AvaLangParser::RuleWhileStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(307);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 22, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(295);
      match(AvaLangParser::T__31);
      setState(296);
      match(AvaLangParser::T__6);
      setState(297);
      expr();
      setState(298);
      match(AvaLangParser::T__7);
      setState(299);
      block();
      setState(300);
      match(AvaLangParser::T__4);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(302);
      match(AvaLangParser::T__31);
      setState(303);
      expr();
      setState(304);
      block();
      setState(305);
      match(AvaLangParser::T__4);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ForStatementContext ------------------------------------------------------------------

AvaLangParser::ForStatementContext::ForStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::TargetListContext* AvaLangParser::ForStatementContext::targetList() {
  return getRuleContext<AvaLangParser::TargetListContext>(0);
}

AvaLangParser::ExprListContext* AvaLangParser::ForStatementContext::exprList() {
  return getRuleContext<AvaLangParser::ExprListContext>(0);
}

AvaLangParser::BlockContext* AvaLangParser::ForStatementContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}


size_t AvaLangParser::ForStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleForStatement;
}

void AvaLangParser::ForStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterForStatement(this);
}

void AvaLangParser::ForStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitForStatement(this);
}


std::any AvaLangParser::ForStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitForStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ForStatementContext* AvaLangParser::forStatement() {
  ForStatementContext *_localctx = _tracker.createInstance<ForStatementContext>(_ctx, getState());
  enterRule(_localctx, 58, AvaLangParser::RuleForStatement);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(327);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 23, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(309);
      match(AvaLangParser::T__32);
      setState(310);
      targetList();
      setState(311);
      match(AvaLangParser::T__33);
      setState(312);
      exprList();
      setState(313);
      match(AvaLangParser::T__28);
      setState(314);
      block();
      setState(315);
      match(AvaLangParser::T__4);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(317);
      match(AvaLangParser::T__32);
      setState(318);
      targetList();
      setState(319);
      match(AvaLangParser::T__33);
      setState(320);
      match(AvaLangParser::T__6);
      setState(321);
      exprList();
      setState(322);
      match(AvaLangParser::T__7);
      setState(323);
      match(AvaLangParser::T__28);
      setState(324);
      block();
      setState(325);
      match(AvaLangParser::T__4);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FuncDeclarationContext ------------------------------------------------------------------

AvaLangParser::FuncDeclarationContext::FuncDeclarationContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::FuncDeclarationContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::BlockContext* AvaLangParser::FuncDeclarationContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}

AvaLangParser::ParamListContext* AvaLangParser::FuncDeclarationContext::paramList() {
  return getRuleContext<AvaLangParser::ParamListContext>(0);
}


size_t AvaLangParser::FuncDeclarationContext::getRuleIndex() const {
  return AvaLangParser::RuleFuncDeclaration;
}

void AvaLangParser::FuncDeclarationContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFuncDeclaration(this);
}

void AvaLangParser::FuncDeclarationContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFuncDeclaration(this);
}


std::any AvaLangParser::FuncDeclarationContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitFuncDeclaration(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::FuncDeclarationContext* AvaLangParser::funcDeclaration() {
  FuncDeclarationContext *_localctx = _tracker.createInstance<FuncDeclarationContext>(_ctx, getState());
  enterRule(_localctx, 60, AvaLangParser::RuleFuncDeclaration);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(329);
    match(AvaLangParser::T__34);
    setState(330);
    match(AvaLangParser::NAME);
    setState(331);
    match(AvaLangParser::T__6);
    setState(333);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(332);
      paramList();
    }
    setState(335);
    match(AvaLangParser::T__7);
    setState(336);
    block();
    setState(337);
    match(AvaLangParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ClassDeclarationContext ------------------------------------------------------------------

AvaLangParser::ClassDeclarationContext::ClassDeclarationContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::ClassDeclarationContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::BlockContext* AvaLangParser::ClassDeclarationContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}

AvaLangParser::ClassHeritageContext* AvaLangParser::ClassDeclarationContext::classHeritage() {
  return getRuleContext<AvaLangParser::ClassHeritageContext>(0);
}


size_t AvaLangParser::ClassDeclarationContext::getRuleIndex() const {
  return AvaLangParser::RuleClassDeclaration;
}

void AvaLangParser::ClassDeclarationContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterClassDeclaration(this);
}

void AvaLangParser::ClassDeclarationContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitClassDeclaration(this);
}


std::any AvaLangParser::ClassDeclarationContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitClassDeclaration(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ClassDeclarationContext* AvaLangParser::classDeclaration() {
  ClassDeclarationContext *_localctx = _tracker.createInstance<ClassDeclarationContext>(_ctx, getState());
  enterRule(_localctx, 62, AvaLangParser::RuleClassDeclaration);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(339);
    match(AvaLangParser::T__35);
    setState(340);
    match(AvaLangParser::NAME);
    setState(342);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__36) {
      setState(341);
      classHeritage();
    }
    setState(344);
    block();
    setState(345);
    match(AvaLangParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ClassHeritageContext ------------------------------------------------------------------

AvaLangParser::ClassHeritageContext::ClassHeritageContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::ClassHeritageContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}


size_t AvaLangParser::ClassHeritageContext::getRuleIndex() const {
  return AvaLangParser::RuleClassHeritage;
}

void AvaLangParser::ClassHeritageContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterClassHeritage(this);
}

void AvaLangParser::ClassHeritageContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitClassHeritage(this);
}


std::any AvaLangParser::ClassHeritageContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitClassHeritage(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ClassHeritageContext* AvaLangParser::classHeritage() {
  ClassHeritageContext *_localctx = _tracker.createInstance<ClassHeritageContext>(_ctx, getState());
  enterRule(_localctx, 64, AvaLangParser::RuleClassHeritage);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(347);
    match(AvaLangParser::T__36);
    setState(348);
    match(AvaLangParser::NAME);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ParamListContext ------------------------------------------------------------------

AvaLangParser::ParamListContext::ParamListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::ParamContext *> AvaLangParser::ParamListContext::param() {
  return getRuleContexts<AvaLangParser::ParamContext>();
}

AvaLangParser::ParamContext* AvaLangParser::ParamListContext::param(size_t i) {
  return getRuleContext<AvaLangParser::ParamContext>(i);
}

tree::TerminalNode* AvaLangParser::ParamListContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}


size_t AvaLangParser::ParamListContext::getRuleIndex() const {
  return AvaLangParser::RuleParamList;
}

void AvaLangParser::ParamListContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterParamList(this);
}

void AvaLangParser::ParamListContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitParamList(this);
}


std::any AvaLangParser::ParamListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitParamList(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ParamListContext* AvaLangParser::paramList() {
  ParamListContext *_localctx = _tracker.createInstance<ParamListContext>(_ctx, getState());
  enterRule(_localctx, 66, AvaLangParser::RuleParamList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(350);
    param();
    setState(355);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 26, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(351);
        match(AvaLangParser::T__9);
        setState(352);
        param(); 
      }
      setState(357);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 26, _ctx);
    }
    setState(361);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__9) {
      setState(358);
      match(AvaLangParser::T__9);
      setState(359);
      match(AvaLangParser::T__37);
      setState(360);
      match(AvaLangParser::NAME);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ParamContext ------------------------------------------------------------------

AvaLangParser::ParamContext::ParamContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::ParamContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::ExprContext* AvaLangParser::ParamContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}


size_t AvaLangParser::ParamContext::getRuleIndex() const {
  return AvaLangParser::RuleParam;
}

void AvaLangParser::ParamContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterParam(this);
}

void AvaLangParser::ParamContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitParam(this);
}


std::any AvaLangParser::ParamContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitParam(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ParamContext* AvaLangParser::param() {
  ParamContext *_localctx = _tracker.createInstance<ParamContext>(_ctx, getState());
  enterRule(_localctx, 68, AvaLangParser::RuleParam);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(363);
    match(AvaLangParser::NAME);
    setState(366);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__10) {
      setState(364);
      match(AvaLangParser::T__10);
      setState(365);
      expr();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TargetListContext ------------------------------------------------------------------

AvaLangParser::TargetListContext::TargetListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::TargetContext *> AvaLangParser::TargetListContext::target() {
  return getRuleContexts<AvaLangParser::TargetContext>();
}

AvaLangParser::TargetContext* AvaLangParser::TargetListContext::target(size_t i) {
  return getRuleContext<AvaLangParser::TargetContext>(i);
}


size_t AvaLangParser::TargetListContext::getRuleIndex() const {
  return AvaLangParser::RuleTargetList;
}

void AvaLangParser::TargetListContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTargetList(this);
}

void AvaLangParser::TargetListContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTargetList(this);
}


std::any AvaLangParser::TargetListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitTargetList(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::TargetListContext* AvaLangParser::targetList() {
  TargetListContext *_localctx = _tracker.createInstance<TargetListContext>(_ctx, getState());
  enterRule(_localctx, 70, AvaLangParser::RuleTargetList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(368);
    target();
    setState(373);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__9) {
      setState(369);
      match(AvaLangParser::T__9);
      setState(370);
      target();
      setState(375);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TargetContext ------------------------------------------------------------------

AvaLangParser::TargetContext::TargetContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::TargetContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

std::vector<AvaLangParser::TrailerContext *> AvaLangParser::TargetContext::trailer() {
  return getRuleContexts<AvaLangParser::TrailerContext>();
}

AvaLangParser::TrailerContext* AvaLangParser::TargetContext::trailer(size_t i) {
  return getRuleContext<AvaLangParser::TrailerContext>(i);
}


size_t AvaLangParser::TargetContext::getRuleIndex() const {
  return AvaLangParser::RuleTarget;
}

void AvaLangParser::TargetContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTarget(this);
}

void AvaLangParser::TargetContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTarget(this);
}


std::any AvaLangParser::TargetContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitTarget(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::TargetContext* AvaLangParser::target() {
  TargetContext *_localctx = _tracker.createInstance<TargetContext>(_ctx, getState());
  enterRule(_localctx, 72, AvaLangParser::RuleTarget);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(376);
    match(AvaLangParser::NAME);
    setState(380);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 216313519602204673) != 0)) {
      setState(377);
      trailer();
      setState(382);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprListContext ------------------------------------------------------------------

AvaLangParser::ExprListContext::ExprListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::ExprContext *> AvaLangParser::ExprListContext::expr() {
  return getRuleContexts<AvaLangParser::ExprContext>();
}

AvaLangParser::ExprContext* AvaLangParser::ExprListContext::expr(size_t i) {
  return getRuleContext<AvaLangParser::ExprContext>(i);
}


size_t AvaLangParser::ExprListContext::getRuleIndex() const {
  return AvaLangParser::RuleExprList;
}

void AvaLangParser::ExprListContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExprList(this);
}

void AvaLangParser::ExprListContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExprList(this);
}


std::any AvaLangParser::ExprListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExprList(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExprListContext* AvaLangParser::exprList() {
  ExprListContext *_localctx = _tracker.createInstance<ExprListContext>(_ctx, getState());
  enterRule(_localctx, 74, AvaLangParser::RuleExprList);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(383);
    expr();
    setState(388);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 31, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(384);
        match(AvaLangParser::T__9);
        setState(385);
        expr(); 
      }
      setState(390);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 31, _ctx);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

AvaLangParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::ExprContext::getRuleIndex() const {
  return AvaLangParser::RuleExpr;
}

void AvaLangParser::ExprContext::copyFrom(ExprContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- ShortLambdaExprAltContext ------------------------------------------------------------------

AvaLangParser::ShortLambdaExprContext* AvaLangParser::ShortLambdaExprAltContext::shortLambdaExpr() {
  return getRuleContext<AvaLangParser::ShortLambdaExprContext>(0);
}

AvaLangParser::ShortLambdaExprAltContext::ShortLambdaExprAltContext(ExprContext *ctx) { copyFrom(ctx); }

void AvaLangParser::ShortLambdaExprAltContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterShortLambdaExprAlt(this);
}
void AvaLangParser::ShortLambdaExprAltContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitShortLambdaExprAlt(this);
}

std::any AvaLangParser::ShortLambdaExprAltContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitShortLambdaExprAlt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- OrExprAltContext ------------------------------------------------------------------

AvaLangParser::OrExprContext* AvaLangParser::OrExprAltContext::orExpr() {
  return getRuleContext<AvaLangParser::OrExprContext>(0);
}

AvaLangParser::OrExprAltContext::OrExprAltContext(ExprContext *ctx) { copyFrom(ctx); }

void AvaLangParser::OrExprAltContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterOrExprAlt(this);
}
void AvaLangParser::OrExprAltContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitOrExprAlt(this);
}

std::any AvaLangParser::OrExprAltContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitOrExprAlt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LambdaExprAltContext ------------------------------------------------------------------

AvaLangParser::LambdaExprContext* AvaLangParser::LambdaExprAltContext::lambdaExpr() {
  return getRuleContext<AvaLangParser::LambdaExprContext>(0);
}

AvaLangParser::LambdaExprAltContext::LambdaExprAltContext(ExprContext *ctx) { copyFrom(ctx); }

void AvaLangParser::LambdaExprAltContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLambdaExprAlt(this);
}
void AvaLangParser::LambdaExprAltContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLambdaExprAlt(this);
}

std::any AvaLangParser::LambdaExprAltContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitLambdaExprAlt(this);
  else
    return visitor->visitChildren(this);
}
AvaLangParser::ExprContext* AvaLangParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 76, AvaLangParser::RuleExpr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(394);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 32, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<AvaLangParser::ShortLambdaExprAltContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(391);
      shortLambdaExpr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<AvaLangParser::LambdaExprAltContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(392);
      lambdaExpr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<AvaLangParser::OrExprAltContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(393);
      orExpr();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ShortLambdaExprContext ------------------------------------------------------------------

AvaLangParser::ShortLambdaExprContext::ShortLambdaExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::ShortLambdaExprContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::ParamListContext* AvaLangParser::ShortLambdaExprContext::paramList() {
  return getRuleContext<AvaLangParser::ParamListContext>(0);
}


size_t AvaLangParser::ShortLambdaExprContext::getRuleIndex() const {
  return AvaLangParser::RuleShortLambdaExpr;
}

void AvaLangParser::ShortLambdaExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterShortLambdaExpr(this);
}

void AvaLangParser::ShortLambdaExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitShortLambdaExpr(this);
}


std::any AvaLangParser::ShortLambdaExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitShortLambdaExpr(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ShortLambdaExprContext* AvaLangParser::shortLambdaExpr() {
  ShortLambdaExprContext *_localctx = _tracker.createInstance<ShortLambdaExprContext>(_ctx, getState());
  enterRule(_localctx, 78, AvaLangParser::RuleShortLambdaExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(396);
    match(AvaLangParser::T__6);
    setState(398);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(397);
      paramList();
    }
    setState(400);
    match(AvaLangParser::T__7);
    setState(401);
    match(AvaLangParser::T__38);
    setState(402);
    expr();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LambdaExprContext ------------------------------------------------------------------

AvaLangParser::LambdaExprContext::LambdaExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::BlockContext* AvaLangParser::LambdaExprContext::block() {
  return getRuleContext<AvaLangParser::BlockContext>(0);
}

AvaLangParser::ParamListContext* AvaLangParser::LambdaExprContext::paramList() {
  return getRuleContext<AvaLangParser::ParamListContext>(0);
}


size_t AvaLangParser::LambdaExprContext::getRuleIndex() const {
  return AvaLangParser::RuleLambdaExpr;
}

void AvaLangParser::LambdaExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLambdaExpr(this);
}

void AvaLangParser::LambdaExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLambdaExpr(this);
}


std::any AvaLangParser::LambdaExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitLambdaExpr(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::LambdaExprContext* AvaLangParser::lambdaExpr() {
  LambdaExprContext *_localctx = _tracker.createInstance<LambdaExprContext>(_ctx, getState());
  enterRule(_localctx, 80, AvaLangParser::RuleLambdaExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(404);
    match(AvaLangParser::T__34);
    setState(405);
    match(AvaLangParser::T__6);
    setState(407);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(406);
      paramList();
    }
    setState(409);
    match(AvaLangParser::T__7);
    setState(410);
    block();
    setState(411);
    match(AvaLangParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- OrExprContext ------------------------------------------------------------------

AvaLangParser::OrExprContext::OrExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::AndExprContext *> AvaLangParser::OrExprContext::andExpr() {
  return getRuleContexts<AvaLangParser::AndExprContext>();
}

AvaLangParser::AndExprContext* AvaLangParser::OrExprContext::andExpr(size_t i) {
  return getRuleContext<AvaLangParser::AndExprContext>(i);
}


size_t AvaLangParser::OrExprContext::getRuleIndex() const {
  return AvaLangParser::RuleOrExpr;
}

void AvaLangParser::OrExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterOrExpr(this);
}

void AvaLangParser::OrExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitOrExpr(this);
}


std::any AvaLangParser::OrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitOrExpr(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::OrExprContext* AvaLangParser::orExpr() {
  OrExprContext *_localctx = _tracker.createInstance<OrExprContext>(_ctx, getState());
  enterRule(_localctx, 82, AvaLangParser::RuleOrExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(413);
    andExpr();
    setState(418);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__39) {
      setState(414);
      match(AvaLangParser::T__39);
      setState(415);
      andExpr();
      setState(420);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AndExprContext ------------------------------------------------------------------

AvaLangParser::AndExprContext::AndExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::NotExprContext *> AvaLangParser::AndExprContext::notExpr() {
  return getRuleContexts<AvaLangParser::NotExprContext>();
}

AvaLangParser::NotExprContext* AvaLangParser::AndExprContext::notExpr(size_t i) {
  return getRuleContext<AvaLangParser::NotExprContext>(i);
}


size_t AvaLangParser::AndExprContext::getRuleIndex() const {
  return AvaLangParser::RuleAndExpr;
}

void AvaLangParser::AndExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterAndExpr(this);
}

void AvaLangParser::AndExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitAndExpr(this);
}


std::any AvaLangParser::AndExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitAndExpr(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::AndExprContext* AvaLangParser::andExpr() {
  AndExprContext *_localctx = _tracker.createInstance<AndExprContext>(_ctx, getState());
  enterRule(_localctx, 84, AvaLangParser::RuleAndExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(421);
    notExpr();
    setState(426);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__40) {
      setState(422);
      match(AvaLangParser::T__40);
      setState(423);
      notExpr();
      setState(428);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- NotExprContext ------------------------------------------------------------------

AvaLangParser::NotExprContext::NotExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::NotExprContext* AvaLangParser::NotExprContext::notExpr() {
  return getRuleContext<AvaLangParser::NotExprContext>(0);
}

AvaLangParser::ComparisonContext* AvaLangParser::NotExprContext::comparison() {
  return getRuleContext<AvaLangParser::ComparisonContext>(0);
}


size_t AvaLangParser::NotExprContext::getRuleIndex() const {
  return AvaLangParser::RuleNotExpr;
}

void AvaLangParser::NotExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterNotExpr(this);
}

void AvaLangParser::NotExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitNotExpr(this);
}


std::any AvaLangParser::NotExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitNotExpr(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::NotExprContext* AvaLangParser::notExpr() {
  NotExprContext *_localctx = _tracker.createInstance<NotExprContext>(_ctx, getState());
  enterRule(_localctx, 86, AvaLangParser::RuleNotExpr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(432);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 37, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(429);
      match(AvaLangParser::T__41);
      setState(430);
      notExpr();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(431);
      comparison();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ComparisonContext ------------------------------------------------------------------

AvaLangParser::ComparisonContext::ComparisonContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::AdditiveContext *> AvaLangParser::ComparisonContext::additive() {
  return getRuleContexts<AvaLangParser::AdditiveContext>();
}

AvaLangParser::AdditiveContext* AvaLangParser::ComparisonContext::additive(size_t i) {
  return getRuleContext<AvaLangParser::AdditiveContext>(i);
}

std::vector<AvaLangParser::CompOpContext *> AvaLangParser::ComparisonContext::compOp() {
  return getRuleContexts<AvaLangParser::CompOpContext>();
}

AvaLangParser::CompOpContext* AvaLangParser::ComparisonContext::compOp(size_t i) {
  return getRuleContext<AvaLangParser::CompOpContext>(i);
}


size_t AvaLangParser::ComparisonContext::getRuleIndex() const {
  return AvaLangParser::RuleComparison;
}

void AvaLangParser::ComparisonContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterComparison(this);
}

void AvaLangParser::ComparisonContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitComparison(this);
}


std::any AvaLangParser::ComparisonContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitComparison(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ComparisonContext* AvaLangParser::comparison() {
  ComparisonContext *_localctx = _tracker.createInstance<ComparisonContext>(_ctx, getState());
  enterRule(_localctx, 88, AvaLangParser::RuleComparison);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(434);
    additive();
    setState(440);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 554153860399104) != 0)) {
      setState(435);
      compOp();
      setState(436);
      additive();
      setState(442);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CompOpContext ------------------------------------------------------------------

AvaLangParser::CompOpContext::CompOpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::CompOpContext::getRuleIndex() const {
  return AvaLangParser::RuleCompOp;
}

void AvaLangParser::CompOpContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterCompOp(this);
}

void AvaLangParser::CompOpContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitCompOp(this);
}


std::any AvaLangParser::CompOpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitCompOp(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::CompOpContext* AvaLangParser::compOp() {
  CompOpContext *_localctx = _tracker.createInstance<CompOpContext>(_ctx, getState());
  enterRule(_localctx, 90, AvaLangParser::RuleCompOp);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(443);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 554153860399104) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AdditiveContext ------------------------------------------------------------------

AvaLangParser::AdditiveContext::AdditiveContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::MultiplicativeContext *> AvaLangParser::AdditiveContext::multiplicative() {
  return getRuleContexts<AvaLangParser::MultiplicativeContext>();
}

AvaLangParser::MultiplicativeContext* AvaLangParser::AdditiveContext::multiplicative(size_t i) {
  return getRuleContext<AvaLangParser::MultiplicativeContext>(i);
}


size_t AvaLangParser::AdditiveContext::getRuleIndex() const {
  return AvaLangParser::RuleAdditive;
}

void AvaLangParser::AdditiveContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterAdditive(this);
}

void AvaLangParser::AdditiveContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitAdditive(this);
}


std::any AvaLangParser::AdditiveContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitAdditive(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::AdditiveContext* AvaLangParser::additive() {
  AdditiveContext *_localctx = _tracker.createInstance<AdditiveContext>(_ctx, getState());
  enterRule(_localctx, 92, AvaLangParser::RuleAdditive);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(445);
    multiplicative();
    setState(450);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 39, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(446);
        _la = _input->LA(1);
        if (!(_la == AvaLangParser::T__48

        || _la == AvaLangParser::T__49)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(447);
        multiplicative(); 
      }
      setState(452);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 39, _ctx);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- MultiplicativeContext ------------------------------------------------------------------

AvaLangParser::MultiplicativeContext::MultiplicativeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::UnaryContext *> AvaLangParser::MultiplicativeContext::unary() {
  return getRuleContexts<AvaLangParser::UnaryContext>();
}

AvaLangParser::UnaryContext* AvaLangParser::MultiplicativeContext::unary(size_t i) {
  return getRuleContext<AvaLangParser::UnaryContext>(i);
}

std::vector<tree::TerminalNode *> AvaLangParser::MultiplicativeContext::IDIV() {
  return getTokens(AvaLangParser::IDIV);
}

tree::TerminalNode* AvaLangParser::MultiplicativeContext::IDIV(size_t i) {
  return getToken(AvaLangParser::IDIV, i);
}


size_t AvaLangParser::MultiplicativeContext::getRuleIndex() const {
  return AvaLangParser::RuleMultiplicative;
}

void AvaLangParser::MultiplicativeContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterMultiplicative(this);
}

void AvaLangParser::MultiplicativeContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitMultiplicative(this);
}


std::any AvaLangParser::MultiplicativeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitMultiplicative(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::MultiplicativeContext* AvaLangParser::multiplicative() {
  MultiplicativeContext *_localctx = _tracker.createInstance<MultiplicativeContext>(_ctx, getState());
  enterRule(_localctx, 94, AvaLangParser::RuleMultiplicative);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(453);
    unary();
    setState(458);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 38) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 38)) & 134242305) != 0)) {
      setState(454);
      _la = _input->LA(1);
      if (!(((((_la - 38) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 38)) & 134242305) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(455);
      unary();
      setState(460);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- UnaryContext ------------------------------------------------------------------

AvaLangParser::UnaryContext::UnaryContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::UnaryContext* AvaLangParser::UnaryContext::unary() {
  return getRuleContext<AvaLangParser::UnaryContext>(0);
}

tree::TerminalNode* AvaLangParser::UnaryContext::INC() {
  return getToken(AvaLangParser::INC, 0);
}

tree::TerminalNode* AvaLangParser::UnaryContext::DEC() {
  return getToken(AvaLangParser::DEC, 0);
}

AvaLangParser::PowerContext* AvaLangParser::UnaryContext::power() {
  return getRuleContext<AvaLangParser::PowerContext>(0);
}


size_t AvaLangParser::UnaryContext::getRuleIndex() const {
  return AvaLangParser::RuleUnary;
}

void AvaLangParser::UnaryContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterUnary(this);
}

void AvaLangParser::UnaryContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitUnary(this);
}


std::any AvaLangParser::UnaryContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitUnary(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::UnaryContext* AvaLangParser::unary() {
  UnaryContext *_localctx = _tracker.createInstance<UnaryContext>(_ctx, getState());
  enterRule(_localctx, 96, AvaLangParser::RuleUnary);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(464);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case AvaLangParser::T__41:
      case AvaLangParser::T__49:
      case AvaLangParser::INC:
      case AvaLangParser::DEC: {
        enterOuterAlt(_localctx, 1);
        setState(461);
        _la = _input->LA(1);
        if (!(((((_la - 42) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 42)) & 6291713) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(462);
        unary();
        break;
      }

      case AvaLangParser::T__6:
      case AvaLangParser::T__53:
      case AvaLangParser::T__55:
      case AvaLangParser::T__56:
      case AvaLangParser::T__57:
      case AvaLangParser::T__58:
      case AvaLangParser::T__59:
      case AvaLangParser::NAME:
      case AvaLangParser::NUMBER:
      case AvaLangParser::STRING:
      case AvaLangParser::FSTRING: {
        enterOuterAlt(_localctx, 2);
        setState(463);
        power();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PowerContext ------------------------------------------------------------------

AvaLangParser::PowerContext::PowerContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::PostfixContext* AvaLangParser::PowerContext::postfix() {
  return getRuleContext<AvaLangParser::PostfixContext>(0);
}

AvaLangParser::UnaryContext* AvaLangParser::PowerContext::unary() {
  return getRuleContext<AvaLangParser::UnaryContext>(0);
}


size_t AvaLangParser::PowerContext::getRuleIndex() const {
  return AvaLangParser::RulePower;
}

void AvaLangParser::PowerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterPower(this);
}

void AvaLangParser::PowerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitPower(this);
}


std::any AvaLangParser::PowerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitPower(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::PowerContext* AvaLangParser::power() {
  PowerContext *_localctx = _tracker.createInstance<PowerContext>(_ctx, getState());
  enterRule(_localctx, 98, AvaLangParser::RulePower);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(466);
    postfix();
    setState(469);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__52) {
      setState(467);
      match(AvaLangParser::T__52);
      setState(468);
      unary();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PostfixContext ------------------------------------------------------------------

AvaLangParser::PostfixContext::PostfixContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::PrimaryContext* AvaLangParser::PostfixContext::primary() {
  return getRuleContext<AvaLangParser::PrimaryContext>(0);
}

std::vector<AvaLangParser::TrailerContext *> AvaLangParser::PostfixContext::trailer() {
  return getRuleContexts<AvaLangParser::TrailerContext>();
}

AvaLangParser::TrailerContext* AvaLangParser::PostfixContext::trailer(size_t i) {
  return getRuleContext<AvaLangParser::TrailerContext>(i);
}


size_t AvaLangParser::PostfixContext::getRuleIndex() const {
  return AvaLangParser::RulePostfix;
}

void AvaLangParser::PostfixContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterPostfix(this);
}

void AvaLangParser::PostfixContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitPostfix(this);
}


std::any AvaLangParser::PostfixContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitPostfix(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::PostfixContext* AvaLangParser::postfix() {
  PostfixContext *_localctx = _tracker.createInstance<PostfixContext>(_ctx, getState());
  enterRule(_localctx, 100, AvaLangParser::RulePostfix);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(471);
    primary();
    setState(475);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(472);
        trailer(); 
      }
      setState(477);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TrailerContext ------------------------------------------------------------------

AvaLangParser::TrailerContext::TrailerContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::TrailerContext::getRuleIndex() const {
  return AvaLangParser::RuleTrailer;
}

void AvaLangParser::TrailerContext::copyFrom(TrailerContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- SliceTrailerContext ------------------------------------------------------------------

AvaLangParser::SliceRangeContext* AvaLangParser::SliceTrailerContext::sliceRange() {
  return getRuleContext<AvaLangParser::SliceRangeContext>(0);
}

AvaLangParser::SliceTrailerContext::SliceTrailerContext(TrailerContext *ctx) { copyFrom(ctx); }

void AvaLangParser::SliceTrailerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterSliceTrailer(this);
}
void AvaLangParser::SliceTrailerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitSliceTrailer(this);
}

std::any AvaLangParser::SliceTrailerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitSliceTrailer(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AttrTrailerContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::AttrTrailerContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::AttrTrailerContext::AttrTrailerContext(TrailerContext *ctx) { copyFrom(ctx); }

void AvaLangParser::AttrTrailerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterAttrTrailer(this);
}
void AvaLangParser::AttrTrailerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitAttrTrailer(this);
}

std::any AvaLangParser::AttrTrailerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitAttrTrailer(this);
  else
    return visitor->visitChildren(this);
}
//----------------- DecTrailerContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::DecTrailerContext::DEC() {
  return getToken(AvaLangParser::DEC, 0);
}

AvaLangParser::DecTrailerContext::DecTrailerContext(TrailerContext *ctx) { copyFrom(ctx); }

void AvaLangParser::DecTrailerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDecTrailer(this);
}
void AvaLangParser::DecTrailerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDecTrailer(this);
}

std::any AvaLangParser::DecTrailerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitDecTrailer(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IncTrailerContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::IncTrailerContext::INC() {
  return getToken(AvaLangParser::INC, 0);
}

AvaLangParser::IncTrailerContext::IncTrailerContext(TrailerContext *ctx) { copyFrom(ctx); }

void AvaLangParser::IncTrailerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterIncTrailer(this);
}
void AvaLangParser::IncTrailerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitIncTrailer(this);
}

std::any AvaLangParser::IncTrailerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitIncTrailer(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IndexTrailerContext ------------------------------------------------------------------

AvaLangParser::ExprContext* AvaLangParser::IndexTrailerContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::IndexTrailerContext::IndexTrailerContext(TrailerContext *ctx) { copyFrom(ctx); }

void AvaLangParser::IndexTrailerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterIndexTrailer(this);
}
void AvaLangParser::IndexTrailerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitIndexTrailer(this);
}

std::any AvaLangParser::IndexTrailerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitIndexTrailer(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallTrailerContext ------------------------------------------------------------------

AvaLangParser::ArgListContext* AvaLangParser::CallTrailerContext::argList() {
  return getRuleContext<AvaLangParser::ArgListContext>(0);
}

AvaLangParser::CallTrailerContext::CallTrailerContext(TrailerContext *ctx) { copyFrom(ctx); }

void AvaLangParser::CallTrailerContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterCallTrailer(this);
}
void AvaLangParser::CallTrailerContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitCallTrailer(this);
}

std::any AvaLangParser::CallTrailerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitCallTrailer(this);
  else
    return visitor->visitChildren(this);
}
AvaLangParser::TrailerContext* AvaLangParser::trailer() {
  TrailerContext *_localctx = _tracker.createInstance<TrailerContext>(_ctx, getState());
  enterRule(_localctx, 102, AvaLangParser::RuleTrailer);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(495);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 45, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<AvaLangParser::AttrTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(478);
      match(AvaLangParser::T__22);
      setState(479);
      match(AvaLangParser::NAME);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<AvaLangParser::IndexTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(480);
      match(AvaLangParser::T__53);
      setState(481);
      expr();
      setState(482);
      match(AvaLangParser::T__54);
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<AvaLangParser::SliceTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(484);
      match(AvaLangParser::T__53);
      setState(485);
      sliceRange();
      setState(486);
      match(AvaLangParser::T__54);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<AvaLangParser::CallTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(488);
      match(AvaLangParser::T__6);
      setState(490);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 7) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
        setState(489);
        argList();
      }
      setState(492);
      match(AvaLangParser::T__7);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<AvaLangParser::IncTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(493);
      match(AvaLangParser::INC);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<AvaLangParser::DecTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(494);
      match(AvaLangParser::DEC);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SliceRangeContext ------------------------------------------------------------------

AvaLangParser::SliceRangeContext::SliceRangeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::ExprContext *> AvaLangParser::SliceRangeContext::expr() {
  return getRuleContexts<AvaLangParser::ExprContext>();
}

AvaLangParser::ExprContext* AvaLangParser::SliceRangeContext::expr(size_t i) {
  return getRuleContext<AvaLangParser::ExprContext>(i);
}


size_t AvaLangParser::SliceRangeContext::getRuleIndex() const {
  return AvaLangParser::RuleSliceRange;
}

void AvaLangParser::SliceRangeContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterSliceRange(this);
}

void AvaLangParser::SliceRangeContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitSliceRange(this);
}


std::any AvaLangParser::SliceRangeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitSliceRange(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::SliceRangeContext* AvaLangParser::sliceRange() {
  SliceRangeContext *_localctx = _tracker.createInstance<SliceRangeContext>(_ctx, getState());
  enterRule(_localctx, 104, AvaLangParser::RuleSliceRange);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(498);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
      setState(497);
      expr();
    }
    setState(500);
    match(AvaLangParser::T__36);
    setState(502);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
      setState(501);
      expr();
    }
    setState(508);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__36) {
      setState(504);
      match(AvaLangParser::T__36);
      setState(506);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 7) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
        setState(505);
        expr();
      }
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ArgListContext ------------------------------------------------------------------

AvaLangParser::ArgListContext::ArgListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::ArgContext *> AvaLangParser::ArgListContext::arg() {
  return getRuleContexts<AvaLangParser::ArgContext>();
}

AvaLangParser::ArgContext* AvaLangParser::ArgListContext::arg(size_t i) {
  return getRuleContext<AvaLangParser::ArgContext>(i);
}


size_t AvaLangParser::ArgListContext::getRuleIndex() const {
  return AvaLangParser::RuleArgList;
}

void AvaLangParser::ArgListContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterArgList(this);
}

void AvaLangParser::ArgListContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitArgList(this);
}


std::any AvaLangParser::ArgListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitArgList(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ArgListContext* AvaLangParser::argList() {
  ArgListContext *_localctx = _tracker.createInstance<ArgListContext>(_ctx, getState());
  enterRule(_localctx, 106, AvaLangParser::RuleArgList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(510);
    arg();
    setState(515);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__9) {
      setState(511);
      match(AvaLangParser::T__9);
      setState(512);
      arg();
      setState(517);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ArgContext ------------------------------------------------------------------

AvaLangParser::ArgContext::ArgContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::ArgContext::getRuleIndex() const {
  return AvaLangParser::RuleArg;
}

void AvaLangParser::ArgContext::copyFrom(ArgContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- NamedArgContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::NamedArgContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::ExprContext* AvaLangParser::NamedArgContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::NamedArgContext::NamedArgContext(ArgContext *ctx) { copyFrom(ctx); }

void AvaLangParser::NamedArgContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterNamedArg(this);
}
void AvaLangParser::NamedArgContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitNamedArg(this);
}

std::any AvaLangParser::NamedArgContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitNamedArg(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PositionalArgContext ------------------------------------------------------------------

AvaLangParser::ExprContext* AvaLangParser::PositionalArgContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::PositionalArgContext::PositionalArgContext(ArgContext *ctx) { copyFrom(ctx); }

void AvaLangParser::PositionalArgContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterPositionalArg(this);
}
void AvaLangParser::PositionalArgContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitPositionalArg(this);
}

std::any AvaLangParser::PositionalArgContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitPositionalArg(this);
  else
    return visitor->visitChildren(this);
}
AvaLangParser::ArgContext* AvaLangParser::arg() {
  ArgContext *_localctx = _tracker.createInstance<ArgContext>(_ctx, getState());
  enterRule(_localctx, 108, AvaLangParser::RuleArg);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(522);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 51, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<AvaLangParser::NamedArgContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(518);
      match(AvaLangParser::NAME);
      setState(519);
      match(AvaLangParser::T__10);
      setState(520);
      expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<AvaLangParser::PositionalArgContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(521);
      expr();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PrimaryContext ------------------------------------------------------------------

AvaLangParser::PrimaryContext::PrimaryContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t AvaLangParser::PrimaryContext::getRuleIndex() const {
  return AvaLangParser::RulePrimary;
}

void AvaLangParser::PrimaryContext::copyFrom(PrimaryContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- NameAtomContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::NameAtomContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::NameAtomContext::NameAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::NameAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterNameAtom(this);
}
void AvaLangParser::NameAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitNameAtom(this);
}

std::any AvaLangParser::NameAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitNameAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- DictAtomContext ------------------------------------------------------------------

AvaLangParser::DictLiteralContext* AvaLangParser::DictAtomContext::dictLiteral() {
  return getRuleContext<AvaLangParser::DictLiteralContext>(0);
}

AvaLangParser::DictAtomContext::DictAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::DictAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDictAtom(this);
}
void AvaLangParser::DictAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDictAtom(this);
}

std::any AvaLangParser::DictAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitDictAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- GroupAtomContext ------------------------------------------------------------------

AvaLangParser::ExprContext* AvaLangParser::GroupAtomContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

AvaLangParser::GroupAtomContext::GroupAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::GroupAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterGroupAtom(this);
}
void AvaLangParser::GroupAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitGroupAtom(this);
}

std::any AvaLangParser::GroupAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitGroupAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ListAtomContext ------------------------------------------------------------------

AvaLangParser::ListLiteralContext* AvaLangParser::ListAtomContext::listLiteral() {
  return getRuleContext<AvaLangParser::ListLiteralContext>(0);
}

AvaLangParser::ListAtomContext::ListAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::ListAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterListAtom(this);
}
void AvaLangParser::ListAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitListAtom(this);
}

std::any AvaLangParser::ListAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitListAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StringAtomContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::StringAtomContext::STRING() {
  return getToken(AvaLangParser::STRING, 0);
}

AvaLangParser::StringAtomContext::StringAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::StringAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterStringAtom(this);
}
void AvaLangParser::StringAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitStringAtom(this);
}

std::any AvaLangParser::StringAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitStringAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- FalseAtomContext ------------------------------------------------------------------

AvaLangParser::FalseAtomContext::FalseAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::FalseAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFalseAtom(this);
}
void AvaLangParser::FalseAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFalseAtom(this);
}

std::any AvaLangParser::FalseAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitFalseAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- NilAtomContext ------------------------------------------------------------------

AvaLangParser::NilAtomContext::NilAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::NilAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterNilAtom(this);
}
void AvaLangParser::NilAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitNilAtom(this);
}

std::any AvaLangParser::NilAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitNilAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BaseAtomContext ------------------------------------------------------------------

AvaLangParser::ArgListContext* AvaLangParser::BaseAtomContext::argList() {
  return getRuleContext<AvaLangParser::ArgListContext>(0);
}

AvaLangParser::BaseAtomContext::BaseAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::BaseAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBaseAtom(this);
}
void AvaLangParser::BaseAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBaseAtom(this);
}

std::any AvaLangParser::BaseAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitBaseAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- FstringAtomContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::FstringAtomContext::FSTRING() {
  return getToken(AvaLangParser::FSTRING, 0);
}

AvaLangParser::FstringAtomContext::FstringAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::FstringAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFstringAtom(this);
}
void AvaLangParser::FstringAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFstringAtom(this);
}

std::any AvaLangParser::FstringAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitFstringAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- TrueAtomContext ------------------------------------------------------------------

AvaLangParser::TrueAtomContext::TrueAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::TrueAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTrueAtom(this);
}
void AvaLangParser::TrueAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTrueAtom(this);
}

std::any AvaLangParser::TrueAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitTrueAtom(this);
  else
    return visitor->visitChildren(this);
}
//----------------- NumberAtomContext ------------------------------------------------------------------

tree::TerminalNode* AvaLangParser::NumberAtomContext::NUMBER() {
  return getToken(AvaLangParser::NUMBER, 0);
}

AvaLangParser::NumberAtomContext::NumberAtomContext(PrimaryContext *ctx) { copyFrom(ctx); }

void AvaLangParser::NumberAtomContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterNumberAtom(this);
}
void AvaLangParser::NumberAtomContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitNumberAtom(this);
}

std::any AvaLangParser::NumberAtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitNumberAtom(this);
  else
    return visitor->visitChildren(this);
}
AvaLangParser::PrimaryContext* AvaLangParser::primary() {
  PrimaryContext *_localctx = _tracker.createInstance<PrimaryContext>(_ctx, getState());
  enterRule(_localctx, 110, AvaLangParser::RulePrimary);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(543);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case AvaLangParser::NAME: {
        _localctx = _tracker.createInstance<AvaLangParser::NameAtomContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(524);
        match(AvaLangParser::NAME);
        break;
      }

      case AvaLangParser::NUMBER: {
        _localctx = _tracker.createInstance<AvaLangParser::NumberAtomContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(525);
        match(AvaLangParser::NUMBER);
        break;
      }

      case AvaLangParser::STRING: {
        _localctx = _tracker.createInstance<AvaLangParser::StringAtomContext>(_localctx);
        enterOuterAlt(_localctx, 3);
        setState(526);
        match(AvaLangParser::STRING);
        break;
      }

      case AvaLangParser::FSTRING: {
        _localctx = _tracker.createInstance<AvaLangParser::FstringAtomContext>(_localctx);
        enterOuterAlt(_localctx, 4);
        setState(527);
        match(AvaLangParser::FSTRING);
        break;
      }

      case AvaLangParser::T__55: {
        _localctx = _tracker.createInstance<AvaLangParser::TrueAtomContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(528);
        match(AvaLangParser::T__55);
        break;
      }

      case AvaLangParser::T__56: {
        _localctx = _tracker.createInstance<AvaLangParser::FalseAtomContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(529);
        match(AvaLangParser::T__56);
        break;
      }

      case AvaLangParser::T__57: {
        _localctx = _tracker.createInstance<AvaLangParser::NilAtomContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(530);
        match(AvaLangParser::T__57);
        break;
      }

      case AvaLangParser::T__53: {
        _localctx = _tracker.createInstance<AvaLangParser::ListAtomContext>(_localctx);
        enterOuterAlt(_localctx, 8);
        setState(531);
        listLiteral();
        break;
      }

      case AvaLangParser::T__59: {
        _localctx = _tracker.createInstance<AvaLangParser::DictAtomContext>(_localctx);
        enterOuterAlt(_localctx, 9);
        setState(532);
        dictLiteral();
        break;
      }

      case AvaLangParser::T__6: {
        _localctx = _tracker.createInstance<AvaLangParser::GroupAtomContext>(_localctx);
        enterOuterAlt(_localctx, 10);
        setState(533);
        match(AvaLangParser::T__6);
        setState(534);
        expr();
        setState(535);
        match(AvaLangParser::T__7);
        break;
      }

      case AvaLangParser::T__58: {
        _localctx = _tracker.createInstance<AvaLangParser::BaseAtomContext>(_localctx);
        enterOuterAlt(_localctx, 11);
        setState(537);
        match(AvaLangParser::T__58);
        setState(538);
        match(AvaLangParser::T__6);
        setState(540);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (((((_la - 7) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
          setState(539);
          argList();
        }
        setState(542);
        match(AvaLangParser::T__7);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ListLiteralContext ------------------------------------------------------------------

AvaLangParser::ListLiteralContext::ListLiteralContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::ExprContext *> AvaLangParser::ListLiteralContext::expr() {
  return getRuleContexts<AvaLangParser::ExprContext>();
}

AvaLangParser::ExprContext* AvaLangParser::ListLiteralContext::expr(size_t i) {
  return getRuleContext<AvaLangParser::ExprContext>(i);
}


size_t AvaLangParser::ListLiteralContext::getRuleIndex() const {
  return AvaLangParser::RuleListLiteral;
}

void AvaLangParser::ListLiteralContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterListLiteral(this);
}

void AvaLangParser::ListLiteralContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitListLiteral(this);
}


std::any AvaLangParser::ListLiteralContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitListLiteral(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ListLiteralContext* AvaLangParser::listLiteral() {
  ListLiteralContext *_localctx = _tracker.createInstance<ListLiteralContext>(_ctx, getState());
  enterRule(_localctx, 112, AvaLangParser::RuleListLiteral);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(545);
    match(AvaLangParser::T__53);
    setState(557);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 8880685083430748161) != 0)) {
      setState(546);
      expr();
      setState(551);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 54, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(547);
          match(AvaLangParser::T__9);
          setState(548);
          expr(); 
        }
        setState(553);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 54, _ctx);
      }
      setState(555);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == AvaLangParser::T__9) {
        setState(554);
        match(AvaLangParser::T__9);
      }
    }
    setState(559);
    match(AvaLangParser::T__54);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DictLiteralContext ------------------------------------------------------------------

AvaLangParser::DictLiteralContext::DictLiteralContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::DictEntryContext *> AvaLangParser::DictLiteralContext::dictEntry() {
  return getRuleContexts<AvaLangParser::DictEntryContext>();
}

AvaLangParser::DictEntryContext* AvaLangParser::DictLiteralContext::dictEntry(size_t i) {
  return getRuleContext<AvaLangParser::DictEntryContext>(i);
}


size_t AvaLangParser::DictLiteralContext::getRuleIndex() const {
  return AvaLangParser::RuleDictLiteral;
}

void AvaLangParser::DictLiteralContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDictLiteral(this);
}

void AvaLangParser::DictLiteralContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDictLiteral(this);
}


std::any AvaLangParser::DictLiteralContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitDictLiteral(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::DictLiteralContext* AvaLangParser::dictLiteral() {
  DictLiteralContext *_localctx = _tracker.createInstance<DictLiteralContext>(_ctx, getState());
  enterRule(_localctx, 114, AvaLangParser::RuleDictLiteral);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(561);
    match(AvaLangParser::T__59);
    setState(573);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME

    || _la == AvaLangParser::STRING) {
      setState(562);
      dictEntry();
      setState(567);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 57, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(563);
          match(AvaLangParser::T__9);
          setState(564);
          dictEntry(); 
        }
        setState(569);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 57, _ctx);
      }
      setState(571);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == AvaLangParser::T__9) {
        setState(570);
        match(AvaLangParser::T__9);
      }
    }
    setState(575);
    match(AvaLangParser::T__60);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DictEntryContext ------------------------------------------------------------------

AvaLangParser::DictEntryContext::DictEntryContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

AvaLangParser::ExprContext* AvaLangParser::DictEntryContext::expr() {
  return getRuleContext<AvaLangParser::ExprContext>(0);
}

tree::TerminalNode* AvaLangParser::DictEntryContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

tree::TerminalNode* AvaLangParser::DictEntryContext::STRING() {
  return getToken(AvaLangParser::STRING, 0);
}


size_t AvaLangParser::DictEntryContext::getRuleIndex() const {
  return AvaLangParser::RuleDictEntry;
}

void AvaLangParser::DictEntryContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterDictEntry(this);
}

void AvaLangParser::DictEntryContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitDictEntry(this);
}


std::any AvaLangParser::DictEntryContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitDictEntry(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::DictEntryContext* AvaLangParser::dictEntry() {
  DictEntryContext *_localctx = _tracker.createInstance<DictEntryContext>(_ctx, getState());
  enterRule(_localctx, 116, AvaLangParser::RuleDictEntry);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(577);
    _la = _input->LA(1);
    if (!(_la == AvaLangParser::NAME

    || _la == AvaLangParser::STRING)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(578);
    match(AvaLangParser::T__36);
    setState(579);
    expr();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

void AvaLangParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  avalangParserInitialize();
#else
  ::antlr4::internal::call_once(avalangParserOnceFlag, avalangParserInitialize);
#endif
}
