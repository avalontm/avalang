
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
      "externStatement", "externFuncDeclaration", "externParamList", "externParam", 
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
      "':'", "'extern'", "'*'", "'=>'", "'or'", "'and'", "'not'", "'=='", 
      "'!='", "'<'", "'>'", "'<='", "'>='", "'+'", "'-'", "'/'", "'%'", 
      "'**'", "'['", "']'", "'true'", "'false'", "'nil'", "'base'", "'{'", 
      "'}'", "'ava'", "'++'", "'--'", "'//'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
      "", "", "", "", "", "", "", "", "", "", "", "", "AVA_LANG", "INC", 
      "DEC", "IDIV", "NAME", "NUMBER", "STRING", "FSTRING", "NEWLINE", "COMMENT", 
      "WS", "LINE_JOIN"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,74,637,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
  	21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
  	28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
  	35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
  	42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
  	49,2,50,7,50,2,51,7,51,2,52,7,52,2,53,7,53,2,54,7,54,2,55,7,55,2,56,7,
  	56,2,57,7,57,2,58,7,58,2,59,7,59,2,60,7,60,2,61,7,61,2,62,7,62,1,0,1,
  	0,5,0,129,8,0,10,0,12,0,132,9,0,1,1,1,1,5,1,136,8,1,10,1,12,1,139,9,1,
  	1,2,1,2,3,2,143,8,2,1,3,1,3,1,3,1,3,4,3,149,8,3,11,3,12,3,150,1,4,1,4,
  	1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,3,4,167,8,4,1,5,4,5,170,
  	8,5,11,5,12,5,171,1,5,1,5,1,6,1,6,1,6,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,
  	3,7,187,8,7,1,8,1,8,1,9,4,9,192,8,9,11,9,12,9,193,1,9,1,9,1,10,1,10,1,
  	10,4,10,201,8,10,11,10,12,10,202,1,10,3,10,206,8,10,1,10,1,10,1,10,1,
  	10,1,10,1,10,1,10,3,10,215,8,10,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,
  	11,1,11,1,11,3,11,227,8,11,1,12,1,12,1,12,1,13,1,13,1,13,4,13,235,8,13,
  	11,13,12,13,236,1,14,1,14,1,14,1,14,1,15,1,15,1,15,1,15,1,16,1,16,1,17,
  	1,17,3,17,251,8,17,1,18,1,18,1,19,1,19,1,20,1,20,1,21,1,21,1,21,1,21,
  	5,21,263,8,21,10,21,12,21,266,9,21,1,21,1,21,3,21,270,8,21,1,22,1,22,
  	1,22,1,23,1,23,1,23,1,24,1,24,3,24,280,8,24,1,25,1,25,1,25,1,25,1,25,
  	5,25,287,8,25,10,25,12,25,290,9,25,1,25,3,25,293,8,25,1,25,1,25,1,26,
  	1,26,1,26,1,26,1,26,1,27,1,27,1,27,1,28,1,28,1,28,1,28,1,28,1,28,1,28,
  	1,28,1,28,1,28,1,28,1,28,3,28,317,8,28,1,29,1,29,1,29,1,29,1,29,1,29,
  	1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,3,29,337,
  	8,29,1,30,1,30,1,30,1,30,3,30,343,8,30,1,30,1,30,1,30,1,30,1,31,1,31,
  	1,31,3,31,352,8,31,1,31,1,31,1,31,1,32,1,32,1,32,1,33,1,33,1,33,1,33,
  	1,33,5,33,365,8,33,10,33,12,33,368,9,33,1,33,5,33,371,8,33,10,33,12,33,
  	374,9,33,1,33,1,33,1,34,1,34,1,34,1,34,3,34,382,8,34,1,34,1,34,5,34,386,
  	8,34,10,34,12,34,389,9,34,1,35,1,35,1,35,5,35,394,8,35,10,35,12,35,397,
  	9,35,1,35,1,35,1,35,3,35,402,8,35,1,36,1,36,1,37,1,37,1,37,5,37,409,8,
  	37,10,37,12,37,412,9,37,1,37,1,37,1,37,3,37,417,8,37,1,38,1,38,1,38,3,
  	38,422,8,38,1,39,1,39,1,39,5,39,427,8,39,10,39,12,39,430,9,39,1,40,1,
  	40,5,40,434,8,40,10,40,12,40,437,9,40,1,41,1,41,1,41,5,41,442,8,41,10,
  	41,12,41,445,9,41,1,42,1,42,1,42,3,42,450,8,42,1,43,1,43,3,43,454,8,43,
  	1,43,1,43,1,43,1,43,1,44,1,44,1,44,3,44,463,8,44,1,44,1,44,1,44,1,44,
  	1,45,1,45,1,45,5,45,472,8,45,10,45,12,45,475,9,45,1,46,1,46,1,46,5,46,
  	480,8,46,10,46,12,46,483,9,46,1,47,1,47,1,47,3,47,488,8,47,1,48,1,48,
  	1,48,1,48,5,48,494,8,48,10,48,12,48,497,9,48,1,49,1,49,1,50,1,50,1,50,
  	5,50,504,8,50,10,50,12,50,507,9,50,1,51,1,51,1,51,5,51,512,8,51,10,51,
  	12,51,515,9,51,1,52,1,52,1,52,3,52,520,8,52,1,53,1,53,1,53,3,53,525,8,
  	53,1,54,1,54,5,54,529,8,54,10,54,12,54,532,9,54,1,55,1,55,1,55,1,55,1,
  	55,1,55,1,55,1,55,1,55,1,55,1,55,1,55,3,55,546,8,55,1,55,1,55,1,55,3,
  	55,551,8,55,1,56,3,56,554,8,56,1,56,1,56,3,56,558,8,56,1,56,1,56,3,56,
  	562,8,56,3,56,564,8,56,1,57,1,57,1,57,5,57,569,8,57,10,57,12,57,572,9,
  	57,1,58,1,58,1,58,1,58,3,58,578,8,58,1,59,1,59,1,59,1,59,1,59,1,59,1,
  	59,1,59,1,59,1,59,1,59,1,59,1,59,1,59,1,59,1,59,3,59,596,8,59,1,59,3,
  	59,599,8,59,1,60,1,60,1,60,1,60,5,60,605,8,60,10,60,12,60,608,9,60,1,
  	60,3,60,611,8,60,3,60,613,8,60,1,60,1,60,1,61,1,61,1,61,1,61,5,61,621,
  	8,61,10,61,12,61,624,9,61,1,61,3,61,627,8,61,3,61,629,8,61,1,61,1,61,
  	1,62,1,62,1,62,1,62,1,62,0,0,63,0,2,4,6,8,10,12,14,16,18,20,22,24,26,
  	28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,
  	74,76,78,80,82,84,86,88,90,92,94,96,98,100,102,104,106,108,110,112,114,
  	116,118,120,122,124,0,8,1,0,64,65,1,0,2,3,1,0,12,17,1,0,44,49,1,0,50,
  	51,3,0,39,39,52,53,66,66,3,0,43,43,51,51,64,65,2,0,67,67,69,69,671,0,
  	130,1,0,0,0,2,137,1,0,0,0,4,142,1,0,0,0,6,144,1,0,0,0,8,166,1,0,0,0,10,
  	169,1,0,0,0,12,175,1,0,0,0,14,186,1,0,0,0,16,188,1,0,0,0,18,191,1,0,0,
  	0,20,214,1,0,0,0,22,226,1,0,0,0,24,228,1,0,0,0,26,231,1,0,0,0,28,238,
  	1,0,0,0,30,242,1,0,0,0,32,246,1,0,0,0,34,248,1,0,0,0,36,252,1,0,0,0,38,
  	254,1,0,0,0,40,256,1,0,0,0,42,258,1,0,0,0,44,271,1,0,0,0,46,274,1,0,0,
  	0,48,277,1,0,0,0,50,281,1,0,0,0,52,296,1,0,0,0,54,301,1,0,0,0,56,316,
  	1,0,0,0,58,336,1,0,0,0,60,338,1,0,0,0,62,348,1,0,0,0,64,356,1,0,0,0,66,
  	359,1,0,0,0,68,377,1,0,0,0,70,390,1,0,0,0,72,403,1,0,0,0,74,405,1,0,0,
  	0,76,418,1,0,0,0,78,423,1,0,0,0,80,431,1,0,0,0,82,438,1,0,0,0,84,449,
  	1,0,0,0,86,451,1,0,0,0,88,459,1,0,0,0,90,468,1,0,0,0,92,476,1,0,0,0,94,
  	487,1,0,0,0,96,489,1,0,0,0,98,498,1,0,0,0,100,500,1,0,0,0,102,508,1,0,
  	0,0,104,519,1,0,0,0,106,521,1,0,0,0,108,526,1,0,0,0,110,550,1,0,0,0,112,
  	553,1,0,0,0,114,565,1,0,0,0,116,577,1,0,0,0,118,598,1,0,0,0,120,600,1,
  	0,0,0,122,616,1,0,0,0,124,632,1,0,0,0,126,129,3,4,2,0,127,129,5,71,0,
  	0,128,126,1,0,0,0,128,127,1,0,0,0,129,132,1,0,0,0,130,128,1,0,0,0,130,
  	131,1,0,0,0,131,1,1,0,0,0,132,130,1,0,0,0,133,136,3,4,2,0,134,136,5,71,
  	0,0,135,133,1,0,0,0,135,134,1,0,0,0,136,139,1,0,0,0,137,135,1,0,0,0,137,
  	138,1,0,0,0,138,3,1,0,0,0,139,137,1,0,0,0,140,143,3,6,3,0,141,143,3,14,
  	7,0,142,140,1,0,0,0,142,141,1,0,0,0,143,5,1,0,0,0,144,148,3,8,4,0,145,
  	149,5,71,0,0,146,147,5,1,0,0,147,149,5,71,0,0,148,145,1,0,0,0,148,146,
  	1,0,0,0,149,150,1,0,0,0,150,148,1,0,0,0,150,151,1,0,0,0,151,7,1,0,0,0,
  	152,167,3,28,14,0,153,167,3,26,13,0,154,167,3,30,15,0,155,167,3,32,16,
  	0,156,167,3,34,17,0,157,167,3,36,18,0,158,167,3,38,19,0,159,167,3,40,
  	20,0,160,167,3,42,21,0,161,167,3,44,22,0,162,167,3,46,23,0,163,167,3,
  	48,24,0,164,167,3,12,6,0,165,167,3,10,5,0,166,152,1,0,0,0,166,153,1,0,
  	0,0,166,154,1,0,0,0,166,155,1,0,0,0,166,156,1,0,0,0,166,157,1,0,0,0,166,
  	158,1,0,0,0,166,159,1,0,0,0,166,160,1,0,0,0,166,161,1,0,0,0,166,162,1,
  	0,0,0,166,163,1,0,0,0,166,164,1,0,0,0,166,165,1,0,0,0,167,9,1,0,0,0,168,
  	170,3,16,8,0,169,168,1,0,0,0,170,171,1,0,0,0,171,169,1,0,0,0,171,172,
  	1,0,0,0,172,173,1,0,0,0,173,174,3,28,14,0,174,11,1,0,0,0,175,176,7,0,
  	0,0,176,177,3,80,40,0,177,13,1,0,0,0,178,187,3,50,25,0,179,187,3,56,28,
  	0,180,187,3,58,29,0,181,187,3,60,30,0,182,187,3,62,31,0,183,187,3,20,
  	10,0,184,187,3,18,9,0,185,187,3,66,33,0,186,178,1,0,0,0,186,179,1,0,0,
  	0,186,180,1,0,0,0,186,181,1,0,0,0,186,182,1,0,0,0,186,183,1,0,0,0,186,
  	184,1,0,0,0,186,185,1,0,0,0,187,15,1,0,0,0,188,189,7,1,0,0,189,17,1,0,
  	0,0,190,192,3,16,8,0,191,190,1,0,0,0,192,193,1,0,0,0,193,191,1,0,0,0,
  	193,194,1,0,0,0,194,195,1,0,0,0,195,196,3,60,30,0,196,19,1,0,0,0,197,
  	198,5,4,0,0,198,200,3,2,1,0,199,201,3,22,11,0,200,199,1,0,0,0,201,202,
  	1,0,0,0,202,200,1,0,0,0,202,203,1,0,0,0,203,205,1,0,0,0,204,206,3,24,
  	12,0,205,204,1,0,0,0,205,206,1,0,0,0,206,207,1,0,0,0,207,208,5,5,0,0,
  	208,215,1,0,0,0,209,210,5,4,0,0,210,211,3,2,1,0,211,212,3,24,12,0,212,
  	213,5,5,0,0,213,215,1,0,0,0,214,197,1,0,0,0,214,209,1,0,0,0,215,21,1,
  	0,0,0,216,217,5,6,0,0,217,218,5,7,0,0,218,219,3,84,42,0,219,220,5,8,0,
  	0,220,221,3,2,1,0,221,227,1,0,0,0,222,223,5,6,0,0,223,224,3,84,42,0,224,
  	225,3,2,1,0,225,227,1,0,0,0,226,216,1,0,0,0,226,222,1,0,0,0,227,23,1,
  	0,0,0,228,229,5,9,0,0,229,230,3,2,1,0,230,25,1,0,0,0,231,234,3,28,14,
  	0,232,233,5,10,0,0,233,235,3,28,14,0,234,232,1,0,0,0,235,236,1,0,0,0,
  	236,234,1,0,0,0,236,237,1,0,0,0,237,27,1,0,0,0,238,239,3,78,39,0,239,
  	240,5,11,0,0,240,241,3,82,41,0,241,29,1,0,0,0,242,243,3,80,40,0,243,244,
  	7,2,0,0,244,245,3,84,42,0,245,31,1,0,0,0,246,247,3,82,41,0,247,33,1,0,
  	0,0,248,250,5,18,0,0,249,251,3,82,41,0,250,249,1,0,0,0,250,251,1,0,0,
  	0,251,35,1,0,0,0,252,253,5,19,0,0,253,37,1,0,0,0,254,255,5,20,0,0,255,
  	39,1,0,0,0,256,257,5,21,0,0,257,41,1,0,0,0,258,259,5,22,0,0,259,264,5,
  	67,0,0,260,261,5,23,0,0,261,263,5,67,0,0,262,260,1,0,0,0,263,266,1,0,
  	0,0,264,262,1,0,0,0,264,265,1,0,0,0,265,269,1,0,0,0,266,264,1,0,0,0,267,
  	268,5,24,0,0,268,270,5,67,0,0,269,267,1,0,0,0,269,270,1,0,0,0,270,43,
  	1,0,0,0,271,272,5,25,0,0,272,273,3,28,14,0,273,45,1,0,0,0,274,275,5,26,
  	0,0,275,276,3,84,42,0,276,47,1,0,0,0,277,279,5,27,0,0,278,280,3,82,41,
  	0,279,278,1,0,0,0,279,280,1,0,0,0,280,49,1,0,0,0,281,282,5,28,0,0,282,
  	283,3,84,42,0,283,284,5,29,0,0,284,288,3,2,1,0,285,287,3,52,26,0,286,
  	285,1,0,0,0,287,290,1,0,0,0,288,286,1,0,0,0,288,289,1,0,0,0,289,292,1,
  	0,0,0,290,288,1,0,0,0,291,293,3,54,27,0,292,291,1,0,0,0,292,293,1,0,0,
  	0,293,294,1,0,0,0,294,295,5,5,0,0,295,51,1,0,0,0,296,297,5,30,0,0,297,
  	298,3,84,42,0,298,299,5,29,0,0,299,300,3,2,1,0,300,53,1,0,0,0,301,302,
  	5,31,0,0,302,303,3,2,1,0,303,55,1,0,0,0,304,305,5,32,0,0,305,306,5,7,
  	0,0,306,307,3,84,42,0,307,308,5,8,0,0,308,309,3,2,1,0,309,310,5,5,0,0,
  	310,317,1,0,0,0,311,312,5,32,0,0,312,313,3,84,42,0,313,314,3,2,1,0,314,
  	315,5,5,0,0,315,317,1,0,0,0,316,304,1,0,0,0,316,311,1,0,0,0,317,57,1,
  	0,0,0,318,319,5,33,0,0,319,320,3,78,39,0,320,321,5,34,0,0,321,322,3,82,
  	41,0,322,323,5,29,0,0,323,324,3,2,1,0,324,325,5,5,0,0,325,337,1,0,0,0,
  	326,327,5,33,0,0,327,328,3,78,39,0,328,329,5,34,0,0,329,330,5,7,0,0,330,
  	331,3,82,41,0,331,332,5,8,0,0,332,333,5,29,0,0,333,334,3,2,1,0,334,335,
  	5,5,0,0,335,337,1,0,0,0,336,318,1,0,0,0,336,326,1,0,0,0,337,59,1,0,0,
  	0,338,339,5,35,0,0,339,340,5,67,0,0,340,342,5,7,0,0,341,343,3,74,37,0,
  	342,341,1,0,0,0,342,343,1,0,0,0,343,344,1,0,0,0,344,345,5,8,0,0,345,346,
  	3,2,1,0,346,347,5,5,0,0,347,61,1,0,0,0,348,349,5,36,0,0,349,351,5,67,
  	0,0,350,352,3,64,32,0,351,350,1,0,0,0,351,352,1,0,0,0,352,353,1,0,0,0,
  	353,354,3,2,1,0,354,355,5,5,0,0,355,63,1,0,0,0,356,357,5,37,0,0,357,358,
  	5,67,0,0,358,65,1,0,0,0,359,360,5,38,0,0,360,361,5,69,0,0,361,362,5,24,
  	0,0,362,366,5,67,0,0,363,365,5,71,0,0,364,363,1,0,0,0,365,368,1,0,0,0,
  	366,364,1,0,0,0,366,367,1,0,0,0,367,372,1,0,0,0,368,366,1,0,0,0,369,371,
  	3,68,34,0,370,369,1,0,0,0,371,374,1,0,0,0,372,370,1,0,0,0,372,373,1,0,
  	0,0,373,375,1,0,0,0,374,372,1,0,0,0,375,376,5,5,0,0,376,67,1,0,0,0,377,
  	378,5,35,0,0,378,379,5,67,0,0,379,381,5,7,0,0,380,382,3,70,35,0,381,380,
  	1,0,0,0,381,382,1,0,0,0,382,383,1,0,0,0,383,387,5,8,0,0,384,386,5,71,
  	0,0,385,384,1,0,0,0,386,389,1,0,0,0,387,385,1,0,0,0,387,388,1,0,0,0,388,
  	69,1,0,0,0,389,387,1,0,0,0,390,395,3,72,36,0,391,392,5,10,0,0,392,394,
  	3,72,36,0,393,391,1,0,0,0,394,397,1,0,0,0,395,393,1,0,0,0,395,396,1,0,
  	0,0,396,401,1,0,0,0,397,395,1,0,0,0,398,399,5,10,0,0,399,400,5,39,0,0,
  	400,402,5,67,0,0,401,398,1,0,0,0,401,402,1,0,0,0,402,71,1,0,0,0,403,404,
  	5,67,0,0,404,73,1,0,0,0,405,410,3,76,38,0,406,407,5,10,0,0,407,409,3,
  	76,38,0,408,406,1,0,0,0,409,412,1,0,0,0,410,408,1,0,0,0,410,411,1,0,0,
  	0,411,416,1,0,0,0,412,410,1,0,0,0,413,414,5,10,0,0,414,415,5,39,0,0,415,
  	417,5,67,0,0,416,413,1,0,0,0,416,417,1,0,0,0,417,75,1,0,0,0,418,421,5,
  	67,0,0,419,420,5,11,0,0,420,422,3,84,42,0,421,419,1,0,0,0,421,422,1,0,
  	0,0,422,77,1,0,0,0,423,428,3,80,40,0,424,425,5,10,0,0,425,427,3,80,40,
  	0,426,424,1,0,0,0,427,430,1,0,0,0,428,426,1,0,0,0,428,429,1,0,0,0,429,
  	79,1,0,0,0,430,428,1,0,0,0,431,435,5,67,0,0,432,434,3,110,55,0,433,432,
  	1,0,0,0,434,437,1,0,0,0,435,433,1,0,0,0,435,436,1,0,0,0,436,81,1,0,0,
  	0,437,435,1,0,0,0,438,443,3,84,42,0,439,440,5,10,0,0,440,442,3,84,42,
  	0,441,439,1,0,0,0,442,445,1,0,0,0,443,441,1,0,0,0,443,444,1,0,0,0,444,
  	83,1,0,0,0,445,443,1,0,0,0,446,450,3,86,43,0,447,450,3,88,44,0,448,450,
  	3,90,45,0,449,446,1,0,0,0,449,447,1,0,0,0,449,448,1,0,0,0,450,85,1,0,
  	0,0,451,453,5,7,0,0,452,454,3,74,37,0,453,452,1,0,0,0,453,454,1,0,0,0,
  	454,455,1,0,0,0,455,456,5,8,0,0,456,457,5,40,0,0,457,458,3,84,42,0,458,
  	87,1,0,0,0,459,460,5,35,0,0,460,462,5,7,0,0,461,463,3,74,37,0,462,461,
  	1,0,0,0,462,463,1,0,0,0,463,464,1,0,0,0,464,465,5,8,0,0,465,466,3,2,1,
  	0,466,467,5,5,0,0,467,89,1,0,0,0,468,473,3,92,46,0,469,470,5,41,0,0,470,
  	472,3,92,46,0,471,469,1,0,0,0,472,475,1,0,0,0,473,471,1,0,0,0,473,474,
  	1,0,0,0,474,91,1,0,0,0,475,473,1,0,0,0,476,481,3,94,47,0,477,478,5,42,
  	0,0,478,480,3,94,47,0,479,477,1,0,0,0,480,483,1,0,0,0,481,479,1,0,0,0,
  	481,482,1,0,0,0,482,93,1,0,0,0,483,481,1,0,0,0,484,485,5,43,0,0,485,488,
  	3,94,47,0,486,488,3,96,48,0,487,484,1,0,0,0,487,486,1,0,0,0,488,95,1,
  	0,0,0,489,495,3,100,50,0,490,491,3,98,49,0,491,492,3,100,50,0,492,494,
  	1,0,0,0,493,490,1,0,0,0,494,497,1,0,0,0,495,493,1,0,0,0,495,496,1,0,0,
  	0,496,97,1,0,0,0,497,495,1,0,0,0,498,499,7,3,0,0,499,99,1,0,0,0,500,505,
  	3,102,51,0,501,502,7,4,0,0,502,504,3,102,51,0,503,501,1,0,0,0,504,507,
  	1,0,0,0,505,503,1,0,0,0,505,506,1,0,0,0,506,101,1,0,0,0,507,505,1,0,0,
  	0,508,513,3,104,52,0,509,510,7,5,0,0,510,512,3,104,52,0,511,509,1,0,0,
  	0,512,515,1,0,0,0,513,511,1,0,0,0,513,514,1,0,0,0,514,103,1,0,0,0,515,
  	513,1,0,0,0,516,517,7,6,0,0,517,520,3,104,52,0,518,520,3,106,53,0,519,
  	516,1,0,0,0,519,518,1,0,0,0,520,105,1,0,0,0,521,524,3,108,54,0,522,523,
  	5,54,0,0,523,525,3,104,52,0,524,522,1,0,0,0,524,525,1,0,0,0,525,107,1,
  	0,0,0,526,530,3,118,59,0,527,529,3,110,55,0,528,527,1,0,0,0,529,532,1,
  	0,0,0,530,528,1,0,0,0,530,531,1,0,0,0,531,109,1,0,0,0,532,530,1,0,0,0,
  	533,534,5,23,0,0,534,551,5,67,0,0,535,536,5,55,0,0,536,537,3,84,42,0,
  	537,538,5,56,0,0,538,551,1,0,0,0,539,540,5,55,0,0,540,541,3,112,56,0,
  	541,542,5,56,0,0,542,551,1,0,0,0,543,545,5,7,0,0,544,546,3,114,57,0,545,
  	544,1,0,0,0,545,546,1,0,0,0,546,547,1,0,0,0,547,551,5,8,0,0,548,551,5,
  	64,0,0,549,551,5,65,0,0,550,533,1,0,0,0,550,535,1,0,0,0,550,539,1,0,0,
  	0,550,543,1,0,0,0,550,548,1,0,0,0,550,549,1,0,0,0,551,111,1,0,0,0,552,
  	554,3,84,42,0,553,552,1,0,0,0,553,554,1,0,0,0,554,555,1,0,0,0,555,557,
  	5,37,0,0,556,558,3,84,42,0,557,556,1,0,0,0,557,558,1,0,0,0,558,563,1,
  	0,0,0,559,561,5,37,0,0,560,562,3,84,42,0,561,560,1,0,0,0,561,562,1,0,
  	0,0,562,564,1,0,0,0,563,559,1,0,0,0,563,564,1,0,0,0,564,113,1,0,0,0,565,
  	570,3,116,58,0,566,567,5,10,0,0,567,569,3,116,58,0,568,566,1,0,0,0,569,
  	572,1,0,0,0,570,568,1,0,0,0,570,571,1,0,0,0,571,115,1,0,0,0,572,570,1,
  	0,0,0,573,574,5,67,0,0,574,575,5,11,0,0,575,578,3,84,42,0,576,578,3,84,
  	42,0,577,573,1,0,0,0,577,576,1,0,0,0,578,117,1,0,0,0,579,599,5,67,0,0,
  	580,599,5,68,0,0,581,599,5,69,0,0,582,599,5,70,0,0,583,599,5,57,0,0,584,
  	599,5,58,0,0,585,599,5,59,0,0,586,599,3,120,60,0,587,599,3,122,61,0,588,
  	589,5,7,0,0,589,590,3,84,42,0,590,591,5,8,0,0,591,599,1,0,0,0,592,593,
  	5,60,0,0,593,595,5,7,0,0,594,596,3,114,57,0,595,594,1,0,0,0,595,596,1,
  	0,0,0,596,597,1,0,0,0,597,599,5,8,0,0,598,579,1,0,0,0,598,580,1,0,0,0,
  	598,581,1,0,0,0,598,582,1,0,0,0,598,583,1,0,0,0,598,584,1,0,0,0,598,585,
  	1,0,0,0,598,586,1,0,0,0,598,587,1,0,0,0,598,588,1,0,0,0,598,592,1,0,0,
  	0,599,119,1,0,0,0,600,612,5,55,0,0,601,606,3,84,42,0,602,603,5,10,0,0,
  	603,605,3,84,42,0,604,602,1,0,0,0,605,608,1,0,0,0,606,604,1,0,0,0,606,
  	607,1,0,0,0,607,610,1,0,0,0,608,606,1,0,0,0,609,611,5,10,0,0,610,609,
  	1,0,0,0,610,611,1,0,0,0,611,613,1,0,0,0,612,601,1,0,0,0,612,613,1,0,0,
  	0,613,614,1,0,0,0,614,615,5,56,0,0,615,121,1,0,0,0,616,628,5,61,0,0,617,
  	622,3,124,62,0,618,619,5,10,0,0,619,621,3,124,62,0,620,618,1,0,0,0,621,
  	624,1,0,0,0,622,620,1,0,0,0,622,623,1,0,0,0,623,626,1,0,0,0,624,622,1,
  	0,0,0,625,627,5,10,0,0,626,625,1,0,0,0,626,627,1,0,0,0,627,629,1,0,0,
  	0,628,617,1,0,0,0,628,629,1,0,0,0,629,630,1,0,0,0,630,631,5,62,0,0,631,
  	123,1,0,0,0,632,633,7,7,0,0,633,634,5,37,0,0,634,635,3,84,42,0,635,125,
  	1,0,0,0,66,128,130,135,137,142,148,150,166,171,186,193,202,205,214,226,
  	236,250,264,269,279,288,292,316,336,342,351,366,372,381,387,395,401,410,
  	416,421,428,435,443,449,453,462,473,481,487,495,505,513,519,524,530,545,
  	550,553,557,561,563,570,577,595,598,606,610,612,622,626,628
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
    setState(130);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 4505860614630670492) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 251) != 0)) {
      setState(128);
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
        case AvaLangParser::T__37:
        case AvaLangParser::T__42:
        case AvaLangParser::T__50:
        case AvaLangParser::T__54:
        case AvaLangParser::T__56:
        case AvaLangParser::T__57:
        case AvaLangParser::T__58:
        case AvaLangParser::T__59:
        case AvaLangParser::T__60:
        case AvaLangParser::INC:
        case AvaLangParser::DEC:
        case AvaLangParser::NAME:
        case AvaLangParser::NUMBER:
        case AvaLangParser::STRING:
        case AvaLangParser::FSTRING: {
          setState(126);
          statement();
          break;
        }

        case AvaLangParser::NEWLINE: {
          setState(127);
          match(AvaLangParser::NEWLINE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(132);
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
    setState(137);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 4505860614630670492) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 251) != 0)) {
      setState(135);
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
        case AvaLangParser::T__37:
        case AvaLangParser::T__42:
        case AvaLangParser::T__50:
        case AvaLangParser::T__54:
        case AvaLangParser::T__56:
        case AvaLangParser::T__57:
        case AvaLangParser::T__58:
        case AvaLangParser::T__59:
        case AvaLangParser::T__60:
        case AvaLangParser::INC:
        case AvaLangParser::DEC:
        case AvaLangParser::NAME:
        case AvaLangParser::NUMBER:
        case AvaLangParser::STRING:
        case AvaLangParser::FSTRING: {
          setState(133);
          statement();
          break;
        }

        case AvaLangParser::NEWLINE: {
          setState(134);
          match(AvaLangParser::NEWLINE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(139);
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
    setState(142);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 4, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(140);
      simpleStatement();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(141);
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
    setState(144);
    smallStatement();
    setState(148); 
    _errHandler->sync(this);
    alt = 1;
    do {
      switch (alt) {
        case 1: {
              setState(148);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case AvaLangParser::NEWLINE: {
                  setState(145);
                  match(AvaLangParser::NEWLINE);
                  break;
                }

                case AvaLangParser::T__0: {
                  setState(146);
                  match(AvaLangParser::T__0);
                  setState(147);
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
      setState(150); 
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
    setState(166);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(152);
      assignStatement();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(153);
      multiAssignStatement();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(154);
      augAssignStatement();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(155);
      exprStatement();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(156);
      returnStatement();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(157);
      breakStatement();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(158);
      continueStatement();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(159);
      passStatement();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(160);
      importStatement();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(161);
      localStatement();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(162);
      raiseStatement();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(163);
      yieldStatement();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(164);
      incDecStatement();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(165);
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
    setState(169); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(168);
      memberModifier();
      setState(171); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == AvaLangParser::T__1

    || _la == AvaLangParser::T__2);
    setState(173);
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
    setState(175);
    _la = _input->LA(1);
    if (!(_la == AvaLangParser::INC

    || _la == AvaLangParser::DEC)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(176);
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

AvaLangParser::ExternStatementContext* AvaLangParser::CompoundStatementContext::externStatement() {
  return getRuleContext<AvaLangParser::ExternStatementContext>(0);
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
    setState(186);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case AvaLangParser::T__27: {
        enterOuterAlt(_localctx, 1);
        setState(178);
        ifStatement();
        break;
      }

      case AvaLangParser::T__31: {
        enterOuterAlt(_localctx, 2);
        setState(179);
        whileStatement();
        break;
      }

      case AvaLangParser::T__32: {
        enterOuterAlt(_localctx, 3);
        setState(180);
        forStatement();
        break;
      }

      case AvaLangParser::T__34: {
        enterOuterAlt(_localctx, 4);
        setState(181);
        funcDeclaration();
        break;
      }

      case AvaLangParser::T__35: {
        enterOuterAlt(_localctx, 5);
        setState(182);
        classDeclaration();
        break;
      }

      case AvaLangParser::T__3: {
        enterOuterAlt(_localctx, 6);
        setState(183);
        tryStatement();
        break;
      }

      case AvaLangParser::T__1:
      case AvaLangParser::T__2: {
        enterOuterAlt(_localctx, 7);
        setState(184);
        modifiedFuncDeclaration();
        break;
      }

      case AvaLangParser::T__37: {
        enterOuterAlt(_localctx, 8);
        setState(185);
        externStatement();
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
    setState(188);
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
    setState(191); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(190);
      memberModifier();
      setState(193); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == AvaLangParser::T__1

    || _la == AvaLangParser::T__2);
    setState(195);
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
    setState(214);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 13, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(197);
      match(AvaLangParser::T__3);
      setState(198);
      block();
      setState(200); 
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(199);
        exceptClause();
        setState(202); 
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while (_la == AvaLangParser::T__5);
      setState(205);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == AvaLangParser::T__8) {
        setState(204);
        finallyClause();
      }
      setState(207);
      match(AvaLangParser::T__4);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(209);
      match(AvaLangParser::T__3);
      setState(210);
      block();

      setState(211);
      finallyClause();
      setState(212);
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
    setState(226);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 14, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(216);
      match(AvaLangParser::T__5);
      setState(217);
      match(AvaLangParser::T__6);
      setState(218);
      expr();
      setState(219);
      match(AvaLangParser::T__7);
      setState(220);
      block();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(222);
      match(AvaLangParser::T__5);
      setState(223);
      expr();
      setState(224);
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
    setState(228);
    match(AvaLangParser::T__8);
    setState(229);
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
    setState(231);
    assignStatement();
    setState(234); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(232);
      match(AvaLangParser::T__9);
      setState(233);
      assignStatement();
      setState(236); 
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
    setState(238);
    targetList();
    setState(239);
    match(AvaLangParser::T__10);
    setState(240);
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
    setState(242);
    target();
    setState(243);
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
    setState(244);
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
    setState(246);
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
    setState(248);
    match(AvaLangParser::T__17);
    setState(250);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
      setState(249);
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
    setState(252);
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
    setState(254);
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
    setState(256);
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
    setState(258);
    match(AvaLangParser::T__21);
    setState(259);
    match(AvaLangParser::NAME);
    setState(264);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__22) {
      setState(260);
      match(AvaLangParser::T__22);
      setState(261);
      match(AvaLangParser::NAME);
      setState(266);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(269);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__23) {
      setState(267);
      antlrcpp::downCast<ImportStatementContext *>(_localctx)->as = match(AvaLangParser::T__23);
      setState(268);
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
    setState(271);
    match(AvaLangParser::T__24);
    setState(272);
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
    setState(274);
    match(AvaLangParser::T__25);
    setState(275);
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
    setState(277);
    match(AvaLangParser::T__26);
    setState(279);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
      setState(278);
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
    setState(281);
    match(AvaLangParser::T__27);
    setState(282);
    expr();
    setState(283);
    match(AvaLangParser::T__28);
    setState(284);
    block();
    setState(288);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__29) {
      setState(285);
      elifClause();
      setState(290);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(292);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__30) {
      setState(291);
      elseClause();
    }
    setState(294);
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
    setState(296);
    match(AvaLangParser::T__29);
    setState(297);
    expr();
    setState(298);
    match(AvaLangParser::T__28);
    setState(299);
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
    setState(301);
    match(AvaLangParser::T__30);
    setState(302);
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
    setState(316);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 22, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(304);
      match(AvaLangParser::T__31);
      setState(305);
      match(AvaLangParser::T__6);
      setState(306);
      expr();
      setState(307);
      match(AvaLangParser::T__7);
      setState(308);
      block();
      setState(309);
      match(AvaLangParser::T__4);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(311);
      match(AvaLangParser::T__31);
      setState(312);
      expr();
      setState(313);
      block();
      setState(314);
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
    setState(336);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 23, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(318);
      match(AvaLangParser::T__32);
      setState(319);
      targetList();
      setState(320);
      match(AvaLangParser::T__33);
      setState(321);
      exprList();
      setState(322);
      match(AvaLangParser::T__28);
      setState(323);
      block();
      setState(324);
      match(AvaLangParser::T__4);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(326);
      match(AvaLangParser::T__32);
      setState(327);
      targetList();
      setState(328);
      match(AvaLangParser::T__33);
      setState(329);
      match(AvaLangParser::T__6);
      setState(330);
      exprList();
      setState(331);
      match(AvaLangParser::T__7);
      setState(332);
      match(AvaLangParser::T__28);
      setState(333);
      block();
      setState(334);
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
    setState(338);
    match(AvaLangParser::T__34);
    setState(339);
    match(AvaLangParser::NAME);
    setState(340);
    match(AvaLangParser::T__6);
    setState(342);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(341);
      paramList();
    }
    setState(344);
    match(AvaLangParser::T__7);
    setState(345);
    block();
    setState(346);
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
    setState(348);
    match(AvaLangParser::T__35);
    setState(349);
    match(AvaLangParser::NAME);
    setState(351);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__36) {
      setState(350);
      classHeritage();
    }
    setState(353);
    block();
    setState(354);
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
    setState(356);
    match(AvaLangParser::T__36);
    setState(357);
    match(AvaLangParser::NAME);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExternStatementContext ------------------------------------------------------------------

AvaLangParser::ExternStatementContext::ExternStatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::ExternStatementContext::STRING() {
  return getToken(AvaLangParser::STRING, 0);
}

tree::TerminalNode* AvaLangParser::ExternStatementContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

std::vector<tree::TerminalNode *> AvaLangParser::ExternStatementContext::NEWLINE() {
  return getTokens(AvaLangParser::NEWLINE);
}

tree::TerminalNode* AvaLangParser::ExternStatementContext::NEWLINE(size_t i) {
  return getToken(AvaLangParser::NEWLINE, i);
}

std::vector<AvaLangParser::ExternFuncDeclarationContext *> AvaLangParser::ExternStatementContext::externFuncDeclaration() {
  return getRuleContexts<AvaLangParser::ExternFuncDeclarationContext>();
}

AvaLangParser::ExternFuncDeclarationContext* AvaLangParser::ExternStatementContext::externFuncDeclaration(size_t i) {
  return getRuleContext<AvaLangParser::ExternFuncDeclarationContext>(i);
}


size_t AvaLangParser::ExternStatementContext::getRuleIndex() const {
  return AvaLangParser::RuleExternStatement;
}

void AvaLangParser::ExternStatementContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExternStatement(this);
}

void AvaLangParser::ExternStatementContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExternStatement(this);
}


std::any AvaLangParser::ExternStatementContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExternStatement(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExternStatementContext* AvaLangParser::externStatement() {
  ExternStatementContext *_localctx = _tracker.createInstance<ExternStatementContext>(_ctx, getState());
  enterRule(_localctx, 66, AvaLangParser::RuleExternStatement);
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
    setState(359);
    match(AvaLangParser::T__37);
    setState(360);
    match(AvaLangParser::STRING);
    setState(361);
    match(AvaLangParser::T__23);
    setState(362);
    match(AvaLangParser::NAME);
    setState(366);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::NEWLINE) {
      setState(363);
      match(AvaLangParser::NEWLINE);
      setState(368);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(372);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__34) {
      setState(369);
      externFuncDeclaration();
      setState(374);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(375);
    match(AvaLangParser::T__4);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExternFuncDeclarationContext ------------------------------------------------------------------

AvaLangParser::ExternFuncDeclarationContext::ExternFuncDeclarationContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::ExternFuncDeclarationContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}

AvaLangParser::ExternParamListContext* AvaLangParser::ExternFuncDeclarationContext::externParamList() {
  return getRuleContext<AvaLangParser::ExternParamListContext>(0);
}

std::vector<tree::TerminalNode *> AvaLangParser::ExternFuncDeclarationContext::NEWLINE() {
  return getTokens(AvaLangParser::NEWLINE);
}

tree::TerminalNode* AvaLangParser::ExternFuncDeclarationContext::NEWLINE(size_t i) {
  return getToken(AvaLangParser::NEWLINE, i);
}


size_t AvaLangParser::ExternFuncDeclarationContext::getRuleIndex() const {
  return AvaLangParser::RuleExternFuncDeclaration;
}

void AvaLangParser::ExternFuncDeclarationContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExternFuncDeclaration(this);
}

void AvaLangParser::ExternFuncDeclarationContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExternFuncDeclaration(this);
}


std::any AvaLangParser::ExternFuncDeclarationContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExternFuncDeclaration(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExternFuncDeclarationContext* AvaLangParser::externFuncDeclaration() {
  ExternFuncDeclarationContext *_localctx = _tracker.createInstance<ExternFuncDeclarationContext>(_ctx, getState());
  enterRule(_localctx, 68, AvaLangParser::RuleExternFuncDeclaration);
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
    setState(377);
    match(AvaLangParser::T__34);
    setState(378);
    match(AvaLangParser::NAME);
    setState(379);
    match(AvaLangParser::T__6);
    setState(381);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(380);
      externParamList();
    }
    setState(383);
    match(AvaLangParser::T__7);
    setState(387);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::NEWLINE) {
      setState(384);
      match(AvaLangParser::NEWLINE);
      setState(389);
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

//----------------- ExternParamListContext ------------------------------------------------------------------

AvaLangParser::ExternParamListContext::ExternParamListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<AvaLangParser::ExternParamContext *> AvaLangParser::ExternParamListContext::externParam() {
  return getRuleContexts<AvaLangParser::ExternParamContext>();
}

AvaLangParser::ExternParamContext* AvaLangParser::ExternParamListContext::externParam(size_t i) {
  return getRuleContext<AvaLangParser::ExternParamContext>(i);
}

tree::TerminalNode* AvaLangParser::ExternParamListContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}


size_t AvaLangParser::ExternParamListContext::getRuleIndex() const {
  return AvaLangParser::RuleExternParamList;
}

void AvaLangParser::ExternParamListContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExternParamList(this);
}

void AvaLangParser::ExternParamListContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExternParamList(this);
}


std::any AvaLangParser::ExternParamListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExternParamList(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExternParamListContext* AvaLangParser::externParamList() {
  ExternParamListContext *_localctx = _tracker.createInstance<ExternParamListContext>(_ctx, getState());
  enterRule(_localctx, 70, AvaLangParser::RuleExternParamList);
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
    setState(390);
    externParam();
    setState(395);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 30, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(391);
        match(AvaLangParser::T__9);
        setState(392);
        externParam(); 
      }
      setState(397);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 30, _ctx);
    }
    setState(401);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__9) {
      setState(398);
      match(AvaLangParser::T__9);
      setState(399);
      match(AvaLangParser::T__38);
      setState(400);
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

//----------------- ExternParamContext ------------------------------------------------------------------

AvaLangParser::ExternParamContext::ExternParamContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* AvaLangParser::ExternParamContext::NAME() {
  return getToken(AvaLangParser::NAME, 0);
}


size_t AvaLangParser::ExternParamContext::getRuleIndex() const {
  return AvaLangParser::RuleExternParam;
}

void AvaLangParser::ExternParamContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExternParam(this);
}

void AvaLangParser::ExternParamContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<AvaLangListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExternParam(this);
}


std::any AvaLangParser::ExternParamContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<AvaLangVisitor*>(visitor))
    return parserVisitor->visitExternParam(this);
  else
    return visitor->visitChildren(this);
}

AvaLangParser::ExternParamContext* AvaLangParser::externParam() {
  ExternParamContext *_localctx = _tracker.createInstance<ExternParamContext>(_ctx, getState());
  enterRule(_localctx, 72, AvaLangParser::RuleExternParam);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(403);
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
  enterRule(_localctx, 74, AvaLangParser::RuleParamList);
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
    setState(405);
    param();
    setState(410);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 32, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(406);
        match(AvaLangParser::T__9);
        setState(407);
        param(); 
      }
      setState(412);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 32, _ctx);
    }
    setState(416);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__9) {
      setState(413);
      match(AvaLangParser::T__9);
      setState(414);
      match(AvaLangParser::T__38);
      setState(415);
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
  enterRule(_localctx, 76, AvaLangParser::RuleParam);
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
    setState(418);
    match(AvaLangParser::NAME);
    setState(421);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__10) {
      setState(419);
      match(AvaLangParser::T__10);
      setState(420);
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
  enterRule(_localctx, 78, AvaLangParser::RuleTargetList);
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
    setState(423);
    target();
    setState(428);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__9) {
      setState(424);
      match(AvaLangParser::T__9);
      setState(425);
      target();
      setState(430);
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
  enterRule(_localctx, 80, AvaLangParser::RuleTarget);
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
    setState(431);
    match(AvaLangParser::NAME);
    setState(435);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & 432627039204343809) != 0)) {
      setState(432);
      trailer();
      setState(437);
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
  enterRule(_localctx, 82, AvaLangParser::RuleExprList);

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
    setState(438);
    expr();
    setState(443);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 37, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(439);
        match(AvaLangParser::T__9);
        setState(440);
        expr(); 
      }
      setState(445);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 37, _ctx);
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
  enterRule(_localctx, 84, AvaLangParser::RuleExpr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(449);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 38, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<AvaLangParser::ShortLambdaExprAltContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(446);
      shortLambdaExpr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<AvaLangParser::LambdaExprAltContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(447);
      lambdaExpr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<AvaLangParser::OrExprAltContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(448);
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
  enterRule(_localctx, 86, AvaLangParser::RuleShortLambdaExpr);
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
    setState(451);
    match(AvaLangParser::T__6);
    setState(453);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(452);
      paramList();
    }
    setState(455);
    match(AvaLangParser::T__7);
    setState(456);
    match(AvaLangParser::T__39);
    setState(457);
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
  enterRule(_localctx, 88, AvaLangParser::RuleLambdaExpr);
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
    setState(459);
    match(AvaLangParser::T__34);
    setState(460);
    match(AvaLangParser::T__6);
    setState(462);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME) {
      setState(461);
      paramList();
    }
    setState(464);
    match(AvaLangParser::T__7);
    setState(465);
    block();
    setState(466);
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
  enterRule(_localctx, 90, AvaLangParser::RuleOrExpr);
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
    setState(468);
    andExpr();
    setState(473);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__40) {
      setState(469);
      match(AvaLangParser::T__40);
      setState(470);
      andExpr();
      setState(475);
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
  enterRule(_localctx, 92, AvaLangParser::RuleAndExpr);
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
    setState(476);
    notExpr();
    setState(481);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__41) {
      setState(477);
      match(AvaLangParser::T__41);
      setState(478);
      notExpr();
      setState(483);
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
  enterRule(_localctx, 94, AvaLangParser::RuleNotExpr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(487);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(484);
      match(AvaLangParser::T__42);
      setState(485);
      notExpr();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(486);
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
  enterRule(_localctx, 96, AvaLangParser::RuleComparison);
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
    setState(489);
    additive();
    setState(495);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1108307720798208) != 0)) {
      setState(490);
      compOp();
      setState(491);
      additive();
      setState(497);
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
  enterRule(_localctx, 98, AvaLangParser::RuleCompOp);
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
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1108307720798208) != 0))) {
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
  enterRule(_localctx, 100, AvaLangParser::RuleAdditive);
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
    setState(500);
    multiplicative();
    setState(505);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 45, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(501);
        _la = _input->LA(1);
        if (!(_la == AvaLangParser::T__49

        || _la == AvaLangParser::T__50)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(502);
        multiplicative(); 
      }
      setState(507);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 45, _ctx);
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
  enterRule(_localctx, 102, AvaLangParser::RuleMultiplicative);
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
    setState(508);
    unary();
    setState(513);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 39) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 39)) & 134242305) != 0)) {
      setState(509);
      _la = _input->LA(1);
      if (!(((((_la - 39) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 39)) & 134242305) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(510);
      unary();
      setState(515);
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
  enterRule(_localctx, 104, AvaLangParser::RuleUnary);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(519);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case AvaLangParser::T__42:
      case AvaLangParser::T__50:
      case AvaLangParser::INC:
      case AvaLangParser::DEC: {
        enterOuterAlt(_localctx, 1);
        setState(516);
        _la = _input->LA(1);
        if (!(((((_la - 43) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 43)) & 6291713) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(517);
        unary();
        break;
      }

      case AvaLangParser::T__6:
      case AvaLangParser::T__54:
      case AvaLangParser::T__56:
      case AvaLangParser::T__57:
      case AvaLangParser::T__58:
      case AvaLangParser::T__59:
      case AvaLangParser::T__60:
      case AvaLangParser::NAME:
      case AvaLangParser::NUMBER:
      case AvaLangParser::STRING:
      case AvaLangParser::FSTRING: {
        enterOuterAlt(_localctx, 2);
        setState(518);
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
  enterRule(_localctx, 106, AvaLangParser::RulePower);
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
    setState(521);
    postfix();
    setState(524);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__53) {
      setState(522);
      match(AvaLangParser::T__53);
      setState(523);
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
  enterRule(_localctx, 108, AvaLangParser::RulePostfix);

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
    setState(526);
    primary();
    setState(530);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 49, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(527);
        trailer(); 
      }
      setState(532);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 49, _ctx);
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
  enterRule(_localctx, 110, AvaLangParser::RuleTrailer);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(550);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 51, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<AvaLangParser::AttrTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(533);
      match(AvaLangParser::T__22);
      setState(534);
      match(AvaLangParser::NAME);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<AvaLangParser::IndexTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(535);
      match(AvaLangParser::T__54);
      setState(536);
      expr();
      setState(537);
      match(AvaLangParser::T__55);
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<AvaLangParser::SliceTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(539);
      match(AvaLangParser::T__54);
      setState(540);
      sliceRange();
      setState(541);
      match(AvaLangParser::T__55);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<AvaLangParser::CallTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(543);
      match(AvaLangParser::T__6);
      setState(545);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 7) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
        setState(544);
        argList();
      }
      setState(547);
      match(AvaLangParser::T__7);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<AvaLangParser::IncTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(548);
      match(AvaLangParser::INC);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<AvaLangParser::DecTrailerContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(549);
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
  enterRule(_localctx, 112, AvaLangParser::RuleSliceRange);
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
    setState(553);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
      setState(552);
      expr();
    }
    setState(555);
    match(AvaLangParser::T__36);
    setState(557);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
      setState(556);
      expr();
    }
    setState(563);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::T__36) {
      setState(559);
      match(AvaLangParser::T__36);
      setState(561);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (((((_la - 7) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
        setState(560);
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
  enterRule(_localctx, 114, AvaLangParser::RuleArgList);
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
    setState(565);
    arg();
    setState(570);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == AvaLangParser::T__9) {
      setState(566);
      match(AvaLangParser::T__9);
      setState(567);
      arg();
      setState(572);
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
  enterRule(_localctx, 116, AvaLangParser::RuleArg);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(577);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 57, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<AvaLangParser::NamedArgContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(573);
      match(AvaLangParser::NAME);
      setState(574);
      match(AvaLangParser::T__10);
      setState(575);
      expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<AvaLangParser::PositionalArgContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(576);
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
  enterRule(_localctx, 118, AvaLangParser::RulePrimary);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(598);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case AvaLangParser::NAME: {
        _localctx = _tracker.createInstance<AvaLangParser::NameAtomContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(579);
        match(AvaLangParser::NAME);
        break;
      }

      case AvaLangParser::NUMBER: {
        _localctx = _tracker.createInstance<AvaLangParser::NumberAtomContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(580);
        match(AvaLangParser::NUMBER);
        break;
      }

      case AvaLangParser::STRING: {
        _localctx = _tracker.createInstance<AvaLangParser::StringAtomContext>(_localctx);
        enterOuterAlt(_localctx, 3);
        setState(581);
        match(AvaLangParser::STRING);
        break;
      }

      case AvaLangParser::FSTRING: {
        _localctx = _tracker.createInstance<AvaLangParser::FstringAtomContext>(_localctx);
        enterOuterAlt(_localctx, 4);
        setState(582);
        match(AvaLangParser::FSTRING);
        break;
      }

      case AvaLangParser::T__56: {
        _localctx = _tracker.createInstance<AvaLangParser::TrueAtomContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(583);
        match(AvaLangParser::T__56);
        break;
      }

      case AvaLangParser::T__57: {
        _localctx = _tracker.createInstance<AvaLangParser::FalseAtomContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(584);
        match(AvaLangParser::T__57);
        break;
      }

      case AvaLangParser::T__58: {
        _localctx = _tracker.createInstance<AvaLangParser::NilAtomContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(585);
        match(AvaLangParser::T__58);
        break;
      }

      case AvaLangParser::T__54: {
        _localctx = _tracker.createInstance<AvaLangParser::ListAtomContext>(_localctx);
        enterOuterAlt(_localctx, 8);
        setState(586);
        listLiteral();
        break;
      }

      case AvaLangParser::T__60: {
        _localctx = _tracker.createInstance<AvaLangParser::DictAtomContext>(_localctx);
        enterOuterAlt(_localctx, 9);
        setState(587);
        dictLiteral();
        break;
      }

      case AvaLangParser::T__6: {
        _localctx = _tracker.createInstance<AvaLangParser::GroupAtomContext>(_localctx);
        enterOuterAlt(_localctx, 10);
        setState(588);
        match(AvaLangParser::T__6);
        setState(589);
        expr();
        setState(590);
        match(AvaLangParser::T__7);
        break;
      }

      case AvaLangParser::T__59: {
        _localctx = _tracker.createInstance<AvaLangParser::BaseAtomContext>(_localctx);
        enterOuterAlt(_localctx, 11);
        setState(592);
        match(AvaLangParser::T__59);
        setState(593);
        match(AvaLangParser::T__6);
        setState(595);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (((((_la - 7) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
          setState(594);
          argList();
        }
        setState(597);
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
  enterRule(_localctx, 120, AvaLangParser::RuleListLiteral);
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
    setState(600);
    match(AvaLangParser::T__54);
    setState(612);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 7) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 7)) & -685373907116490751) != 0)) {
      setState(601);
      expr();
      setState(606);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 60, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(602);
          match(AvaLangParser::T__9);
          setState(603);
          expr(); 
        }
        setState(608);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 60, _ctx);
      }
      setState(610);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == AvaLangParser::T__9) {
        setState(609);
        match(AvaLangParser::T__9);
      }
    }
    setState(614);
    match(AvaLangParser::T__55);
   
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
  enterRule(_localctx, 122, AvaLangParser::RuleDictLiteral);
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
    setState(616);
    match(AvaLangParser::T__60);
    setState(628);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == AvaLangParser::NAME

    || _la == AvaLangParser::STRING) {
      setState(617);
      dictEntry();
      setState(622);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 63, _ctx);
      while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
        if (alt == 1) {
          setState(618);
          match(AvaLangParser::T__9);
          setState(619);
          dictEntry(); 
        }
        setState(624);
        _errHandler->sync(this);
        alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 63, _ctx);
      }
      setState(626);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == AvaLangParser::T__9) {
        setState(625);
        match(AvaLangParser::T__9);
      }
    }
    setState(630);
    match(AvaLangParser::T__61);
   
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
  enterRule(_localctx, 124, AvaLangParser::RuleDictEntry);
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
    setState(632);
    _la = _input->LA(1);
    if (!(_la == AvaLangParser::NAME

    || _la == AvaLangParser::STRING)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(633);
    match(AvaLangParser::T__36);
    setState(634);
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
