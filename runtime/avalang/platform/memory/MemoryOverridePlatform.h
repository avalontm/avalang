#ifndef AVA_PLATFORM_MEMORY_MEMORYOVERRIDEPLATFORM_H
#define AVA_PLATFORM_MEMORY_MEMORYOVERRIDEPLATFORM_H

// Fase 7 (avapack): IPlatform que envuelve el backend real (WinPlatform,
// normalmente) y reemplaza SOLO FileSystem() por un MemoryFileSystem --
// todo lo demas (Threads, Clock, Libraries, Console, Environment, Process,
// Timer, CreateMutex) se delega tal cual al backend real, sin cambios de
// comportamiento. Pensado para instalarse una unica vez, al arrancar el
// .exe empacado, via VmPlatformAccessor::SetOverride (ver
// ../../src/vm/vm_platform_accessor.h) -- ava_cli/avahost normales nunca
// llaman SetOverride y no se ven afectados.

#include "../interfaces/IPlatform.h"
#include "MemoryFileSystem.h"

#include <memory>

namespace ava {
namespace platform {

class MemoryOverridePlatform : public IPlatform {
public:
    MemoryOverridePlatform(std::unique_ptr<IPlatform> real, std::unique_ptr<MemoryFileSystem> memory_fs)
        : real_(std::move(real)), memory_fs_(std::move(memory_fs)) {}
    ~MemoryOverridePlatform() override = default;

    IFileSystem& FileSystem() override { return *memory_fs_; }
    IThreadFactory& Threads() override { return real_->Threads(); }
    IClock& Clock() override { return real_->Clock(); }
    ILibraryLoader& Libraries() override { return real_->Libraries(); }
    IConsole& Console() override { return real_->Console(); }
    IEnvironment& Environment() override { return real_->Environment(); }
    IProcess& Process() override { return real_->Process(); }
    ITimer& Timer() override { return real_->Timer(); }

    IMutex* CreateMutex() override { return real_->CreateMutex(); }

    MemoryFileSystem& MemoryFs() { return *memory_fs_; }

private:
    std::unique_ptr<IPlatform> real_;
    std::unique_ptr<MemoryFileSystem> memory_fs_;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MEMORY_MEMORYOVERRIDEPLATFORM_H
