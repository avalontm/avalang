#ifndef AVA_PLATFORM_BAREKERNEL_TARGET_SYSCALL_H
#define AVA_PLATFORM_BAREKERNEL_TARGET_SYSCALL_H

// uint32_t/int32_t/int64_t/size_t/intptr_t/uintptr_t vienen de
// ../../ckm_contract.h (typedefs via macros del compilador, sin
// <stdint.h>/<stddef.h> reales -- este toolchain es --without-headers,
// ver el comentario en ese archivo). Este header solo se incluye desde
// ahi (confirmado: unico includer en todo el arbol), siempre despues de
// esos typedefs, asi que no hace falta -- ni funciona -- volver a incluir
// <stdint.h>/<stddef.h> aca.
#include "ckm_syscall_numbers.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Raw syscall ABI for the currently supplied target:
 *   i386: int 0x80
 *   eax = syscall number
 *   ebx, ecx, edx, esi, edi = arguments 1..5
 *   eax = signed return value
 *
 * The kernel source supplied for x86 explicitly implements this convention.
 * x86_64 and ARM64 are intentionally not claimed here because the supplied
 * source does not contain a complete user-entry ABI for them.
 */
#if defined(__i386__)

static inline intptr_t ckm_raw_syscall0(uint32_t n) {
    intptr_t r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n)
                      : "memory");
    return r;
}

static inline intptr_t ckm_raw_syscall1(uint32_t n, uintptr_t a1) {
    intptr_t r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "b"(a1)
                      : "memory");
    return r;
}

static inline intptr_t ckm_raw_syscall2(uint32_t n, uintptr_t a1, uintptr_t a2) {
    intptr_t r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "b"(a1), "c"(a2)
                      : "memory");
    return r;
}

static inline intptr_t ckm_raw_syscall3(uint32_t n, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    intptr_t r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "b"(a1), "c"(a2), "d"(a3)
                      : "memory");
    return r;
}

static inline intptr_t ckm_raw_syscall4(uint32_t n, uintptr_t a1, uintptr_t a2,
                                        uintptr_t a3, uintptr_t a4) {
    intptr_t r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "b"(a1), "c"(a2), "d"(a3), "S"(a4)
                      : "memory");
    return r;
}

static inline intptr_t ckm_raw_syscall5(uint32_t n, uintptr_t a1, uintptr_t a2,
                                        uintptr_t a3, uintptr_t a4, uintptr_t a5) {
    intptr_t r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(n), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5)
                      : "memory");
    return r;
}

#else
# error "The supplied kernel source does not provide a complete user syscall ABI for this target architecture yet."
#endif

static inline int ckm_open(const char* p, int f, int m) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_OPEN, (uintptr_t)p,
                                  (uintptr_t)f, (uintptr_t)m);
}
static inline int ckm_close(int fd) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_CLOSE, (uintptr_t)fd);
}
static inline long ckm_read(int fd, void* b, long n) {
    return (long)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_READ, (uintptr_t)fd,
                                   (uintptr_t)b, (uintptr_t)n);
}
static inline long ckm_write(int fd, const void* b, long n) {
    return (long)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_WRITE, (uintptr_t)fd,
                                   (uintptr_t)b, (uintptr_t)n);
}
static inline int ckm_lseek(int fd, int32_t off, int whence) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_LSEEK, (uintptr_t)fd,
                                  (uintptr_t)off, (uintptr_t)whence);
}
struct CkmTargetFileStat {
    uint32_t st_size;
    uint32_t st_mode;
    uint32_t st_mtime;
};

static inline int ckm_stat(const char* p, struct CkmStat* st) {
    if (!st) return -22;
    CkmTargetFileStat raw;
    intptr_t r = ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_STAT,
                                   (uintptr_t)p, (uintptr_t)&raw);
    if (r < 0) return (int)r;
    st->size = raw.st_size;
    st->mode = raw.st_mode;
    st->is_directory = 0; /* target stat currently reports regular mode only */
    return 0;
}
static inline int ckm_unlink(const char* p) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_UNLINK, (uintptr_t)p);
}
static inline int ckm_mkdir(const char* p) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_MKDIR, (uintptr_t)p, 0700);
}
static inline int ckm_rmdir(const char* p) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_RMDIR, (uintptr_t)p);
}
static inline int ckm_getcwd(char* p, size_t n) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_GETCWD, (uintptr_t)p, (uintptr_t)n);
}
static inline int ckm_chdir(const char* p) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_CHDIR, (uintptr_t)p);
}
struct CkmTargetDirEntry {
    char name[256];
    uint32_t type;
    uint32_t size;
};

static inline void* ckm_opendir(const char* path) {
    int fd = ckm_open(path, 0, 0);
    return fd < 0 ? (void*)0 : (void*)(uintptr_t)(uint32_t)fd;
}

static inline int ckm_readdir(void* h, struct CkmDirEntry* e) {
    if (!h || !e) return -22;
    CkmTargetDirEntry raw;
    intptr_t r = ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_READDIR,
                                   (uintptr_t)h, (uintptr_t)&raw,
                                   (uintptr_t)sizeof(raw));
    /*
     * The kernel's sys_readdir returns 1 = entry read, 0 = end of
     * directory, and a negative value on error. The CKM contract (see
     * ckm_contract.h) wants the opposite polarity for success/end:
     * 0 = entry filled, 1 = end. Translate here so the generic PAL
     * adapter (Layer B) never has to know about the kernel's convention.
     */
    if (r < 0) return (int)r;
    if (r == 0) return 1; /* end of directory */

    for (size_t i = 0; i < sizeof(e->name); ++i) {
        e->name[i] = raw.name[i];
        if (raw.name[i] == '\0') break;
        if (i + 1 == sizeof(e->name)) e->name[i] = '\0';
    }
    e->is_directory = raw.type ? 1u : 0u;
    return 0; /* entry filled */
}

static inline int ckm_closedir(void* h) {
    return ckm_close((int)(uintptr_t)h);
}

static inline void* ckm_dlopen(const char* p, int f) {
    return (void*)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_DLOPEN, (uintptr_t)p, (uintptr_t)f);
}
static inline void* ckm_dlsym(void* h, const char* s) {
    return (void*)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_DLSYM, (uintptr_t)h, (uintptr_t)s);
}
static inline int ckm_dlclose(void* h) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_DLCLOSE, (uintptr_t)h);
}
static inline const char* ckm_dlerror(void) {
    static char buffer[256];
    intptr_t r = ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_DLERROR,
                                   (uintptr_t)buffer, sizeof(buffer));
    return r < 0 ? "" : buffer;
}

static inline void* ckm_malloc(size_t n) {
    return (void*)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_MALLOC, (uintptr_t)n);
}
static inline void ckm_free(void* p) {
    (void)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_FREE, (uintptr_t)p);
}
static inline void* ckm_realloc(void* p, size_t n) {
    return (void*)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_REALLOC, (uintptr_t)p, (uintptr_t)n);
}
static inline void* ckm_memalign(size_t a, size_t n) {
    return (void*)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_MEMALIGN, (uintptr_t)a, (uintptr_t)n);
}

struct CkmTimeSpec {
    int64_t tv_sec;
    int64_t tv_nsec;
};
static inline int ckm_clock_gettime(int id, CkmTimeSpec* ts) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_CLOCK_GETTIME, (uintptr_t)id,
                                  (uintptr_t)ts);
}
struct CkmTargetTimeVal {
    int32_t tv_sec;
    int32_t tv_usec;
};
static inline int64_t ckm_gettimeofday_seconds(void) {
    CkmTargetTimeVal tv = {0, 0};
    intptr_t r = ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_GETTIMEOFDAY,
                                   (uintptr_t)&tv, 0);
    return r < 0 ? r : tv.tv_sec;
}
static inline int64_t ckm_now_ms(void) {
    CkmTargetTimeVal tv = {0, 0};
    intptr_t r = ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_GETTIMEOFDAY,
                                   (uintptr_t)&tv, 0);
    if (r < 0) return r;
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
static inline int64_t ckm_highres_now_ns(void) {
    CkmTargetTimeVal tv = {0, 0};
    intptr_t r = ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_GETTIMEOFDAY,
                                   (uintptr_t)&tv, 0);
    if (r < 0) return r;
    return (int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_usec * 1000LL;
}
static inline int ckm_sleep_ms(uint32_t ms) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_SLEEP, (uintptr_t)ms);
}
static inline int ckm_getpid(void) {
    return (int)ckm_raw_syscall0(AVA_TARGET_SYSCALL_NUM_GETPID);
}
/*
 * NOTE: the kernel's sys_spawn is (path, flags, target_uid) -- it does not
 * take argv/argc. The CKM contract's ckm_spawn(path, argv, argc) therefore
 * cannot be implemented faithfully yet (no argv-passing mechanism exists on
 * the kernel side, and sys_waitpid is unimplemented -- see ckm_waitpid
 * below). This wrapper is unreachable while CKM_CAP_PROCESS_EXEC=0 in
 * BareKernelCaps_target.h; argv/argc are intentionally dropped rather than
 * silently reinterpreted as kernel arguments.
 */
static inline int ckm_spawn(const char* p, const char* const* argv, int argc) {
    (void)argv;
    (void)argc;
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_SPAWN, (uintptr_t)p,
                                  /* flags */ 0, /* target_uid */ 0);
}
static inline void ckm_exit(int code) {
    (void)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_EXIT, (uintptr_t)code);
    for (;;) {}
}
/*
 * The kernel's sys_waitpid is registered but hard-coded to return ENOSYS
 * (not yet implemented). There is no SYS_WAITPID entry in the kernel's
 * SyscallNumber enum to call even if we wanted to. Fail the same way the
 * kernel itself does, rather than guessing a number. Only reachable if
 * CKM_CAP_PROCESS_EXEC is turned on without this being implemented first.
 */
static inline int ckm_waitpid(int pid, int* out_exit_code) {
    (void)pid;
    (void)out_exit_code;
    return -38; /* ENOSYS, matches Syscall::ErrorCode::ENOSYS */
}
static inline int ckm_getenv(const char* n, char* b, size_t s) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_GETENV, (uintptr_t)n,
                                  (uintptr_t)b, (uintptr_t)s);
}
static inline int ckm_setenv(const char* n, const char* v) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_SETENV, (uintptr_t)n, (uintptr_t)v);
}
static inline int ckm_set_text_color(int c) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_SET_TEXT_COLOR, (uintptr_t)c);
}

#ifdef __cplusplus
}
#endif

#endif
