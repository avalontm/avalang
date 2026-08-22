#ifndef AVA_PLATFORM_MEMORY_MEMORYFILESYSTEM_H
#define AVA_PLATFORM_MEMORY_MEMORYFILESYSTEM_H

// Fase 7 (avapack, plan_ava_pack.md): IFileSystem respaldado por RAM.
// No cambia PAL_ABI.h ni IFileSystem.h -- es solo una implementacion nueva
// de la interfaz existente, seleccionable en runtime via
// VmPlatformAccessor::SetOverride (ver ../../src/vm/vm_platform_accessor.h).
//
// Cada archivo registrado guarda un ContentProvider (no el contenido en
// claro directamente): RegisterFile() solo anota tamano + una funcion que
// produce el contenido bajo demanda. ReadFile() invoca el provider cada
// vez que se llama -- no cachea el resultado -- para que el plaintext
// nunca viva en el mapa mas tiempo del que tarda esa llamada. El caller
// tipico (avapack/src/main_zerodisk.cpp) captura la clave AES-256 y el
// EmbeddedFile por referencia en el provider y descifra ahi mismo, en
// memoria, sin tocar disco en ningun punto.
//
// Para rutas que no esten registradas (p.ej. el stdlib real, que sigue
// viviendo en disco al lado de avalang.dll), delega opcionalmente a un
// IFileSystem* real pasado en el constructor -- si es nullptr, esas
// operaciones simplemente fallan (false / -1 / vacio), no hay fallback.

#include "../interfaces/IFileSystem.h"

#include "../barekernel/stdcompat/ava_stdcompat.h"

namespace ava {
namespace platform {

class MemoryFileSystem : public IFileSystem {
public:
    // Produce el contenido del archivo en out_content y devuelve true, o
    // devuelve false si por alguna razon no pudo (p.ej. clave invalida).
    using ContentProvider = avastd::function<bool(avastd::string& out_content)>;

    explicit MemoryFileSystem(IFileSystem* fallback = nullptr);
    ~MemoryFileSystem() override = default;

    // Registra (o reemplaza) un archivo virtual. `size` es el tamano en
    // claro ya conocido de antemano (evita tener que invocar el provider
    // solo para responder FileSize()).
    void RegisterFile(const avastd::string& path, int64_t size, ContentProvider provider);
    void RemoveFile(const avastd::string& path);

    bool ReadFile(const avastd::string& path, avastd::string& out_content) override;
    bool WriteFile(const avastd::string& path, const avastd::string& content) override;
    bool DeleteFile(const avastd::string& path) override;

    bool CreateDirectory(const avastd::string& path) override;
    bool DeleteDirectory(const avastd::string& path) override;
    bool EnumerateDirectory(const avastd::string& path, avastd::vector<DirEntry>& out_entries) override;

    bool Exists(const avastd::string& path) override;
    bool IsDirectory(const avastd::string& path) override;
    int64_t FileSize(const avastd::string& path) override;

    avastd::string GetExecutableDirectory() override;

    // Normaliza separadores de ruta ('\\' -> '/') para que las claves
    // registradas (siempre con '/', ver avapack::EmbeddedFile::path)
    // matcheen contra las rutas que arma ModuleResolver con el separador
    // nativo del SO (module.cpp, JoinPath). No toca mayusculas/minusculas.
    static avastd::string NormalizeKey(const avastd::string& path);

private:
    struct Entry {
        int64_t size = 0;
        ContentProvider provider;
    };

    IFileSystem* fallback_;
    avastd::mutex mutex_;
    avastd::unordered_map<avastd::string, Entry> files_;
};

} // namespace platform
} // namespace ava

#endif // AVA_PLATFORM_MEMORY_MEMORYFILESYSTEM_H
