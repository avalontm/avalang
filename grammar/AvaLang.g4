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
    : simpleStatement
    | compoundStatement
    ;

simpleStatement
    : smallStatement (NEWLINE | ';' NEWLINE)*
    ;

smallStatement
    : assignStatement
    | augAssignStatement
    | exprStatement
    | returnStatement
    | breakStatement
    | continueStatement
    | passStatement
    | importStatement
    | localStatement
    | raiseStatement
    | yieldStatement
    | incDecStatement
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
    | tryStatement
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

// --- simple statements -------------------------------------------------

assignStatement
    : targetList '=' exprList
    ;

augAssignStatement
    : target op=('+=' | '-=' | '*=' | '/=' | '%=') expr
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
    ;

raiseStatement
    : 'raise' expr
    ;

yieldStatement
    : 'yield' exprList?
    ;

// --- compound statements ------------------------------------------------

ifStatement
    : 'if' expr 'then' block elifClause* elseClause? 'end'
    ;

elifClause
    : 'elif' expr 'then' block
    ;

elseClause
    : 'else' block
    ;

whileStatement
    : 'while' '(' expr ')' block 'end'
    | 'while' expr block 'end'
    ;

forStatement
    : 'for' targetList 'in' exprList 'then' block 'end'
    | 'for' targetList 'in' '(' exprList ')' 'then' block 'end'
    ;

funcDeclaration
    : 'func' NAME '(' paramList? ')' block 'end'
    ;

classDeclaration
    : 'class' NAME classHeritage? block 'end'
    ;

classHeritage
    : ':' NAME
    ;

paramList
    : param (',' param)* (',' '*' NAME)?
    ;

param
    : NAME ('=' expr)?
    ;

// --- targets --------------------------------------------------------

targetList
    : target (',' target)*
    ;

target
    : NAME trailer*
    ;

// --- expressions ------------------------------------------------------

exprList
    : expr (',' expr)*
    ;

expr
    : shortLambdaExpr                           # shortLambdaExprAlt
    | lambdaExpr                                # lambdaExprAlt
    | orExpr                                    # orExprAlt
    ;

shortLambdaExpr
    : '(' paramList? ')' '=>' expr
    ;

lambdaExpr
    : 'func' '(' paramList? ')' block 'end'
    ;

orExpr
    : andExpr ('or' andExpr)*
    ;

andExpr
    : notExpr ('and' notExpr)*
    ;

notExpr
    : 'not' notExpr
    | comparison
    ;

comparison
    : additive (compOp additive)*
    ;

compOp
    : '==' | '!=' | '<' | '>' | '<=' | '>='
    ;

additive
    : multiplicative (('+' | '-') multiplicative)*
    ;

multiplicative
    : unary (('*' | '/' | '%') unary)*
    ;

unary
    : ( '-' | 'not' | INC | DEC ) unary
    | power
    ;

power
    : postfix ('**' unary)?
    ;

postfix
    : primary trailer*
    ;

trailer
    : '.' NAME                     # attrTrailer
    | '[' expr ']'                 # indexTrailer
    | '[' sliceRange ']'           # sliceTrailer
    | '(' argList? ')'             # callTrailer
    | INC                          # incTrailer
    | DEC                          # decTrailer
    ;

sliceRange
    : expr? ':' expr? (':' expr?)?
    ;

argList
    : arg (',' arg)*
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
    | '(' expr ')'                  # groupAtom
    | 'base' '(' argList? ')'      # baseAtom
    ;

listLiteral
    : '[' (expr (',' expr)* ','?)? ']'
    ;

dictLiteral
    : '{' (dictEntry (',' dictEntry)* ','?)? '}'
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

NAME
    : [a-zA-Z_] [a-zA-Z_0-9]*
    ;

NUMBER
    : DIGIT+ ('.' DIGIT+)?
    ;

fragment DIGIT
    : [0-9]
    ;

STRING
    : '"' ( ~["\\\r\n] | ESCAPE_SEQ )* '"'
    | '\'' ( ~['\\\r\n] | ESCAPE_SEQ )* '\''
    ;

FSTRING
    : '$"' ( ~["\\\r\n] | ESCAPE_SEQ | '{' | '}' )* '"'
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

LINE_JOIN
    : '\\' '\r'? '\n' -> skip
    ;