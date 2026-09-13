#include "project/avaproj_user_file.h"

#include <filesystem>
#include <system_error>

#include <pugixml.hpp>

namespace studio {

namespace {

std::string ChildTextOr(const pugi::xml_node& parent, const char* name, const std::string& fallback) {
    pugi::xml_node child = parent.child(name);
    if (!child) return fallback;
    const char* text = child.text().as_string();
    return (text != nullptr && text[0] != '\0') ? std::string(text) : fallback;
}

// Fix: mismo problema y misma solucion que en avaproj_file.cpp -- escritura
// atomica (temp + rename) para que un guardado interrumpido no deje el
// .avaproj.user corrupto ni truncado.
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

std::optional<AvaProjUserFile> LoadAvaProjUserFile(const std::string& path) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(path.c_str());
    if (!result) return std::nullopt;

    pugi::xml_node root = doc.child("ProjectUser");
    if (!root) return std::nullopt;

    AvaProjUserFile data;
    data.repo_root = ChildTextOr(root, "RepoRoot", "");
    data.ava_cli_path = ChildTextOr(root, "AvaCliPath", "");
    data.key_file = ChildTextOr(root, "KeyFile", "");
    data.vcpkg_root = ChildTextOr(root, "VcpkgRoot", "");
    data.compiler_path_desktop = ChildTextOr(root, "CompilerPathDesktop", "");
    data.compiler_path_barekernel = ChildTextOr(root, "CompilerPathBareKernel", "");
    data.force_so = root.child("ForceSo").text().as_bool();
    data.force_runtime = root.child("ForceRuntime").text().as_bool();
    return data;
}

bool SaveAvaProjUserFile(const std::string& path, const AvaProjUserFile& data) {
    pugi::xml_document doc;

    pugi::xml_node declaration = doc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "utf-8";

    pugi::xml_node root = doc.append_child("ProjectUser");
    root.append_child("RepoRoot").text().set(data.repo_root.c_str());
    root.append_child("AvaCliPath").text().set(data.ava_cli_path.c_str());
    root.append_child("KeyFile").text().set(data.key_file.c_str());
    root.append_child("VcpkgRoot").text().set(data.vcpkg_root.c_str());
    root.append_child("CompilerPathDesktop").text().set(data.compiler_path_desktop.c_str());
    root.append_child("CompilerPathBareKernel").text().set(data.compiler_path_barekernel.c_str());
    root.append_child("ForceSo").text().set(data.force_so);
    root.append_child("ForceRuntime").text().set(data.force_runtime);

    return AtomicSaveXml(doc, path);
}

}  // namespace studio
