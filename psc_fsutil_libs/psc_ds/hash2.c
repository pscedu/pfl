/* $Id$ */

#include <sys/param.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "psc_ds/hash2.h"
#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

union entry_id_u {
	uint64_t	*id;
	const char	*str;
	void		*p;
};

struct psc_lockedlist psc_hashtbls =
    PLL_INITIALIZER(&psc_hashtbls, struct psc_hashtbl, pht_lentry);

/*
 * "This well-known hash function
 *    was used in P.J. Weinberger's C compiler
 *    (cf. Compilers: Principles, Techniques,
 *    and Tools, by Aho, Sethi & Ullman,
 *    Addison-Wesley, 1988, p. 436)."
 */
__static inline int
psc_hash_stringify(const char *s)
{
	const char *p;
	unsigned h = 0, g;

	if (s == NULL)
		return -1;
	for (p = s; *p != '\0'; p++) {
		h = (h << 4) + (*p);
		if ((g = h & 0xf0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	return (h);
}

void
_psc_hashtbl_init(struct psc_hashtbl *t, int flags,
    ptrdiff_t idoff, ptrdiff_t hentoff, int nbuckets,
    int (*cmp)(const void *, const void *), const char *fmt, ...)
{
	struct psc_hashbkt *b;
	va_list ap;
	int i;

	if (nbuckets == 0)
		psc_fatalx("hash table size must be non-zero for modulus to work");

	memset(t, 0, sizeof(*t));
	INIT_PSCLIST_ENTRY(&t->pht_lentry);
	LOCK_INIT(&t->pht_lock);
	t->pht_nbuckets = nbuckets;
	t->pht_buckets = PSCALLOC(nbuckets * sizeof(*t->pht_buckets));
	t->pht_flags = flags;
	t->pht_idoff = idoff;
	t->pht_hentoff = hentoff;
	t->pht_cmp = cmp;

	va_start(ap, fmt);
	vsnprintf(t->pht_name, sizeof(t->pht_name), fmt, ap);
	va_end(ap);

	for (i = 0, b = t->pht_buckets; i < nbuckets; i++, b++) {
		INIT_PSCLIST_HEAD(&b->phb_listhd);
		LOCK_INIT(&b->phb_lock);
		atomic_set(&b->phb_nitems, 0);
	}
	pll_add(&psc_hashtbls, t);
}

/**
 * psc_hashtbl_lookup - find a hash table by its name, return NULL if none exists.
 * @name: name of hash table.
 */
struct psc_hashtbl *
psc_hashtbl_lookup(const char *name)
{
	struct psc_hashtbl *t;

	PLL_LOCK(&psc_hashtbls);
	psclist_for_each_entry(t, &psc_hashtbls.pll_listhd, pht_lentry)
		if (strcmp(t->pht_name, name) == 0)
			break;
	PLL_ULOCK(&psc_hashtbls);
	return (t);
}

/**
 * psc_hashtbl_destroy - reclaim a hash table's resources.
 * @t: table to destroy.
 * Notes: make sure the hash table has been emptied!
 */
void
psc_hashtbl_destroy(struct psc_hashtbl *t)
{
	pll_remove(&psc_hashtbls, t);
#if 0
	for (i = 0; i < t->pht_nbuckets; i++)
		if (atomic_read(&t->pht_buckets[i].phb_nitems))
			psc_fatalx("psc_hashtbl_destroy: "
			    "hash table not empty");
#endif
	PSCFREE(t->pht_buckets);
}

__static struct psc_hashbkt *
psc_hashbkt_getv(const struct psc_hashtbl *t, va_list ap)
{
	struct psc_hashbkt *b;

	if (t->pht_flags & PHTF_STR)
		b = &t->pht_buckets[psc_hash_stringify(va_arg(ap,
		    const char *)) % t->pht_nbuckets];
	else
		b = &t->pht_buckets[va_arg(ap,
		    uint64_t) % t->pht_nbuckets];
	return (b);
}

__static struct psc_hashbkt *
psc_hashbkt_getu(const struct psc_hashtbl *t, const union entry_id_u *idp)
{
	struct psc_hashbkt *b;

	if (t->pht_flags & PHTF_STR)
		b = &t->pht_buckets[psc_hash_stringify(idp->str) %
		    t->pht_nbuckets];
	else
		b = &t->pht_buckets[*idp->id % t->pht_nbuckets];
	return (b);
}

/**
 * psc_hashbkt_get - locate the bucket containing an item with the
 *	given ID.
 * @t: table to search.
 * @ap: variable argument which should consist of one of either type of:
 *	- uint64_t hash ID value
 *	- const char * string ID value
 */
struct psc_hashbkt *
psc_hashbkt_get(const struct psc_hashtbl *t, ...)
{
	struct psc_hashbkt *b;
	va_list ap;

//	PSC_HASHTBL_LOCK(t);
	va_start(ap, t);
	b = psc_hashbkt_getv(t, ap);
	va_end(ap);
//	psc_hashbkt_lock(b);
//	b->phb_refcnt++;
//	psc_hashbkt_unlock(b);
//	PSC_HASHTBL_ULOCK(t);
	return (b);
}

__static void *
_psc_hashbkt_searchv(const struct psc_hashtbl *t,
    struct psc_hashbkt *b, int flags, const void *cmp,
    void (*cbf)(void *), va_list ap)
{
	union entry_id_u idu;
	const char *strid;
	uint64_t id;
	int locked;
	void *p;

	id = 0; /* gcc */
	strid = NULL; /* gcc */

	if (t->pht_cmp)
		psc_assert(cmp);
	else
		psc_assert(cmp == NULL);

	if (t->pht_flags & PHTF_STR)
		strid = va_arg(ap, const char *);
	else
		id = va_arg(ap, uint64_t);

	locked = reqlock(&b->phb_lock);
	PSC_HASHBKT_FOREACH_ENTRY(t, p, b) {
		idu.p = (char *)p + t->pht_idoff;
		if (t->pht_flags & PHTF_STR) {
			if (strcmp(strid, idu.str))
				continue;
		} else if (id != *idu.id)
			continue;
		if (t->pht_cmp == NULL || t->pht_cmp(cmp, p)) {
			if (cbf)
				cbf(p);
			break;
		}
	}
	if (p && (flags & PHLF_DEL)) {
		psclist_del((struct psclist_head *)((char *)p +
		    t->pht_hentoff));
		atomic_dec(&b->phb_nitems);
	}
	ureqlock(&b->phb_lock, locked);
	return (p);
}

void *
_psc_hashbkt_search(const struct psc_hashtbl *t,
    struct psc_hashbkt *b, int flags, const void *cmp,
    void (*cbf)(void *), ...)
{
	va_list ap;
	void *p;

	va_start(ap, cbf);
	p = _psc_hashbkt_searchv(t, b, flags, cmp, cbf, ap);
	va_end(ap);
	return (p);
}

void *
_psc_hashtbl_search(const struct psc_hashtbl *t, int flags,
    const void *cmp, void (*cbf)(void *), ...)
{
	struct psc_hashbkt *b;
	va_list ap, aq;
	void *p;

	va_start(ap, cbf);
	va_copy(aq, ap);
	b = psc_hashbkt_getv(t, aq);
	va_end(aq);
	p = _psc_hashbkt_searchv(t, b, flags, cmp, cbf, ap);
	va_end(ap);
	return (p);
}

/**
 * psc_hashent_init - initialize a item for use with a hash table.
 * @t: the hash table the item will be associated with.
 * @p: item to initialize.
 */
void
psc_hashent_init(const struct psc_hashtbl *t, void *p)
{
	struct psclist_head *e;

	psc_assert(p);
	e = (struct psclist_head *)((char *)p + t->pht_hentoff);
	INIT_PSCLIST_ENTRY(e);
}

/**
 * psc_hashent_conjoint - test if an item belongs in a hash table.
 * @t: the hash table.
 * @p: item to check.
 */
int
psc_hashent_conjoint(const struct psc_hashtbl *t, void *p)
{
	struct psclist_head *e;

	psc_assert(p);
	e = (struct psclist_head *)((char *)p + t->pht_hentoff);
	return (psclist_conjoint(e));
}

/**
 * psc_hashent_remove - remove an item from the hash table its in.
 * @t: the hash table.
 * @p: the item to remove from hash table.
 */
void
psc_hashent_remove(const struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	union entry_id_u idu;
	int locked;

	psc_assert(p);
	idu.p = (char *)p + t->pht_idoff;
	b = psc_hashbkt_getu(t, &idu);
	locked = reqlock(&b->phb_lock);
	psclist_del((struct psclist_head *)((char *)p + t->pht_hentoff));
	atomic_dec(&b->phb_nitems);
	ureqlock(&b->phb_lock, locked);
}

/**
 * psc_hashbkt_add_item - add an item to a hash bucket.
 * @t: the hash table.
 * @p: item to add.
 */
void
psc_hashbkt_add_item(const struct psc_hashtbl *t, struct psc_hashbkt *b,
    void *p)
{
	int locked;

	psc_assert(p);
	locked = reqlock(&b->phb_lock);
	psclist_xadd((struct psclist_head *)((char *)p + t->pht_hentoff),
	    &b->phb_listhd);
	atomic_inc(&b->phb_nitems);
	ureqlock(&b->phb_lock, locked);
}

/**
 * psc_hashtbl_add_item - add an item to a hash table.
 * @t: the hash table.
 * @p: item to add.
 */
void
psc_hashtbl_add_item(const struct psc_hashtbl *t, void *p)
{
	struct psc_hashbkt *b;
	union entry_id_u idu;
	int locked;

	psc_assert(p);
	idu.p = (char *)p + t->pht_idoff;
	b = psc_hashbkt_getu(t, &idu);
	locked = reqlock(&b->phb_lock);
	psclist_xadd((struct psclist_head *)((char *)p + t->pht_hentoff),
	    &b->phb_listhd);
	atomic_inc(&b->phb_nitems);
	ureqlock(&b->phb_lock, locked);
}

/**
 * psc_hashtbl_getstats - query a hash table for its bucket usage stats.
 * @t: the hash table.
 * @totalbucks: value-result pointer to # of buckets available.
 * @usedbucks: value-result pointer to # of buckets in use.
 * @nents: value-result pointer to # items in hash table.
 * @maxbucklen: value-result pointer to maximum bucket length.
 */
void
psc_hashtbl_getstats(const struct psc_hashtbl *t, int *totalbucks,
    int *usedbucks, int *nents, int *maxbucklen)
{
	struct psc_hashbkt *b;
	int bucklen;

	*nents = 0;
	*usedbucks = 0;
	*maxbucklen = 0;
	*totalbucks = t->pht_nbuckets;
	PSC_HASHTBL_FOREACH_BUCKET(b, t) {
		bucklen = atomic_read(&b->phb_nitems);
		if (bucklen) {
			++*usedbucks;
			*nents += bucklen;
			*maxbucklen = MAX(*maxbucklen, bucklen);
		}
	}
}

/**
 * psc_hashtbl_prstats - print hash table bucket usage stats.
 * @t: the hash table.
 */
void
psc_hashtbl_prstats(const struct psc_hashtbl *t)
{
	int totalbucks, usedbucks, nents, maxbucklen;

	psc_hashtbl_getstats(t, &totalbucks, &usedbucks, &nents,
	    &maxbucklen);
	printf("used %d/total %d, nents=%d, maxlen=%d\n",
	    usedbucks, totalbucks, nents, maxbucklen);
}
