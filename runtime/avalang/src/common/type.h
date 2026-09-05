#ifndef AVA_COMMON_TYPE_H
#define AVA_COMMON_TYPE_H

#include <memory>
#include <string>

// Phase 1 ("Definir el sistema de
// tipos"). This header only establishes the vocabulary of types that the
// rest of the type system will build on. It intentionally does NOT touch
// the grammar, the AST, the symbol table, or the compiler yet -- those are
// later phases (2, 3, 4, ...) of the same plan.
//
// Initial primitive set, per the plan (section 5):
//   int, float, bool, string
//
// Reserved for later phases (do not implement yet):
//   array, list, map, function
//
// `Type::Unknown` represents "no annotation / not yet inferred" -- it is
// the value a symbol has before inference (Phase 5) runs, and is also what
// a not-yet-declared identifier resolves to. It is NOT a type a variable
// can be declared `as`; it only exists as an internal placeholder.
//
// `Type::Object` (Phase 13, "Clases y objetos") represents "instance of
// SOME user-defined class" -- WHICH class is deliberately not encoded in
// this enum (AvaLang classes are open-ended, declared by the script being
// compiled, not a fixed set this header could enumerate). Any code that
// needs to know the specific class pairs a Type::Object value with a
// class name from TypeRef below, rather than growing this enum a case at
// a time. TypeName()/TypeFromName() below intentionally do NOT handle
// Type::Object -- resolving `as SomeClassName` requires the compiler's
// own table of compiled classes (see Compiler::ResolveTypeName), which
// this header has no access to; the compiler layer builds TypeRef values
// for classes itself.
//
// `Type::List`/`Type::Dict` (Phase 15, "Colecciones") are the collection
// counterpart of `Type::Object`: "a list" / "a dict", with WHAT it holds
// left to TypeRef's element_type/key_type below, same reasoning as Object
// leaving WHICH class to TypeRef's class_name. Unlike Object, there is no
// declaration-site syntax for these yet (the plan's `List<int>` is
// explicitly conditional on AvaLang adopting generics, section 19) --
// TypeFromName() below deliberately does NOT recognize "list"/"dict" as
// annotation keywords, so `x as list` still reports "unknown type in
// annotation" exactly as before this phase. These two Type values are
// only ever produced by inference (list/dict literals, indexing into one,
// slicing a list -- see Compiler::InferExprType/InferExprTypeRef), never
// by an explicit `as` annotation, until a later phase gives them one.

namespace ava {

enum class Type {
    Unknown,  // no explicit annotation and/or not inferred yet
    Int,
    Float,
    Bool,
    String,
    Object,   // instance of a user-defined class; see TypeRef's class_name
    List,     // a list; see TypeRef's element_type (Phase 15)
    Dict,     // a dict; see TypeRef's key_type/element_type (Phase 15)
};

// Canonical source-level spelling of a type, e.g. for error messages
// ("expected int, received string") and for round-tripping an annotation
// back to source form. Matches the keywords used in `as Type` per the
// plan (section 2).
inline const char* TypeName(Type type) {
    switch (type) {
        case Type::Int:    return "int";
        case Type::Float:  return "float";
        case Type::Bool:   return "bool";
        case Type::String: return "string";
        case Type::Object: return "object";  // generic fallback -- callers
                                              // that have a class name
                                              // available use TypeRef's
                                              // DisplayType (compiler.cpp)
                                              // instead of this.
        case Type::List:   return "list";    // generic fallback -- callers
                                              // that have an element type
                                              // available use TypeRef's
                                              // DisplayType (compiler.cpp)
                                              // instead of this.
        case Type::Dict:   return "dict";    // same, for key/value types.
        case Type::Unknown:
        default:
            return "unknown";
    }
}

// Resolves a source-level type keyword (as it will appear after `as` once
// Phase 2 wires this into the grammar) to a Type. Returns Type::Unknown for
// anything not yet recognized as a primitive type name -- callers in later
// phases (e.g. the parser/AST builder) are responsible for turning that
// into a proper "unknown type" diagnostic; this function itself never
// throws or reports errors, it is a pure lookup.
inline Type TypeFromName(const std::string& name) {
    if (name == "int")    return Type::Int;
    if (name == "float")  return Type::Float;
    if (name == "bool")   return Type::Bool;
    if (name == "string") return Type::String;
    return Type::Unknown;
}

// True for the primitive types defined so far (Phase 1's initial set).
// Deliberately excludes Unknown and Object (Phase 13) -- a class instance
// is a valid, resolved type, just not one of the four original
// primitives; this predicate is specifically about that Phase-1 set, not
// "any valid type". Later phases that add array/list/map/function should
// NOT make this return true for them either, same reasoning.
inline bool IsPrimitiveType(Type type) {
    switch (type) {
        case Type::Int:
        case Type::Float:
        case Type::Bool:
        case Type::String:
            return true;
        default:
            return false;
    }
}

// Phase 13 ("Clases y objetos"). A
// resolved type together with, when type == Type::Object, which class it
// refers to (class_name is empty/meaningless for every other Type value).
// Kept separate from the plain Type enum rather than growing Type a case
// per user-defined class -- see the Type::Object comment above. Used
// wherever class identity actually matters (annotation/reassignment/
// return-type validation, field/method lookup); ordinary Type is still
// used everywhere that only needs to distinguish the four primitives plus
// Unknown/Object-as-a-bucket (e.g. ValidateBinOpTypes, which treats every
// Object the same regardless of class, same as it already does for
// String -- see compiler.cpp).
// Phase 15 ("Colecciones"). element_type
// and key_type are meaningful only when type == Type::List (element_type =
// element type) or Type::Dict (key_type = key type, element_type = value
// type -- reusing element_type for both List's element and Dict's value
// keeps this to two extra members instead of three separate
// value/element/key fields). nullptr means "not resolved" -- an empty list
// literal (`[]`), a list whose elements don't all agree on one type, or a
// list/dict that hasn't been inferred from a literal at all (e.g. a
// function parameter with no annotation syntax to carry one yet) -- NOT
// the same as `type == Type::Unknown`, which means "not a list/dict at
// all". A shared_ptr rather than a TypeRef-by-value member because TypeRef
// recursively contains itself (a list of lists needs element_type to
// itself carry an element_type) -- a by-value member would make TypeRef an
// incomplete type.
struct TypeRef {
    Type type = Type::Unknown;
    std::string class_name;  // meaningful only when type == Type::Object
    std::shared_ptr<TypeRef> element_type;  // List: element; Dict: value
    std::shared_ptr<TypeRef> key_type;      // Dict only: key
};

// Phase 15. Structural equality between
// two TypeRefs, used wherever a plain `a.type == b.type` (plus the
// Object-only class_name check) used to be enough before List/Dict existed
// -- see ValidateTypeAnnotation/ValidateReassignment/CheckReturnType in
// compiler.cpp. Recurses into element_type/key_type for List/Dict, same
// "class name only matters for Object" shape the rest of the type system
// already used. When EITHER side's element_type (or key_type) is nullptr
// ("not resolved", see the comment on TypeRef above) that particular
// component is treated as compatible rather than a mismatch -- same
// "Type::Unknown never causes a false positive" convention every validator
// in this type system has followed since Phase 6, just applied one level
// deeper for collections. Two Lists/Dicts with mismatched *resolved*
// element/key types (e.g. `list of int` vs `list of string`) DO count as a
// mismatch.
inline bool TypeRefEquals(const TypeRef& a, const TypeRef& b) {
    if (a.type != b.type) return false;
    if (a.type == Type::Object) return a.class_name == b.class_name;
    if (a.type == Type::List) {
        if (a.element_type && b.element_type) return TypeRefEquals(*a.element_type, *b.element_type);
        return true;
    }
    if (a.type == Type::Dict) {
        bool key_ok = (!a.key_type || !b.key_type) || TypeRefEquals(*a.key_type, *b.key_type);
        bool val_ok = (!a.element_type || !b.element_type) || TypeRefEquals(*a.element_type, *b.element_type);
        return key_ok && val_ok;
    }
    return true;
}

} // namespace ava

#endif // AVA_COMMON_TYPE_H
