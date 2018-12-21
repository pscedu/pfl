/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2006-2018, Pittsburgh Supercomputing Center
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "pfl/lock.h"
#include "pfl/pthrutil.h"

struct pfl_mutex;

#define	PFL_WAITQ_NAME_MAX	32

#ifdef HAVE_LIBPTHREAD

# include <pthread.h>

struct pfl_waitq {
	struct pfl_mutex	wq_mut;
	pthread_cond_t		wq_cond;
	char			wq_name[PFL_WAITQ_NAME_MAX];
	int			wq_nwaiters;
	int			wq_flags;
};

#define PWQF_NOLOG		(1 << 0)

# define PFL_WAITQ_INIT(name)	{ PSC_MUTEX_INIT, PTHREAD_COND_INITIALIZER, (name), 0, 0 }

#else /* HAVE_LIBPTHREAD */

struct pfl_waitq {
	int			wq_nwaiters;
};

# define PFL_WAITQ_INIT	{ 0 }

#endif

/*
 * Wait until resource managed by wq_cond is available.
 * @wq: wait queue.
 * @lk: optional lock to prevent race condition in waiting.
 */
#define pfl_waitq_wait(wq, lk)		 _pfl_waitq_waitabs((wq), PFL_LOCKPRIMT_SPIN, (lk), NULL)
#define pfl_waitq_waitf(wq, fl, p)	 _pfl_waitq_waitabs((wq), (fl), (p), NULL)

/*
 * Wait at most the amount of time specified (relative to calling time)
 * for the resource managed by wq_cond to become available.
 * @wq: the wait queue to wait on.
 * @lk: optional lock needed to protect the list (optional).
 * @s: number of seconds to wait for (optional).
 * @ns: number of nanoseconds to wait for (optional).
 * Returns: ETIMEDOUT if the resource did not become available if
 * @s or @ns was specififed.
 */
#define pfl_waitq_waitrel(wq, lk, s, ns) _pfl_waitq_waitrel((wq), PFL_LOCKPRIMT_SPIN, (lk), (s), (ns))

#define pfl_waitq_waitrel_s(wq, lk, s)	 pfl_waitq_waitrel((wq), (lk), (s), 0L)
#define pfl_waitq_waitrel_us(wq, lk, us) pfl_waitq_waitrel((wq), (lk), 0L, (us) * 1000L)
#define pfl_waitq_waitrel_ms(wq, lk, ms) pfl_waitq_waitrel((wq), (lk), 0L, (ms) * 1000L * 1000L)
#define pfl_waitq_waitrel_tv(wq, lk, tv) pfl_waitq_waitrel((wq), (lk), (tv)->tv_sec, (tv)->tv_usec * 1000L)
#define pfl_waitq_waitrel_ts(wq, lk, tv) pfl_waitq_waitrel((wq), (lk), (tv)->tv_sec, (tv)->tv_nsec)

#define pfl_waitq_waitrelf_us(wq, fl, p, us)	\
					_pfl_waitq_waitrel((wq), (fl), (p), 0L, (us) * 1000L)

#define pfl_waitq_waitabs(wq, lk, ts)	_pfl_waitq_waitabs((wq), PFL_LOCKPRIMT_SPIN, (lk), (ts))

/*
 * Determine number of threads waiting on a waitq.
 * @wq: wait queue.
 */
#define pfl_waitq_nwaiters(wq)		(wq)->wq_nwaiters

#define pfl_waitq_init(wq, name)	_pfl_waitq_init((wq), (name), 0)
#define pfl_waitq_init_nolog(wq, name)	_pfl_waitq_init((wq), (name), PWQF_NOLOG)

void	_pfl_waitq_init(struct pfl_waitq *, const char *, int);
void	 pfl_waitq_destroy(struct pfl_waitq *);
void	 pfl_waitq_wakeone(struct pfl_waitq *);
void	 pfl_waitq_wakeall(struct pfl_waitq *);
int	_pfl_waitq_waitrel(struct pfl_waitq *, enum pfl_lockprim, void *, long, long);
int	_pfl_waitq_waitabs(struct pfl_waitq *, enum pfl_lockprim, void *, const struct timespec *);

#endif /* _PFL_WAITQ_H_ */
