#ifndef AVA_COMPILER_OBFUSCATE_H
#define AVA_COMPILER_OBFUSCATE_H

#include <cstdint>
#include <string>
#include <vector>
#include "../vm/proto.h"

namespace ava {

// Pase de ofuscación de bytecode, Parte 1: símbolos (nombres de función y
// de archivo fuente que hoy viven en Proto::debug_name / source_name).
//
// Se corre DESPUÉS de Compiler::Compile() y ANTES de proto_io::WriteProto,
// sobre el árbol de Proto ya construido -- no toca compiler.cpp. Un build
// normal (avacli build, sin --obfuscate) nunca pasa por acá: debug_name y
// source_name quedan intactos para que los stack traces sigan siendo
// legibles en desarrollo.
//
// Qué SÍ hace este pase:
//   - Reemplaza debug_name de cada Proto (top-level y cada función/método/
//     lambda hijo) por un identificador opaco derivado de un hash con
//     semilla local al build (module_seed), no de una tabla fija ni de
//     una clave global del framework.
//   - Reemplaza source_name por lo mismo, para no filtrar rutas de
//     archivo/estructura de carpetas del proyecto original.
//   - Opcionalmente descarta debug_lines (mapeo instrucción->línea), que
//     de por sí no tiene valor sin poder ver el .ava original.
//   - Devuelve un mapa símbolo original -> símbolo ofuscado (SymbolMap),
//     pensado para guardarse LOCALMENTE junto al proyecto fuente (nunca
//     embebido en el build distribuido) y así poder traducir un stack
//     trace reportado por un usuario de vuelta a nombres reales.
//
// Qué NO hace (todavía; ver plan_ava_pack.md Fase 6, Partes 2 y 3):
//   - No toca los string literals de la constant pool (Proto::constants)
//     -- eso es la Parte 2, ofuscación de strings.
//   - No reescribe el grafo de instrucciones (JMP/TEST) -- eso es la
//     Parte 3, control-flow flattening.
//
// module_seed no es una clave secreta ni tiene que serlo: su único rol es
// hacer que dos builds del mismo proyecto (o dos proyectos distintos) no
// produzcan los mismos IDs ofuscados, para que alguien no pueda armar un
// diccionario símbolo-ofuscado -> símbolo-real reusable entre builds.
// avacli genera uno aleatorio por invocación de `build --obfuscate`
// (ver build_command.cpp); no hay ninguna semilla hardcodeada en este
// archivo ni en ningún otro.

struct ObfuscateOptions {
    uint64_t module_seed = 0;
    bool strip_debug_lines = true;

    // Parte 2: ofuscar los Value::String de Proto::constants (mensajes,
    // literales, y también los nombres usados por GETGLOBAL/GETATTR/
    // SETATTR, que en este VM viven como strings en la misma constant
    // pool -- ver vm/opcodes.h).
    //
    // IMPORTANTE, corrige algo que se planteó como "lazy, decodificado
    // solo al acceder" en la propuesta original: NO puede ser lazy en el
    // sentido de "decodificar cada string la primera vez que el VM la
    // toca". GETGLOBAL necesita el nombre real de la variable para
    // resolver Globals[K[Bx]] -- si ese string sigue ofuscado en el
    // momento en que el VM ejecuta esa instrucción, la resolución falla.
    // La decodificación real ocurre UNA vez, completa, sobre todo el
    // árbol de constants, inmediatamente después de deserializar el
    // .avbc y antes de VM::Run (ver DeobfuscateStrings más abajo) -- no
    // hay un beneficio real de "memoria" en diferirla más que eso, y
    // prometerlo sería impreciso.
    bool obfuscate_strings = false;

    // Parte 3: control-flow flattening. Reescribe el cuerpo de cada Proto
    // elegible como un dispatcher unico (registro de estado + cadena
    // EQK/TEST/JMP) en vez de JMP/TEST directos entre bloques -- un lector
    // del bytecode ya no ve que bloque sigue a cual sin resolver primero
    // que constante de estado corresponde a cada bloque.
    //
    // flatten_functions vacio = todo Proto elegible se aplana. No vacio =
    // solo los Proto cuyo debug_name (ANTES de la Parte 1, symbol
    // renaming -- el orden de pases en ObfuscateProto corre flatten
    // primero por eso) matchea un nombre de la lista. La anotacion en el
    // .ava fuente que el plan original preveia (marcar una funcion como
    // "a aplanar" desde el propio codigo, via el frontend ANTLR) sigue
    // sin implementarse -- este flag es el mecanismo de bajo nivel sobre
    // el que esa anotacion se apoyaria una vez exista.
    //
    // Elegibilidad real (ver FlattenProtoControlFlow): se salta cualquier
    // Proto que use TRY/TRY_END/CATCH/RAISE/YIELD/RESUME (el layout de pc
    // que esas instrucciones asumen no esta verificado contra el
    // dispatcher, mas seguro no tocarlas por ahora) o que tenga menos de
    // dos bloques basicos (nada que aplanar).
    bool flatten_control_flow = false;
    std::vector<std::string> flatten_functions;
};

struct SymbolMapEntry {
    std::string kind;         // "function" | "source_file"
    std::string original;     // debug_name / source_name original
    std::string obfuscated;   // valor que quedó en el Proto tras el pase
};

// Ofusca `root` y todo su árbol de child_protos in-place.
// Si `out_symbol_map` no es null, se le agregan (append) las entradas
// generadas -- el caller decide qué hacer con ellas (ej. escribirlas a un
// .avmap junto al build, ver build_command.cpp).
//
// Si options.obfuscate_strings es true, además transforma cada
// Value::String de cada Proto::constants (de root y de todo el árbol)
// con un keystream reversible derivado de module_seed. El recorrido del
// árbol es determinista (mismo orden que la construcción del Proto), así
// que DeobfuscateStrings con el mismo module_seed revierte exactamente
// esta transformación -- no hace falta guardar nada más aparte del seed.
void ObfuscateProto(Proto& root,
                     const ObfuscateOptions& options,
                     std::vector<SymbolMapEntry>* out_symbol_map = nullptr);

// Revierte la ofuscación de strings aplicada por ObfuscateProto con
// options.obfuscate_strings=true y el mismo module_seed. Se llama UNA vez,
// sobre el árbol completo, inmediatamente después de reconstruir el Proto
// (ej. tras proto_io::ReadProto) y antes de correrlo en el VM -- ver nota
// en ObfuscateOptions::obfuscate_strings sobre por qué no puede ser lazy.
//
// No toca debug_name/source_name (eso es la Parte 1, y ya quedó resuelto
// -- o bien nunca se ofuscó porque el build no pidió --obfuscate, o bien
// se perdió a propósito porque el .avbc se escribió con
// ProtoIoOptions::strip_debug_info=true y no hay nada que revertir).
void DeobfuscateStrings(Proto& root, uint64_t module_seed);

// Aplana el bytecode de un unico Proto in-place (no recursivo -- no toca
// child_protos). No hay contraparte "deflatten": a diferencia de la
// ofuscacion de strings, esto no necesita revertirse en runtime, el VM
// ejecuta el dispatcher tal cual quedo escrito en el .avbc.
//
// Devuelve false y deja proto sin tocar si no es elegible (ver arriba).
// `tag` solo necesita ser unico por Proto dentro del mismo build -- se usa
// junto con seed para derivar las constantes de estado y el orden
// (shuffle) de las comparaciones del dispatcher; no hace falta que sea
// estable entre builds.
bool FlattenProtoControlFlow(Proto& proto, uint64_t seed, const std::string& tag);

// Serializa un SymbolMap a un formato de texto simple (una entrada por
// línea: "kind\toriginal\tobfuscated"), pensado para guardarse junto al
// proyecto fuente del lado del desarrollador. Nunca debe embeberse en el
// binario/bytecode distribuido -- eso anularía el propósito del pase.
std::string FormatSymbolMap(const std::vector<SymbolMapEntry>& map);

} // namespace ava

#endif // AVA_COMPILER_OBFUSCATE_H
