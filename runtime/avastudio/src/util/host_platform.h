#pragma once

#include <string>

namespace studio::util {

// El sistema operativo *host* donde corre AvaStudio (no el target de
// compilacion del proyecto -- eso es AvaProjTarget en avaproj_file.h).
// Se usa para decidir que leyenda mostrar en los combos de "Tipo de
// salida" (Executable/Library) cuando el proyecto apunta a Desktop,
// ya que la extension real del binario depende de en que OS se genera.
enum class HostPlatform { kWindows, kLinux, kMacOS, kUnknown };

// Determinado en tiempo de compilacion via macros del preprocesador
// (_WIN32 / __APPLE__ / __linux__), asi que refleja el OS donde corre
// el propio AvaStudio, sin necesidad de I/O ni de linkear nada extra.
HostPlatform DetectedHostPlatform();

// Nombre legible del host ("Windows" / "Linux" / "macOS" / "Unknown"),
// sin traducir -- se usa como argumento de TrFormat("build.platform_value", ...).
std::string HostPlatformName(HostPlatform platform);

// Extension de archivo ejecutable para el host actual: ".exe" en
// Windows, "" (sin extension) en Linux/macOS.
std::string HostExecutableExtension(HostPlatform platform);

// Extension de biblioteca dinamica para el host actual: ".dll" en
// Windows, ".so" en Linux, ".dylib" en macOS.
std::string HostLibraryExtension(HostPlatform platform);

}
