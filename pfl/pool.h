/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

/*
 * Memory pool routines and definitions.
 */

#ifndef _PFL_POOL_H_
#define _PFL_POOL_H_

#include <sys/types.h>

#include <stdarg.h>

#include "pfl/explist.h"
#include "pfl/dynarray.h"
#include "pfl/listcache.h"
#include "pfl/lockedlist.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/memnode.h"
#include "pfl/mlist.h"

struct psc_poolmgr;

/*
 * Poolsets contain a group of poolmgrs which can reap memory from each
 * other.
 *
 * Dynamic arrays are used here so pools can be members of multiple sets
 * without having to worry about allocating/managing multiple
 * psclist_head members.
 */
struct psc_poolset {
	psc_spinlock_t		  pps_lock;
	struct psc_dynarray	  pps_pools;		/* poolmasters in set */
};

/*
 * Pool masters are containers for pool managers.  Pool managers
 * manage buffers local to a memory node.  On non-NUMA architectures,
 * they manage buffers globally.
 */
struct psc_poolmaster {
	psc_spinlock_t		  pms_lock;
	struct psc_dynarray	  pms_poolmgrs;		/* NUMA pools */
	struct psc_dynarray	  pms_sets;		/* poolset memberships */

	/* for initializing memnode poolmgrs */
	char			  pms_name[PEXL_NAME_MAX];
	ptrdiff_t		  pms_offset;		/* item offset to psc_listentry */
	int			  pms_entsize;		/* size of an entry item */
	int			  pms_total;		/* #items to populate */
	int			  pms_min;		/* min bound of #items */
	int			  pms_max;		/* max bound of #items */
	int			  pms_flags;		/* flags */
	void			 *pms_mwcarg;		/* mlist cond arg */
	int			  pms_thres;		/* autoresize threshold */

	int			(*pms_initf)(struct psc_poolmgr *, void *);
	void			(*pms_destroyf)(void *);
	int			(*pms_reclaimcb)(struct psc_poolmgr *);
};

/*
 * Pool managers are like free lists containing items which can be
 * retrieved from the pool and put into circulation across other list
 * caches or in any way seen fit.
 */
struct psc_poolmgr {
	union {
		struct psc_listcache	ppmu_lc;	/* free pool entries */
		struct psc_mlist	ppmu_ml;
		struct psc_explist	ppmu_explist;
	} ppm_u;
	struct psc_poolmaster	 *ppm_master;

	int			  ppm_total;		/* #items in circulation */
	int			  ppm_min;		/* min bound of #items */
	int			  ppm_max;		/* max bound of #items */
	int			  ppm_thres;		/* autoresize threshold */
	int			  ppm_flags;		/* flags */
	int			  ppm_entsize;		/* flags */
	uint64_t		  ppm_ngrow;		/* #allocs */
	uint64_t		  ppm_nshrink;		/* #deallocs */
	atomic_t		  ppm_nwaiters;		/* #thrs waiting for item */
	struct pfl_mutex	  ppm_reclaim_mutex;	/* exclusive reclamation */

	/* routines to initialize, teardown, & reclaim pool entries */
	int			(*ppm_initf)(struct psc_poolmgr *, void *);
	void			(*ppm_destroyf)(void *);
	int			(*ppm_reclaimcb)(struct psc_poolmgr *);
#define ppm_explist	ppm_u.ppmu_explist
#define ppm_lc		ppm_u.ppmu_lc
#define ppm_ml		ppm_u.ppmu_ml

#define ppm_lentry	ppm_explist.pexl_lentry
#define ppm_nfree	ppm_explist.pexl_nitems
#define ppm_name	ppm_explist.pexl_name
#define ppm_nseen	ppm_explist.pexl_nseen
#define ppm_offset	ppm_explist.pexl_offset
#define ppm_pll		ppm_explist.pexl_pll
};

/* Pool manager flags */
#define PPMF_NONE		0		/* no pool manager flag specified */
#define PPMF_AUTO		(1 << 0)	/* pool automatically resizes */
#define PPMF_PIN		(1 << 1)	/* mlock(2) items */
#define PPMF_MLIST		(1 << 2)	/* backend storage is mgt'd via mlist */

#define POOL_LOCK(m)		PLL_LOCK(&(m)->ppm_pll)
#define POOL_TRYLOCK(m)		PLL_TRYLOCK(&(m)->ppm_pll)
#define POOL_ULOCK(m)		PLL_ULOCK(&(m)->ppm_pll)
#define POOL_RLOCK(m)		PLL_RLOCK(&(m)->ppm_pll)
#define POOL_URLOCK(m, lk)	PLL_URLOCK(&(m)->ppm_pll, (lk))

/* Sanity check */
#define POOL_CHECK(m)							\
	do {								\
		psc_assert((m)->ppm_min >= 0);				\
		psc_assert((m)->ppm_max >= 0);				\
		psc_assert((m)->ppm_total >= 0);			\
	} while (0)

#define POOL_IS_MLIST(m)	((m)->ppm_flags & PPMF_MLIST)

#define PSC_POOLSET_INIT	{ SPINLOCK_INIT, DYNARRAY_INIT }

/* default value of pool fill before freeing items directly on pool_return */
#define POOL_AUTOSIZE_THRESH 80

/**
 * psc_poolmaster_init - Initialize a pool resource.
 * @p: pool master.
 * @type: managed structure type.
 * @member: name of psclist_head structure used to interlink managed structs.
 * @total: # of initial entries to allocate.
 * @min: for autosizing pools, smallest number of pool entries to shrink to.
 * @max: for autosizing pools, highest number of pool entries to grow to.
 * @initf: managed structure initialization constructor.
 * @destroyf: managed structure destructor.
 * @reclaimcb: for accessing stale items outside the pool during low memory
 *	conditions.
 * @namefmt: printf(3)-like name of pool for external access.
 */
#define psc_poolmaster_init(p, type, member, flags, total, min,	max,	\
	    initf, destroyf, reclaimcb, namefmt, ...)			\
	_psc_poolmaster_init((p), sizeof(type), offsetof(type, member),	\
	    (flags), (total), (min), (max), (initf), (destroyf),	\
	    (reclaimcb), NULL, (namefmt), ## __VA_ARGS__)

#define psc_poolmaster_initml(p, type, member, flags, total, min, max,	\
	    initf, destroyf, reclaimcb, mlcarg, namefmt, ...)		\
	_psc_poolmaster_init((p), sizeof(type), offsetof(type, member),	\
	    (flags) | PPMF_MLIST, (total), (min), (max), (initf),	\
	    (destroyf),	(reclaimcb), (mlcarg), (namefmt), ## __VA_ARGS__)

#define psc_poolmaster_getmgr(p)	_psc_poolmaster_getmgr((p), psc_memnode_getid())

/**
 * psc_pool_shrink - Decrease #items in a pool.
 * @m: the pool manager.
 * @i: #items to remove from pool.
 */
#define psc_pool_shrink(m, i)		_psc_pool_shrink((m), (i), 0)
#define psc_pool_tryshrink(m, i)	_psc_pool_shrink((m), (i), 1)

#define PPGF_NONBLOCK			(1 << 0)

#if PFL_DEBUG >= 2
# define _PSC_POOL_CHECK_OBJ(m, p)	psc_assert(pfl_memchk((p), 0xa5, (m)->ppm_entsize));
# define _PSC_POOL_CLEAR_OBJ(m, p)	memset((p), 0xa5, (m)->ppm_entsize)
#else
# define _PSC_POOL_CHECK_OBJ(m, p)	do { } while (0)
# define _PSC_POOL_CLEAR_OBJ(m, p)	do { } while (0)
#endif

#define _PSC_POOL_GET(m, fl)						\
	{								\
		void *_ptr;						\
									\
		_ptr = _psc_pool_get((m), (fl));			\
		psclog_info("got item %p from pool %s", _ptr,		\
		    (m)->ppm_name);					\
		_PSC_POOL_CHECK_OBJ((m), _ptr);				\
		_ptr;							\
	}

#define psc_pool_get(m)			(_PSC_POOL_GET((m), 0))
#define psc_pool_tryget(m)		(_PSC_POOL_GET((m), PPGF_NONBLOCK))

#define psc_pool_return(m, p)						\
	do {								\
		_PSC_POOL_CLEAR_OBJ((m), (p));				\
		_psc_pool_return((m), (p));				\
		psclog_info("returned item %p to pool %s", (p),		\
		    (m)->ppm_name);					\
		(p) = NULL;						\
	} while (0)

struct psc_poolmgr *
	_psc_poolmaster_getmgr(struct psc_poolmaster *, int);
void	_psc_poolmaster_init(struct psc_poolmaster *, size_t, ptrdiff_t,
		int, int, int, int, int (*)(struct psc_poolmgr *, void *),
		void (*)(void *), int (*)(struct psc_poolmgr *),
		void *, const char *, ...);
void	_psc_poolmaster_initv(struct psc_poolmaster *, size_t, ptrdiff_t,
		int, int, int, int, int (*)(struct psc_poolmgr *, void *),
		void (*)(void *), int (*)(struct psc_poolmgr *),
		void *, const char *, va_list);

struct psc_poolmgr *
	  psc_pool_lookup(const char *);
void	*_psc_pool_get(struct psc_poolmgr *, int);
int	  psc_pool_gettotal(struct psc_poolmgr *);
int	  psc_pool_grow(struct psc_poolmgr *, int);
int	  psc_pool_nfree(struct psc_poolmgr *);
void	  psc_pool_reapmem(size_t);
void	  psc_pool_resize(struct psc_poolmgr *);
void	 _psc_pool_return(struct psc_poolmgr *, void *);
int	  psc_pool_settotal(struct psc_poolmgr *, int);
void	  psc_pool_share(struct psc_poolmaster *);
int	 _psc_pool_shrink(struct psc_poolmgr *, int, int);
void	  psc_pool_unshare(struct psc_poolmaster *);

void	  psc_poolset_disbar(struct psc_poolset *, struct psc_poolmaster *);
void	  psc_poolset_enlist(struct psc_poolset *, struct psc_poolmaster *);
void	  psc_poolset_init(struct psc_poolset *);

extern struct psc_lockedlist	psc_pools;

#endif /* _PFL_POOL_H_ */