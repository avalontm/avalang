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
    : smallStatement (NEWLINE | ';' NEWLINE)+
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
    ;

// atributo con modificador(es): `static contador = 0`,
// `private vidaSecreta = 100`, `static private x = 1`.
// Reusa assignStatement tal cual (targetList '=' exprList), así que
// admite lo mismo que ya admite una asignación común -- no hace falta
// una regla de "declaración de atributo" separada.
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
    | tryStatement
    | modifiedFuncDeclaration
    | asyncFuncDeclaration
    | externStatement
    | selectStatement
    ;

// --- class member visibility/storage modifiers -------------------------
//
// Diseño: ver DISENO_visibilidad_clases_avalang.md, Fase A.
//
// Estas reglas son deliberadamente aditivas y no tocan `block`,
// `classDeclaration` ni `statement`: sintácticamente quedan válidas en
// cualquier lugar donde ya vale un statement (no solo dentro de una
// clase), igual que el resto de esta gramática no distingue contexto.
// La restricción real -- que `static`/`private` solo tengan sentido
// dentro de un cuerpo de clase -- se valida en el compilador (ver
// Fase C del documento de diseño), no acá. Esto evita reescribir
// `classDeclaration`/`block` (reutilizados por funciones, if, while,
// for, etc.) y mantiene el riesgo de romper algo existente en cero.
memberModifier
    : 'static'
    | 'private'
    ;

// func con modificador(es): `static func x() ... end`,
// `private func x() ... end`, `static private func x() ... end`.
modifiedFuncDeclaration
    : memberModifier+ funcDeclaration
    ;

// `async func x() ... end` -- mismo molde que modifiedFuncDeclaration:
// una regla aditiva separada en vez de tocar funcDeclaration/block, para
// no arriesgar romper nada existente. `await` (ver primary) solo es
// valido lexicamente dentro del cuerpo de una de estas -- eso lo valida
// el compilador (Compiler::in_async_func_), no la gramatica.
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

// --- simple statements -------------------------------------------------

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
    ;

raiseStatement
    : 'raise' expr
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

// --- select (VB6-style switch) -------------------------------------------
//
// `select expr ... case ... then ... case ... then ... else ... end`.
// Deliberadamente NO usa `end select` ni `case else`: como el resto de
// avalang, todo bloque cierra con un `end` solo, y `elseClause` (ya
// definido arriba, reusado por ifStatement) cubre el caso final. Se
// resuelve en AstBuilder desazucarando a un IfStmt equivalente (ver
// ast_builder.cpp), asi que no hace falta tocar compiler.cpp ni la VM.
selectStatement
    : 'select' expr (NEWLINE)* caseClause+ elseClause? 'end'
    ;

caseClause
    : 'case' caseItem (',' caseItem)* 'then' block
    ;

// Tres formas de comparar contra el valor de `select`:
//   90 to 100      -> valor >= 90 and valor <= 100
//   is >= 60       -> valor >= 60 (cualquier compOp)
//   <expr>         -> valor == expr (default, igualdad exacta)
caseItem
    : expr 'to' expr    # caseItemRange
    | 'is' compOp expr  # caseItemRelational
    | expr               # caseItemEquals
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

// --- extern (FFI) -------------------------------------------------------
//
// Diseño: ver EXTERN_FFI_DESIGN.md.
// `extern "lib" as Alias func Foo(a, b) func Bar() end` -- bloque de solo
// declaraciones (sin cuerpo), resuelto por el runtime contra una
// librería nativa. Requiere alias obligatorio (ver diseño, sección
// "Why Alias Is Mandatory").
externStatement
    : 'extern' STRING 'as' NAME (NEWLINE)* externFuncDeclaration* 'end'
    ;

externFuncDeclaration
    : 'func' NAME '(' externParamList? ')' (NEWLINE)*
    ;

externParamList
    : externParam (',' externParam)* (',' '*' NAME)?
    ;

externParam
    : NAME
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
    : unary (('*' | '/' | '%' | IDIV) unary)*
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
    | 'base' ('.' NAME)? '(' argList? ')' trailer*  # baseAtom
    | 'yield' exprList?             # yieldAtom
    | 'await' expr                  # awaitAtom
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

IDIV
    : '//'
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
    : '$"' FSTR_ITEM* '"'
    ;

fragment FSTR_ITEM
    : ~["\\{}\r\n]
    | ESCAPE_SEQ
    | '{{'
    | '}}'
    | '{' FSTR_INNER* '}'
    ;

// Content of a {...} interpolation inside an FSTRING. Recursive so that
// nested braces (dict literals, nested f-strings) and quoted strings
// (which may themselves contain '{'/'}' as ordinary characters) are
// consumed as a unit instead of the lexer mistaking an interpolation's
// internal '"' for the closing quote of the whole f-string.
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

// LINE_JOIN ('\' + newline -> skip) se saco de aca: era un splice sin
// contexto al estilo C -- se comia CUALQUIER backslash suelto antes de
// llegar al parser, sin importar si de verdad estaba continuando una
// expresion incompleta o no. Un caracter sin ningun proposito (ej. un '\'
// solo en su propia linea) desaparecia en silencio en vez de dar syntax
// error, y ningun .ava del repo lo usaba. Sin esta regla, un '\' que no
// forma parte de ningun otro token (STRING, ESCAPE_SEQ, etc.) cae al path
// de error normal del lexer -- reportado por AvaLangErrorListener con
// linea/columna como cualquier otro caracter invalido.