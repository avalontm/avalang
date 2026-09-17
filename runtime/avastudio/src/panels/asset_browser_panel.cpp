#include "panels/asset_browser_panel.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

#include "designer/asset_catalog.h"
#include "designer/selection_manager.h"
#include "imgui.h"
#include "panels/designer_canvas.h"
#include "util/i18n.h"

namespace studio {

namespace {

namespace fs = std::filesystem;

struct AssetEntry {
    std::string relative_path;
    designer::AssetKind kind = designer::AssetKind::Unknown;
};

struct CachedAssetCatalog {
    std::string project_root;
    std::vector<AssetEntry> entries;
    bool loaded = false;
};

CachedAssetCatalog g_cached_catalog;

std::vector<AssetEntry> ScanAssets(const std::string& project_root) {
    std::vector<AssetEntry> entries;
    const fs::path assets_root = fs::path(project_root) / "assets";

    std::error_code exists_ec;
    if (!fs::exists(assets_root, exists_ec) || exists_ec) {
        return entries;
    }

    std::error_code iter_ec;
    for (fs::recursive_directory_iterator it(assets_root, fs::directory_options::skip_permission_denied, iter_ec),
         end;
         it != end && !iter_ec; it.increment(iter_ec)) {
        std::error_code type_ec;
        if (!it->is_regular_file(type_ec) || type_ec) {
            continue;
        }

        std::error_code rel_ec;
        const fs::path relative = fs::relative(it->path(), fs::path(project_root), rel_ec);
        if (rel_ec) {
            continue;
        }

        AssetEntry entry;
        entry.relative_path = relative.generic_string();
        if (!designer::IsRelativeAssetPath(entry.relative_path)) {
            continue;
        }
        entry.kind = designer::ClassifyAssetPath(entry.relative_path);
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(),
              [](const AssetEntry& a, const AssetEntry& b) { return a.relative_path < b.relative_path; });
    return entries;
}

const std::vector<AssetEntry>& GetOrBuildCatalog(const std::string& project_root, bool force_refresh) {
    if (!force_refresh && g_cached_catalog.loaded && g_cached_catalog.project_root == project_root) {
        return g_cached_catalog.entries;
    }

    g_cached_catalog.project_root = project_root;
    g_cached_catalog.entries = ScanAssets(project_root);
    g_cached_catalog.loaded = true;
    return g_cached_catalog.entries;
}

const char* SectionKeyForKind(designer::AssetKind kind) {
    switch (kind) {
        case designer::AssetKind::Image:
            return "panel.assets.images";
        case designer::AssetKind::Font:
            return "panel.assets.fonts";
        case designer::AssetKind::Localization:
            return "panel.assets.localization";
        default:
            return "panel.assets.other";
    }
}

std::optional<PropertyEdit> DrawAssetSection(const char* section_key, const std::vector<AssetEntry>& entries,
                                              designer::AssetKind kind, bool has_selection,
                                              const std::string& node_id, int tab_id) {
    std::vector<const AssetEntry*> filtered;
    for (const AssetEntry& entry : entries) {
        if (entry.kind == kind) {
            filtered.push_back(&entry);
        }
    }
    if (filtered.empty()) {
        return std::nullopt;
    }

    std::optional<PropertyEdit> result;
    if (ImGui::CollapsingHeader(util::Tr(section_key).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const AssetEntry* entry : filtered) {
            ImGui::PushID(entry->relative_path.c_str());
            ImGui::TextUnformatted(entry->relative_path.c_str());
            ImGui::SameLine();

            ImGui::BeginDisabled(!has_selection);
            if (ImGui::SmallButton(util::Tr("panel.assets.insert").c_str())) {
                PropertyEdit edit;
                edit.tab_id = tab_id;
                edit.node_id = node_id;
                edit.kind = PropertyEditKind::kValue;
                edit.key = "source";
                edit.new_value = entry->relative_path;
                result = edit;
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
    }
    return result;
}

}

std::optional<PropertyEdit> DrawAssetBrowserPanel(const std::string& project_root, int tab_id, bool* p_open) {
    std::optional<PropertyEdit> result;

    if (!ImGui::Begin(util::Tr("panel.assets.title").c_str(), p_open)) {
        ImGui::End();
        return result;
    }

    if (project_root.empty()) {
        ImGui::TextDisabled("%s", util::Tr("panel.assets.no_project").c_str());
        ImGui::End();
        return result;
    }

    const bool force_refresh = ImGui::Button(util::Tr("panel.assets.refresh").c_str());
    ImGui::Separator();

    const std::vector<AssetEntry>& entries = GetOrBuildCatalog(project_root, force_refresh);
    if (entries.empty()) {
        ImGui::TextDisabled("%s", util::Tr("panel.assets.empty").c_str());
        ImGui::End();
        return result;
    }

    const designer::SelectionManager* selection = GetDesignerSelectionManager(tab_id);
    const std::string selected_node_id = selection ? selection->Primary() : std::string();
    const bool has_selection = !selected_node_id.empty();
    if (!has_selection) {
        ImGui::TextDisabled("%s", util::Tr("panel.assets.no_selection").c_str());
    }

    const designer::AssetKind kinds[] = {designer::AssetKind::Image, designer::AssetKind::Font,
                                          designer::AssetKind::Localization, designer::AssetKind::Unknown};
    for (designer::AssetKind kind : kinds) {
        if (std::optional<PropertyEdit> edit = DrawAssetSection(SectionKeyForKind(kind), entries, kind,
                                                                  has_selection, selected_node_id, tab_id)) {
            result = edit;
        }
    }

    ImGui::End();
    return result;
}

}
