#ifndef AVA_PLATFORM_BAREKERNEL_TARGET_SYSCALL_NUMBERS_H
#define AVA_PLATFORM_BAREKERNEL_TARGET_SYSCALL_NUMBERS_H

/*
 * Target syscall-number table.
 *
 * Values below are copied from the kernel project's own
 * include/syscall/syscall_types.hpp (enum SyscallNumber). They are the
 * kernel's real public ABI, not guesses.
 *
 * If the kernel project's enum changes, update this file to match -- do not
 * let it drift silently. This header is the single place BareKernel's target
 * binding depends on for numeric values.
 */

#define AVA_TARGET_SYSCALL_NUM_EXIT             0
#define AVA_TARGET_SYSCALL_NUM_GETPID           4
#define AVA_TARGET_SYSCALL_NUM_SLEEP            8
#define AVA_TARGET_SYSCALL_NUM_SPAWN            14

#define AVA_TARGET_SYSCALL_NUM_MALLOC           20
#define AVA_TARGET_SYSCALL_NUM_FREE             21
#define AVA_TARGET_SYSCALL_NUM_REALLOC          22
#define AVA_TARGET_SYSCALL_NUM_MEMALIGN         23

#define AVA_TARGET_SYSCALL_NUM_OPEN             40
#define AVA_TARGET_SYSCALL_NUM_CLOSE            41
#define AVA_TARGET_SYSCALL_NUM_READ             42
#define AVA_TARGET_SYSCALL_NUM_WRITE            43
#define AVA_TARGET_SYSCALL_NUM_LSEEK            44
#define AVA_TARGET_SYSCALL_NUM_STAT             45
#define AVA_TARGET_SYSCALL_NUM_UNLINK           54

#define AVA_TARGET_SYSCALL_NUM_GETCWD           80
#define AVA_TARGET_SYSCALL_NUM_CHDIR            81
#define AVA_TARGET_SYSCALL_NUM_MKDIR            82
#define AVA_TARGET_SYSCALL_NUM_RMDIR            83
#define AVA_TARGET_SYSCALL_NUM_READDIR          84

#define AVA_TARGET_SYSCALL_NUM_GETTIMEOFDAY     100
#define AVA_TARGET_SYSCALL_NUM_CLOCK_GETTIME    105

#define AVA_TARGET_SYSCALL_NUM_SET_TEXT_COLOR   126

#define AVA_TARGET_SYSCALL_NUM_GETENV           222
#define AVA_TARGET_SYSCALL_NUM_SETENV           223

#define AVA_TARGET_SYSCALL_NUM_DLOPEN           230
#define AVA_TARGET_SYSCALL_NUM_DLSYM            231
#define AVA_TARGET_SYSCALL_NUM_DLCLOSE          232
#define AVA_TARGET_SYSCALL_NUM_DLERROR          233

#endif
