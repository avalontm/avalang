#pragma once

#include "imgui.h"

namespace studio {

ImFont* LoadDefaultFont(float size_px = 16.0f);

ImFont* LoadBoldFont(float size_px = 16.0f);

ImFont* GetCodeFont();

}
