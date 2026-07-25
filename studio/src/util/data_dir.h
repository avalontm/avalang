#pragma once

#include <string>

namespace studio::util {

// Resuelve la carpeta "data" junto al ejecutable (no el cwd del proceso --
// mismo razonamiento que ResolveWorkspaceDir en main.cpp para "scripts/":
// dónde vive el .exe es estable sin importar cómo se lo haya lanzado
// -doble click, acceso directo, debugger-, el cwd no). Ahí es donde viven
// keyword_docs.csv y builtin_signatures.csv -- el usuario los puede editar
// con cualquier editor de texto sin recompilar Ava Studio.
//
// Devuelve la ruta con "/" al final. No crea el directorio -- a diferencia
// del workspace de scripts, si "data" no existe o los CSV no están, el
// llamador debe caer a la tabla embebida (ver DefaultKeywordDocs() /
// DefaultBuiltinSignatures()) en vez de fallar.
std::string ResolveDataDir();

// Default "base modules" folder, next to the executable (same reasoning
// as ResolveDataDir): "<exe_dir>/modules". This is what the Properties
// dialog's blank/unset path resolves to at runtime -- the field itself
// stays empty so settings.ini never bakes in a machine-specific absolute
// path (see util/settings.h); the user can still point it anywhere via
// Properties, and that explicit choice is what actually gets persisted.
// No trailing separator.
std::string ResolveDefaultModulesDir();

// Lee un archivo completo como texto. Devuelve false (sin tocar `out`) si
// no se pudo abrir -- "no existe" no es un error real acá, es la señal de
// "usá el fallback embebido".
bool ReadFileToString(const std::string& path, std::string& out);

} // namespace studio::util
