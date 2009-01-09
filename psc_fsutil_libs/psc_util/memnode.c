/* $Id$ */

#include <pthread.h>
#include <string.h>

#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/memnode.h"

__static psc_spinlock_t		psc_memnodes_lock = LOCK_INITIALIZER;
__static struct dynarray	psc_memnodes = DYNARRAY_INIT;
__static pthread_key_t		psc_memnodes_key;

void
psc_memnode_init(void)
{
	int rc;

	rc = pthread_key_create(&psc_memnodes_key, NULL);
	if (rc)
		psc_fatalx("pthread_key_create: %s", strerror(rc));
}

struct psc_memnode *
psc_memnode_get(void)
{
	struct psc_memnode *pmn, **pmnv;
	int memnid;

	pmn = pthread_getspecific(psc_memnodes_key);
	if (pmn)
		return (pmn);

	memnid = psc_memnode_getid();
	spinlock(&psc_memnodes_lock);
	dynarray_hintlen(&psc_memnodes, memnid + 1);
	pmnv = dynarray_get(&psc_memnodes);
	pmn = pmnv[memnid];
	if (pmn == NULL) {
		pmn = pmnv[memnid] = PSCALLOC(sizeof(*pmn));
		LOCK_INIT(&pmn->pmn_lock);
		dynarray_init(&pmn->pmn_keys);
	}
	freelock(&psc_memnodes_lock);
	pthread_setspecific(psc_memnodes_key, pmn);
	return (pmn);
}

void *
psc_memnode_getkey(struct psc_memnode *pmn, int key)
{
	int locked;
	void *val;

	val = NULL;
	locked = reqlock(&pmn->pmn_lock);
	if (dynarray_len(&pmn->pmn_keys) > key)
		val = dynarray_getpos(&pmn->pmn_keys, key);
	ureqlock(&pmn->pmn_lock, locked);
	return (val);
}

void
psc_memnode_setkey(struct psc_memnode *pmn, int pos, void *val)
{
	int locked;
	void **v;

	locked = reqlock(&pmn->pmn_lock);
	dynarray_hintlen(&pmn->pmn_keys, pos + 1);
	v = dynarray_get(&pmn->pmn_keys);
	v[pos] = val;
	ureqlock(&pmn->pmn_lock, locked);
}
