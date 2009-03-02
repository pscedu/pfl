/* $Id$ */

/*
 * Threading library, integrated with logging subsystem along with some
 * other useful utilities.
 */

#include "psc_util/subsys.h"

#include <sys/syscall.h>

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "psc_types.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/hash.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/alloc.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/mem.h"
#include "psc_util/strlcpy.h"
#include "psc_util/thread.h"

__static pthread_key_t	psc_thrkey;
__static pthread_key_t	psc_logkey;
struct psc_lockedlist	psc_threads =
    PLL_INITIALIZER(&psc_threads, struct psc_thread, pscthr_lentry);

/**
 * pscthr_destroy - Thread destructor.
 * @arg: thread data.
 */
__static void
_pscthr_destroy(void *arg)
{
	struct psc_thread *thr = arg;

	reqlock(&thr->pscthr_lock);
	PSCFREE(thr->pscthr_loglevels);

	pll_remove(&psc_threads, thr);

	if (thr->pscthr_dtor)
		thr->pscthr_dtor(thr->pscthr_private);
	if (thr->pscthr_flags & PTF_FREE)
		free(thr);
}

/*
 * pscthr_sigusr1 - catch SIGUSR1: pause the thread.
 * @sig: signal number.
 */
void
_pscthr_sigusr1(__unusedx int sig)
{
	struct psc_thread *thr;
	int locked;

	thr = pscthr_get();
	while (!tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags |= PTF_PAUSED;
	while (thr->pscthr_flags & PTF_PAUSED) {
		psc_waitq_wait(&thr->pscthr_waitq,
		    &thr->pscthr_lock);
		spinlock(&thr->pscthr_lock);
	}
	ureqlock(&thr->pscthr_lock, locked);
}

/*
 * pscthr_sigusr2 - catch SIGUSR2: unpause the thread.
 * @sig: signal number.
 */
void
_pscthr_sigusr2(__unusedx int sig)
{
	struct psc_thread *thr;
	int locked;

	thr = pscthr_get();
	while (!tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags &= ~PTF_PAUSED;
	psc_waitq_wakeall(&thr->pscthr_waitq);
	ureqlock(&thr->pscthr_lock, locked);
}

/**
 * pscthrs_init - Initialize threading subsystem.
 */
void
pscthrs_init(void)
{
	int rc;

	rc = pthread_key_create(&psc_thrkey, _pscthr_destroy);
	if (rc)
		psc_fatalx("pthread_key_create: %s", strerror(rc));
	rc = pthread_key_create(&psc_logkey, free);
	if (rc)
		psc_fatalx("pthread_key_create: %s", strerror(rc));
}

/**
 * pscthr_get_canfail - Retrieve thread info from thread-local storage unless
 *	uninitialized.
 */
struct psc_thread *
pscthr_get_canfail(void)
{
	return (pthread_getspecific(psc_thrkey));
}

/**
 * pscthr_get - Retrieve thread info from thread-local storage.
 */
struct psc_thread *
pscthr_get(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	psc_assert(thr);
	return (thr);
}

/**
 * _pscthr_finish_init: final initilization code common among all
 *	instantiation methods.
 * @thr: thread to finish initializing.
 */
void
_pscthr_finish_init(struct psc_thread *thr)
{
	struct sigaction sa;
	int n, rc;

	if (thr->pscthr_privsiz)
		thr->pscthr_private = psc_alloc(thr->pscthr_privsiz,
		    PAF_NOLOG);

	thr->pscthr_loglevels = psc_alloc(psc_nsubsys *
	    sizeof(*thr->pscthr_loglevels), PAF_NOLOG);
	for (n = 0; n < psc_nsubsys; n++)
		thr->pscthr_loglevels[n] = psc_log_getlevel_ss(n);
	thr->pscthr_pthread = pthread_self();
	thr->pscthr_thrid = syscall(SYS_gettid);
	rc = pthread_setspecific(psc_thrkey, thr);
	if (rc)
		psc_fatalx("pthread_setspecific: %s", strerror(rc));

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _pscthr_sigusr1;
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		psc_fatal("sigaction");

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _pscthr_sigusr2;
	if (sigaction(SIGUSR2, &sa, NULL) == -1)
		psc_fatal("sigaction");

	pll_add(&psc_threads, thr);
}

/**
 * pscthr_bind_memnode: bind a thread to a specific NUMA memnode.
 * @thr: thread structure.
 */
void
_pscthr_bind_memnode(struct psc_thread *thr)
{
#ifdef HAVE_NUMA
	nodemask_t nm;

	nodemask_zero(&nm);
	nodemask_set(&nm, cpuset_p_rel_to_sys_mem(syscall(SYS_gettid),
	    thr->pscthr_memnid));
	if (numa_run_on_node_mask(&nm) == -1)
		psc_fatal("numa");
	numa_set_membind(&nm);
#else
	(void)thr; /* avoid unused warnings */
#endif
}

/**
 * pscthr_begin: each new thread begins its life here.
 *	This routine invokes the thread's real start routine once
 *	it's safe after the thread who created us has finished our
 *	(external) initialization.
 * @arg: thread structure.
 */
__static void *
_pscthr_begin(void *arg)
{
	struct psc_thread *thr, *oldthr = arg;

	_pscthr_bind_memnode(oldthr);

	/* Setup a localised copy of the thread. */
	thr = psc_alloc(sizeof(*thr), PAF_NOLOG);
	psc_waitq_init(&thr->pscthr_waitq);
	LOCK_INIT(&thr->pscthr_lock);
	spinlock(&thr->pscthr_lock);

	/* Copy values from original. */
	spinlock(&oldthr->pscthr_lock);
	thr->pscthr_type = oldthr->pscthr_type;
	thr->pscthr_startf = oldthr->pscthr_startf;
	thr->pscthr_privsiz = oldthr->pscthr_privsiz;
	thr->pscthr_flags = oldthr->pscthr_flags;
	thr->pscthr_dtor = oldthr->pscthr_dtor;
	strlcpy(thr->pscthr_name, oldthr->pscthr_name,
	    sizeof(thr->pscthr_name));

	_pscthr_finish_init(thr);

	/* Inform the spawner where our memory is. */
	oldthr->pscthr_private = thr;
	psc_waitq_wakeall(&oldthr->pscthr_waitq);

	/* Wait for the spawner to finish us. */
	do {
		psc_waitq_wait(&thr->pscthr_waitq, &thr->pscthr_lock);
		spinlock(&thr->pscthr_lock);
	} while ((thr->pscthr_flags & PTF_READY) == 0);
	freelock(&thr->pscthr_lock);
	return (thr->pscthr_startf(thr));
}

/**
 * pscthr_init - Initialize a thread.
 * @type: application-specific thread type.
 * @startf: thread execution routine.  By specifying a NULL routine,
 *	no pthread will be spawned (assuming that an actual pthread
 *	already exists or will be taken care of).
 * @privsiz: size of thread-type-specific data.
 * @flags: operational flags.
 * @dtor: optional destructor to run when/if thread exits.
 * @namefmt: application-specific printf(3) name for thread.
 */
struct psc_thread *
_pscthr_init(int type, int flags, void *(*startf)(void *),
    void (*dtor)(void *), size_t privsiz, int memnid,
    const char *namefmt, ...)
{
	struct psc_thread mythr, *thr;
	va_list ap;
	int rc;

	if (flags & PTF_PAUSED)
		psc_fatalx("pscthr_init: PTF_PAUSED specified");

	thr = startf ? &mythr : psc_alloc(sizeof(*thr), PAF_NOLOG);
	memset(thr, 0, sizeof(*thr));
	psc_waitq_init(&thr->pscthr_waitq);
	LOCK_INIT(&thr->pscthr_lock);
	thr->pscthr_type = type;
	thr->pscthr_startf = startf;
	thr->pscthr_privsiz = privsiz;
	thr->pscthr_flags = flags | PTF_RUN;
	thr->pscthr_dtor = dtor;
	thr->pscthr_memnid = memnid;

	va_start(ap, namefmt);
	rc = vsnprintf(thr->pscthr_name, sizeof(thr->pscthr_name),
	    namefmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf: %s", namefmt);
	if (rc >= (int)sizeof(thr->pscthr_name))
		psc_fatalx("pscthr_init: thread name too long: %s", namefmt);

	/* Pin thread until initialization is complete. */
	spinlock(&thr->pscthr_lock);
	if (startf) {
		/*
		 * Thread will finish initializing in its own context
		 * and set pscthr_private to the location of the new
		 * localized memory.
		 */
		if ((rc = pthread_create(&thr->pscthr_pthread,
		    NULL, _pscthr_begin, thr)) != 0)
			psc_fatalx("pthread_create: %s", strerror(rc));
		psc_waitq_wait(&thr->pscthr_waitq, &thr->pscthr_lock);
		thr = thr->pscthr_private;
		if (thr->pscthr_privsiz == 0)
			pscthr_setready(thr);
	} else {
		/* Initializing our own thread context. */
		_pscthr_bind_memnode(thr);
		_pscthr_finish_init(thr);
	}
	return (thr);
}

/**
 * pscthr_setready - set thread state to READY.
 * @thr: thread ready to start.
 */
void
pscthr_setready(struct psc_thread *thr)
{
	reqlock(&thr->pscthr_lock);
	thr->pscthr_flags |= PTF_READY;
	psc_waitq_wakeall(&thr->pscthr_waitq);
	freelock(&thr->pscthr_lock);
}

/**
 * psc_log_getlevel - get thread logging level for the specified subsystem.
 * This routine is not intended for general-purpose usage.
 * @subsys: subsystem ID.
 */
int
psc_log_getlevel(int subsys)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (psc_log_getlevel_ss(subsys));
	if (subsys >= psc_nsubsys)
		psc_fatalx("subsystem out of bounds (%d)", subsys);
	return (thr->pscthr_loglevels[subsys]);
}

/*
 * pscthr_getname - get thread name.
 * This routine is not intended for general-purpose usage.
 */
const char *
pscthr_getname(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (NULL);
	return (thr->pscthr_name);
}

/*
 * pscthr_setpause - set thread pause state.
 * @thr: the thread.
 * @pause: whether to pause or unpause the thread.
 */
void
pscthr_setpause(struct psc_thread *thr, int pause)
{
	spinlock(&thr->pscthr_lock);
	if (pause ^ (thr->pscthr_flags & PTF_PAUSED))
		pthread_kill(thr->pscthr_pthread,
		    pause ? SIGUSR1 : SIGUSR2);
	freelock(&thr->pscthr_lock);
}

/**
 * psclog_getdatamem - Obtain logging info from thread-local storage.
 * @d: data pointer.
 */
struct psclog_data *
psclog_getdatamem(void)
{
	return (pthread_getspecific(psc_logkey));
}

/**
 * psclog_setdatamem - Store logging info into thread-local storage.
 * @d: data pointer.
 */
void
psclog_setdatamem(struct psclog_data *d)
{
	int rc;

	rc = pthread_setspecific(psc_logkey, d);
	if (rc)
		err(1, "pthread_setspecific: %s",
		    strerror(rc));
}

/**
 * psc_get_hostname - Override hostname retrieval to access thread-local
 *	storage for hostname.
 */
char *
psc_get_hostname(void)
{
	return (psclog_getdata()->pld_hostname);
}

/*
 * pscthr_setrun - Set the PTF_RUN flag of a thread.
 * @thr: thread to modify.
 * @run: boolean whether or not the thread should run.
 */
void
pscthr_setrun(struct psc_thread *thr, int run)
{
	int locked;

	locked = reqlock(&thr->pscthr_lock);
	if (run) {
		thr->pscthr_flags |= PTF_RUN;
		psc_waitq_wakeall(&thr->pscthr_waitq);
	} else
		thr->pscthr_flags &= ~PTF_RUN;
	ureqlock(&thr->pscthr_lock, locked);
}

/**
 * pscthr_run - Ensure a thread should a main loop iteration.
 */
int
pscthr_run(void)
{
	struct psc_thread *thr;

	thr = pscthr_get();
	spinlock(&thr->pscthr_lock);
	if ((thr->pscthr_flags & PTF_RUN) == 0)
		do {
			psc_waitq_wait(&thr->pscthr_waitq,
			    &thr->pscthr_lock);
			spinlock(&thr->pscthr_lock);
		} while ((thr->pscthr_flags) == 0);
	freelock(&thr->pscthr_lock);
	return (1);
}
