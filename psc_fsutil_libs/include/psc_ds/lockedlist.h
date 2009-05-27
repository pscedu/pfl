/* $Id$ */

/*
 * Locked lists are thread-safe linked lists.
 */

#ifndef _PFL_LOCKEDLIST_H_
#define _PFL_LOCKEDLIST_H_

#include <sys/types.h>

#include <stddef.h>
#include <string.h>

#include "psc_ds/list.h"
#include "psc_util/assert.h"
#include "psc_util/lock.h"

struct psc_lockedlist {
	struct psclist_head	 pll_listhd;
	int			 pll_nitems;
	ptrdiff_t		 pll_offset;	/* offset of the linkage sub-structure */
	int			 pll_flags;
	union {
		psc_spinlock_t	 pllu_lock;
		psc_spinlock_t	*pllu_lockp;
	} pll_u;
#define pll_lockp	pll_u.pllu_lockp
#define pll_lock	pll_u.pllu_lock
};

#define PLLF_EXTLOCK	(1 << 0)	/* lock is external */

#define PLL_GETLOCK(pll) ((pll)->pll_flags & PLLF_EXTLOCK ?	\
	(pll)->pll_lockp : &(pll)->pll_lock)

#define PLL_LOCK(pll)		spinlock(PLL_GETLOCK(pll))
#define PLL_RLOCK(pll)		reqlock(PLL_GETLOCK(pll))
#define PLL_ULOCK(pll)		freelock(PLL_GETLOCK(pll))
#define PLL_URLOCK(pll, l)	ureqlock(PLL_GETLOCK(pll), (l))

#define PLL_FOREACH(p, pll)					\
	psclist_for_each_entry2((p), &(pll)->pll_listhd,	\
	    (pll)->pll_offset)

#define PLL_FOREACH_SAFE(p, t, pll)				\
	psclist_for_each_entry2_safe((p), (t),			\
	    &(pll)->pll_listhd, (pll)->pll_offset)

#define PLL_INITIALIZER(pll, type, member)			\
	{ PSCLIST_HEAD_INIT((pll)->pll_listhd), 0, 		\
	  offsetof(type, member), 0, { LOCK_INITIALIZER } }

#define pll_init(pll, type, member, lock)			\
	_pll_init((pll), offsetof(type, member), (lock))

static __inline void
_pll_init(struct psc_lockedlist *pll, int offset, psc_spinlock_t *lkp)
{
	memset(pll, 0, sizeof(*pll));
	INIT_PSCLIST_HEAD(&pll->pll_listhd);
	if (lkp) {
		pll->pll_flags |= PLLF_EXTLOCK;
		pll->pll_lockp = lkp;
	} else
		LOCK_INIT(&pll->pll_lock);
	pll->pll_offset = offset;
}

static __inline int
pll_nitems(struct psc_lockedlist *pll)
{
	int n, locked;

	locked = PLL_RLOCK(pll);
	n = pll->pll_nitems;
	PLL_URLOCK(pll, locked);
	return (n);
}

#define pll_add(pll, p)		_pll_add((pll), (p), 0)
#define pll_addstack(pll, p)	_pll_add((pll), (p), 0)
#define pll_addqueue(pll, p)	_pll_add((pll), (p), 1)
#define pll_addhead(pll, p)	_pll_add((pll), (p), 0)
#define pll_addtail(pll, p)	_pll_add((pll), (p), 1)

static __inline void
_pll_add(struct psc_lockedlist *pll, void *p, int tail)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + pll->pll_offset;
	locked = PLL_RLOCK(pll);
	if (tail)
		psclist_xadd_tail(e, &pll->pll_listhd);
	else
		psclist_xadd_head(e, &pll->pll_listhd);
	pll->pll_nitems++;
	PLL_URLOCK(pll, locked);
}

#define PLLF_HEAD	(1 << 0)
#define PLLF_STACK	(1 << 0)
#define PLLF_TAIL	(1 << 1)
#define PLLF_QUEUE	(1 << 1)
#define PLLF_PEEK	(1 << 2)

static __inline void *
_pll_get(struct psc_lockedlist *pll, int flags)
{
	struct psclist_head *e;
	int locked;

	psc_assert((flags & PLLF_HEAD) ^ (flags & PLLF_TAIL));

	locked = PLL_RLOCK(pll);
	if (psclist_empty(&pll->pll_listhd)) {
		PLL_URLOCK(pll, locked);
		return (NULL);
	}
	if (flags & PLLF_TAIL)
		e = psclist_last(&pll->pll_listhd);
	else
		e = psclist_first(&pll->pll_listhd);
	if ((flags & PLLF_PEEK) == 0) {
		psclist_del(e);
		pll->pll_nitems--;
	}
	PLL_URLOCK(pll, locked);
	return ((char *)e - pll->pll_offset);
}

#define pll_get(pll)		_pll_get((pll), PLLF_HEAD)
#define pll_gethd(pll)		_pll_get((pll), PLLF_HEAD)
#define pll_gettail(pll)	_pll_get((pll), PLLF_TAIL)
#define pll_gethdpeek(pll)	_pll_get((pll), PLLF_HEAD | PLLF_PEEK)

static __inline void
pll_remove(struct psc_lockedlist *pll, void *p)
{
	int locked;
	void *e;

	psc_assert(p);
	e = (char *)p + pll->pll_offset;
	locked = PLL_RLOCK(pll);
	psclist_del(e);
	pll->pll_nitems--;
	PLL_URLOCK(pll, locked);
}

static __inline int
pll_empty(struct psc_lockedlist *pll)
{
	int locked, empty;

	locked = PLL_RLOCK(pll);
	empty = psclist_empty(&pll->pll_listhd);
	PLL_URLOCK(pll, locked);
	return (empty);
}

#endif /* _PFL_LOCKEDLIST_H_ */
