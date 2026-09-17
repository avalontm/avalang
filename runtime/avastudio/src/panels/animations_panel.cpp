#include "panels/animations_panel.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "animation/AnimationBinding.h"
#include "designer/animation_authoring.h"
#include "designer/preview.h"
#include "imgui.h"
#include "panels/designer_canvas.h"
#include "util/i18n.h"

namespace studio {

namespace {

std::vector<std::pair<std::string, std::vector<const design::NodeAnimation*>>> GroupByNode(
    const std::vector<const design::NodeAnimation*>& animations) {
    std::unordered_map<std::string, std::vector<const design::NodeAnimation*>> by_node;
    for (const design::NodeAnimation* animation : animations) {
        by_node[animation->node_id].push_back(animation);
    }

    std::vector<std::pair<std::string, std::vector<const design::NodeAnimation*>>> grouped(by_node.begin(),
                                                                                             by_node.end());
    std::sort(grouped.begin(), grouped.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return grouped;
}

void DrawAnimationRow(const design::NodeAnimation& animation,
                       avalang::ui::animation::AnimationController* controller) {
    ImGui::PushID(&animation);
    ImGui::TextUnformatted(designer::DescribeAnimation(animation.spec).c_str());
    ImGui::SameLine();

    ImGui::BeginDisabled(controller == nullptr);
    if (ImGui::SmallButton(util::Tr("panel.animations.play").c_str())) {
        avalang::ui::animation::PlayAnimationSpec(animation.spec, controller);
    }
    ImGui::EndDisabled();
    if (controller == nullptr && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", util::Tr("panel.animations.preview_only").c_str());
    }
    ImGui::PopID();
}

}

void DrawAnimationsPanel(const design::DesignDocument& doc, int tab_id, bool* p_open) {
    if (!ImGui::Begin(util::Tr("panel.animations.title").c_str(), p_open)) {
        ImGui::End();
        return;
    }

    const std::vector<const design::NodeAnimation*> animations = design::ResolvedAnimations(doc);
    if (animations.empty()) {
        ImGui::TextDisabled("%s", util::Tr("panel.animations.empty").c_str());
        ImGui::End();
        return;
    }

    if (GetDesignerCanvasMode(tab_id) != designer::CanvasMode::Preview) {
        ImGui::TextDisabled("%s", util::Tr("panel.animations.preview_only").c_str());
    }
    avalang::ui::animation::AnimationController* controller = GetDesignerAnimationController(tab_id);

    for (const auto& [node_id, node_animations] : GroupByNode(animations)) {
        ImGui::PushID(node_id.c_str());
        if (ImGui::CollapsingHeader(node_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const design::NodeAnimation* animation : node_animations) {
                DrawAnimationRow(*animation, controller);
            }
        }
        ImGui::PopID();
    }

    ImGui::End();
}

}
