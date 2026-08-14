#include "MemoryFileSystem.h"

namespace ava {
namespace platform {

MemoryFileSystem::MemoryFileSystem(IFileSystem* fallback) : fallback_(fallback) {}

std::string MemoryFileSystem::NormalizeKey(const std::string& path) {
    std::string out = path;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    return out;
}

void MemoryFileSystem::RegisterFile(const std::string& path, int64_t size, ContentProvider provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry entry;
    entry.size = size;
    entry.provider = std::move(provider);
    files_[NormalizeKey(path)] = std::move(entry);
}

void MemoryFileSystem::RemoveFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    files_.erase(NormalizeKey(path));
}

bool MemoryFileSystem::ReadFile(const std::string& path, std::string& out_content) {
    ContentProvider provider;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(NormalizeKey(path));
        if (it != files_.end()) {
            provider = it->second.provider;
        }
    }
    if (provider) {
        return provider(out_content);
    }
    if (fallback_) return fallback_->ReadFile(path, out_content);
    return false;
}

bool MemoryFileSystem::WriteFile(const std::string& path, const std::string& content) {
    // El runtime empacado (Fase 7) nunca escribe archivos del proyecto --
    // esto solo importa para lo que no este registrado como virtual, y ahi
    // se delega al filesystem real si existe.
    if (fallback_) return fallback_->WriteFile(path, content);
    return false;
}

bool MemoryFileSystem::DeleteFile(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(NormalizeKey(path));
        if (it != files_.end()) {
            files_.erase(it);
            return true;
        }
    }
    if (fallback_) return fallback_->DeleteFile(path);
    return false;
}

bool MemoryFileSystem::CreateDirectory(const std::string& path) {
    // No hay directorios reales en el mapa en memoria -- las rutas
    // virtuales son planas (path completo como clave). Delegar al
    // fallback es lo unico razonable para rutas fuera del proyecto
    // embebido (p.ej. el stdlib real).
    if (fallback_) return fallback_->CreateDirectory(path);
    return true;
}

bool MemoryFileSystem::DeleteDirectory(const std::string& path) {
    if (fallback_) return fallback_->DeleteDirectory(path);
    return true;
}

bool MemoryFileSystem::EnumerateDirectory(const std::string& path, std::vector<DirEntry>& out_entries) {
    // No implementado sobre el mapa virtual (ModuleResolver/ModuleCache
    // nunca lo necesitan -- solo usan ReadFile/Exists, ver module.cpp).
    // Delega al fallback para no romper otros consumidores de IFileSystem
    // (p.ej. el explorador de proyecto de avastudio) si alguna vez corren
    // bajo este override.
    if (fallback_) return fallback_->EnumerateDirectory(path, out_entries);
    return false;
}

bool MemoryFileSystem::Exists(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (files_.find(NormalizeKey(path)) != files_.end()) return true;
    }
    if (fallback_) return fallback_->Exists(path);
    return false;
}

bool MemoryFileSystem::IsDirectory(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (files_.find(NormalizeKey(path)) != files_.end()) return false; // es un archivo, no un directorio
    }
    if (fallback_) return fallback_->IsDirectory(path);
    return false;
}

int64_t MemoryFileSystem::FileSize(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(NormalizeKey(path));
        if (it != files_.end()) return it->second.size;
    }
    if (fallback_) return fallback_->FileSize(path);
    return -1;
}

std::string MemoryFileSystem::GetExecutableDirectory() {
    // No tiene sentido en un mapa virtual -- siempre delega, para que
    // Extern/FFI (vm_extern.cpp) siga encontrando modules/ al lado del
    // .exe real aunque el resto del filesystem este overrideado.
    if (fallback_) return fallback_->GetExecutableDirectory();
    return "";
}

} // namespace platform
} // namespace ava
