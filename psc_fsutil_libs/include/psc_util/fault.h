/* $Id$ */

#ifndef _PFL_FAULT_H_
#define _PFL_FAULT_H_

#include "psc_ds/hash2.h"
#include "psc_util/lock.h"

#define PSC_FAULT_NAME_MAX	32

struct psc_fault {
	psc_spinlock_t		pflt_lock;
	struct psc_hashent	pflt_hentry;
	char			pflt_name[PSC_FAULT_NAME_MAX];
	int			pflt_flags;		/* see below */
	int			pflt_hits;		/* #times triggered so far */
	int			pflt_unhits;		/* #times un-triggered so far */
	long			pflt_delay;		/* usec */
	int			pflt_count;		/* #times to respond with programmed behavior */
	int			pflt_begin;		/* #times to skip programmed behavior */
	int			pflt_chance;		/* liklihood of event to occur from 0-100 */
	int			pflt_retval;		/* forced return code */
};

/* fault flags */
#define PFLTF_ACTIVE	(1 << 0)		/* fault point is activated */

#define	psc_fault_lock(pflt)	spinlock(&(pflt)->pflt_lock)
#define	psc_fault_unlock(pflt)	freelock(&(pflt)->pflt_lock)

struct psc_fault *
	psc_fault_lookup(const char *);
int	psc_fault_add(const char *);
void	psc_fault_here(const char *, int *);
void	psc_fault_init(void);
int	psc_fault_register(const char *, int, int, int, int, int);
int	psc_fault_remove(const char *);

extern struct psc_hashtbl psc_fault_table;
extern const char *psc_fault_names[];

#endif /* _PFL_FAULT_H_ */
