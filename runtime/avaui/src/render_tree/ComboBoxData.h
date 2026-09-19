#pragma once

#include <string>
#include <vector>

namespace avalang {
namespace ui {
namespace render {

struct ComboBoxItem {
    std::string value;
    std::string label;
    bool selected = false;
};

using ComboBoxItems = std::vector<ComboBoxItem>;

}
}
}
