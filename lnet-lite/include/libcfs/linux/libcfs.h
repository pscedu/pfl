/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
#ifndef __LIBCFS_LINUX_LIBCFS_H__
#define __LIBCFS_LINUX_LIBCFS_H__

#ifndef __LIBCFS_LIBCFS_H__
#error Do not #include this file directly. #include <libcfs/libcfs.h> instead
#endif

#include <stdarg.h>
#include <libcfs/linux/linux-mem.h>
#include <libcfs/linux/linux-time.h>
#include <libcfs/linux/linux-prim.h>
#include <libcfs/linux/linux-lock.h>
#include <libcfs/linux/linux-fs.h>
#include <libcfs/linux/linux-tcpip.h>

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#else
#include <libcfs/types.h>
#endif


#ifdef __KERNEL__
# include <linux/types.h>
# include <linux/time.h>
# include <asm/timex.h>
#else
# include <sys/types.h>
# include <sys/time.h>
# define do_gettimeofday(tv) gettimeofday(tv, NULL);
typedef unsigned long long cycles_t;
#endif

#ifndef __KERNEL__
/* Userpace byte flipping */
#ifdef __APPLE__
# include <machine/endian.h>
# include <strings.h>
# define  bswap_16(x) ((((x) & 0x0f) << 8) | \
		       (((x) & 0xf0) >> 8))

# define  bswap_32(x) ((((x) & 0x000f) << 24) | \
		       (((x) & 0x00f0) <<  8) | \
		       (((x) & 0x0f00) >>  8) | \
		       (((x) & 0xf000) >> 24))

# define  bswap_64(x) ((((x) & 0x0000000f) << 56) | \
		       (((x) & 0x000000f0) << 40) | \
		       (((x) & 0x00000f00) << 24) | \
		       (((x) & 0x0000f000) <<  8) | \
		       (((x) & 0x000f0000) >>  8) | \
		       (((x) & 0x00f00000) >> 24) | \
		       (((x) & 0x0f000000) >> 40) | \
		       (((x) & 0xf0000000) >> 56))
#else
# include <endian.h>
# include <byteswap.h>
#endif
# define __swab16(x) bswap_16(x)
# define __swab32(x) bswap_32(x)
# define __swab64(x) bswap_64(x)
# define __swab16s(x) do {*(x) = bswap_16(*(x));} while (0)
# define __swab32s(x) do {*(x) = bswap_32(*(x));} while (0)
# define __swab64s(x) do {*(x) = bswap_64(*(x));} while (0)
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define le16_to_cpu(x) (x)
#  define cpu_to_le16(x) (x)
#  define le32_to_cpu(x) (x)
#  define cpu_to_le32(x) (x)
#  define le64_to_cpu(x) (x)
#  define cpu_to_le64(x) (x)
# else
#  if __BYTE_ORDER == __BIG_ENDIAN
#   define le16_to_cpu(x) bswap_16(x)
#   define cpu_to_le16(x) bswap_16(x)
#   define le32_to_cpu(x) bswap_32(x)
#   define cpu_to_le32(x) bswap_32(x)
#   define le64_to_cpu(x) bswap_64(x)
#   define cpu_to_le64(x) bswap_64(x)
#  else
#   error "Unknown byte order"
#  endif /* __BIG_ENDIAN */
# endif /* __LITTLE_ENDIAN */
#endif /* ! __KERNEL__ */

struct ptldebug_header {
        __u32 ph_len;
        __u32 ph_flags;
        __u32 ph_subsys;
        __u32 ph_mask;
        __u32 ph_cpu_id;
        __u32 ph_sec;
        __u64 ph_usec;
        __u32 ph_stack;
        __u32 ph_pid;
        __u32 ph_extern_pid;
        __u32 ph_line_num;
} __attribute__((packed));

#ifdef __KERNEL__
# include <linux/sched.h> /* THREAD_SIZE */
#else
# ifndef THREAD_SIZE /* x86_64 has THREAD_SIZE in userspace */
#  define THREAD_SIZE 8192
# endif
#endif

#define LUSTRE_TRACE_SIZE (THREAD_SIZE >> 5)

#if defined(__KERNEL__) && !defined(__x86_64__)
# ifdef  __ia64__
#  define CDEBUG_STACK() (THREAD_SIZE -                                 \
                          ((unsigned long)__builtin_dwarf_cfa() &       \
                           (THREAD_SIZE - 1)))
# else
#  define CDEBUG_STACK() (THREAD_SIZE -                                 \
                          ((unsigned long)__builtin_frame_address(0) &  \
                           (THREAD_SIZE - 1)))
# endif /* __ia64__ */

#define __CHECK_STACK(file, func, line)                                 \
do {                                                                    \
        unsigned long _stack = CDEBUG_STACK();                          \
                                                                        \
        if (_stack > 3*THREAD_SIZE/4 && _stack > libcfs_stack) {        \
                libcfs_stack = _stack;                                  \
                libcfs_debug_msg(NULL, DEBUG_SUBSYSTEM, D_WARNING,      \
                                 file, func, line,                      \
                                 "maximum lustre stack %lu\n", _stack); \
              /*panic("LBUG");*/                                        \
        }                                                               \
} while (0)
#define CHECK_STACK()     __CHECK_STACK(__FILE__, __func__, __LINE__)
#else /* !__KERNEL__ */
#define __CHECK_STACK(X, Y, Z) do { } while(0)
#define CHECK_STACK() do { } while(0)
#define CDEBUG_STACK() (0L)
#endif /* __KERNEL__ */

/* initial pid  */
#define LUSTRE_LNET_PID          12345

#define ENTRY_NESTING_SUPPORT (1)
#define ENTRY_NESTING   do {;} while (0)
#define EXIT_NESTING   do {;} while (0)
#define __current_nesting_level() (0)

/*
 * Platform specific declarations for cfs_curproc API (libcfs/curproc.h)
 *
 * Implementation is in linux-curproc.c
 */
#define CFS_CURPROC_COMM_MAX (sizeof ((struct task_struct *)0)->comm)

#if defined(__KERNEL__)
#include <linux/capability.h>
typedef kernel_cap_t cfs_kernel_cap_t;
#else
typedef __u32 cfs_kernel_cap_t;
#endif

#if defined(__KERNEL__)
/*
 * No stack-back-tracing in Linux for now.
 */
struct cfs_stack_trace {
};

#ifndef WITH_WATCHDOG
#define WITH_WATCHDOG
#endif

#endif

#endif /* _LINUX_LIBCFS_H */
