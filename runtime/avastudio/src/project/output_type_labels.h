#pragma once

#include <string>

#include "project/avaproj_file.h"
#include "util/host_platform.h"

namespace studio {

// Leyenda a mostrar para `output_type` de un proyecto con el target
// indicado (`is_barekernel`). Compartida entre el combo editable de
// Propiedades y la vista de solo lectura del panel de Build para que
// nunca queden desincronizadas.
//
// - BareKernel: leyenda generica ("Executable (BareKernel)" / "Library
//   (BareKernel)"), no depende del host: BareKernel no genera un .exe/
//   .dll del host, asi que su extension real no tiene sentido aca.
// - Desktop: leyenda con la extension real del *host* donde corre
//   AvaStudio ahora mismo (`host_platform`), ya que hoy el build de
//   Desktop siempre compila para el propio host (ver AVASTUDIO_EXE_SUFFIX
//   en build_panel.cpp). Windows -> ".exe"/".dll", Linux -> sin
//   extension/".so", macOS -> sin extension/".dylib".
std::string DescribeOutputType(util::HostPlatform host_platform, bool is_barekernel, AvaProjOutputType output_type);

}
