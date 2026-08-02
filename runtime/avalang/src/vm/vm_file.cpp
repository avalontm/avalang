#include "vm.h"
#include "vm_internal.h"
#include "../frontend/frontend.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ava {

std::string VM::GetCurrentDir() const {
    if (!current_dir_.empty()) return current_dir_;
    return GetCurrentWorkingDir();
}

void VM::SetCurrentDir(const std::string& dir) {
    current_dir_ = dir;
}

Value VM::RunFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + file_path);
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    file.close();
    
    std::string dir = GetFileDir(file_path);
    std::string prev_dir = GetCurrentDir();
    SetCurrentDir(dir);
    
    current_module_ = file_path;
    
    try {
        auto proto = CompileSource(source, file_path);
        auto result = Run(proto);
        SetCurrentDir(prev_dir);
        return result;
    } catch (...) {
        SetCurrentDir(prev_dir);
        throw;
    }
}

} // namespace ava
