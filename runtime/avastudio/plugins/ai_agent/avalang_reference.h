#pragma once

#include <cstddef>
#include <string>

// El agente no comparte proceso con el editor (ai_agent_plugin es un
// SHARED library aparte, ver CMakeLists.txt de este plugin -- no linkea
// contra el resto de Ava Studio), así que no puede llamar directo a
// studio::KeywordDocs()/studio::BuiltinSignatures() (src/languages/).
// Este módulo lee los mismos dos CSV por su cuenta -- misma fuente de
// datos que ya usan los tooltips del editor:
//
//   data/keyword_docs.csv       -- cada palabra clave con su sintaxis
//                                   exacta + ejemplo + explicación
//   data/builtin_signatures.csv -- funciones built-in con firma y doc
//
// (runtime/avalang/grammar/AvaLang.g4 es la fuente de verdad formal si
// algún día estos CSV quedan desactualizados respecto de la gramática,
// pero no se parsea en runtime acá -- son los CSV, ya curados a mano en
// sync con la gramática, los que se leen.)
//
// Dos formas de usar esto en ai_agent_plugin.cpp, no excluyentes:
//
//  - BuildAvalangReferenceMessage(): un "cheat sheet" compacto de TODAS
//    las palabras clave y builtins, para mandar como mensaje "system"
//    fijo en cada request a OpenRouter -- independiente de si
//    auto_context está prendido, porque sin esto el modelo no tiene
//    ninguna señal de que AvaLang usa `end`/`func`/`then` en vez de
//    indentación/`def`/`:` como Python o JS.
//  - LookupAvalangSyntax(): la fila exacta (sintaxis + ejemplo + doc) de
//    UNA palabra clave o builtin puntual, para el tool
//    avalang_syntax_lookup (ver tools.h/.cpp) -- más barato en tokens
//    por turno que repetir el cheat sheet entero, pero depende de que el
//    modelo decida llamarlo.

// Referencia compacta de toda la sintaxis de AvaLang (palabras clave +
// builtins), pensada para vivir como mensaje "system" fijo. Se trunca a
// max_chars si hiciera falta (nunca debería, el cheat sheet completo hoy
// entra cómodo en unos pocos cientos de tokens). Si ninguno de los dos
// CSV pudo leerse, devuelve una referencia mínima hardcodeada en vez de
// string vacío -- igual que KeywordDocs()/BuiltinSignatures() del editor
// nunca dejan al usuario sin nada por un CSV roto o ausente.
std::string BuildAvalangReferenceMessage(size_t max_chars);

// Busca `name` (case-insensitive) primero entre las palabras clave y
// después entre los builtins. Devuelve un JSON string con la fila
// encontrada (name, syntax, example, doc -- o name, params, doc para un
// builtin), o `{"found": false, ...}` con una sugerencia si no está en
// ninguna de las dos tablas. Nunca lanza -- pensado para ser el string
// que ExecuteReadOnlyTool devuelve tal cual como resultado del tool.
std::string LookupAvalangSyntax(const std::string& name);
