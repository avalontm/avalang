#pragma once

#include <cstddef>

namespace avalang {
namespace ui {
namespace fonts {

struct FontBytes {
    const unsigned char* data = nullptr;
    std::size_t size = 0;
};

FontBytes DefaultRegular();

FontBytes DefaultBold();

}
}
}