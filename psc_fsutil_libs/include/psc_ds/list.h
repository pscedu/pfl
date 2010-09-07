/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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
 * Simple doubly linked list implementation loosely based off <linux/list.h>.
 */

#ifndef _PFL_LIST_H_
#define _PFL_LIST_H_

#include <stdint.h>
#include <stdio.h>

#include "psc_util/log.h"

/* note: this structure is used for both a head and an entry */
struct psclist_head {
	struct psclist_head		*plh_next;
	struct psclist_head		*plh_prev;
#ifdef DEBUG
	uint64_t			 plh_magic;
	struct psclist_head		*plh_owner;
#endif
};

#define psc_listentry psclist_head

#define PLENT_MAGIC			UINT64_C(0x1234123412341234)

#ifdef DEBUG
# define PSCLIST_HEAD_INIT(name)	{ &(name), &(name), PLENT_MAGIC, &(name) }
# define PSC_LISTENTRY_INIT		{ NULL, NULL, PLENT_MAGIC, NULL }
#else
# define PSCLIST_HEAD_INIT(name)	{ &(name), &(name) }
# define PSC_LISTENTRY_INIT		{ NULL, NULL }
#endif

#define PSCLIST_HEAD(name)							\
	struct psclist_head name = PSCLIST_HEAD_INIT(name)

#define psc_listhd_first(hd)			(hd)->plh_next
#define psc_listhd_last(hd)			(hd)->plh_prev

/**
 * psc_lentry_next - Access the entry following the specified entry.
 * @e: entry
 */
#define psc_lentry_next(e)			(e)->plh_next

/**
 * psc_lentry_prev - Access the entry before the specified entry.
 * @e: entry
 */
#define psc_lentry_prev(e)			(e)->plh_prev

#ifdef DEBUG
#define INIT_PSCLIST_HEAD(hd)							\
	do {									\
		psc_listhd_first(hd) = (hd);					\
		psc_listhd_last(hd) = (hd);					\
		(hd)->plh_owner = (hd);						\
		(hd)->plh_magic = PLENT_MAGIC;					\
	} while (0)

#define INIT_PSC_LISTENTRY(e)							\
	do {									\
		psc_lentry_prev(e) = NULL;					\
		psc_lentry_next(e) = NULL;					\
		(e)->plh_owner = NULL;						\
		(e)->plh_magic = PLENT_MAGIC;					\
	} while (0)
#else
#define INIT_PSCLIST_HEAD(hd)							\
	do {									\
		psc_listhd_first(hd) = (hd);					\
		psc_listhd_last(hd) = (hd);					\
	} while (0)

#define INIT_PSC_LISTENTRY(e)							\
	do {									\
		psc_lentry_prev(e) = NULL;					\
		psc_lentry_next(e) = NULL;					\
	} while (0)
#endif

static __inline void
_psclist_add(struct psc_listentry *e, struct psclist_entry *prev,
    struct psc_listentry *next)
{
#ifdef DEBUG
	psc_assert(e->plh_owner == NULL);
	psc_assert(prev->plh_owner && next->plh_owner);
	psc_assert(prev->plh_owner == next->plh_owner);

	psc_assert(e->plh_magic == PLENT_MAGIC);
	psc_assert(prev->plh_magic == PLENT_MAGIC);
	psc_assert(next->plh_magic == PLENT_MAGIC);

	psc_assert(psc_lentry_prev(e) == NULL && psc_lentry_next(e) == NULL);
	psc_assert(psc_lentry_prev(prev) && psc_lentry_next(prev));
	psc_assert(psc_lentry_prev(next) && psc_lentry_next(next));

	e->plh_owner = prev->plh_owner;
#endif

	psc_lentry_prev(next) = e;
	psc_lentry_next(e) = next;
	psc_lentry_prev(e) = prev;
	psc_lentry_next(prev) = e;
}

#define psclist_add_head(e, hd)		_psclist_add((e), (hd), psc_listhd_first(hd))
#define psclist_add_tail(e, hd)		_psclist_add((e), psc_listhd_last(hd), (hd))
#define psclist_add_after(e, before)	_psclist_add((e), (before), psc_lentry_next(before))
#define psclist_add_before(e, after)	_psclist_add((e), psc_lentry_prev(after), (after))

#define psclist_add(e, hd)		psclist_add_tail((e), (hd))

/**
 * psclist_del - Delete an entry from the list it is contained within.
 * @e: the entry to remove from a list.
 * @hd: containing list head.
 */
static __inline void
psclist_del(struct psclist_head *entry, __unusedx const void *hd)
{
	struct psc_listentry *prev, *next;

#ifdef DEBUG
	psc_assert(e->plh_owner == hd);
	psc_assert(e->plh_magic == PLENT_MAGIC);
	psc_assert(psc_lentry_prev(e) && psc_lentry_next(e));
	if (psc_lentry_prev(e) == psc_lentry_next(e))
		psc_assert(psc_lentry_prev(e) == hd);
#else
	(void)hd;
#endif

	prev = psc_lentry_prev(e);
	next = psc_lentry_next(e);

	psc_lentry_next(prev) = next;
	psc_lentry_prev(next) = prev;

#ifdef DEBUG
	e->plh_owner = NULL;
	psc_lentry_next(e) = psc_lentry_prev(e) = NULL;
#endif
}

/**
 * psc_listhd_empty - Tests whether a list has no items.
 * @hd: the list head to test.
 */
static __inline int
psc_listhd_empty(const struct psclist_head *hd)
{
#ifdef DEBUG
	psc_assert(hd->plh_magic == PLENT_MAGIC);
	psc_assert(hd->plh_owner == hd);
	psc_assert(psc_lentry_prev(hd) && psc_lentry_next(hd));
	if (psc_lentry_prev(hd) == hd)
		psc_assert(psc_lentry_next(hd) == hd);
	if (psc_lentry_next(hd) == hd)
		psc_assert(psc_lentry_prev(hd) == hd);
#endif
	return (psc_listhd_first(hd) == hd);
}

/**
 * psclist_disjoint - Test whether a psc_listentry is not on a list.
 * @entry: the psc_listentry to test.
 */
#define psclist_disjoint(e)	(psc_lentry_prev(e) == NULL && psc_lentry_next(e) == NULL)

/**
 * psclist_conjoint - Test whether a psc_listentry is on a list.
 * @entry: the psc_listentry to test.
 */
static __inline int
psclist_conjoint(const struct psc_listentry *e, const struct psclist_head *hd)
{
#ifdef DEBUG
	psc_assert(e->plh_magic == PLENT_MAGIC);
	if (hd == NULL) {
		psc_warnx("conjoint passed NULL");
		hd = e->plh_owner;
	}
	psc_assert(psc_lentry_prev(e) && psc_lentry_next(e));
	if (psc_lentry_prev(e) == psc_lentry_next(e))
		psc_assert(psc_lentry_prev(e) == (hd));
#else
	(void)hd;
#endif
	return (psc_lentry_prev(e) && psc_lentry_next(e));
}

/**
 * psc_lentry_obj2 - Get the struct for a list entry given offset.
 * @e: the psc_listentry.
 * @type: the type of the struct this entry is embedded in.
 * @memb: the structure member name of the psc_listentry.
 */
#define psc_lentry_obj2(e, type, offset)					\
	((type *)((char *)(e) - (offset)))

/**
 * psc_lentry_obj - Get the struct for a list entry.
 * @e: the psc_listentry.
 * @type: the type of the struct this entry is embedded in.
 * @memb: the structure member name of the psc_listentry.
 */
#define psc_lentry_obj(e, type, memb)						\
	psc_lentry_obj2((e), type, offsetof(type, memb))

#ifdef DEBUG
#define psc_lentry_hd(e)		(e)->plh_owner
#else
#define psc_lentry_hd(e)		NULL
#endif

/**
 * psc_listhd_first_obj - Grab first item from a list head or NULL if empty.
 * @hd: list head.
 * @type: structure type containing a psc_listentry.
 * @memb: structure member name of psc_listentry.
 */
#define psc_listhd_first_obj(hd, type, memb)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj(psc_listhd_first(hd), type, memb))

#define psc_listhd_first_obj2(hd, type, offset)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj2(psc_listhd_first(hd), type, offset))

/**
 * psc_listhd_last_obj - Grab last item from a list head or NULL if empty.
 * @hd: list head.
 * @type: structure type containing a psc_listentry.
 * @memb: structure member name of psc_listentry.
 */
#define psc_listhd_last_obj(hd, type, memb)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj(psc_listhd_last(hd), type, memb))

#define psc_listhd_last_obj2(hd, type, offset)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj2(psc_listhd_last(hd), type, offset))

static __inline void *
_psclist_next_obj(struct psclist_head *hd, void *p,
    unsigned long offset, int wantprev)
{
	struct psc_listentry *e, *n;

	psc_assert(p);
	e = (void *)((char *)p + offset);

	/*
	 * Ensure integrity of entry: must be contained in
	 * a list and must not inadvertenly be a head!
	 */
	psc_assert(psc_lentry_next(e) && psc_lentry_prev(e));
	n = wantprev ? psc_lentry_prev(e) : psc_lentry_next(e);
	psc_assert(n != e);
	if (n == hd)
		return (NULL);
	return ((char *)n - offset);
}

/**
 * psclist_prev_obj - Grab the item before the specified item on a list.
 * @hd: head of the list.
 * @p: item on list.
 * @memb: psc_listentry member name in structure.
 */
#define psclist_prev_obj(hd, p, memb)						\
	_psclist_next_obj((hd), (p), offsetof(typeof(*(p)), memb), 1)

#define psclist_prev_obj2(hd, p, offset)					\
	_psclist_next_obj((hd), (p), (offset), 1)

/**
 * psclist_next_obj - Grab the item following the specified item on a list.
 * @hd: head of the list.
 * @p: item on list.
 * @memb: psc_listentry member name in structure.
 */
#define psclist_next_obj(hd, p, memb)						\
	_psclist_next_obj((hd), (p), offsetof(typeof(*(p)), memb), 0)

#define psclist_next_obj2(hd, p, offset)					\
	_psclist_next_obj((hd), (p), (offset), 0)

/**
 * psclist_for_each - Iterate over a psclist.
 * @e: the &struct psclist_head to use as a loop counter.
 * @head: the head for your psclist.
 */
#define psclist_for_each(e, hd)							\
	for ((e) = psc_listhd_first(hd);					\
	     (e) != (hd) || ((e) = NULL);					\
	     (e) = psc_lentry_next(e))

/**
 * psclist_for_each_safe - Iterate over a list safe against removal
 *	of the iterating entry.
 * @e: the entry to use as a loop counter.
 * @n: another entry to use as temporary storage.
 * @hd: the head of the list.
 */
#define psclist_for_each_safe(e, n, hd)						\
	for ((e) = psc_listhd_first(hd), (n) = psc_lentry_next(e);		\
	    (e) != (hd) || ((e) = (n) = NULL);					\
	    (e) = (n), (n) = psc_lentry_next(e))

/**
 * psclist_for_each_entry_safe - Iterate over list of given type safe
 *	against removal of list entry
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry_safe(p, n, hd, memb)				\
	for ((p) = psc_listhd_first_obj((hd), typeof(*(p)), memb),		\
	     (n) = psclist_next_obj((hd), (p), memb);				\
	     (p) || ((n) = NULL);						\
	     (p) = (n), (n) = psclist_next_obj((hd), (n), memb))

/**
 * psclist_for_each_entry_safe_backwards - Iterate backwards over a list safe
 *	against removal of entries.
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry_safe_backwards(p, n, hd, memb)			\
	for ((p) = psc_listhd_first_obj((hd), typeof(*(p)), memb),		\
	     (n) = psclist_prev_obj((hd), (p), memb);				\
	     (p) || ((n) = NULL);						\
	     (p) = (n), (n) = psclist_prev_obj((hd), (n), memb))

/**
 * psclist_for_each_entry - Iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry(p, hd, memb)					\
	for ((p) = psc_listhd_first_obj((hd), typeof(*(p)), memb);		\
	     (p); (p) = psclist_next_obj((hd), (p), memb))

/**
 * psclist_for_each_entry_backwards - Iterate backwards over a list.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry_backwards(p, hd, memb)				\
	for ((p) = psc_listhd_last_obj((hd), typeof(*(p)), memb);		\
	     (p); (p) = psclist_prev_obj((hd), (p), memb))

/**
 * psclist_for_each_entry2 - Iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2(p, hd, offset)					\
	for ((p) = psc_listhd_first_obj2((hd), typeof(*(p)), (offset));		\
	     (p); (p) = psclist_next_obj2((hd), (p), (offset)))

/**
 * psclist_for_each_entry2_backwards - iterate backwards over a list.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_backwards(p, hd, offset)			\
	for ((p) = psc_listhd_last_obj2((hd), typeof(*(p)), (offset));		\
	     (p); (p) = psclist_prev_obj2((hd), (p), (offset)))

/**
 * psclist_for_each_entry2_safe - iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_safe(p, n, hd, offset)				\
	for ((p) = psc_listhd_first_obj2((hd), typeof(*(p)), (offset)),		\
	     (n) = psclist_next_obj2((hd), (p), (offset));			\
	     (p) || ((n) = NULL);						\
	     (p) = (n), (n) = psclist_next_obj2((hd), (n), (offset)))

void psclist_sort(void **, struct psclist_head *, int, ptrdiff_t,
	void (*)(void *, size_t, size_t, int (*)(const void *, const void *)),
	int (*)(const void *, const void *));

static __inline void
psclist_add_sorted(struct psclist_head *hd, struct psc_listentry *e,
    int (*cmpf)(const void *, const void *), ptrdiff_t offset)
{
	struct psclist_head *t;

	psc_assert(e);
	psclist_for_each(t, hd)
		if (cmpf((char *)e - offset, (char *)t - offset) > 0) {
			psclist_add_after(e, t);
			return;
		}
	psclist_add(e, hd);
}

#endif /* _PFL_LIST_H_ */
