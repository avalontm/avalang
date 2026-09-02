#ifndef AVA_COMPILER_COMPILER_H
#define AVA_COMPILER_COMPILER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../ast/ast.h"
#include "../common/symbol.h"
#include "../vm/proto.h"

namespace ava {

class Compiler {
public:
    std::shared_ptr<Proto> Compile(const std::shared_ptr<Chunk>& chunk,
                                    const std::string& source_name = "");

private:
    struct JmpPatch {
        size_t instr_idx;
        int32_t offset;
        size_t target_idx;
    };

    std::shared_ptr<Proto> proto_;
    // Source line of the statement currently being compiled (see
    // CompileStmt), stamped onto every instruction Emit() produces while
    // compiling it so Proto::debug_lines can map instructions back to
    // source for error reporting. 0 = unknown (e.g. instructions emitted
    // outside any CompileStmt call, such as an implicit trailing RETURN).
    int current_line_ = 0;
    // 1-based source column matching current_line_ above; 0 = unknown.
    // Only used for AvaError reporting (debug_lines only tracks line).
    int current_col_ = 0;
    // Path of the file being compiled; stamped onto proto_->source_name
    // (top-level Compile()) and onto every sub-Compiler's proto_ (lambda,
    // free function, class method) so runtime errors can report the
    // correct file, including inside imported modules.
    std::string source_name_;
    uint16_t next_reg_ = 0;
    uint16_t max_reg_ = 0;
    uint16_t result_reg_ = 0;
    std::unordered_map<std::string, uint16_t> locals_;
    // Phase 4 of AvaLang_Plan_Sistema_de_Tipos.md. Scoped the same way as
    // locals_ above: one map per Compiler instance, so a function/method/
    // lambda's own sub-Compiler (see CompileFunc, CompileClass, LambdaExpr)
    // has its own symbols_, separate from its parent's -- same lifetime and
    // shadowing behavior as locals_, just tracking type info instead of a
    // register. Also used for the root/top-level Compiler's globals (see
    // DeclareSymbol), since AvaLang has no separate global symbol table
    // today. Populated by DeclareSymbol (see CompileStmt/CompileExprToReg's
    // AssignStmt handling); nothing reads from it yet -- that starts in
    // Phase 5 (inference reads/writes inferredType) and Phase 6
    // (validation reads declaredType/effectiveType to check assignments).
    std::unordered_map<std::string, Symbol> symbols_;
    // Phase 9 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de
    // parámetros en llamadas"). Same scoping as symbols_/locals_ above --
    // one map per Compiler instance. Populated by the free function
    // CollectFuncSignatures (compiler.cpp), called from CompileChunk
    // exactly where RejectDuplicateFuncDefs already is, so it runs on
    // every chunk this Compiler ever compiles (the top-level module body,
    // or -- since CompileChunk is also called directly on `this` for
    // if/while/for bodies, not just via a fresh `Compiler sub` -- any
    // nested block sharing this same instance; see the symbols_ comment
    // above for why that sharing is the existing architecture, not new
    // here). Maps a function name to its parameters' (name, declared
    // Type) pairs, resolved from FuncDef::param_types (Phase 8) via
    // TypeFromName -- Type::Unknown for an unannotated parameter.
    // Consumed by CheckCallArgs below. Known gap, left for whenever it's
    // actually needed: a call from inside one function's body to a
    // DIFFERENT function's top-level definition isn't checked, because
    // each function compiles in its own `Compiler sub` with its own empty
    // known_funcs_ -- there is no cross-instance/global signature table
    // yet (same category of limitation as InferExprType's NameExpr
    // lookup not following upvalues, Phase 5).
    std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>> known_funcs_;
    // Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de
    // retornos"). Companion to known_funcs_ above: same population site
    // (CollectFuncSignatures, extended this phase to also fill this map)
    // and the exact same scoping/limitations (per-Compiler-instance,
    // current chunk only -- see known_funcs_'s comment for why). Maps a
    // function name to its resolved return Type (Phase 8's `return_type`
    // string via TypeFromName -- Type::Unknown for no annotation or an
    // unrecognized type name). Consumed by InferExprType's CallExpr case
    // so a call's own value participates in inference wherever the callee
    // is a plain NameExpr with a known signature in this chunk (e.g.
    // `y = add(1, 2)` now infers y -> int when `add` declares `as int`);
    // everything else (obj.method(), a lambda stored in a variable, a call
    // to a function outside this chunk) still infers Type::Unknown, same
    // gap as known_funcs_ itself.
    // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md: class-aware (TypeRef,
    // not plain Type) since a free function's declared return type can
    // itself be a class name (`func make() as User ... end`) -- see
    // ResolveTypeName. Populated alongside known_funcs_ above by the same
    // CollectFuncSignatures call, same per-Compiler-instance scoping.
    std::unordered_map<std::string, TypeRef> known_func_returns_;
    // Bugfix (Aug 2026 build/test pass): companion to known_funcs_ above,
    // same population site (CollectFuncSignatures) and same
    // per-Compiler-instance/current-chunk scoping. Holds every bare NAME
    // that is the target of a plain assignment (`f = ...`, `f as T = ...`,
    // `local f = ...`, etc.) at the TOP LEVEL of the chunk being compiled
    // (module top level, or any chunk CollectFuncSignatures is called on --
    // same scope as known_funcs_, i.e. it does NOT recurse into
    // if/while/for/try bodies, matching CollectFuncSignatures' own
    // top-level-statements-only scan).
    //
    // Why this exists: a name assigned at top level (`f = (x) => x*2`)
    // compiles to SETGLOBAL, never touching locals_ -- see the repeated
    // `if (!is_top_level_ && ... locals_.find(...))` pattern throughout
    // this file. Before this fix, NameShadowsGlobalCallable only checked
    // locals_/upvalues/instance_attrs_, so calling such a name (`f(5)`),
    // whether at top level or from inside a nested function/method that
    // never declared `f` itself, produced a false "function 'f' is not
    // defined" from CheckCallArgs -- even though at runtime GETGLOBAL
    // would have found it just fine. Consulted by
    // NameShadowsGlobalCallable exactly like known_funcs_ is consulted by
    // CheckCallArgs, and copied to every `Compiler sub` at the same 3
    // sites known_funcs_/known_func_returns_ are (CompileFunc,
    // LambdaExpr's CompileExpr case, CompileClass's method loop) so a
    // nested scope can see top-level assigned names the same way it
    // already sees top-level function defs.
    std::unordered_set<std::string> known_top_level_globals_;
    // Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md. The CURRENT function
    // body's declared return type (Phase 8's FuncDef::return_type,
    // resolved via TypeFromName), set once per Compiler instance right
    // before CompileChunk(func->body) runs -- CompileFunc for a free
    // function, and the method-compiling loop in CompileClass, both set
    // this on their own `sub` before compiling the body (same "each
    // function/method compiles in its own Compiler sub" architecture as
    // is_top_level_/in_async_func_ above, so this is never inherited from
    // an enclosing function and never leaks into a nested one).
    // Type::Unknown here means "nothing to check": either this Compiler
    // instance never got a function body at all (module top level, or an
    // `if`/`while`/`for` block sharing its enclosing function's instance),
    // or the function genuinely has no `as Type` after its parameter list.
    // Phase 14 of AvaLang_Plan_Sistema_de_Tipos.md ("Lambdas y funciones
    // como valores"): LambdaExpr's own `Compiler sub` (CompileExpr's
    // LambdaExpr case) now sets this too, exactly like CompileFunc does,
    // from LambdaExpr::return_type -- so `return` inside a lambda that
    // declares `(x as int) as int => ...` (or the `func(x as int) as int
    // ... end` anonymous form) is checked the same way a named function's
    // is. A lambda with no return annotation (including the bare
    // `x => expr` form, which has no syntax for one at all) leaves this
    // Type::Unknown, same "nothing to check" convention as everywhere
    // else.
    // Read by CheckReturnType, called from both ReturnStmt sites
    // (CompileStmt and its CompileExprToReg mirror).
    // Phase 13: class-aware (TypeRef) for the same reason as
    // known_func_returns_ above -- a method or free function can declare
    // `as SomeClass` as its return type.
    TypeRef current_return_type_;
    // true solo para el Compiler raíz (nivel de módulo/script, ver
    // Compile()). Cada función/método/lambda compila en su propio
    // `Compiler sub` (CompileFunc, CompileClass, LambdaExpr) y ese sub
    // pone esto en false. Decide si una asignación a un nombre simple
    // (`x = ...`, con o sin `local` delante -- la gramática descarta esa
    // palabra clave, ver ast_builder.cpp:visitLocalStatement) se
    // resuelve como variable local a ese scope (registro persistente) o
    // como variable global (SETGLOBAL/GETGLOBAL), ver CompileStmt.
    bool is_top_level_ = true;
    // true solo dentro del `Compiler sub` que compila el cuerpo de un
    // `async func` (ver CompileFunc). Igual que is_top_level_, no se
    // hereda a un `func` normal anidado dentro de uno async -- cada
    // función/método/lambda compila en su propio `Compiler sub` desde
    // cero, así que un `func` anidado empieza con esto en false, igual
    // que en JS/Python/C# (await solo vale en el cuerpo léxico directo
    // de la función async que lo contiene). Ver CompileExpr(AwaitExpr).
    bool in_async_func_ = false;
    std::unordered_map<std::string, ClassObj*> compiled_classes_;
    // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos").
    // className -> fieldName -> resolved TypeRef. Populated in
    // CompileClass from AssignStmt::explicit_type when a class-body field
    // carries one (`name as string`), else from InferExprType(a->value)
    // for a literal default (the same literal kinds CompileClass's own
    // instance_defaults-building loop already recognizes -- nothing new
    // is inferred here beyond what that loop already reads). Inherited
    // (merged into a subclass's own entry) the same way instance_defaults
    // is. Consumed by InferExprTypeRef's AttrExpr case, so `object.field`
    // resolves to a real type wherever `object` itself resolves to a
    // known class.
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> class_field_types_;
    // Fase 3 de PLAN_VALIDACION_ESTATICA.md. className -> nombres de
    // atributo asignados como `this.<attr> = ...` en CUALQUIER método de
    // la clase (recorriendo if/while/for/try y lambdas anidadas -- ver
    // CollectDynamicThisAttrs en compiler.cpp), sin importar si ese
    // nombre tiene también una declaración `var attr = ...` en el cuerpo
    // de la clase. AvaLang permite atributos dinámicos (SETATTR sobre
    // cualquier nombre, ver AssignStmt's AttrExpr-target branch en
    // CompileStmt), así que class_field_types_ -- que solo ve campos
    // declarados -- NO alcanza para saber si `obj.cb()` es una llamada a
    // un método real o a un campo que guarda una lambda asignada
    // dinámicamente (`this.cb = func() ... end`). CheckMethodCallArgs
    // consulta este mapa, además de class_field_types_, antes de decidir
    // que un `obj.attr(...)` sin método real es un error -- mismo
    // "no false positive" que NameShadowsGlobalCallable aplica del lado
    // de las llamadas libres. Heredado igual que class_field_types_.
    std::unordered_map<std::string, std::unordered_set<std::string>> class_dynamic_attrs_;
    // className -> methodName -> resolved return TypeRef. Mirrors
    // known_func_returns_ but keyed by (class, method) instead of a bare
    // name, closing the gap CollectFuncSignatures' own comment documents
    // ("methods are called via AttrExpr ... never looks them up here").
    // "__init__" (the constructor) is included like any other method.
    // Populated in CompileClass's method-compiling loop, right where
    // sub.current_return_type_ is set. NOT inherited with "__init__"
    // excluded, same as CompileClass's own method-inheritance loop
    // (a subclass's constructor is never copied from its base).
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> class_method_returns_;
    // className -> methodName -> (param name, declared Type) list. Plain
    // Type (primitives only), NOT TypeRef -- same scope limit
    // CollectFuncSignatures already has for free-function parameters (a
    // parameter typed `as SomeClass` is left Type::Unknown, "nothing to
    // check", rather than extended to full class-awareness here). Powers
    // CheckMethodCallArgs, the obj.method(...) companion to CheckCallArgs.
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>>
        class_method_params_;
    // Phase 16 of AvaLang_Plan_Sistema_de_Tipos.md ("extern"). Alias ->
    // funcName -> (param name, declared Type) list -- the exact same
    // shape and scope limit as class_method_params_ right above (plain
    // Type only, not TypeRef: an extern parameter typed `as SomeClass`
    // would resolve Type::Unknown here, same "nothing to check" outcome
    // CollectFuncSignatures/class_method_params_ already give a
    // class-typed free-function/method parameter -- the plan gives
    // `extern` no reason to be more permissive than those). Populated in
    // CompileExtern, in file order -- same "must be defined earlier in
    // the file" convention compiled_classes_/class_method_params_ already
    // has (there is no hoisting/pre-pass for `extern`, same as `class`).
    // Powers CheckMethodCallArgs's extern branch (Alias.Func(...) is an
    // AttrExpr callee, exactly like obj.method(...)).
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, Type>>>>
        extern_func_params_;
    // Phase 16. Alias -> funcName -> resolved return TypeRef. Mirrors
    // class_method_returns_ (TypeRef, not plain Type, for the same reason
    // known_func_returns_/class_method_returns_ already are -- an
    // extern function's declared return type could in principle name a
    // class via ResolveTypeName, even though a native call can't actually
    // produce a class instance at runtime; kept TypeRef-typed anyway for
    // uniformity with every other *_returns_ map rather than special-
    // casing extern to a plainer type here). Populated alongside
    // extern_func_params_ in CompileExtern. Consumed by
    // InferExprTypeRef's CallExpr/AttrExpr-callee branch, the same site
    // that already reads class_method_returns_ for obj.method().
    std::unordered_map<std::string, std::unordered_map<std::string, TypeRef>> extern_func_returns_;
    // Fase 4 de PLAN_VALIDACION_ESTATICA.md. `import mod` (un solo
    // segmento, sin `as alias`) hace que VM::DoImport/PlaceModuleInScope
    // (vm_import.cpp) vuelque TODOS los globals de nivel superior del
    // módulo -- funciones incluidas -- directo al scope global de quien
    // importa (mismo comportamiento que "from X import *"), en runtime,
    // vía SetGlobal por cada entrada. El compiler no tiene forma de saber
    // qué nombres son esos: un módulo nativo los arma en C++
    // (builtin_names.h no los lista, porque no son builtins globales) y
    // un módulo basado en archivo recién se compila cuando ava_run()
    // ejecuta el import, no durante esta compilación. Sin este flag,
    // CheckCallArgs (Fase 2) marcaría como "not defined" cualquier
    // función legítima traída así -- un falso positivo real, no
    // hipotético (ver samples/test/main.ava: `import system` seguido de
    // `Console.WriteLine(...)`, aunque ese caso puntual ya es inofensivo
    // porque Console se usa vía AttrExpr, no como llamada libre; el caso
    // que sí rompe es un módulo de archivo que exporta una función usada
    // como `greet()` en vez de `mod.greet()`). CompileImport lo enciende
    // apenas ve un import con esa forma; de ahí en más, mismo "no false
    // positive" que el resto del archivo: CheckCallArgs deja de poder
    // afirmar que una función NO existe, así que dejar de intentarlo. No
    // hace falta para CheckMethodCallArgs (Fase 3): un AttrExpr como
    // `mod.func()` ya es seguro sin este flag, porque `mod`/el alias
    // nunca queda en symbols_ (no hay `var mod = ...`), así que
    // InferExprTypeRef ya lo resuelve como tipo desconocido y
    // CheckMethodCallArgs ya hace `return` antes de poder tirar error.
    // Heredado hacia cada `Compiler sub` en los mismos 3 sitios que ya
    // copian known_funcs_ (lambda, CompileFunc, método de clase): un
    // import de nivel superior debe seguir protegiendo llamadas dentro
    // de funciones/métodos/lambdas anidados en el mismo chunk.
    bool has_wildcard_import_ = false;
    ClassObj* current_base_class_ = nullptr;
    bool is_init_ = false;
    std::unordered_set<std::string> instance_attrs_;
    std::vector<std::pair<std::string, uint16_t>> parent_locals_;
    Compiler* parent_ = nullptr;

    std::vector<JmpPatch> pending_breaks_;
    std::vector<JmpPatch> pending_continues_;

    // Bug #42: pila de bloques 'finally' de los try/except que se estan
    // compilando en este momento (uno por cada TryStmt anidado que
    // todavia no termino de compilarse), de mas interno a mas externo.
    // CompileStmt lo consulta al compilar un ReturnStmt para inyectar
    // esos finally's (mas interno primero) antes del OpCode::RETURN --
    // ver CompileTry y CompileStmt. Guarda punteros porque vive dentro
    // del AST (TryStmt ya es dueño del vector), no hace falta copiarlo.
    std::vector<const std::vector<std::shared_ptr<StmtNode>>*> pending_finally_stack_;

    uint32_t for_depth_ = 0;

    // Fase 1 del plan break/continue: cuantos bucles (while/for/for-range)
    // envuelven el punto que se esta compilando ahora mismo, en este mismo
    // Compiler instance (cada func/metodo/lambda compila en su propio
    // `Compiler sub`, asi que esto nunca "atraviesa" un limite de funcion,
    // igual que in_async_func_/is_top_level_ arriba). CompileWhile y
    // CompileForIterator/CompileForRange lo incrementan antes de compilar
    // su cuerpo y lo decrementan despues, en cada punto de salida --
    // mismo patron que for_depth_ ya usa. BreakStmt/ContinueStmt (en
    // CompileStmt y su espejo CompileExprToReg) lo consultan: si es 0,
    // no hay bucle contenedor y se lanza un AvaError en vez de emitir un
    // JMP que jamas se parchea (bug real: antes de esta fase, `break`/
    // `continue` fuera de un bucle compilaba "bien" pero dejaba un salto
    // sin destino real, un bug silencioso en vez de un error de
    // compilacion).
    int loop_depth_ = 0;

    void Reset();

    uint16_t AllocReg();
    void FreeRegs(uint16_t count);
    uint16_t AddConstant(const Value& v);
    static Value MakeString(const std::string& s);

    void Emit(OpCode op, uint16_t a = 0, uint16_t b = 0, uint16_t c = 0);

    // Punto UNICO donde current_line_/current_col_ se actualizan a partir de
    // una statement. CompileStmt y CompileExprToReg son los dos puntos de
    // entrada que compilan una StmtNode "de punta a punta" (CompileChunk
    // manda la ultima statement del chunk por CompileExprToReg en vez de
    // CompileStmt, para soportar el implicit-return del ultimo valor) --
    // los dos DEBEN llamar a esto antes de emitir nada, o current_line_
    // queda pegado en lo que dejo la statement anterior y los errores se
    // reportan en la statement equivocada. Si en el futuro aparece un
    // tercer punto de entrada de este tipo, tiene que llamar a esto tambien
    // en vez de reimplementar el check.
    void StampLine(const std::shared_ptr<StmtNode>& stmt);

    // Phase 4 of AvaLang_Plan_Sistema_de_Tipos.md: records/updates a name
    // in symbols_ when an AssignStmt declares or assigns it (see
    // CompileStmt/CompileExprToReg). declared_type is
    // TypeFromName(AssignStmt::explicit_type) -- Type::Unknown when the
    // statement carries no `as Type` annotation (plain `x = expr`, or a
    // plain reassignment to an already-declared name). inferred_type
    // (Phase 5) is InferExprType(a->value) at the call site -- Type::Unknown
    // for a type declaration with no initializer, or any expression form
    // InferExprType doesn't resolve. A brand-new name gets a fresh Symbol;
    // an existing one only has declaredType/inferredType overwritten when
    // this call actually supplies something other than Type::Unknown for
    // that field (Type::Unknown never downgrades an already-known value) --
    // detecting a real conflict between two different explicit annotations,
    // or between successive inferred types, for the same name is Phase
    // 6/7's job, not this one's. effectiveType is refreshed either way.
    //
    // Phase 15 of AvaLang_Plan_Sistema_de_Tipos.md ("Colecciones"): the two
    // Type+class-name pairs this used to take were replaced with two full
    // TypeRef parameters, so a name assigned a list/dict literal also gets
    // its element_type/key_type (see type.h's TypeRef and symbol.h's
    // Symbol) recorded alongside declaredType/inferredType, not just the
    // Type::List/Type::Dict tag with the element type thrown away -- same
    // "only overwrite when this call's TypeRef.type isn't Unknown" rule as
    // before, now applied to the whole TypeRef (type + class_name +
    // element_type + key_type move together, never partially).
    void DeclareSymbol(const std::string& name, const TypeRef& declared, const TypeRef& inferred = TypeRef{});

    // Phase 13 of AvaLang_Plan_Sistema_de_Tipos.md ("Clases y objetos").
    // Resolves an `as Type` spelling to either a primitive (TypeFromName,
    // unchanged) or, when that fails, a class name already present in
    // compiled_classes_ -- same "must be defined earlier in the file"
    // ordering CompileClass already enforces for base classes (section 6's
    // rule extended to annotations/return types). Neither primitive nor a
    // known class resolves to {Type::Unknown, ""}, same "unknown type"
    // meaning ValidateTypeAnnotation already reports for a typo'd
    // primitive name.
    TypeRef ResolveTypeName(const std::string& name);

    // Phase 13. TypeRef-returning companion to InferExprType above -- same
    // walk, but also resolves WHICH class an Object-typed result belongs
    // to, for the handful of expression forms that can actually produce
    // one: a NameExpr already in symbols_, a CallExpr instantiating a
    // known class or calling a known free function/method with a
    // class-typed return, and an AttrExpr resolving a known class's field.
    // InferExprType(expr) itself is unchanged and still returns
    // Type::Unknown for all of these (see its own Phase 12 comment) --
    // callers that don't need the class name keep using it as before;
    // only the new class-aware call sites (annotation/reassignment/
    // return-type validation, method-call argument checks) use this.
    // Phase 15 of AvaLang_Plan_Sistema_de_Tipos.md ("Colecciones") extended
    // this walk with real branches for ListExpr/DictExpr (element/value
    // type from the literal's own items, when they agree on one type) and
    // for IndexExpr/SliceExpr (reads the element/value type back off
    // whatever InferExprTypeRef(obj) resolves to) -- closing the gap the
    // Phase 12 audit comment on InferExprType explicitly left for this
    // phase to close ("`items[0]`... needs collection element types (Phase
    // 15)"). See the definition in compiler.cpp for exactly what still
    // isn't resolved (nested collections beyond one level of recursion
    // through TypeRef::element_type work automatically since the TypeRef
    // itself recurses, but mixed-type literals and empty literals still
    // fall back to "unknown element type", same convention as everywhere
    // else in this type system).
    TypeRef InferExprTypeRef(const std::shared_ptr<ExprNode>& expr);

    // Phase 5 of AvaLang_Plan_Sistema_de_Tipos.md ("Inferencia"). Infers the
    // ava::Type of an expression without compiling it (no bytecode emitted,
    // no registers touched) -- purely a lookup/walk over the AST, called
    // from DeclareSymbol's call sites right before/alongside CompileExpr on
    // the same value. Handles: number/string/f-string/bool literals; a
    // NameExpr already in symbols_ (its current effectiveType, searching
    // this Compiler's scope only -- no upvalue/parent lookup yet, same
    // scoping caveat as symbols_ itself); unary `not` (-> Bool), unary
    // neg/inc/dec (-> operand's type); binary comparison/logical operators
    // (-> Bool, per AvaLang's own semantics -- whether the operands
    // themselves are compatible is Phase 11, not checked here); and binary
    // arithmetic where both operands resolve to the same numeric type (->
    // that type) or one Int and one Float (-> Float, numeric promotion).
    // Anything else (calls, indexing, slicing, attributes, lists/dicts,
    // lambdas, base/await/yield, or an operand that itself infers Unknown)
    // returns Type::Unknown -- those are later phases (8-15) to resolve, not
    // failures of this one.
    Type InferExprType(const std::shared_ptr<ExprNode>& expr);

    // Phase 7 of AvaLang_Plan_Sistema_de_Tipos.md ("Asignaciones
    // posteriores"). Called from the three AssignStmt branches that call
    // DeclareSymbol for a NameExpr target with a value (has_local,
    // brand-new local, and top-level global -- see CompileStmt/
    // CompileExprToReg), right before DeclareSymbol overwrites whatever
    // symbol already exists for `name`. No-op when `a->explicit_type` is
    // non-empty (this line has its own `as Type`, already checked by
    // ValidateTypeAnnotation against ITS OWN annotation -- see
    // ValidateReassignment's comment in compiler.cpp for why a second,
    // different explicit annotation isn't cross-checked against an older
    // one here) or when `a->is_local` (a `local` declaration always starts
    // fresh, plan section 11, "Scope y shadowing con `local`" -- see
    // AssignStmt::is_local in ast.h for the current architecture's limits
    // on what `local` can actually shadow). Otherwise looks `name` up in
    // symbols_ and defers to the free function ValidateReassignment.
    void CheckReassignment(const AssignStmt* a, const std::string& name, const TypeRef& inferred);

    // Phase 9 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de
    // parámetros en llamadas"). See the definition in compiler.cpp for
    // what it does and does not check.
    void CheckCallArgs(const std::string& func_name, const CallExpr* c);
    // Fase 2 de PLAN_VALIDACION_ESTATICA.md. true si `name` resuelve a
    // algo que NO es un global (local, parámetro, upvalue, o this.attr en
    // un método) -- espejo exacto de las condiciones que la rama NameExpr
    // de CompileExpr ya chequea antes de emitir GETGLOBAL. CheckCallArgs
    // lo consulta antes de reportar "function is not defined": un nombre
    // así podría ser una lambda guardada en una variable, no un typo.
    bool NameShadowsGlobalCallable(const std::string& name) const;

    // Phase 13. obj.method(...) companion to CheckCallArgs above -- see
    // class_method_params_'s comment. Shared argument-checking loop
    // between the two lives in CheckCallArgsAgainst.
    void CheckMethodCallArgs(const AttrExpr* callee, const CallExpr* c);
    void CheckCallArgsAgainst(const std::vector<std::pair<std::string, Type>>& params,
                               const std::string& label, const CallExpr* c);

    // Phase 10 of AvaLang_Plan_Sistema_de_Tipos.md ("Validación de
    // retornos"). See the definition in compiler.cpp for what it does and
    // does not check, including the "empty return in a non-void function"
    // decision (plan section 14).
    void CheckReturnType(const ReturnStmt* r);

    // Phase 11 of AvaLang_Plan_Sistema_de_Tipos.md ("Operadores"). See the
    // static ValidateBinOpTypes/ValidateUnOpTypes definitions in
    // compiler.cpp for the actual compatibility table and the reasoning
    // behind it (built from vm_arith.cpp/vm_compare.cpp's real behavior,
    // not the plan's illustrative table verbatim -- e.g. '+' with a String
    // operand never errors, since it concatenates at runtime; '==', '!=',
    // 'and', 'or' are never checked, since none of them can actually throw
    // at runtime for any operand types).
    void CheckBinOpTypes(const BinOpExpr* b);
    void CheckUnOpTypes(const UnOpExpr* u);

    uint16_t CompileExpr(const std::shared_ptr<ExprNode>& expr);
    void CompileStmt(const std::shared_ptr<StmtNode>& stmt);
    void CompileChunk(const std::vector<std::shared_ptr<StmtNode>>& stmts);
    uint16_t CompileExprToReg(const std::shared_ptr<StmtNode>& stmt);

    void PatchJump(size_t instr_idx);
    void PatchContinueJump(size_t instr_idx, size_t loop_start);

    void CompileIf(const IfStmt* stmt);
    void CompileWhile(const WhileStmt* stmt);
    void CompileFor(const ForStmt* stmt);
    // Fase 4 del plan break/continue/operadores: `for i = a to b [step s]`.
    void CompileForRange(const ForRangeStmt* stmt);
    void CompileFunc(const FuncDef* func);
    void EmitDefaultsPrologue(const std::vector<std::pair<std::string, std::shared_ptr<ExprNode>>>& params,
                               uint16_t param_reg_base);
    void CompileClass(const ClassDef* cls);
    void CompileImport(const ImportStmt* stmt);
    void CompileExtern(const ExternStmt* stmt);
    void CompileTry(const TryStmt* stmt);
    void CompileRaise(const RaiseStmt* stmt);
    void CompileMultiAssign(const MultiAssignStmt* stmt);
    uint16_t CompileFStringExpression(const std::string& expr_str);

    enum class IteratorKind { List, Coroutine, Dict };
    IteratorKind DetectIteratorKind(const std::shared_ptr<ExprNode>& iterable);
    void CompileForIterator(const ForStmt* stmt);
    void CompileForList(const ForStmt* stmt, uint32_t depth);
    void CompileForCoroutine(const ForStmt* stmt, uint32_t depth);
    void CompileForDict(const ForStmt* stmt, uint32_t depth);
    void CompileForDynamic(const ForStmt* stmt, uint32_t depth);

    std::shared_ptr<ExprNode> ParseFStringExpr(const std::string& expr_str);

    static OpCode BinOpToOpcode(BinOp op);
    static bool IsShortCircuit(BinOp op);

    int16_t FindUpvalue(const std::string& name);

private:
    std::shared_ptr<ExprNode> ParseExpr(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseOrExpr(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseAndExpr(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseComparison(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseAddSub(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseMulDiv(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParseUnary(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParsePower(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParsePostfix(const std::string& s, size_t& pos);
    std::shared_ptr<ExprNode> ParsePrimary(const std::string& s, size_t& pos);
};

} // namespace ava

#endif // AVA_COMPILER_COMPILER_H