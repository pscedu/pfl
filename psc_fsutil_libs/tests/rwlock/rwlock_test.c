/* $Id$ */

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/random.h"

#define NTHRS_MAX	32
#define NLOCKS_MAX	8192
#define SLEEP_US	8

struct thr {
	struct psclist_head lentry;
	char name[40];
	int st;
};

int nlocks = 2000;
int nrd = 8;
int nwr = 3;

pthread_rwlock_t lk = PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP;
struct psclist_head thrs = PSCLIST_HEAD_INIT(thrs);
const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-n locks] [-r readers] [-w writers]\n", progname);
	exit(1);
}

void *
rd_main(void *arg)
{
	struct thr *thr = arg;
	int rc;

	for (; thr->st < nlocks; thr->st++) {
//		do {
//			rc = pthread_rwlock_tryrdlock(&lk);
//			if (rc)
//				usleep(1);
//		} while (rc);
		rc = pthread_rwlock_rdlock(&lk);
		if (rc)
			errx(1, "rdlock: %s", strerror(rc));
//		rc = pthread_rwlock_rdlock(&lk);
//		if (rc != EBUSY)
//			errx(1, "rdlock: %s", strerror(rc));

		usleep(SLEEP_US);
		rc = pthread_rwlock_unlock(&lk);
		if (rc)
			errx(1, "unlock: %s", strerror(rc));
		sched_yield();
	}
	return (NULL);
}

void *
wr_main(void *arg)
{
	struct thr *thr = arg;
	int rc;

	for (; thr->st < nlocks; thr->st++) {
		if (psc_random32u(10) == 20) {
			rc = pthread_rwlock_rdlock(&lk);
			if (rc)
				errx(1, "rdlock: %s", strerror(rc));
			usleep(SLEEP_US);
		}
		rc = pthread_rwlock_wrlock(&lk);
		if (rc)
			errx(1, "wrlock: %s", strerror(rc));

		rc = pthread_rwlock_wrlock(&lk);
		if (rc != EDEADLOCK)
			errx(1, "wrlock: %s", strerror(rc));

		usleep(SLEEP_US);
		rc = pthread_rwlock_unlock(&lk);
		if (rc)
			errx(1, "unlock: %s", strerror(rc));
		sched_yield();
		usleep(100);
	}
	return (NULL);
}

void
spawn(void *(*f)(void *), const char *namefmt, ...)
{
	struct thr *thr;
	pthread_t pthr;
	va_list ap;
	int rc;

	thr = PSCALLOC(sizeof(*thr));

	psclist_xadd_tail(&thr->lentry, &thrs);

	va_start(ap, namefmt);
	vsnprintf(thr->name, sizeof(thr->name), namefmt, ap);
	va_end(ap);

	rc = pthread_create(&pthr, NULL, f, thr);
	if (rc)
		errx(1, "pthread_create: %s", strerror(rc));
}

int
main(int argc, char *argv[])
{
	struct thr *thr;
	int run, i, c;
	char *endp;
	long l;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "n:r:w:")) != -1)
		switch (c) {
		case 'n':
			l = strtol(optarg, &endp, 10);
			if (l < 1 || l > NLOCKS_MAX ||
			    *endp != '\0' || optarg == endp)
				errx(1, "invalid argument: %s", optarg);
			nlocks = (int)l;
			break;
		case 'r':
			l = strtol(optarg, &endp, 10);
			if (l < 1 || l > NTHRS_MAX ||
			    *endp != '\0' || optarg == endp)
				errx(1, "invalid argument: %s", optarg);
			nrd = (int)l;
			break;
		case 'w':
			l = strtol(optarg, &endp, 10);
			if (l < 1 || l > NTHRS_MAX ||
			    *endp != '\0' || optarg == endp)
				errx(1, "invalid argument: %s", optarg);
			nwr = (int)l;
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	for (i = 0; i < nrd; i++)
		spawn(rd_main, "rd%d", i);
	for (i = 0; i < nwr; i++)
		spawn(wr_main, "wr%d", i);
	printf("total");
	psclist_for_each_entry(thr, &thrs, lentry)
		printf(" %5s", thr->name);
	printf("\n");
	do {
		printf("\r%5d", nlocks);

		run = 0;
		psclist_for_each_entry(thr, &thrs, lentry) {
			if (thr->st != nlocks)
				run = 1;
			printf(" %5d", thr->st);
		}

		fflush(stdout);
		usleep(1000000 / 24);
	} while (run);
	printf("\n");
	exit(0);
}
