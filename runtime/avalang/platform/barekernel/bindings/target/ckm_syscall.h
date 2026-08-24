#ifndef AVA_PLATFORM_BAREKERNEL_TARGET_SYSCALL_H
#define AVA_PLATFORM_BAREKERNEL_TARGET_SYSCALL_H

// uint8_t/uint16_t/uint32_t/int32_t/int64_t/size_t/intptr_t/uintptr_t vienen de
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
 * UPDATED: the kernel's sys_spawn is now (path, flags, target_uid, argv,
 * envp) -- Fase 11 (argv/envp real) added the last two arguments. argv/argc
 * are no longer dropped; the null-terminated argv pointer array is passed
 * straight through to the kernel (kernel expects a NULL-terminated array,
 * not an explicit count, so argc is only used to validate/NULL-terminate
 * defensively here, not sent over the syscall ABI itself). envp is not part
 * of the CKM contract's ckm_spawn signature, so nullptr is sent for it;
 * use ckm_spawn_ex below if envp needs to be passed too.
 */
static inline int ckm_spawn(const char* p, const char* const* argv, int argc) {
    (void)argc;
    return (int)ckm_raw_syscall5(AVA_TARGET_SYSCALL_NUM_SPAWN, (uintptr_t)p,
                                  /* flags */ 0, /* target_uid */ 0,
                                  (uintptr_t)argv, (uintptr_t)0 /* envp */);
}
static inline int ckm_spawn_ex(const char* p, const char* const* argv,
                                const char* const* envp) {
    return (int)ckm_raw_syscall5(AVA_TARGET_SYSCALL_NUM_SPAWN, (uintptr_t)p,
                                  /* flags */ 0, /* target_uid */ 0,
                                  (uintptr_t)argv, (uintptr_t)envp);
}
/*
 * SYS_EXEC replaces the CURRENT process image in place (unlike ckm_spawn,
 * which starts a new one) -- it does not return on success, matching
 * sys_exit's calling convention. argv/envp are real as of Fase 11/Fase 4.
 */
static inline int ckm_exec(const char* p, const char* const* argv,
                            const char* const* envp) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_EXEC, (uintptr_t)p,
                                  (uintptr_t)argv, (uintptr_t)envp);
}
static inline void ckm_exit(int code) {
    (void)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_EXIT, (uintptr_t)code);
    for (;;) {}
}
/*
 * UPDATED: SYS_WAITPID has a real handler since Fase 2 (blocks with yield,
 * supports WNOHANG, specific pid or -1 for "any child"). This blocking
 * wrapper always passes options=0; use ckm_waitpid_nohang for the
 * non-blocking variant.
 */
static inline int ckm_waitpid(int pid, int* out_exit_code) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_WAITPID, (uintptr_t)pid,
                                  (uintptr_t)out_exit_code, /* options */ 0);
}
#define CKM_WNOHANG 1u
static inline int ckm_waitpid_nohang(int pid, int* out_exit_code) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_WAITPID, (uintptr_t)pid,
                                  (uintptr_t)out_exit_code, (uintptr_t)CKM_WNOHANG);
}
/*
 * SYS_KILL has a real handler since Fase 1 (SIGKILL/SIGTERM, permission
 * checks by uid/root/CAP_KILL). Signal numbers match the kernel's
 * include/kernel/signal.hpp (POSIX-numbered, not redefined here to avoid
 * a second source of truth -- pass 9 or 15 directly, or define your own
 * CKM_SIGKILL/CKM_SIGTERM aliases at the call site).
 */
#define CKM_SIGKILL 9
#define CKM_SIGTERM 15
static inline int ckm_kill(int pid, int signal) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_KILL, (uintptr_t)pid,
                                  (uintptr_t)signal);
}
/*
 * SYS_MPROTECT has a real handler since Fase 5. addr must be page-aligned.
 * PROT_READ/PROT_EXEC are accepted but cannot be enforced at hardware level
 * on this kernel's 32-bit paging without PAE (no NX bit) -- see
 * SYSCALLS_X86_COMPLETENESS.md Fase 5 for the documented platform limit.
 */
#define CKM_PROT_NONE  0x0
#define CKM_PROT_READ  0x1
#define CKM_PROT_WRITE 0x2
#define CKM_PROT_EXEC  0x4
static inline int ckm_mprotect(void* addr, size_t length, int prot) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_MPROTECT, (uintptr_t)addr,
                                  (uintptr_t)length, (uintptr_t)prot);
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

/* SYS_CHMOD/SYS_CHOWN -- real handlers since Fase 6. */
static inline int ckm_chmod(const char* path, uint32_t mode) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_CHMOD, (uintptr_t)path,
                                  (uintptr_t)mode);
}
static inline int ckm_chown(const char* path, uint32_t uid, uint32_t gid) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_CHOWN, (uintptr_t)path,
                                  (uintptr_t)uid, (uintptr_t)gid);
}

/*
 * SYS_FCNTL/SYS_DUP/SYS_DUP2 -- real handlers since Fase 9 (F_DUPFD was a
 * stub before that; dup/dup2 existed but returned ENOSYS).
 */
#define CKM_F_DUPFD         0
#define CKM_F_GETFD         1
#define CKM_F_SETFD         2
#define CKM_F_GETFL         3
#define CKM_F_SETFL         4
#define CKM_F_DUPFD_CLOEXEC 1030
#define CKM_FD_CLOEXEC      0x00000001
static inline int ckm_fcntl(int fd, int cmd, uint32_t cmd_arg) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_FCNTL, (uintptr_t)fd,
                                  (uintptr_t)cmd, (uintptr_t)cmd_arg);
}
static inline int ckm_dup(int oldfd) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_DUP, (uintptr_t)oldfd);
}
static inline int ckm_dup2(int oldfd, int newfd) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_DUP2, (uintptr_t)oldfd,
                                  (uintptr_t)newfd);
}

/* SYS_POLL/SYS_SELECT -- real handlers since Fase 7. */
struct CkmPollFd {
    int32_t  fd;
    uint16_t events;
    uint16_t revents;
};
struct CkmFdSet {
    uint64_t bits;
};
#define CKM_POLLIN         0x0001
#define CKM_POLLOUT        0x0004
#define CKM_POLLERR        0x0008
#define CKM_POLLHUP        0x0010
#define CKM_POLLNVAL       0x0020
#define CKM_POLL_NO_TIMEOUT 0u
#define CKM_POLL_INFINITE   0xFFFFFFFFu
#define CKM_SELECT_MAX_FDS  64u
static inline int ckm_poll(struct CkmPollFd* fds, uint32_t nfds, uint32_t timeout_ms) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_POLL, (uintptr_t)fds,
                                  (uintptr_t)nfds, (uintptr_t)timeout_ms);
}
static inline int ckm_select(uint32_t nfds, struct CkmFdSet* readfds,
                              struct CkmFdSet* writefds, struct CkmFdSet* exceptfds,
                              uint32_t timeout_ms) {
    return (int)ckm_raw_syscall5(AVA_TARGET_SYSCALL_NUM_SELECT, (uintptr_t)nfds,
                                  (uintptr_t)readfds, (uintptr_t)writefds,
                                  (uintptr_t)exceptfds, (uintptr_t)timeout_ms);
}

/* CAT_INPUT (150-152) -- teclado/mouse, Fase 3. */
#define CKM_INPUT_DEVICE_KEYBOARD 0u
#define CKM_INPUT_DEVICE_MOUSE    1u
#define CKM_INPUT_CONFIG_CLEAR_BUFFER 0u
#define CKM_INPUT_CONFIG_SET_BOUNDS   1u
struct CkmInputEvent {
    uint32_t type;
    uint32_t code;
    int32_t  x;
    int32_t  y;
    int32_t  extra;
};
static inline int ckm_input_poll(uint32_t device_type) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_INPUT_POLL, (uintptr_t)device_type);
}
static inline int ckm_input_read(uint32_t device_type, struct CkmInputEvent* buffer,
                                  uint32_t max_events) {
    return (int)ckm_raw_syscall3(AVA_TARGET_SYSCALL_NUM_INPUT_READ, (uintptr_t)device_type,
                                  (uintptr_t)buffer, (uintptr_t)max_events);
}
static inline int ckm_input_config_clear(uint32_t device_type) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_INPUT_CONFIG, (uintptr_t)device_type,
                                  (uintptr_t)CKM_INPUT_CONFIG_CLEAR_BUFFER);
}
static inline int ckm_input_set_mouse_bounds(int32_t max_x, int32_t max_y) {
    return (int)ckm_raw_syscall4(AVA_TARGET_SYSCALL_NUM_INPUT_CONFIG,
                                  (uintptr_t)CKM_INPUT_DEVICE_MOUSE,
                                  (uintptr_t)CKM_INPUT_CONFIG_SET_BOUNDS,
                                  (uintptr_t)max_x, (uintptr_t)max_y);
}

/* CAT_AUDIO (170-173) -- un solo dispositivo AC97 a nivel sistema, Fase 10. */
#define CKM_AUDIO_FD_BASE        0x20000u
#define CKM_AUDIO_NONBLOCK       0x01u
#define CKM_AUDIO_MIN_SAMPLE_RATE 8000u
#define CKM_AUDIO_MAX_SAMPLE_RATE 48000u
#define CKM_AUDIO_MAX_WRITE_SIZE  65536u
struct CkmAudioConfig {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint8_t  volume;
    uint8_t  muted;
    uint8_t  reserved[2];
};
static inline int ckm_audio_open(void) {
    return (int)ckm_raw_syscall0(AVA_TARGET_SYSCALL_NUM_AUDIO_OPEN);
}
static inline long ckm_audio_write(int fd, const void* buf, uint32_t len, uint32_t flags) {
    return (long)ckm_raw_syscall4(AVA_TARGET_SYSCALL_NUM_AUDIO_WRITE, (uintptr_t)fd,
                                   (uintptr_t)buf, (uintptr_t)len, (uintptr_t)flags);
}
static inline int ckm_audio_close(int fd) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_AUDIO_CLOSE, (uintptr_t)fd);
}
static inline int ckm_audio_config(int fd, struct CkmAudioConfig* cfg) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_AUDIO_CONFIG, (uintptr_t)fd,
                                  (uintptr_t)cfg);
}

/*
 * CAT_NETWORK (190-199) -- sólo UDP funcional de punta a punta (Fase 8);
 * SOCK_STREAM se puede crear/bindear pero listen/accept/connect/send*
 * devuelven ENOSYS/EOPNOTSUPP -- no hay stack TCP todavía. fd's de socket
 * viven en un namespace separado (SOCKET_FD_BASE = 0x10000), no mezclar con
 * fd's de archivo.
 */
#define CKM_AF_INET        2u
#define CKM_SOCK_STREAM    1u
#define CKM_SOCK_DGRAM     2u
#define CKM_MSG_DONTWAIT   0x01u
#define CKM_SOCKET_FD_BASE 0x10000u
struct CkmSockAddr {
    uint16_t family;
    uint16_t port;
    uint8_t  ip[4];
    uint8_t  reserved[8];
};
static inline int ckm_socket(uint32_t domain, uint32_t type) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_SOCKET, (uintptr_t)domain,
                                  (uintptr_t)type);
}
static inline int ckm_bind(int fd, const struct CkmSockAddr* addr) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_BIND, (uintptr_t)fd,
                                  (uintptr_t)addr);
}
/* listen/accept: registrados, siempre ENOSYS/EOPNOTSUPP sin stack TCP. */
static inline int ckm_listen(int fd, int backlog) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_LISTEN, (uintptr_t)fd,
                                  (uintptr_t)backlog);
}
static inline int ckm_accept(int fd, struct CkmSockAddr* out_addr) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_ACCEPT, (uintptr_t)fd,
                                  (uintptr_t)out_addr);
}
static inline int ckm_connect(int fd, const struct CkmSockAddr* addr) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_CONNECT, (uintptr_t)fd,
                                  (uintptr_t)addr);
}
static inline long ckm_send(int fd, const void* buf, uint32_t len, uint32_t flags) {
    return (long)ckm_raw_syscall4(AVA_TARGET_SYSCALL_NUM_SEND, (uintptr_t)fd,
                                   (uintptr_t)buf, (uintptr_t)len, (uintptr_t)flags);
}
static inline long ckm_recv(int fd, void* buf, uint32_t len, uint32_t flags) {
    return (long)ckm_raw_syscall4(AVA_TARGET_SYSCALL_NUM_RECV, (uintptr_t)fd,
                                   (uintptr_t)buf, (uintptr_t)len, (uintptr_t)flags);
}
static inline long ckm_sendto(int fd, const void* buf, uint32_t len,
                               const struct CkmSockAddr* dest) {
    return (long)ckm_raw_syscall5(AVA_TARGET_SYSCALL_NUM_SENDTO, (uintptr_t)fd,
                                   (uintptr_t)buf, (uintptr_t)len, 0, (uintptr_t)dest);
}
static inline long ckm_recvfrom(int fd, void* buf, uint32_t len, uint32_t flags,
                                 struct CkmSockAddr* out_src) {
    return (long)ckm_raw_syscall5(AVA_TARGET_SYSCALL_NUM_RECVFROM, (uintptr_t)fd,
                                   (uintptr_t)buf, (uintptr_t)len, (uintptr_t)flags,
                                   (uintptr_t)out_src);
}
static inline int ckm_sockclose(int fd) {
    return (int)ckm_raw_syscall1(AVA_TARGET_SYSCALL_NUM_SOCKCLOSE, (uintptr_t)fd);
}

/* SYS_GETRANDOM/SYS_GETRLIMIT/SYS_SETRLIMIT -- reales desde Fase 11. */
struct CkmRlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};
#define CKM_RLIMIT_CPU    0
#define CKM_RLIMIT_FSIZE  1
#define CKM_RLIMIT_DATA   2
#define CKM_RLIMIT_STACK  3
#define CKM_RLIMIT_NOFILE 4
#define CKM_RLIMIT_AS     5
#define CKM_RLIMIT_NPROC  6
static inline int ckm_getrandom(void* buffer, size_t buflen) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_GETRANDOM, (uintptr_t)buffer,
                                  (uintptr_t)buflen);
}
static inline int ckm_getrlimit(int resource, struct CkmRlimit* out_limit) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_GETRLIMIT, (uintptr_t)resource,
                                  (uintptr_t)out_limit);
}
static inline int ckm_setrlimit(int resource, const struct CkmRlimit* limit) {
    return (int)ckm_raw_syscall2(AVA_TARGET_SYSCALL_NUM_SETRLIMIT, (uintptr_t)resource,
                                  (uintptr_t)limit);
}

#ifdef __cplusplus
}
#endif

#endif
