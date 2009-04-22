/* $Id$ */

/*
 * Memory pool routines and definitions.
 */

#ifndef _PFL_POOL_H_
#define _PFL_POOL_H_

#include <sys/types.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/listcache.h"
#include "psc_ds/listguts.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/assert.h"
#include "psc_util/lock.h"
#include "psc_util/memnode.h"
#include "psc_util/mlist.h"

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
	struct dynarray		  pps_pools;		/* poolmasters in set */
};

/*
 * Pool masters are containers for pool managers.  Pool managers
 * manage buffers local to a memory node.  On non-NUMA architectures,
 * they manage buffers globally.
 */
struct psc_poolmaster {
	psc_spinlock_t		  pms_lock;
	struct dynarray		  pms_poolmgrs;		/* NUMA pools */
	struct dynarray		  pms_sets;		/* poolset memberships */

	/* for initializing memnode poolmgrs */
	char			  pms_name[PLG_NAME_MAX];
	ptrdiff_t		  pms_offset;		/* entry offset to listhead */
	int			  pms_entsize;		/* size of entry in pool */
	int			  pms_total;		/* #items to populate */
	int			  pms_min;		/* min bound of #items */
	int			  pms_max;		/* max bound of #items */
	int			  pms_flags;		/* flags */
	void			 *pms_mlcarg;		/* mlist cond arg */
	int			  pms_thres;		/* autoresize threshold */

	int			(*pms_initf)(struct psc_poolmgr *, void *);
	void			(*pms_destroyf)(void *);
	int			(*pms_reclaimcb)(struct psc_poolmgr *);
};

/*
 * Pools managers are like free lists containing items which can be
 * retrieved from the pool and put into circulation across other list
 * caches or in any way seen fit.
 */
struct psc_poolmgr {
	union {
		struct psc_listcache	ppmu_lc;	/* free pool entries */
		struct psc_mlist	ppmu_ml;
		struct psc_listguts	ppmu_lg;
	} ppm_u;
	struct psclist_head	  ppm_all_lentry;
	struct psc_poolmaster	 *ppm_master;

	int			  ppm_total;		/* #items in circulation */
	int			  ppm_min;		/* min bound of #items */
	int			  ppm_max;		/* max bound of #items */
	int			  ppm_thres;		/* autoresize threshold */
	int			  ppm_flags;		/* flags */
	uint64_t		  ppm_ngrow;		/* #allocs */
	uint64_t		  ppm_nshrink;		/* #deallocs */
	atomic_t		  ppm_nwaiters;		/* #thrs waiting for item */
	pthread_mutex_t		  ppm_reclaim_mutex;	/* exclusive reclamation */

	/* routines to initialize, teardown, & reclaim pool entries */
	int			(*ppm_initf)(struct psc_poolmgr *, void *);
	void			(*ppm_destroyf)(void *);
	int			(*ppm_reclaimcb)(struct psc_poolmgr *);
#define ppm_lc ppm_u.ppmu_lc
#define ppm_ml ppm_u.ppmu_ml
#define ppm_lg ppm_u.ppmu_lg
};

/* Pool manager flags. */
#define PPMF_NONE	0		/* no pool manager flag specified */
#define PPMF_AUTO	(1 << 0)	/* pool automatically resizes */
#define PPMF_NOLOCK	(1 << 1)	/* pool ents shouldn't be mlock'd */
#define PPMF_MLIST	(1 << 2)	/* backend storage is mgt'd via mlist */

#define POOL_LOCK(m)		spinlock(&(m)->ppm_lg.plg_lock)
#define POOL_TRYLOCK(m)		trylock(&(m)->ppm_lg.plg_lock)
#define POOL_UNLOCK(m)		freelock(&(m)->ppm_lg.plg_lock)
#define POOL_RLOCK(m)		reqlock(&(m)->ppm_lg.plg_lock)
#define POOL_URLOCK(m, l)	ureqlock(&(m)->ppm_lg.plg_lock, (l))

/* Sanity check */
#define POOL_CHECK(m)							\
	do {								\
		psc_assert((m)->ppm_min >= 0);				\
		psc_assert((m)->ppm_max >= 0);				\
		psc_assert((m)->ppm_total >= 0);			\
	} while (0)

#define POOL_IS_MLIST(m)	((m)->ppm_flags & PPMF_MLIST)

#define PSC_POOLSET_INIT	{ LOCK_INITIALIZER, DYNARRAY_INIT }

/* default value of pool fill before freeing items directly on pool_return */
#define POOL_AUTOSIZE_THRESH 80

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

#define psc_pool_tryget(p)		lc_getnb(&(p)->ppm_lc)

#define psc_pool_shrink(m, i)		_psc_pool_shrink((m), (i), 0)
#define psc_pool_tryshrink(m, i)	_psc_pool_shrink((m), (i), 1)

struct psc_poolmgr *
	_psc_poolmaster_getmgr(struct psc_poolmaster *, int);
void	_psc_poolmaster_init(struct psc_poolmaster *, size_t, ptrdiff_t,
		int, int, int, int, int (*)(struct psc_poolmgr *, void *),
		void (*)(void *), int (*)(struct psc_poolmgr *),
		void *, const char *, ...);

int	 psc_pool_grow(struct psc_poolmgr *, int);
int	_psc_pool_shrink(struct psc_poolmgr *, int, int);
int	 psc_pool_settotal(struct psc_poolmgr *, int);
void	 psc_pool_resize(struct psc_poolmgr *);
void	 psc_pool_reapmem(size_t);
void	*psc_pool_get(struct psc_poolmgr *);
void	 psc_pool_return(struct psc_poolmgr *, void *);
struct psc_poolmgr *
	 psc_pool_lookup(const char *);
void	 psc_pool_share(struct psc_poolmaster *);
void	 psc_pool_unshare(struct psc_poolmaster *);

void	 psc_poolset_init(struct psc_poolset *);
void	 psc_poolset_enlist(struct psc_poolset *, struct psc_poolmaster *);
void	 psc_poolset_disbar(struct psc_poolset *, struct psc_poolmaster *);

extern struct psc_lockedlist	psc_pools;

#endif /* _PFL_POOL_H_ */
