#include "project/output_type_labels.h"

#include "util/i18n.h"

namespace studio {

std::string DescribeOutputType(util::HostPlatform host_platform, bool is_barekernel, AvaProjOutputType output_type) {
    const bool is_library = output_type == AvaProjOutputType::kLibrary;

    // BareKernel ya se identifica por el combo/label de "Target" que va
    // justo al lado -- repetir "(BareKernel)" en cada opcion de este
    // combo es redundante. Ademas no tiene una extension de host real
    // que mostrar (no es un .exe/.dll del sistema operativo actual), asi
    // que se muestra el nombre genérico sin sufijo.
    if (is_barekernel) {
        return util::Tr(is_library ? "project_properties.output_library_plain"
                                    : "project_properties.output_exe_plain");
    }

    const std::string key =
        is_library ? "project_properties.output_library_desktop" : "project_properties.output_exe_desktop";
    const std::string ext =
        is_library ? util::HostLibraryExtension(host_platform) : util::HostExecutableExtension(host_platform);
    const std::string suffix = ext.empty() ? "" : " (" + ext + ")";
    return util::TrFormat(key, {suffix});
}

}
