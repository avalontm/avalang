#include "project/avaproj_file.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include <pugixml.hpp>

namespace studio {

namespace fs = std::filesystem;

std::string TargetToString(AvaProjTarget target) {
    switch (target) {
        case AvaProjTarget::kBareKernel:
            return "BareKernel";
        case AvaProjTarget::kDesktop:
        default:
            return "Desktop";
    }
}

AvaProjTarget TargetFromString(const std::string& value) {
    if (value == "BareKernel") return AvaProjTarget::kBareKernel;
    return AvaProjTarget::kDesktop;
}

std::string OutputTypeToString(AvaProjOutputType type) {
    switch (type) {
        case AvaProjOutputType::kLibrary:
            return "Library";
        case AvaProjOutputType::kExe:
        default:
            return "Exe";
    }
}

AvaProjOutputType OutputTypeFromString(const std::string& value) {
    if (value == "Library") return AvaProjOutputType::kLibrary;
    return AvaProjOutputType::kExe;
}

namespace {

std::string ChildTextOr(const pugi::xml_node& parent, const char* name, const std::string& fallback) {
    pugi::xml_node child = parent.child(name);
    if (!child) return fallback;
    const char* text = child.text().as_string();
    return (text != nullptr && text[0] != '\0') ? std::string(text) : fallback;
}

}  // namespace

std::optional<AvaProjFile> LoadAvaProjFile(const std::string& path) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.c_str());
    if (!result) return std::nullopt;

    pugi::xml_node project_node = doc.child("Project");
    if (!project_node) return std::nullopt;

    AvaProjFile data;

    pugi::xml_node main_group;
    pugi::xml_node build_group;
    for (pugi::xml_node group : project_node.children("PropertyGroup")) {
        const std::string label = group.attribute("Label").as_string();
        if (label == "Build" && !build_group) {
            build_group = group;
        } else if (label.empty() && !main_group) {
            main_group = group;
        }
    }

    if (main_group) {
        data.project_name = ChildTextOr(main_group, "ProjectName", "");
        data.entry_file = ChildTextOr(main_group, "EntryFile", "main.ava");

        // Legacy files (before Target/OutputType were split into two
        // independent axes) only had <OutputType>Exe|BareKernel|Library
        // </OutputType>, so "BareKernel" meant the *target*, not a kind of
        // output -- those files always meant "BareKernel executable" since
        // Library didn't exist for that target yet. Only fall back to that
        // reading when <Target> itself is absent; once a file has been
        // saved by this version it always carries both elements.
        const bool has_target_element = static_cast<bool>(main_group.child("Target"));
        const std::string legacy_output_type = ChildTextOr(main_group, "OutputType", "Exe");
        if (!has_target_element && legacy_output_type == "BareKernel") {
            data.target = AvaProjTarget::kBareKernel;
            data.output_type = AvaProjOutputType::kExe;
        } else {
            data.target = TargetFromString(ChildTextOr(main_group, "Target", "Desktop"));
            data.output_type = OutputTypeFromString(legacy_output_type);
        }

        data.modules_path = ChildTextOr(main_group, "ModulesPath", "");
        data.icon = ChildTextOr(main_group, "Icon", "");
        data.out_dir = ChildTextOr(main_group, "OutDir", "bin");
    }

    if (build_group) {
        data.obfuscate = build_group.child("Obfuscate").text().as_bool();
        data.obfuscate_strings = build_group.child("ObfuscateStrings").text().as_bool();
        data.flatten_control_flow = build_group.child("FlattenControlFlow").text().as_bool();
        data.zero_disk = build_group.child("ZeroDisk").text().as_bool();
        data.debug_unencrypted = build_group.child("DebugUnencrypted").text().as_bool();
        data.uses_ui = build_group.child("UsesUi").text().as_bool();
    }

    for (pugi::xml_node item_group : project_node.children("ItemGroup")) {
        for (pugi::xml_node reference : item_group.children("Reference")) {
            const std::string include = reference.attribute("Include").as_string();
            if (!include.empty()) data.references.push_back({include});
        }
    }

    return data;
}

namespace {

// Fix: escritura atomica. Antes se llamaba doc.save_file(path, ...)
// directo sobre el archivo final -- si la escritura se interrumpe (el
// proceso muere, el archivo esta bloqueado a mitad de camino por un
// antivirus/OneDrive, se llena el disco, etc.) el .avaproj puede quedar
// truncado o a medio escribir, y ademas SaveAvaProjFile devolvia false
// pero nadie chequeaba ese resultado (ver project_config.cpp / main.cpp),
// asi que la falla era completamente silenciosa: la seleccion vivia bien
// en memoria durante la sesion (por eso "andaba"), pero al reabrir se leia
// el archivo viejo/corrupto y parecia que "no persistia". Escribiendo a un
// temporal en la misma carpeta y haciendo rename() al final, o se escribe
// el archivo completo o no se toca el original.
bool AtomicSaveXml(const pugi::xml_document& doc, const std::string& path) {
    namespace fs = std::filesystem;
    const fs::path final_path(path);
    const fs::path tmp_path = fs::path(path + ".tmp");

    std::error_code ec;
    fs::create_directories(final_path.parent_path(), ec);

    if (!doc.save_file(tmp_path.c_str(), "  ")) {
        fs::remove(tmp_path, ec);
        return false;
    }

    fs::rename(tmp_path, final_path, ec);
    if (ec) {
        fs::remove(tmp_path, ec);
        return false;
    }
    return true;
}

}  // namespace

bool SaveAvaProjFile(const std::string& path, const AvaProjFile& data) {
    pugi::xml_document doc;

    pugi::xml_node declaration = doc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "utf-8";

    pugi::xml_node project_node = doc.append_child("Project");
    project_node.append_attribute("Sdk") = "AvaLang.Sdk/1.0";

    pugi::xml_node main_group = project_node.append_child("PropertyGroup");
    main_group.append_child("ProjectName").text().set(data.project_name.c_str());
    main_group.append_child("EntryFile").text().set(data.entry_file.c_str());
    main_group.append_child("Target").text().set(TargetToString(data.target).c_str());
    main_group.append_child("OutputType").text().set(OutputTypeToString(data.output_type).c_str());
    main_group.append_child("ModulesPath").text().set(data.modules_path.c_str());
    main_group.append_child("Icon").text().set(data.icon.c_str());
    main_group.append_child("OutDir").text().set(data.out_dir.c_str());

    pugi::xml_node build_group = project_node.append_child("PropertyGroup");
    build_group.append_attribute("Label") = "Build";
    build_group.append_child("Obfuscate").text().set(data.obfuscate);
    build_group.append_child("ObfuscateStrings").text().set(data.obfuscate_strings);
    build_group.append_child("FlattenControlFlow").text().set(data.flatten_control_flow);
    build_group.append_child("ZeroDisk").text().set(data.zero_disk);
    build_group.append_child("DebugUnencrypted").text().set(data.debug_unencrypted);
    build_group.append_child("UsesUi").text().set(data.uses_ui);

    if (!data.references.empty()) {
        pugi::xml_node item_group = project_node.append_child("ItemGroup");
        for (const AvaProjReference& reference : data.references) {
            pugi::xml_node reference_node = item_group.append_child("Reference");
            reference_node.append_attribute("Include") = reference.include.c_str();
        }
    }

    return AtomicSaveXml(doc, path);
}

std::vector<std::string> FindAllAvaProjInDir(const std::string& dir) {
    std::vector<std::string> found;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return found;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".avaproj") continue;
        found.push_back(entry.path().string());
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::string FindAvaProjInDir(const std::string& dir) {
    std::vector<std::string> found = FindAllAvaProjInDir(dir);
    return found.size() == 1 ? found.front() : "";
}

}  // namespace studio
