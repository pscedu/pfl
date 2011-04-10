/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_PTHRUTIL_H_
#define _PFL_PTHRUTIL_H_

#include <pthread.h>

#include "pfl/pfl.h"

#ifndef HAVE_PTHREAD_BARRIER
# include "pfl/compat/pthread_barrier.h"
#endif

#ifdef PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
# define PSC_MUTEX_INITIALIZER PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
#elif defined(PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP)
# define PSC_MUTEX_INITIALIZER PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP
#else
# define PSC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

#define psc_mutex_ensure_locked(m)	psc_mutex_ensure_locked_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_haslock(m)		psc_mutex_haslock_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_init(m)		psc_mutex_init_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_lock(m)		psc_mutex_lock_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_reqlock(m)		psc_mutex_reqlock_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_trylock(m)		psc_mutex_trylock_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_tryreqlock(m, lk)	psc_mutex_tryreqlock_pci(PFL_CALLERINFO(), (m), (lk))
#define psc_mutex_unlock(m)		psc_mutex_unlock_pci(PFL_CALLERINFO(), (m))
#define psc_mutex_ureqlock(m, lk)	psc_mutex_ureqlock_pci(PFL_CALLERINFO(), (m), (lk))

void	psc_mutex_ensure_locked_pci(struct pfl_callerinfo *, pthread_mutex_t *);
int	psc_mutex_haslock_pci(struct pfl_callerinfo *, pthread_mutex_t *);
void	psc_mutex_init_pci(struct pfl_callerinfo *, pthread_mutex_t *);
void	psc_mutex_lock_pci(struct pfl_callerinfo *, pthread_mutex_t *);
int	psc_mutex_reqlock_pci(struct pfl_callerinfo *, pthread_mutex_t *);
int	psc_mutex_trylock_pci(struct pfl_callerinfo *, pthread_mutex_t *);
int	psc_mutex_tryreqlock_pci(struct pfl_callerinfo *, pthread_mutex_t *, int *);
void	psc_mutex_unlock_pci(struct pfl_callerinfo *, pthread_mutex_t *);
void	psc_mutex_ureqlock_pci(struct pfl_callerinfo *, pthread_mutex_t *, int);

struct psc_rwlock {
	pthread_rwlock_t	pr_rwlock;
	pthread_t		pr_writer;
};

#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
# define PSC_RWLOCK_INITIALIZER { PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP, 0 }
#else
# define PSC_RWLOCK_INITIALIZER { PTHREAD_RWLOCK_INITIALIZER, 0 }
#endif

#define psc_rwlock_hasrdlock(rw)	psc_rwlock_hasrdlock_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_haswrlock(rw)	psc_rwlock_haswrlock_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_init(rw)		psc_rwlock_init_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_rdlock(rw)		psc_rwlock_rdlock_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_reqrdlock(rw)	psc_rwlock_reqrdlock_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_reqwrlock(rw)	psc_rwlock_reqwrlock_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_unlock(rw)		psc_rwlock_unlock_pci(PFL_CALLERINFO(), (rw))
#define psc_rwlock_ureqlock(rw, lk)	psc_rwlock_ureqlock_pci(PFL_CALLERINFO(), (rw), (lk))
#define psc_rwlock_wrlock(rw)		psc_rwlock_wrlock_pci(PFL_CALLERINFO(), (rw))

int	psc_rwlock_hasrdlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);
int	psc_rwlock_haswrlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);
void	psc_rwlock_init_pci(struct pfl_callerinfo *, struct psc_rwlock *);
void	psc_rwlock_rdlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);
int	psc_rwlock_reqrdlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);
int	psc_rwlock_reqwrlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);
void	psc_rwlock_unlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);
void	psc_rwlock_ureqlock_pci(struct pfl_callerinfo *, struct psc_rwlock *, int);
void	psc_rwlock_wrlock_pci(struct pfl_callerinfo *, struct psc_rwlock *);

#endif /* _PFL_PTHRUTIL_H_ */
