/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2001 Cluster File Systems, Inc. <braam@clusterfs.com>
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Basic library routines.
 *
 */

#ifndef __LIBCFS_LINUX_CFS_MEM_H__
#define __LIBCFS_LINUX_CFS_MEM_H__

#ifndef __LIBCFS_LIBCFS_H__
#error Do not #include this file directly. #include <libcfs/libcfs.h> instead
#endif

#ifdef __KERNEL__
# include <linux/mm.h>
# include <linux/vmalloc.h>
# include <linux/pagemap.h>
# include <linux/slab.h>
# ifdef HAVE_MM_INLINE
#  include <linux/mm_inline.h>
# endif

typedef struct page                     cfs_page_t;
#define CFS_PAGE_SIZE                   PAGE_CACHE_SIZE
#define CFS_PAGE_SHIFT                  PAGE_CACHE_SHIFT
#define CFS_PAGE_MASK                   PAGE_CACHE_MASK

cfs_page_t *cfs_alloc_page(unsigned int flags);
#define cfs_free_page(p)		__free_pages(p, 0)

static inline void *cfs_page_address(cfs_page_t *page)
{
        /*
         * XXX nikita: do NOT call portals_debug_msg() (CDEBUG/ENTRY/EXIT)
         * from here: this will lead to infinite recursion.
         */
        return page_address(page);
}

static inline void *cfs_kmap(cfs_page_t *page)
{
        return kmap(page);
}

static inline void cfs_kunmap(cfs_page_t *page)
{
        kunmap(page);
}

static inline void cfs_get_page(cfs_page_t *page)
{
        get_page(page);
}

static inline int cfs_page_count(cfs_page_t *page)
{
        return page_count(page);
}

#define cfs_page_index(p)       ((p)->index)

/*
 * Memory allocator
 * XXX Liang: move these declare to public file
 */
extern void *cfs_alloc(size_t nr_bytes, u_int32_t flags);
extern void  cfs_free(void *addr);

extern void *cfs_alloc_large(size_t nr_bytes);
extern void  cfs_free_large(void *addr);

/*
 * In Linux there is no way to determine whether current execution context is
 * blockable.
 */
#define CFS_ALLOC_ATOMIC_TRY   CFS_ALLOC_ATOMIC

/*
 * SLAB allocator
 * XXX Liang: move these declare to public file
 */
typedef kmem_cache_t    cfs_mem_cache_t;
extern cfs_mem_cache_t * cfs_mem_cache_create (const char *, size_t, size_t, unsigned long);
extern int cfs_mem_cache_destroy ( cfs_mem_cache_t * );
extern void *cfs_mem_cache_alloc ( cfs_mem_cache_t *, int);
extern void cfs_mem_cache_free ( cfs_mem_cache_t *, void *);

/*
 */
#define CFS_DECL_MMSPACE                mm_segment_t __oldfs
#define CFS_MMSPACE_OPEN                do { __oldfs = get_fs(); set_fs(get_ds());} while(0)
#define CFS_MMSPACE_CLOSE               set_fs(__oldfs)

#else   /* !__KERNEL__ */
#ifdef HAVE_ASM_PAGE_H
#include <asm/page.h>           /* needed for PAGE_SIZE - rread */
#endif

#define PAGE_CACHE_SIZE         PAGE_SIZE
#include <libcfs/user-prim.h>
/* __KERNEL__ */
#endif

#endif /* __LINUX_CFS_MEM_H__ */
