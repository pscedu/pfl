/* $Id$ */

#ifndef _PFL_HASH_H_
#define _PFL_HASH_H_

#include "psc_types.h"
#include "psc_ds/hash.h"
#include "psc_ds/list.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

#define LOCK_BUCKET(hb)		spinlock(&(hb)->hbucket_lock)
#define ULOCK_BUCKET(hb)	freelock(&(hb)->hbucket_lock)

#define LOCK_HASHTBL(ht)	spinlock(&(ht)->htable_lock)
#define ULOCK_HASHTBL(ht)	freelock(&(ht)->htable_lock)

#define GET_BUCKET(t,i) &(t)->htable_buckets[(i) % (t)->htable_size]

#define HTNAME_MAX 30

struct hash_bucket {
	struct psclist_head	  hbucket_list;	/* Entry list head */
	psc_spinlock_t		  hbucket_lock;	/* Spinlock for this bucket */
};

#define HASH_BUCKET_SZ sizeof(struct hash_bucket)

struct hash_table {
	char			  htable_name[HTNAME_MAX];
	struct psclist_head	  htable_entry;

	int			  htable_size;
	int			  htable_strlen_max;
	psc_spinlock_t		  htable_lock;
	struct hash_bucket	 *htable_buckets;
	int			(*htcompare)(const void *, const void *);
};

struct hash_entry {
	struct psclist_head	  hentry_lentry;/* Entry list pointers */
	u64			 *hentry_id;	/* Pointer to the hash element id */
	void			 *private;	/* pointer to private data */
};

/**
 * hash_entry - get the struct for this entry
 * @ptr:	the struct hash_entry pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the hash_entry struct within the struct.
 */
#define hash_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/*
 * String Hash Defines
 */
#define SGET_BUCKET(t, i) &(t)->htable_buckets[str_hash((i)) % (t)->htable_size]

struct hash_entry_str {
	struct psclist_head	  hentry_str_lentry;/* Entry list pointers */
	const char		 *hentry_str_id;   /* Pointer to the hash element str */
	void			 *private;	   /* pointer to private data  */
};

/*
 * "This well-known hash function
 *    was used in P.J. Weinberger's C compiler
 *    (cf. Compilers: Principles, Techniques,
 *    and Tools, by Aho, Sethi & Ullman,
 *    Addison-Wesley, 1988, p. 436)."
 */
static inline int
str_hash(const char *s)
{
	const char *p;
	unsigned h = 0, g;

	if ( s == NULL )
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

struct hash_entry * get_hash_entry(const struct hash_table *, u64, const void *, void (*)(void *));
struct hash_entry_str * get_hash_entry_str(struct hash_table *, const char *);

int  del_hash_entry(struct hash_table *, u64);
int  del_hash_entry_str(struct hash_table *, const char *);
void add_hash_entry(struct hash_table *, struct hash_entry *);
void add_hash_entry_str(struct hash_table *, struct hash_entry_str *);
void hash_table_printstats(struct hash_table *);
void hash_table_stats(struct hash_table *, int *, int *, int *, int *);
void init_hash_entry(struct hash_entry *, u64 *, void *);
void init_hash_entry_str(struct hash_entry_str *, const char *, void *);
void init_hash_table(struct hash_table *, int, const char *, ...);

extern struct psclist_head hashTablesList;
extern psc_spinlock_t hashTablesListLock;

#endif /* _PFL_HASH_H_ */
