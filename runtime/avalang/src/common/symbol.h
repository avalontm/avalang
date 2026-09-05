#ifndef AVA_COMMON_SYMBOL_H
#define AVA_COMMON_SYMBOL_H

#include <memory>
#include <string>

#include "type.h"

// Phase 4 ("Tabla de simbolos"). Symbol
// is the per-name record the compiler's symbol table keeps once a name is
// declared (see Compiler::symbols_ and Compiler::DeclareSymbol in
// src/compiler/compiler.h/.cpp), so later phases have somewhere to look a
// name's type up: Phase 5 (inference), Phase 6 (validating annotations),
// Phase 7 (validating reassignments), and so on.
//
// Plan example (section 8):
//   age as int = 25   -> declaredType=Int, inferredType=Int, effectiveType=Int
//   age = 25          -> declaredType=Unknown, inferredType=Int, effectiveType=Int
//
// This phase only introduces the structure and starts recording
// declaredType, straight from an `as Type` annotation when the AST carries
// one (see ast.h, AssignStmt::explicit_type, Phase 3). inferredType is NOT
// computed yet -- that is Phase 5 -- so it stays Type::Unknown for every
// symbol for now, and effectiveType only ever reflects declaredType (or
// Unknown when there isn't one) until Phase 5 gives it something real to
// fall back to.

namespace ava {

struct Symbol {
    std::string name;
    Type declaredType = Type::Unknown;   // from `as Type`; Unknown = no annotation
    Type inferredType = Type::Unknown;   // Phase 5, not implemented yet
    Type effectiveType = Type::Unknown;  // declaredType if present, else inferredType

    // Phase 13 ("Clases y objetos").
    // Parallel to declaredType/inferredType/effectiveType above, meaningful
    // only when the corresponding *Type field is Type::Object (a class
    // instance, see type.h's TypeRef). Kept as separate fields instead of
    // switching declaredType/inferredType/effectiveType to TypeRef so every
    // existing comparison against them elsewhere in the compiler (`==
    // Type::Unknown`, `!= inferred_type`, etc.) keeps compiling unchanged --
    // only code that specifically needs to know WHICH class reads these.
    std::string declaredClassName;
    std::string inferredClassName;
    std::string effectiveClassName;

    // Phase 15 ("Colecciones").
    // Parallel to declaredClassName/inferredClassName/effectiveClassName
    // above, meaningful only when the corresponding *Type field is
    // Type::List (elementType only) or Type::Dict (keyType + elementType,
    // elementType reused as the value type -- see TypeRef's comment in
    // type.h for why). nullptr means "element/key type not resolved" (an
    // empty list literal, a list whose elements don't all agree, or a
    // list/dict this symbol never got assigned a literal for), which is
    // NOT the same as the *Type field itself being Type::Unknown.
    std::shared_ptr<TypeRef> declaredElementType;
    std::shared_ptr<TypeRef> inferredElementType;
    std::shared_ptr<TypeRef> effectiveElementType;
    std::shared_ptr<TypeRef> declaredKeyType;
    std::shared_ptr<TypeRef> inferredKeyType;
    std::shared_ptr<TypeRef> effectiveKeyType;

    // Recomputes effectiveType (and effectiveClassName/effectiveElementType/
    // effectiveKeyType) from declaredType/inferredType (and their
    // class-name/element-type/key-type counterparts). Kept as its own step
    // (instead of inlined at every call site) so Phase 5 only has to touch
    // this one place once inferredType actually gets computed.
    void RefreshEffectiveType() {
        effectiveType = (declaredType != Type::Unknown) ? declaredType : inferredType;
        effectiveClassName = (declaredType != Type::Unknown) ? declaredClassName : inferredClassName;
        effectiveElementType = (declaredType != Type::Unknown) ? declaredElementType : inferredElementType;
        effectiveKeyType = (declaredType != Type::Unknown) ? declaredKeyType : inferredKeyType;
    }
};

} // namespace ava

#endif // AVA_COMMON_SYMBOL_H
