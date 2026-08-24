#ifndef AVA_PLATFORM_BAREKERNEL_TARGET_CAPS_H
#define AVA_PLATFORM_BAREKERNEL_TARGET_CAPS_H

// Capabilities observed from the supplied kernel source.
//
// These values describe what the current kernel ABI actually exposes,
// not what BareKernel wishes existed. Unsupported facilities remain 0
// so the generic PAL degrades instead of calling a nonexistent service.

#define CKM_CAP_DYNAMIC_LOADING  1
#define CKM_CAP_THREADS          0
#define CKM_CAP_MUTEX            0
#define CKM_CAP_ENVVARS          1
#define CKM_CAP_DIR_ENUM         1
#define CKM_CAP_COLOR            1
#define CKM_CAP_PROCESS_EXEC     1  /* SYS_WAITPID real desde Fase 2, SYS_EXEC real desde Fase 4 */
#define CKM_CAP_TIMERS           0  /* no kernel timer queue */
#define CKM_CAP_STD_EXCEPTIONS   0  /* not established by the supplied source */
#define CKM_CAP_LIBSTDCPP        0  /* target is freestanding/no-libstdc++ */

#endif
