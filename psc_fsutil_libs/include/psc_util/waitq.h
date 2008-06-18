/* $Id$ */

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "psc_util/lock.h"
#include "psc_ds/list.h"

#if HAVE_LIBPTHREAD

# include <pthread.h>

struct psc_wait_queue {
	pthread_mutex_t		wq_mut;
	pthread_cond_t		wq_cond;
};

#else /* HAVE_LIBPTHREAD */

struct psc_wait_queue {
	psc_spinlock_t		wq_mut;
	struct psclist_head	wq_sleepers;
};

#endif /* HAVE_LIBPTHREAD */

typedef struct psc_wait_queue psc_waitq_t;

void psc_waitq_init(psc_waitq_t *q);
void psc_waitq_wait(psc_waitq_t *q, psc_spinlock_t *k);
int  psc_waitq_timedwait(psc_waitq_t *q, psc_spinlock_t *k,
			 const struct timespec *abstime);
void psc_waitq_wakeup(psc_waitq_t *q);
void psc_waitq_wakeall(psc_waitq_t *q);

/* Compatibility for LNET code. */
typedef psc_waitq_t		wait_queue_head_t;
#define init_waitqueue_head(q)	psc_waitq_init(q)
#define wake_up(q)		psc_waitq_wakeup(q)

#endif /* _PFL_WAITQ_H_ */
