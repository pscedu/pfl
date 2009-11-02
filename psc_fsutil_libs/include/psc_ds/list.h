/* $Id$ */

/*
 * Simple doubly linked list implementation loosely based off <linux/list.h>.
 */

#ifndef _PFL_LIST_H_
#define _PFL_LIST_H_

#include <stdio.h>

#include "psc_util/log.h"

struct psclist_head {
	struct psclist_head *znext;
	struct psclist_head *zprev;
};

#define PSCLIST_HEAD_INIT(name)		{ &(name), &(name) }
#define PSCLIST_ENTRY_INIT		{ NULL, NULL }

#define PSCLIST_HEAD(name) \
	struct psclist_head name = PSCLIST_HEAD_INIT(name)

#define INIT_PSCLIST_HEAD(ptr)		\
	do {				\
		(ptr)->znext = (ptr);	\
		(ptr)->zprev = (ptr);	\
	} while (0)

#define INIT_PSCLIST_ENTRY(ptr)		\
	do {				\
		(ptr)->znext = NULL;	\
		(ptr)->zprev = NULL;	\
	} while (0)

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal psclist manipulation where we know
 * the prev/next entries already!
 */
static __inline void
_psclist_add(struct psclist_head *new, struct psclist_head *prev,
	struct psclist_head *next)
{
	next->zprev = new;
	new->znext = next;
	new->zprev = prev;
	prev->znext = new;
}

/**
 * psclist_xadd - add an element to a list and check for exclusive membership.
 * @new: entry to be added
 * @head: psclist_head to add it after
 *
 * Insert a new entry after the specified head.
 * Ensure entry doesn't already belong to another list.
 * This is good for implementing stacks.
 */
static __inline void
psclist_xadd(struct psclist_head *new, struct psclist_head *head)
{
	/*
	 * _psclist_add() does not really rely on the back/forward
	 * points of new, so we might want to remove this assert.
	 * If so, there will be no need to add INIT_PSCLIST_ENTRY()
	 * before calling us. It was added to catch adding the same
	 * entry twice historically.  - Zhihui 02/27/2009
	 */
	psc_assert(new->zprev == NULL && new->znext == NULL);
	_psclist_add(new, head, head->znext);
}

#define psclist_xadd_head(e, hd)	psclist_xadd((e), (hd))
#define psclist_xadd_after(e, t)	psclist_xadd((e), (t))
#define psclist_xadd_before(e, t)	psclist_xadd_tail((e), (t))

/**
 * psclist_xadd_tail - add a new entry and check for exclusive membership.
 * @new: new entry to be added
 * @head: psclist head to add it before
 *
 * Insert a new entry before the specified head.
 * Ensure entry doesn't already belong to another list.
 * This is useful for implementing queues.
 */
static __inline void
psclist_xadd_tail(struct psclist_head *new, struct psclist_head *head)
{
	psc_assert(new->zprev == NULL && new->znext == NULL);
	_psclist_add(new, head->zprev, head);
}

/*
 * Delete a psclist entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal psclist manipulation where we know
 * the prev/next entries already!
 */
static __inline void
_psclist_del(struct psclist_head *prev, struct psclist_head *next)
{
	next->zprev = prev;
	prev->znext = next;
}

/**
 * psclist_del - deletes entry from psclist.
 * @entry: the element to delete from the psclist.
 * Note: psclist_empty on entry does not return true after this,
 *	the entry is in an undefined state.
 */
static __inline void
psclist_del(struct psclist_head *entry)
{
	_psclist_del(entry->zprev, entry->znext);
	entry->znext = entry->zprev = NULL;
}

/**
 * psclist_empty - tests whether a psclist is empty.
 * @head: the psclist head to test.
 */
static __inline int
psclist_empty(const struct psclist_head *hd)
{
	psc_assert(hd->znext && hd->zprev);
#if 0
	if (hd->znext == head) {
		if (hd->zprev != hd)
			abort();
		return (1);
	} else
		return (0);
#endif
	return (hd->znext == hd);
}

/**
 * psclist_disjoint - tests whether a psclist entry is not a member of a list.
 * @entry: the psclist entry to test.
 */
#define psclist_disjoint(ent)	((ent)->znext == NULL && (ent)->zprev == NULL)

/**
 * psclist_conjoint - tests whether a psclist entry is a member of a list.
 * @entry: the psclist entry to test.
 */
#define psclist_conjoint(ent)	((ent)->znext != NULL && (ent)->zprev != NULL)

/**
 * psclist_splice - join two psclists
 * @psclist: the new psclist to add.
 * @head: the place to add it in the first psclist.
 */
static __inline void
psclist_splice(struct psclist_head *list, struct psclist_head *head)
{
	struct psclist_head *first = list->znext;

	if (first != list) {
		struct psclist_head *last = list->zprev;
		struct psclist_head *at = head->znext;

		first->zprev = head;
		head->znext = first;

		last->znext = at;
		at->zprev = last;
	}
}

/**
 * psclist_entry - get the struct for this entry
 * @ptr: the &struct psclist_head pointer.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the struct psclist_head within the struct.
 * XXX rewrite using offsetof().
 */
#define psclist_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * psclist_for_each - iterate over a psclist
 * @pos: the &struct psclist_head to use as a loop counter.
 * @head: the head for your psclist.
 */
#define psclist_for_each(pos, head)				\
	for ((pos) = (head)->znext;				\
	    ((pos) != (head)) || ((pos) = NULL);		\
		(pos) = (pos)->znext)

/**
 * psclist_for_each_safe - iterate over a psclist safe against removal of psclist entry
 * @pos: the &struct psclist_head to use as a loop counter.
 * @n: another &struct psclist_head to use as temporary storage
 * @head: the head for your psclist.
 */
#define psclist_for_each_safe(pos, n, head)			\
	for ((pos) = (head)->znext, (n) = (pos)->znext;		\
	    ((pos) != (head)) || ((pos) = (n) = NULL);		\
	    (pos) = (n), (n) = (pos)->znext)

/**
 * psclist_first - grab first entry from a psclist
 * @head: the head for your psclist.
 */
#define psclist_first(head) (head)->znext

/**
 * psclist_first_entry - grab first item from a psclist
 * @hd: list head.
 * @type: entry type.
 * @memb: list_head member name in entry structure.
 */
#define psclist_first_entry(hd, type, memb)			\
	(psclist_empty(hd) ? NULL :				\
	 psclist_entry((hd)->znext, type, memb))

/**
 * psclist_last - grab last list entry.
 * @hd: list head.
 */
#define psclist_last(hd) (hd)->zprev

/**
 * psclist_last_entry - grab last list item.
 * @hd: list head.
 * @type: entry type.
 * @memb: list_head member name in entry structure.
 */
#define psclist_last_entry(hd, type, memb)			\
	(psclist_empty(hd) ? NULL :				\
	 psclist_entry((hd)->zprev, type, memb))

/**
 * psclist_next - grab the entry following the specified entry.
 * @e: entry
 */
#define psclist_next(e) (e)->znext

/**
 * psclist_next_entry - grab the item following the specified entry.
 * @hd: list head
 * @p: item on list.
 * @type: entry type.
 * @memb: list_head member name in entry structure.
 */
#define psclist_next_entry(hd, p, memb)				\
	_psclist_next_entry((hd), (p), offsetof(typeof(*(p)), memb), 0)

static __inline void *
_psclist_next_entry(struct psclist_head *hd, void *p,
    unsigned long offset, int wantprev)
{
	struct psclist_head *e, *n;

	psc_assert(p);
	e = (void *)((char *)p + offset);

	/*
	 * Ensure integrity of entry: must be contained in
	 * a list and must not inadvertenly be a head!
	 */
	psc_assert(e->znext && e->zprev);
	n = wantprev ? e->zprev : e->znext;
	psc_assert(n != e);
	if (n == hd)
		return (NULL);
	return ((char *)n - offset);
}

/**
 * psclist_prev - grab the entry before the specified entry.
 * @e: entry
 */
#define psclist_prev(e) (e)->zprev

/**
 * psclist_prev_entry - grab the item before the specified entry.
 * @hd: list head
 * @e: entry
 * @type: entry type.
 * @memb: list_head member name in entry structure.
 */
#define psclist_prev_entry(hd, p, memb)				\
	_psclist_next_entry((hd), (p), offsetof(typeof(*(p)), memb), 1)

/**
 * psclist_for_each_entry_safe - iterate over list of given type safe
 *	against removal of list entry
 * @pos: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage.
 * @hd: the head for your list.
 * @member: list entry member of structure.
 */
#define psclist_for_each_entry_safe(pos, n, hd, member)				\
	for ((pos) = psclist_entry((hd)->znext, typeof(*(pos)), member),	\
	    (n) = psclist_entry((pos)->member.znext, typeof(*(n)), member);	\
	    (&(pos)->member != (hd)) || ((pos) = (n) = NULL);			\
	    (pos) = (n), (n) = psclist_entry((n)->member.znext, typeof(*(n)), member))

/**
 * psclist_for_each_entry_safe_backwards - iterate backwards over a list safe
 *	against removal of entries.
 * @pos: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage.
 * @hd: the head for your list.
 * @member: list entry member of structure.
 */
#define psclist_for_each_entry_safe_backwards(pos, n, hd, member)		\
	for ((pos) = psclist_entry((hd)->zprev, typeof(*(pos)), member),	\
	    (n) = psclist_entry((pos)->member.zprev, typeof(*(n)), member);	\
	    (&(pos)->member != (hd)) || ((pos) = (n) = NULL);			\
	    (pos) = (n), (n) = psclist_entry((pos)->member.zprev, typeof(*(n)), member))

/**
 * psclist_for_each_entry - iterate over list of given type.
 * @pos: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @member: list entry member of structure.
 */
#define psclist_for_each_entry(pos, hd, member)					\
	for ((pos) = psclist_entry((hd)->znext, typeof(*(pos)), member);	\
	    (&(pos)->member != (hd)) || ((pos) = NULL);				\
	    (pos) = psclist_entry((pos)->member.znext, typeof(*(pos)), member))

/**
 * psclist_for_each_entry_backwards - iterate backwards over a list.
 * @pos: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @member: list entry member of structure.
 */
#define psclist_for_each_entry_backwards(pos, hd, member)			\
	for ((pos) = psclist_entry((hd)->zprev, typeof(*(pos)), member);	\
	    (&(pos)->member != (hd)) || ((pos) = NULL);				\
	    (pos) = psclist_entry((pos)->member.zprev, typeof(*(pos)), member))

/**
 * psclist_for_each_entry2 - iterate over list of given type.
 * @pos: the type * to use as a loop counter.
 * @head: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2(pos, head, offset)				\
	for ((pos) = (void *)(((char *)(head)->znext) - (offset));		\
	    (((char *)pos) + (offset) != (void *)(head)) || ((pos) = NULL);	\
	    (pos) = (void *)(((char *)(((struct psclist_head *)(((char *)pos) +	\
	      (offset)))->znext)) - (offset)))

/**
 * psclist_for_each_entry2_backwards - iterate backwards over a list.
 * @pos: the type * to use as a loop counter.
 * @head: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_backwards(pos, head, offset)			\
	for ((pos) = (void *)(((char *)(head)->zprev) - (offset));		\
	    (((char *)pos) + (offset) != (void *)(head)) || ((pos) = NULL);	\
	    (pos) = (void *)(((char *)(((struct psclist_head *)(((char *)pos) +	\
	      (offset)))->zprev)) - (offset)))

/**
 * psclist_for_each_entry2_safe - iterate over list of given type.
 * @pos: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage
 * @head: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_safe(pos, n, head, offset)			\
	for ((pos) = (void *)(((char *)(head)->znext) - (offset)),		\
	    (n) = (void *)(((char *)(((struct psclist_head *)(((char *)pos) +	\
	      (offset)))->znext)) - (offset));					\
	    (((char *)pos) + (offset) != (void *)(head)) ||			\
	      ((pos) = (n) = NULL); (pos) = (n),				\
	    (n) = (void *)(((char *)(((struct psclist_head *)(((char *)n) +	\
	      (offset)))->znext)) - (offset)))

static __inline void
psclist_add_sorted(struct psclist_head *hd, struct psclist_head *elem,
    int (*cmpf)(const void *a, const void *b), ptrdiff_t offset)
{
	struct psclist_head *e;

	psc_assert(elem);
	psclist_for_each_entry2(e, hd, offset)
		if (cmpf((char *)elem - offset, (char*)e - offset) > 0) {
			psclist_xadd(elem, e);
			return;
		}
	psclist_xadd_tail(elem, hd);
}

#endif /* _PFL_LIST_H_ */
