/* $Id$ */

#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"

#define STARTWATCH(t) gettimeofday(&(t)[0], NULL)
#define STOPWATCH(t)  gettimeofday(&(t)[1], NULL)

psc_spinlock_t lock = LOCK_INITIALIZER;
atomic_t idx = ATOMIC_INIT(0);
atomic_t nworkers = ATOMIC_INIT(0);
int nthrs = 32;
int nruns = 8000;
int *buf;
const char *progname;

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-n nruns] [-t nthr]\n",
	    progname);
	exit(1);
}

void *
thr_main(__unusedx void *arg)
{
	int *p, i;

	for (i = 0, p = buf; i < nruns; i++, p++) {
		spinlock(&lock);
		atomic_inc(&idx);
		*p = i;
		freelock(&lock);
		usleep(1);
	}
	atomic_dec(&nworkers);
	return 0;
}

int
main(int argc, char *argv[])
{
	struct timeval tv[2], res;
	int oldidx, c, i, *j;

	progname = argv[0];
	pfl_init();
	while (((c = getopt(argc, argv, "n:t:")) != -1))
		switch (c) {
		case 't':
			nthrs = atoi(optarg);
			break;
		case 'n':
			nruns = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	buf = PSCALLOC(nruns * sizeof(*buf));

	atomic_set(&nworkers, nthrs);
	for (i = 0; i < nthrs; i++)
		pscthr_init(0, 0, thr_main, NULL, 0, "thr%d", i);

	oldidx = 0;
	while (atomic_read(&nworkers)) {
		STARTWATCH(tv);
		sleep(1);
		STOPWATCH(tv);
		timersub(&tv[1], &tv[0], &res);
		i = atomic_read(&idx);
		fprintf(stderr, "%d/%d current lock cnt; LPS %f\n",
		    i, nruns * nthrs,
		    (i - oldidx) / (res.tv_sec + res.tv_usec * 1e-6));
		oldidx = i;
	}

	for (i = 0, j = buf; i < nruns; j++, i++)
		if (*j != i)
			psc_fatalx("position %d should be %d, not %d",
			    i, i, *j);
	return 0;
}
