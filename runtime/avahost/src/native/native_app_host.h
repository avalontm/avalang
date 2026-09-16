#pragma once
#include <string>

namespace avahost {
namespace native {

int RunNativeApp(const std::string& projectDir, const std::string& entryFile,
                  int width, int height, std::string& outError);

}
}
