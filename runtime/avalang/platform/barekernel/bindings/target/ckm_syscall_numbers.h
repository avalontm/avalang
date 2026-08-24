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
#define AVA_TARGET_SYSCALL_NUM_EXEC             2
#define AVA_TARGET_SYSCALL_NUM_WAITPID          3
#define AVA_TARGET_SYSCALL_NUM_GETPID           4
#define AVA_TARGET_SYSCALL_NUM_KILL             6
#define AVA_TARGET_SYSCALL_NUM_SLEEP            8
#define AVA_TARGET_SYSCALL_NUM_SPAWN            14

#define AVA_TARGET_SYSCALL_NUM_MALLOC           20
#define AVA_TARGET_SYSCALL_NUM_FREE             21
#define AVA_TARGET_SYSCALL_NUM_REALLOC          22
#define AVA_TARGET_SYSCALL_NUM_MEMALIGN         23
#define AVA_TARGET_SYSCALL_NUM_MPROTECT         26

#define AVA_TARGET_SYSCALL_NUM_OPEN             40
#define AVA_TARGET_SYSCALL_NUM_CLOSE            41
#define AVA_TARGET_SYSCALL_NUM_READ             42
#define AVA_TARGET_SYSCALL_NUM_WRITE            43
#define AVA_TARGET_SYSCALL_NUM_LSEEK            44
#define AVA_TARGET_SYSCALL_NUM_STAT             45
#define AVA_TARGET_SYSCALL_NUM_FCNTL            48
#define AVA_TARGET_SYSCALL_NUM_DUP              49
#define AVA_TARGET_SYSCALL_NUM_DUP2             50
#define AVA_TARGET_SYSCALL_NUM_UNLINK           54
#define AVA_TARGET_SYSCALL_NUM_CHMOD            57
#define AVA_TARGET_SYSCALL_NUM_CHOWN            58
#define AVA_TARGET_SYSCALL_NUM_POLL             59
#define AVA_TARGET_SYSCALL_NUM_SELECT           60

#define AVA_TARGET_SYSCALL_NUM_GETCWD           80
#define AVA_TARGET_SYSCALL_NUM_CHDIR            81
#define AVA_TARGET_SYSCALL_NUM_MKDIR            82
#define AVA_TARGET_SYSCALL_NUM_RMDIR            83
#define AVA_TARGET_SYSCALL_NUM_READDIR          84

#define AVA_TARGET_SYSCALL_NUM_GETTIMEOFDAY     100
#define AVA_TARGET_SYSCALL_NUM_CLOCK_GETTIME    105

#define AVA_TARGET_SYSCALL_NUM_SET_TEXT_COLOR   126

#define AVA_TARGET_SYSCALL_NUM_INPUT_POLL       150
#define AVA_TARGET_SYSCALL_NUM_INPUT_READ       151
#define AVA_TARGET_SYSCALL_NUM_INPUT_CONFIG     152

#define AVA_TARGET_SYSCALL_NUM_AUDIO_OPEN       170
#define AVA_TARGET_SYSCALL_NUM_AUDIO_WRITE      171
#define AVA_TARGET_SYSCALL_NUM_AUDIO_CLOSE      172
#define AVA_TARGET_SYSCALL_NUM_AUDIO_CONFIG     173

#define AVA_TARGET_SYSCALL_NUM_SOCKET           190
#define AVA_TARGET_SYSCALL_NUM_BIND             191
#define AVA_TARGET_SYSCALL_NUM_LISTEN           192
#define AVA_TARGET_SYSCALL_NUM_ACCEPT           193
#define AVA_TARGET_SYSCALL_NUM_CONNECT          194
#define AVA_TARGET_SYSCALL_NUM_SEND             195
#define AVA_TARGET_SYSCALL_NUM_RECV             196
#define AVA_TARGET_SYSCALL_NUM_SENDTO           197
#define AVA_TARGET_SYSCALL_NUM_RECVFROM         198
#define AVA_TARGET_SYSCALL_NUM_SOCKCLOSE        199

#define AVA_TARGET_SYSCALL_NUM_GETENV           222
#define AVA_TARGET_SYSCALL_NUM_SETENV           223

#define AVA_TARGET_SYSCALL_NUM_DLOPEN           230
#define AVA_TARGET_SYSCALL_NUM_DLSYM            231
#define AVA_TARGET_SYSCALL_NUM_DLCLOSE          232
#define AVA_TARGET_SYSCALL_NUM_DLERROR          233
#define AVA_TARGET_SYSCALL_NUM_GETRANDOM        234
#define AVA_TARGET_SYSCALL_NUM_GETRLIMIT        235
#define AVA_TARGET_SYSCALL_NUM_SETRLIMIT        236

#endif
