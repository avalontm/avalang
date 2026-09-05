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
    ;

// --- type annotations (`as Type`) ---------------------------------------
//
// Design: see AvaLang_Plan_Sistema_de_Tipos.md, Phase 2. Deliberately kept
// separate from `assignStatement`/`target` instead of extending those:
// `target` (NAME trailer*) also covers assignment to indices/attributes
// (`items[0] = x`, `obj.attr = x`), where a type annotation makes no
// semantic sense -- the plan only covers annotating simple variable
// declarations (a bare NAME). Keeping these rules separate makes
// `items[0] as int = x` invalid at the syntax level, and leaves
// `assignStatement`/`target`/`targetList` untouched for everything else
// (multi-assign, augAssign, incDec, for, `static`/`private`, etc.).
//
// `as` is already used by `importStatement` and `externStatement` with a
// different meaning (alias); there is no ambiguity because those start
// with the `import`/`extern` keywords, while this production starts
// directly with NAME, same as `assignStatement`.
typedAssignStatement
    : NAME typeAnnotation '=' expr
    ;

// Type declaration without an initializer (`age as int`, see Phase 2 and
// plan section 11, "Declaracion sin inicializar"). What value `age` holds
// before the first assignment, and the rest of the semantics, are left to
// later phases (Phase 4 onward); this only enables the syntax.
typedDeclStatement
    : NAME typeAnnotation
    ;

typeAnnotation
    : 'as' NAME
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

// `local` shadows/forces a new declaration in the current scope (see plan,
// Phase 7, "Scope y shadowing con local"): applies equally to a plain
// assignment (`local x = 1`) and to this phase's new typed forms
// (`local x as int = 1`, `local x as int`).
localStatement
    : 'local' assignStatement
    | 'local' typedAssignStatement
    | 'local' typedDeclStatement
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
    : expr NAME expr     # caseItemRange
    | 'is' compOp expr  # caseItemRelational
    | expr              # caseItemEquals
    ;

whileStatement
    : 'while' '(' expr ')' block 'end'
    | 'while' expr block 'end'
    ;

// La tercera alternativa (Fase 4 del plan break/continue/operadores) es
// el bucle numerico estilo VB6 `for i = a to b step s`. Deliberadamente
// SIN alt labels (igual que las dos alternativas ya existentes) -- las
// tres comparten un mismo ForStatementContext y se distinguen en
// AstBuilder::visitForStatement por `ctx->targetList() == nullptr`
// (unica de las tres que no pasa por targetList/exprList/'in', asi que
// no hay ambiguedad: se distingue en el segundo token, '=' vs 'in').
forStatement
    : 'for' targetList 'in' exprList 'then' block 'end'
    | 'for' targetList 'in' '(' exprList ')' 'then' block 'end'
    | 'for' NAME '=' expr NAME expr (NAME expr)? 'then' block 'end'
    ;

// `returnType` (`as Type` after the parameter list, before the body) is
// optional and additive -- see plan, Phase 2, "Funciones". Only added to
// `funcDeclaration` (named func) in this phase; `lambdaExpr`/
// `shortLambdaExpr` get their own type syntax in Phase 14 (lambdas), which
// reuses `typeAnnotation` differently (see plan section 18) and does not
// touch this rule.
funcDeclaration
    : 'func' NAME '(' paramList? ')' returnType? block 'end'
    ;

returnType
    : typeAnnotation
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

// Phase 16 of AvaLang_Plan_Sistema_de_Tipos.md ("extern"). `typeAnnotation?`
// on externParam and `returnType?` after the parameter list mirror
// `param`/`funcDeclaration` exactly (see those rules' own comments) --
// same rule reused, not a new one, so there is nothing new to disambiguate
// against `extern "lib" as Alias`'s own `as` (that one is consumed before
// `externFuncDeclaration*` is even reached, same non-ambiguity the grammar
// already documents above for `typedAssignStatement` vs `importStatement`/
// `externStatement`). An extern function still has no body/defaults (no
// native value to fall back to), so unlike `param` this does NOT gain
// `('=' expr)?` -- only the type annotation part of that phase's syntax
// applies here.
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

// `typeAnnotation?` enables `a as int` (see plan, Phase 2, "Funciones").
// Placed before the default (`= expr`), matching the plan's example order
// (`func add(a as int, b as int) as int`); a parameter with both a type
// and a default (`a as int = 10`) also becomes valid through this rule,
// which the plan doesn't explicitly ask for in this phase but falls out
// naturally from combining the two existing rules at no extra grammar cost.
param
    : NAME typeAnnotation? ('=' expr)?
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
    | singleParamLambdaExpr                     # singleParamLambdaExprAlt
    | lambdaExpr                                # lambdaExprAlt
    | ternaryExpr                                # orExprAlt
    ;

// Fase 3 del plan break/continue/operadores: `cond ? then : else`.
// Asociativo a la derecha (permite anidado `a ? b : c ? d : e`) porque
// las ramas `then`/`else` son `expr` completo, que puede volver a matchear
// otro ternaryExpr. Nivel de precedencia mas bajo que orExpr, mismo lugar
// que ocupa en C/JS -- por eso reemplaza a `orExpr` como cuerpo del alt
// `orExprAlt` en vez de agregarse como alternativa nueva de `expr`.
ternaryExpr
    : orExpr ('?' NEWLINE* expr NEWLINE* ':' NEWLINE* expr)?
    ;

// Phase 14 of AvaLang_Plan_Sistema_de_Tipos.md ("Lambdas y funciones como
// valores"). `singleParamLambdaExpr` is new this phase: the untyped,
// paren-less single-parameter form from the plan's first example
// (`callback = x => x * 2`). It has no way to carry a `typeAnnotation` --
// that is exactly the plan's own rule (section 18: "en cuanto se anota
// algún parámetro con `as Type`, los paréntesis pasan a ser obligatorios"),
// enforced here structurally (there is no `as Type` slot in this rule at
// all) rather than as a separate check later. Placed before `shortLambdaExpr`
// so a bare `x => expr` doesn't need to fall through; ANTLR4's ALL(*)
// prediction picks the matching alternative by looking past the NAME to
// the `=>`, same non-ambiguity argument as `typedAssignStatement` vs
// `assignStatement` above (both start with NAME, differ later).
singleParamLambdaExpr
    : NAME '=>' expr
    ;

// `returnType?` mirrors `funcDeclaration` (section "Funciones" comment
// above): reuses the same `typeAnnotation` rule, placed after the
// parameter list and before `=>`/the body -- never after the body, to
// avoid the cast ambiguity noted in plan section 21. Progression per the
// plan (section 18): `(x) => x*2` (no types) already worked before this
// phase via `paramList`'s existing `typeAnnotation?` on `param`; this
// phase adds the RETURN type slot on both lambda forms, symmetric with
// what `funcDeclaration` already had since Phase 2.
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
    | comparison
    ;

comparison
    : bitOr (compOp NEWLINE* bitOr)*
    ;

compOp
    : '==' | '!=' | '<' | '>' | '<=' | '>='
    ;

// --- bitwise operators ---------------------------------------------------
//
// Ver AvaLang_Plan_Break_Continue_Operadores.md, Fase 2. Insertados entre
// `comparison` y `additive`, jerarquia estandar estilo C/Python/JS: los
// bitwise quedan por debajo de comparacion (`a & b == c` es raro pero se
// parsea como `a & (b == c)`... no -- al reves: comparison llama a bitOr,
// asi que `a & b == c` se parsea como `(a & b) == c`, igual que en la
// mayoria de esos lenguajes salvo C, donde es al reves; se eligio el
// orden mas intuitivo) y por encima de additive (`a + b & c` es
// `(a + b) & c`, igual que C/Python). AND/XOR/OR bitwise son tres niveles
// separados (misma jerarquia que esos lenguajes: `&` mas fuerte que `^`,
// `^` mas fuerte que `|`). `~` (complemento) se agrega a `unary`, junto a
// `-`/`not`. Solo operan sobre Number (truncados a entero en runtime,
// mismo criterio que IDIV) -- ver vm_arith.cpp OpBand/OpBor/etc.
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