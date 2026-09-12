grammar AvaLang;

AVA_LANG
    : 'ava'
    ;

chunk
    : (statement | NEWLINE)*
    ;

block
    : (statement | NEWLINE)*
    ;

statement
    : compoundStatement
    | simpleStatement
    ;

simpleStatement
    : smallStatement ((NEWLINE | ';' NEWLINE)+)?
    ;

smallStatement
    : assignStatement
    | multiAssignStatement
    | augAssignStatement
    | exprStatement
    | returnStatement
    | breakStatement
    | continueStatement
    | passStatement
    | importStatement
    | localStatement
    | raiseStatement
    | incDecStatement
    | modifiedAssignStatement
    | typedAssignStatement
    | typedDeclStatement
    | interfaceMethodSignature
    ;

typedAssignStatement
    : NAME typeAnnotation '=' expr
    ;

typedDeclStatement
    : NAME typeAnnotation
    ;

typeAnnotation
    : 'as' NAME
    ;

modifiedAssignStatement
    : memberModifier+ assignStatement
    ;

incDecStatement
    : (INC | DEC) target
    ;

compoundStatement
    : ifStatement
    | whileStatement
    | forStatement
    | funcDeclaration
    | classDeclaration
    | interfaceDeclaration
    | tryStatement
    | modifiedFuncDeclaration
    | asyncFuncDeclaration
    | externStatement
    | selectStatement
    ;

memberModifier
    : 'static'
    | 'private'
    | 'override'
    ;

modifiedFuncDeclaration
    : attributeList? (NEWLINE)* memberModifier+ funcDeclaration
    ;

asyncFuncDeclaration
    : 'async' funcDeclaration
    ;

tryStatement
    : 'try' block (exceptClause)+ (finallyClause)? 'end'
    | 'try' block (finallyClause) 'end'
    ;

exceptClause
    : 'catch' '(' expr ')' block
    | 'catch' expr block
    ;

finallyClause
    : 'finally' block
    ;

multiAssignStatement
    : assignStatement (',' assignStatement)+
    ;

assignStatement
    : targetList '=' exprList
    ;

augAssignStatement
    : target op=('+=' | '-=' | '*=' | '/=' | '%=' | '//=') expr
    ;

exprStatement
    : exprList
    ;

returnStatement
    : 'return' exprList?
    ;

breakStatement
    : 'break'
    ;

continueStatement
    : 'continue'
    ;

passStatement
    : 'pass'
    ;

importStatement
    : 'import' NAME ('.' NAME)* (as='as' NAME)?
    ;

localStatement
    : 'local' assignStatement
    | 'local' typedAssignStatement
    | 'local' typedDeclStatement
    ;

raiseStatement
    : 'raise' expr
    ;

ifStatement
    : 'if' expr 'then' block elifClause* elseClause? 'end'
    ;

elifClause
    : 'elif' expr 'then' block
    ;

elseClause
    : 'else' block
    ;

selectStatement
    : 'select' expr (NEWLINE)* caseClause+ elseClause? 'end'
    ;

caseClause
    : 'case' caseItem (',' caseItem)* 'then' block
    ;

caseItem
    : expr NAME expr     # caseItemRange
    | 'is' compOp expr  # caseItemRelational
    | expr              # caseItemEquals
    ;

whileStatement
    : 'while' '(' expr ')' block 'end'
    | 'while' expr block 'end'
    ;

forStatement
    : 'for' targetList 'in' exprList 'then' block 'end'
    | 'for' targetList 'in' '(' exprList ')' 'then' block 'end'
    | 'for' NAME '=' expr NAME expr (NAME expr)? 'then' block 'end'
    ;

funcDeclaration
    : attributeList? (NEWLINE)* 'func' NAME '(' paramList? ')' returnType? block 'end'
    ;

returnType
    : typeAnnotation
    ;

attribute
    : '[' NAME ']'
    ;

attributeList
    : attribute+
    ;

classDeclaration
    : 'class' NAME classHeritage? block 'end'
    ;

classHeritage
    : ':' NAME (',' NAME)*
    ;

interfaceDeclaration
    : 'interface' NAME interfaceHeritage? block 'end'
    ;

interfaceHeritage
    : ':' NAME (',' NAME)*
    ;

interfaceMethodSignature
    : 'func' NAME '(' externParamList? ')' returnType? (NEWLINE)*
    ;

externStatement
    : 'extern' STRING 'as' NAME (NEWLINE)* externFuncDeclaration* 'end'
    ;

externFuncDeclaration
    : 'func' NAME '(' externParamList? ')' returnType? (NEWLINE)*
    ;

externParamList
    : externParam (',' externParam)* (',' '*' NAME)?
    ;

externParam
    : NAME typeAnnotation?
    ;

paramList
    : param (',' param)* (',' '*' NAME)?
    ;

param
    : NAME typeAnnotation? ('=' expr)?
    ;

targetList
    : target (',' target)*
    ;

target
    : NAME trailer*
    ;

exprList
    : expr (',' expr)*
    ;

expr
    : shortLambdaExpr                          #ShortLambdaExprAlt
    | singleParamLambdaExpr               #SingleParamLambdaExprAlt
    | lambdaExpr  #LambdaExprAlt
    | ternaryExpr  #OrExprAlt
    ;

ternaryExpr
    : orExpr ('?' NEWLINE* expr NEWLINE* ':' NEWLINE* expr)?
    ;

singleParamLambdaExpr
    : NAME '=>' expr
    ;

shortLambdaExpr
    : '(' paramList? ')' returnType? '=>' expr
    ;

lambdaExpr
    : 'func' '(' paramList? ')' returnType? block 'end'
    ;

orExpr
    : andExpr ('or' NEWLINE* andExpr)*
    ;

andExpr
    : notExpr ('and' NEWLINE* notExpr)*
    ;

notExpr
    : 'not' notExpr
    | comparison ('is' NAME)?
    ;

comparison
    : bitOr (compOp NEWLINE* bitOr)*
    ;

compOp
    : '==' | '!=' | '<' | '>' | '<=' | '>='
    ;

bitOr
    : bitXor ('|' NEWLINE* bitXor)*
    ;

bitXor
    : bitAnd ('^' NEWLINE* bitAnd)*
    ;

bitAnd
    : shift ('&' NEWLINE* shift)*
    ;

shift
    : additive (('<<' | '>>') NEWLINE* additive)*
    ;

additive
    : multiplicative (('+' | '-') NEWLINE* multiplicative)*
    ;

multiplicative
    : unary (('*' | '/' | '%' | IDIV) NEWLINE* unary)*
    ;

unary
    : ( '-' | 'not' | '~' | INC | DEC ) unary
    | power
    ;

power
    : postfix ('**' NEWLINE* unary)?
    ;

postfix
    : primary trailer*
    ;

trailer
    : '.' NAME                     # attrTrailer
    | '.' 'base' ('.' NAME)? '(' argList? ')'  # baseCallTrailer
    | '[' expr ']'                 # indexTrailer
    | '[' sliceRange ']'           # sliceTrailer
    | '(' NEWLINE* argList? NEWLINE* ')'  # callTrailer
    | INC                          # incTrailer
    | DEC                          # decTrailer
    ;

sliceRange
    : expr? ':' expr? (':' expr?)?
    ;

argList
    : arg (NEWLINE* ',' NEWLINE* arg)*
    ;

arg
    : NAME '=' expr     # namedArg
    | expr              # positionalArg
    ;

primary
    : NAME                          # nameAtom
    | NUMBER                        # numberAtom
    | STRING                        # stringAtom
    | FSTRING                       # fstringAtom
    | 'true'                        # trueAtom
    | 'false'                       # falseAtom
    | 'nil'                         # nilAtom
    | listLiteral                   # listAtom
    | dictLiteral                   # dictAtom
    | '(' NEWLINE* expr NEWLINE* ')'      # groupAtom
    | 'base' ('.' NAME)? '(' argList? ')' trailer*  # baseAtom
    | 'new' NAME '(' NEWLINE* argList? NEWLINE* ')' # newInstanceAtom
    | 'yield' exprList?             # yieldAtom
    | 'await' expr                  # awaitAtom
    ;

listLiteral
    : '[' NEWLINE* (expr (NEWLINE* ',' NEWLINE* expr)* NEWLINE* ','?)? NEWLINE* ']'
    ;

dictLiteral
    : '{' NEWLINE* (dictEntry (NEWLINE* ',' NEWLINE* dictEntry)* NEWLINE* ','?)? NEWLINE* '}'
    ;

dictEntry
    : (NAME | STRING) ':' expr
    ;

// ---------------------------------------------------------------------
// Lexer rules
// ---------------------------------------------------------------------

INC
    : '++'
    ;

DEC
    : '--'
    ;

IDIV
    : '//'
    ;

NAME
    : [a-zA-Z_] [a-zA-Z_0-9]*
    ;

NUMBER
    : DIGIT+ ('.' DIGIT+)? EXPONENT?
    ;

fragment EXPONENT
    : [eE] [+-]? DIGIT+
    ;

fragment DIGIT
    : [0-9]
    ;

STRING
    : '"' ( ~["\\\r\n] | ESCAPE_SEQ )* '"'
    | '\'' ( ~['\\\r\n] | ESCAPE_SEQ )* '\''
    ;

FSTRING
    : '$"' FSTR_ITEM* '"'
    ;

fragment FSTR_ITEM
    : ~["\\{}\r\n]
    | ESCAPE_SEQ
    | '{{'
    | '}}'
    | '{' FSTR_INNER* '}'
    ;

fragment FSTR_INNER
    : ~["'{}\r\n]
    | ESCAPE_SEQ
    | '{' FSTR_INNER* '}'
    | '"' ( ~["\\\r\n] | ESCAPE_SEQ )* '"'
    | '\'' ( ~['\\\r\n] | ESCAPE_SEQ )* '\''
    ;

fragment ESCAPE_SEQ
    : '\\' [btnr"'\\]
    ;

NEWLINE
    : ( '\r'? '\n' [ \t]* )+
    ;

COMMENT
    : '#' ~[\r\n]* -> skip
    ;

WS
    : [ \t]+ -> skip
    ;
