#ifndef AVA_BUILTINS_SYSTEM_MODULE_H
#define AVA_BUILTINS_SYSTEM_MODULE_H

#include "vm/vm.h"
#include "vm/value.h"

namespace ava {

// A (member name, native function) pair used to fill in a namespace
// Dict -- the same AvaNativeFn signature RegisterNative/
// RegisterBuiltinMethod already use, just not registered as a VM
// global or method: it only ever gets called through a Dict entry
// (Namespace.Member(...)), same call path OpGetAttr+OpCall already
// give any other Dict value.
struct NativeNamespaceMember {
    avastd::string name;
    AvaNativeFn fn;
};

// Builds a fresh, empty Dict Value. Used both as the base for
// BuildNativeNamespace's caller and directly wherever a namespace
// needs non-function entries too (e.g. Console.Colors, a Dict of
// string constants sitting next to Console's methods).
Value MakeDict();

// Builds a fresh Dict Value whose entries are Native functions, one
// per member. user_data on every produced NativeObj is nullptr --
// none of the areas this plan wires up need it (unlike, say,
// OpGetAttr's primitive method dispatch, which stashes `this` there).
Value BuildNativeNamespace(const avastd::vector<NativeNamespaceMember>& members);

// Sets (overwriting if already present) a Dict Value's entry for
// `key`. `dict_val` must already be a Dict.
void SetDictEntry(Value& dict_val, const avastd::string& key, Value entry);

// Registers the `import system` native module (Phase 1 of
// AVALANG_IMPORT_SYSTEM_PLAN.md): a root Dict with one child Dict per
// System.* area (Console, DateTime, Environment, IO). Each child is
// still empty at this phase -- filling them in with real PAL-backed
// members is Phases 2-5.5 of the same plan. Call once per VM, same as
// RegisterBuiltinGlobals/RegisterBuiltinMethods.
void RegisterSystemModule(VM& vm);

} // namespace ava

#endif // AVA_BUILTINS_SYSTEM_MODULE_H
