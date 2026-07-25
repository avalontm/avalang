#pragma once

#include <string>
#include <unordered_map>

#include "languages/function_index.h"

namespace studio {

// Hand-authored signatures for every free-function builtin AvaLang
// registers globally (core/src/builtins/builtin_init.cpp's
// RegisterBuiltinGlobals -> core/src/builtins/builtin_natives.h/.cpp).
// This is NOT introspected from a running VM -- Ava Studio's editor has
// no such API today (see engine/engine_bridge.h) -- so it's kept in sync
// by hand with that file. If a builtin is added/changed there, mirror it
// here too, or the Code Editor's autocomplete/parameter hints will drift
// out of date with what actually runs.
//
// FunctionIndex::Rebuild() merges this table in *after* scanning the
// local buffer and its imports, and only for names not already found --
// so a script that declares its own top-level `func print(...)` still
// shows its own signature/doc instead of this one (see
// FunctionSignature::overridable and vm.cpp's SetGlobal: builtins and
// user globals share the same table, so a local declaration really does
// shadow the builtin at runtime, not just in this tooltip).
const std::unordered_map<std::string, FunctionSignature>& BuiltinSignatures();

} // namespace studio
