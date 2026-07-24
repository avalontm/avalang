#ifndef AVA_UI_BUILTINS_H
#define AVA_UI_BUILTINS_H

#include "avalang.h"

namespace ava {
namespace ui {

// Placeholder for UI-related native functions callable from AvaLang
// scripts (analogous to RegisterBuiltinGlobals in builtins/builtin_init.cpp,
// but for `ui.*` helpers instead of language-level globals).
//
// NOTE: no call site for this file was found anywhere else in this
// snapshot. Not wired into ava_vm_create() — left unregistered on purpose
// so it can't silently change VM behavior. Wire it up once you confirm
// what natives this was meant to expose.
void RegisterUIBuiltins(AvaVM* vm);

} // namespace ui
} // namespace ava

#endif // AVA_UI_BUILTINS_H
