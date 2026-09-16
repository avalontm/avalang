#include "layout/DensityScale.h"

#include "platform/contract/IPlatform.h"

namespace avalang {
namespace ui {
namespace layout {

float CurrentDensityScale() {
    float scaling = platform::GetPlatform().Display().PrimaryMonitor().scaling;
    return scaling > 0.0f ? scaling : 1.0f;
}

double LogicalToPhysical(double logicalUnits) {
    return logicalUnits * static_cast<double>(CurrentDensityScale());
}

double PhysicalToLogical(double physicalPixels) {
    return physicalPixels / static_cast<double>(CurrentDensityScale());
}

}
}
}
