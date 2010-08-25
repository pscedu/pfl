/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/listcache.h"

const char *progname;
struct psclist_head hd = PSCLIST_HEAD_INIT(hd);
int nitems;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

struct m {
	int v;
	struct psclist_head lentry;
};

void
addelem(int v)
{
	struct m *m;

	m = PSCALLOC(sizeof(*m));
	m->v = v;
	psclist_xadd(&m->lentry, &hd);
	nitems++;
}

int
shift(void)
{
	struct m *m;

	m = psclist_first_entry(&hd, struct m, lentry);
	if (m == NULL)
		return (0);
	psclist_del(&m->lentry);
	printf("v: %d\n", m->v);
	PSCFREE(m);
	return (1);
}

int
m_cmp(const void *a, const void *b)
{
	struct m * const *ma = a, *x = *ma, * const *mb = b, *y = *mb;

	return (CMP(x->v, y->v));
}

int
main(int argc, char *argv[])
{
	struct psc_listcache lc;
	struct timespec ts;
	struct m *m;
	void *p;
	int i;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	addelem(4);
	addelem(2);
	addelem(9);
	addelem(23);
	addelem(47);
	addelem(5);
	addelem(8);

	for (i = 0; i < 100; i++) {
		p = PSCALLOC(sizeof(void *) * nitems);
		psclist_sort(p, &hd, nitems, offsetof(struct m, lentry),
		    qsort, m_cmp);
		PSCFREE(p);
	}

	while (shift())
		;

	lc_init(&lc, struct m, lentry);

	m = PSCALLOC(sizeof(*m));
	m->v = 5;
	lc_addqueue(&lc, m);

	m = PSCALLOC(sizeof(*m));
	m->v = 8;
	lc_addqueue(&lc, m);

	m = PSCALLOC(sizeof(*m));
	m->v = 13;
	lc_addqueue(&lc, m);

	memset(&ts, 0, sizeof(ts));
	ts.tv_sec = time(NULL) + 1;

	m = lc_gettimed(&lc, &ts);
	psc_assert(m->v == 5);
	PSCFREE(m);

	m = lc_gettimed(&lc, &ts);
	psc_assert(m->v == 8);
	PSCFREE(m);

	m = lc_gettimed(&lc, &ts);
	psc_assert(m->v == 13);
	PSCFREE(m);

	exit(0);
}
