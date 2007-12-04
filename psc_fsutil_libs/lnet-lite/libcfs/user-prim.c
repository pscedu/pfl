/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2004 Cluster File Systems, Inc.
 * Author: Nikita Danilov <nikita@clusterfs.com>
 *
 * This file is part of Lustre, http://www.lustre.org.
 *
 * Lustre is free software; you can redistribute it and/or modify it under the
 * terms of version 2 of the GNU General Public License as published by the
 * Free Software Foundation.
 *
 * Lustre is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Lustre; if not, write to the Free Software Foundation, Inc., 675 Mass
 * Ave, Cambridge, MA 02139, USA.
 *
 * Implementation of portable APIs for user-level.
 *
 */

/* Implementations of portable APIs for liblustre */

/*
 * liblustre is single-threaded, so most "synchronization" APIs are trivial.
 */

#ifndef __KERNEL__

#include <sys/mman.h>
#ifndef  __CYGWIN__
#include <stdint.h>
#ifdef HAVE_ASM_PAGE_H
#include <asm/page.h>
#endif
#ifdef HAVE_SYS_USER_H
#include <sys/user.h>
#endif
#else
#include <sys/types.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#if defined(__APPLE__)
#include <mach/vm_param.h>
#else
#include <sys/vfs.h>
#endif

#include <libcfs/libcfs.h>
#include <libcfs/kp30.h>
#include <sys/user.h>

#include "psc_util/cdefs.h"
/*
 * Sleep channel. No-op implementation.
 */

void cfs_waitq_init(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitlink_init(struct cfs_waitlink *link)
{
        LASSERT(link != NULL);
        (void)link;
}

void cfs_waitq_add(struct cfs_waitq *waitq, struct cfs_waitlink *link)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

void cfs_waitq_add_exclusive(struct cfs_waitq *waitq, struct cfs_waitlink *link)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

void cfs_waitq_forward(struct cfs_waitlink *link, struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

void cfs_waitq_del(struct cfs_waitq *waitq, struct cfs_waitlink *link)
{
        LASSERT(waitq != NULL);
        LASSERT(link != NULL);
        (void)waitq;
        (void)link;
}

int cfs_waitq_active(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
        return 0;
}

void cfs_waitq_signal(struct cfs_waitq *waitq)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitq_signal_nr(struct cfs_waitq *waitq, __unusedx int nr)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitq_broadcast(struct cfs_waitq *waitq, __unusedx int state)
{
        LASSERT(waitq != NULL);
        (void)waitq;
}

void cfs_waitq_wait(struct cfs_waitlink *link)
{
        LASSERT(link != NULL);
        (void)link;
}

int64_t cfs_waitq_timedwait(struct cfs_waitlink *link, 
                            __unusedx int state, 
                            __unusedx int64_t timeout)
{
        LASSERT(link != NULL);
        (void)link;
        return 0;
}

/*
 * Allocator
 */

cfs_page_t *cfs_alloc_page(__unusedx unsigned int flags)
{
        cfs_page_t *pg = malloc(sizeof(*pg));

        if (!pg)
                return NULL;
        pg->addr = malloc(PAGE_SIZE);

        if (!pg->addr) {
                free(pg);
                return NULL;
        }
        return pg;
}

void cfs_free_page(cfs_page_t *pg)
{
        free(pg->addr);
        free(pg);
}

void *cfs_page_address(cfs_page_t *pg)
{
        return pg->addr;
}

void *cfs_kmap(cfs_page_t *pg)
{
        return pg->addr;
}

void cfs_kunmap(__unusedx cfs_page_t *pg)
{
}

/*
 * SLAB allocator
 */

cfs_mem_cache_t *
cfs_mem_cache_create(const char *name, size_t objsize, 
                     __unusedx size_t off, __unusedx unsigned long flags)
{
        cfs_mem_cache_t *c;

        c = malloc(sizeof(*c));
        if (!c)
                return NULL;
        c->size = objsize;
        CDEBUG(D_MALLOC, "alloc slab cache %s at %p, objsize %d\n",
               name, c, (int)objsize);
        return c;
}

int cfs_mem_cache_destroy(cfs_mem_cache_t *c)
{
        CDEBUG(D_MALLOC, "destroy slab cache %p, objsize %u\n", c, c->size);
        free(c);
        return 0;
}

void *cfs_mem_cache_alloc(cfs_mem_cache_t *c, int gfp)
{
        return cfs_alloc(c->size, gfp);
}

void cfs_mem_cache_free(__unusedx cfs_mem_cache_t *c, void *addr)
{
        cfs_free(addr);
}

/*
 * This uses user-visible declarations from <linux/kdev_t.h>
 */
#ifdef __LINUX__
#include <linux/kdev_t.h>
#endif

#ifndef MKDEV

#define MAJOR(dev)      ((dev)>>8)
#define MINOR(dev)      ((dev) & 0xff)
#define MKDEV(ma,mi)    ((ma)<<8 | (mi))

#endif

cfs_rdev_t cfs_rdev_build(cfs_major_nr_t major, cfs_minor_nr_t minor)
{
        return MKDEV(major, minor);
}

cfs_major_nr_t cfs_rdev_major(cfs_rdev_t rdev)
{
        return MAJOR(rdev);
}

cfs_minor_nr_t cfs_rdev_minor(cfs_rdev_t rdev)
{
        return MINOR(rdev);
}

void cfs_enter_debugger(void)
{
        /*
         * nothing for now.
         */
}

void cfs_daemonize(__unusedx char *str)
{
        return;
}

cfs_sigset_t cfs_block_allsigs(void)
{
        cfs_sigset_t   all;
        cfs_sigset_t   old;
        int            rc;

        sigfillset(&all);
        rc = sigprocmask(SIG_SETMASK, &all, &old);
        LASSERT(rc == 0);

        return old;
}

cfs_sigset_t cfs_block_sigs(cfs_sigset_t blocks)
{
        cfs_sigset_t   old;
        int   rc;
        
        rc = sigprocmask(SIG_SETMASK, &blocks, &old);
        LASSERT (rc == 0);

        return old;
}

void cfs_restore_sigs(cfs_sigset_t old)
{
        int   rc = sigprocmask(SIG_SETMASK, &old, NULL);

        LASSERT (rc == 0);
}

int cfs_signal_pending(void)
{
        cfs_sigset_t    empty;
        cfs_sigset_t    set;
        int  rc;

        rc = sigpending(&set);
        LASSERT (rc == 0);

        sigemptyset(&empty);

        return !memcmp(&empty, &set, sizeof(set));
}

void cfs_clear_sigpending(void)
{
        return;
}

#ifdef __LINUX__

/*
 * In glibc (NOT in Linux, so check above is not right), implement
 * stack-back-tracing through backtrace() function.
 */
#include <execinfo.h>

void cfs_stack_trace_fill(struct cfs_stack_trace *trace)
{
        backtrace(trace->frame, sizeof_array(trace->frame));
}

void *cfs_stack_trace_frame(struct cfs_stack_trace *trace, int frame_no)
{
        if (0 <= frame_no && frame_no < sizeof_array(trace->frame))
                return trace->frame[frame_no];
        else
                return NULL;
}

#else

void cfs_stack_trace_fill(__unusedx struct cfs_stack_trace *trace)
{}
void *cfs_stack_trace_frame(__unusedx struct cfs_stack_trace *trace, 
                            __unusedx int frame_no)
{
        return NULL;
}

/* __LINUX__ */
#endif

void lbug_with_loc(char *file, const char *func, const int line)
{
        /* No libcfs_catastrophe in userspace! */
        libcfs_debug_msg(NULL, 0, D_EMERG, file, func, line, "LBUG\n");
        abort();
}


/* !__KERNEL__ */
#endif

/*
 * Local variables:
 * c-indentation-style: "K&R"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 80
 * scroll-step: 1
 * End:
 */
