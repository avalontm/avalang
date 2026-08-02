#pragma once

#include <string>
#include <unordered_map>

#include "languages/function_index.h"

namespace studio {

// Signatures for every free-function builtin AvaLang registers globally
// (core/src/builtins/builtin_init.cpp's RegisterBuiltinGlobals ->
// core/src/builtins/builtin_natives.h/.cpp).
//
// The data lives in data/builtin_signatures.csv, next to ava_studio.exe
// -- editable without recompiling. This is NOT introspected from a
// running VM (Ava Studio's editor has no such API today, see
// engine/engine_bridge.h), so the CSV is kept in sync by hand with that
// file: if a builtin is added/changed there, mirror it in the CSV too,
// or the Code Editor's autocomplete/parameter hints will drift out of
// date with what actually runs. If the CSV is missing or fails to parse,
// BuiltinSignatures() falls back to DefaultBuiltinSignatures(), which
// mirrors the CSV's shipped content.
//
// FunctionIndex::Rebuild() merges this table in *after* scanning the
// local buffer and its imports, and only for names not already found --
// so a script that declares its own top-level `func print(...)` still
// shows its own signature/doc instead of this one (see
// FunctionSignature::overridable and vm.cpp's SetGlobal: builtins and
// user globals share the same table, so a local declaration really does
// shadow the builtin at runtime, not just in this tooltip).
const std::unordered_map<std::string, FunctionSignature>& BuiltinSignatures();

// The hardcoded fallback table, used when data/builtin_signatures.csv is
// missing or fails to parse. Also what tools/dump_docs.cpp writes out to
// bootstrap a fresh builtin_signatures.csv.
const std::unordered_map<std::string, FunctionSignature>& DefaultBuiltinSignatures();

} // namespace studio
