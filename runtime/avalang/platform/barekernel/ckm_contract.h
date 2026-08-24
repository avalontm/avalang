#ifndef AVA_PLATFORM_BAREKERNEL_CKM_CONTRACT_H
#define AVA_PLATFORM_BAREKERNEL_CKM_CONTRACT_H

// Por que este header NO incluye <stdint.h>/<stddef.h>: mismo problema
// que ya documenta runtime/avalang/platform/barekernel/stdcompat/ava_types.h
// -- el <stdint.h> de este toolchain (--without-headers, sin newlib) es
// solo un wrapper que hace `#include_next <stdint.h>` esperando una libc
// mas abajo en el search path; sin libc instalada, ese include_next
// falla ("stdint.h: No such file or directory"). ckm_contract.h es la
// primera cabecera del arbol de BareKernel que se compila con
// AVA_BAREKERNEL_TARGET_BINDING (arrastra bindings/target/ckm_syscall.h,
// que tiene el mismo problema, ver ese archivo), asi que es donde este
// error aparece primero -- no es un problema nuevo, es el mismo hueco de
// ava_types.h en un archivo que todavia no habia sido ejercitado por el
// syntax-check sustituto (sin i686-elf-g++ real disponible, ver
// docs/architecture/RUNTIME_CORE_AUDIT.md Seccion 11.2).
//
// Los mismos macros predefinidos del compilador (GCC/Clang, con o sin
// -ffreestanding, con o sin libc) que <stdint.h>/<stddef.h> usan para
// autogenerarse. Cubre tambien los tipos que necesita
// bindings/target/ckm_syscall.h (incluido mas abajo, dentro del bloque
// AVA_BAREKERNEL_TARGET_BINDING) -- ese archivo ya no incluye
// <stdint.h>/<stddef.h> por su cuenta, ver el comentario ahi.
typedef __UINT8_TYPE__  uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __INT32_TYPE__  int32_t;
typedef __INT64_TYPE__  int64_t;
typedef __SIZE_TYPE__   size_t;
typedef __INTPTR_TYPE__  intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;


#ifdef __cplusplus
extern "C" {
#endif

#define CKM_O_RDONLY   0x0000
#define CKM_O_WRONLY   0x0001
#define CKM_O_RDWR     0x0002
#define CKM_O_CREAT    0x0040
#define CKM_O_TRUNC    0x0200
#define CKM_O_APPEND   0x0400

#define CKM_STDIN      0
#define CKM_STDOUT     1
#define CKM_STDERR     2

#define CKM_S_IRUSR    0400
#define CKM_S_IWUSR    0200
#define CKM_S_IRWXU    0700

#define CKM_RTLD_NOW   0x0002
#define CKM_RTLD_LAZY  0x0001

#define CKM_ENOENT     (-2)
#define CKM_EACCES     (-13)
#define CKM_EEXIST     (-17)
#define CKM_EINVAL     (-22)
#define CKM_ENOTDIR    (-20)
#define CKM_ENOTEMPTY  (-39)
#define CKM_EBADF      (-9)
#define CKM_EFAULT     (-14)
#define CKM_ENOSPC     (-28)
#define CKM_ECHILD     (-10)
#define CKM_ENOMEM     (-12)
#define CKM_ERANGE     (-34)

struct CkmStat {
    uint64_t size;
    uint32_t is_directory;
    uint32_t mode;
};

struct CkmDirEntry {
    char     name[256];
    uint32_t is_directory;
};

#if defined(AVA_BAREKERNEL_TARGET_BINDING)
#  include "ckm_syscall.h"
#endif

#if !defined(AVA_BAREKERNEL_TARGET_BINDING)
int     ckm_open(const char* path, int flags, int mode);
int     ckm_close(int fd);
long    ckm_read(int fd, void* buf, long count);
long    ckm_write(int fd, const void* buf, long count);
int     ckm_stat(const char* path, struct CkmStat* out);
int     ckm_unlink(const char* path);
int     ckm_mkdir(const char* path);
int     ckm_rmdir(const char* path);

void*   ckm_opendir(const char* path);
int     ckm_readdir(void* handle, struct CkmDirEntry* out);
int     ckm_closedir(void* handle);

void*   ckm_dlopen(const char* path, int flags);
void*   ckm_dlsym(void* handle, const char* symbol);
int     ckm_dlclose(void* handle);
const char* ckm_dlerror(void);

void*   ckm_malloc(size_t size);
void    ckm_free(void* ptr);

int64_t ckm_now_ms(void);
int64_t ckm_highres_now_ns(void);
void    ckm_sleep_ms(uint32_t milliseconds);

int     ckm_getpid(void);
int     ckm_spawn(const char* path, const char* const* argv, int argc);
int     ckm_waitpid(int pid, int* out_exit_code);
void    ckm_exit(int code);

int     ckm_getcwd(char* buf, size_t size);
int     ckm_chdir(const char* path);

int     ckm_getenv(const char* name, char* buf, size_t size);
int     ckm_setenv(const char* name, const char* value);

#endif

typedef void (*CkmThreadFunc)(void* arg);
int     ckm_thread_create(CkmThreadFunc func, void* arg);
int     ckm_thread_join(int thread_handle);

typedef struct CkmMutex {
    uint32_t state;
} CkmMutex;
int     ckm_mutex_init(CkmMutex* m);
int     ckm_mutex_lock(CkmMutex* m);
int     ckm_mutex_unlock(CkmMutex* m);
int     ckm_mutex_trylock(CkmMutex* m);


#ifdef __cplusplus
}
#endif

#endif
