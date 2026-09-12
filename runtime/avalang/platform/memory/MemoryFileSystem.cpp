#include "MemoryFileSystem.h"

namespace ava {
namespace platform {

MemoryFileSystem::MemoryFileSystem(IFileSystem* fallback) : fallback_(fallback) {}

avastd::string MemoryFileSystem::NormalizeKey(const avastd::string& path) {
    avastd::string out = path;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    return out;
}

void MemoryFileSystem::RegisterFile(const avastd::string& path, int64_t size, ContentProvider provider) {
    avastd::lock_guard<avastd::mutex> lock(mutex_);
    Entry entry;
    entry.size = size;
    entry.provider = avastd::move(provider);
    files_[NormalizeKey(path)] = avastd::move(entry);
}

void MemoryFileSystem::RemoveFile(const avastd::string& path) {
    avastd::lock_guard<avastd::mutex> lock(mutex_);
    files_.erase(NormalizeKey(path));
}

bool MemoryFileSystem::ReadFile(const avastd::string& path, avastd::string& out_content) {
    ContentProvider provider;
    {
        avastd::lock_guard<avastd::mutex> lock(mutex_);
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

bool MemoryFileSystem::WriteFile(const avastd::string& path, const avastd::string& content) {
    // El runtime empacado (Fase 7) nunca escribe archivos del proyecto --
    // esto solo importa para lo que no este registrado como virtual, y ahi
    // se delega al filesystem real si existe.
    if (fallback_) return fallback_->WriteFile(path, content);
    return false;
}

bool MemoryFileSystem::DeleteFile(const avastd::string& path) {
    {
        avastd::lock_guard<avastd::mutex> lock(mutex_);
        auto it = files_.find(NormalizeKey(path));
        if (it != files_.end()) {
            files_.erase(it);
            return true;
        }
    }
    if (fallback_) return fallback_->DeleteFile(path);
    return false;
}

bool MemoryFileSystem::CreateDirectory(const avastd::string& path) {
    // No hay directorios reales en el mapa en memoria -- las rutas
    // virtuales son planas (path completo como clave). Delegar al
    // fallback es lo unico razonable para rutas fuera del proyecto
    // embebido (p.ej. el stdlib real).
    if (fallback_) return fallback_->CreateDirectory(path);
    return true;
}

bool MemoryFileSystem::DeleteDirectory(const avastd::string& path) {
    if (fallback_) return fallback_->DeleteDirectory(path);
    return true;
}

bool MemoryFileSystem::EnumerateDirectory(const avastd::string& path, avastd::vector<DirEntry>& out_entries) {
    avastd::string prefix = NormalizeKey(path);
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    avastd::unordered_map<avastd::string, bool> children;  // name -> is_directory
    {
        avastd::lock_guard<avastd::mutex> lock(mutex_);
        for (auto& [key, entry] : files_) {
            (void)entry;
            if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) continue;
            avastd::string rest = key.substr(prefix.size());
            size_t slash = rest.find('/');
            if (slash == avastd::string::npos) {
                children[rest] = false;
            } else {
                children[rest.substr(0, slash)] = true;
            }
        }
    }

    if (!children.empty()) {
        for (auto& [name, is_dir] : children) {
            DirEntry entry;
            entry.name = name;
            entry.is_directory = is_dir;
            out_entries.push_back(avastd::move(entry));
        }
        return true;
    }

    // Ningun archivo registrado vive bajo `path` en el mapa virtual --
    // ni siquiera como carpeta implicita. Delegar al fallback (real
    // filesystem) si hay uno; un runtime empacado con fallback_ nulo
    // simplemente no ve nada ahi, consistente con el resto de esta
    // clase (Fase 7: "sin fallback, esas operaciones fallan").
    if (fallback_) return fallback_->EnumerateDirectory(path, out_entries);
    return false;
}

bool MemoryFileSystem::Exists(const avastd::string& path) {
    {
        avastd::lock_guard<avastd::mutex> lock(mutex_);
        if (files_.find(NormalizeKey(path)) != files_.end()) return true;
    }
    if (fallback_) return fallback_->Exists(path);
    return false;
}

bool MemoryFileSystem::IsDirectory(const avastd::string& path) {
    avastd::string normalized = NormalizeKey(path);
    {
        avastd::lock_guard<avastd::mutex> lock(mutex_);
        if (files_.find(normalized) != files_.end()) return false; // es un archivo, no un directorio

        // Namespace-carpeta (module.cpp, ResolveModulePath): ningun
        // archivo registrado se llama exactamente `path`, pero si algo
        // vive "debajo" de `path/`, entonces `path` es una carpeta
        // virtual aunque el mapa no la tenga como entrada propia (es
        // plano, ver comentario de la clase).
        avastd::string prefix = normalized.empty() ? avastd::string() : normalized + "/";
        if (!prefix.empty()) {
            for (auto& [key, entry] : files_) {
                (void)entry;
                if (key.size() > prefix.size() && key.compare(0, prefix.size(), prefix) == 0) return true;
            }
        }
    }
    if (fallback_) return fallback_->IsDirectory(path);
    return false;
}

int64_t MemoryFileSystem::FileSize(const avastd::string& path) {
    {
        avastd::lock_guard<avastd::mutex> lock(mutex_);
        auto it = files_.find(NormalizeKey(path));
        if (it != files_.end()) return it->second.size;
    }
    if (fallback_) return fallback_->FileSize(path);
    return -1;
}

avastd::string MemoryFileSystem::GetExecutableDirectory() {
    // No tiene sentido en un mapa virtual -- siempre delega, para que
    // Extern/FFI (vm_extern.cpp) siga encontrando modules/ al lado del
    // .exe real aunque el resto del filesystem este overrideado.
    if (fallback_) return fallback_->GetExecutableDirectory();
    return "";
}

} // namespace platform
} // namespace ava
