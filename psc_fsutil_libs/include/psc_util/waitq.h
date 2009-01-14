/* $Id$ */

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "psc_ds/list.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

#if HAVE_LIBPTHREAD

# include <pthread.h>

struct psc_waitq {
	pthread_mutex_t		wq_mut;
	pthread_cond_t		wq_cond;
	atomic_t		wq_nwaitors;
};

#else /* HAVE_LIBPTHREAD */

struct psc_waitq {
	atomic_t		wq_nwaitors;
};

#endif /* HAVE_LIBPTHREAD */

typedef struct psc_waitq psc_waitq_t;

void psc_waitq_init(psc_waitq_t *);
void psc_waitq_wait(psc_waitq_t *, psc_spinlock_t *);
int  psc_waitq_timedwait(psc_waitq_t *, psc_spinlock_t *,
	const struct timespec *);
void psc_waitq_wakeone(psc_waitq_t *);
void psc_waitq_wakeall(psc_waitq_t *);

/* Compatibility for LNET code. */
typedef psc_waitq_t		wait_queue_head_t;
#define init_waitqueue_head(q)	psc_waitq_init(q)
#define wake_up(q)		psc_waitq_wakeone(q)

#endif /* _PFL_WAITQ_H_ */
