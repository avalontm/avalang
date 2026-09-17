#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "animation/Easing.h"
#include "components/IComponent.h"
#include "parser/AvauiParser.h"

namespace studio::designer {

std::string DescribeAnimation(const avalang::ui::parser::AnimationSpec& spec);

std::unordered_map<avalang::ui::ComponentId, std::vector<avalang::ui::parser::AnimationSpec>>
GroupAnimationsByTarget(const std::vector<avalang::ui::parser::AnimationSpec>& specs);

std::string EasingLabel(avalang::ui::animation::EasingFunction easing);

}
