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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_util/log.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

#define PTR_0	((void *)0x00)
#define PTR_1	((void *)0x01)
#define PTR_2	((void *)0x02)
#define PTR_3	((void *)0x03)
#define PTR_4	((void *)0x04)

#define PTR_a	((void *)0x0a)
#define PTR_b	((void *)0x0b)
#define PTR_c	((void *)0x0c)

void
check(struct psc_dynarray *da, ...)
{
	void *p, *checkp;
	va_list ap;
	int j;

	va_start(ap, da);
	DYNARRAY_FOREACH(p, j, da) {
		checkp = va_arg(ap, void *);
		psc_assert(p == checkp);
	}
	va_end(ap);
}

void
display(struct psc_dynarray *da)
{
	void *p;
	int j;

	DYNARRAY_FOREACH(p, j, da)
		printf("%p ", p);
	printf("\n");
}

int
cmp(const void *a, const void *b)
{
	return (CMP(a, b));
}

int
pcmp(const void *a, const void *b)
{
	const void * const *x = a;
	const void * const *y = b;

	return (CMP(*x, *y));
}

int
main(int argc, char *argv[])
{
	struct psc_dynarray da = DYNARRAY_INIT;
	void *p;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	p = PTR_4; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_4, NULL);
	psc_dynarray_splice(&da, 0, 1, NULL, 0); check(&da, NULL);

	p = PTR_4; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_4, NULL);
	p = PTR_3; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_3, PTR_4, NULL);
	p = PTR_2; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_2, PTR_3, PTR_4, NULL);
	p = PTR_1; psc_dynarray_splice(&da, 0, 0, &p, 1); check(&da, PTR_1, PTR_2, PTR_3, PTR_4, NULL);

	psc_dynarray_splice(&da, 0, 0, NULL, 0); check(&da, PTR_1, PTR_2, PTR_3, PTR_4, NULL);

	p = PTR_a; psc_dynarray_splice(&da, 2, 1, &p, 1); check(&da, PTR_1, PTR_2, PTR_a, PTR_4, NULL);
	p = PTR_b; psc_dynarray_splice(&da, 3, 0, &p, 1); check(&da, PTR_1, PTR_2, PTR_a, PTR_b, PTR_4, NULL);

	psc_dynarray_sort(&da, qsort, pcmp); check(&da, PTR_1, PTR_2, PTR_4, PTR_a, PTR_b, NULL);

	psc_assert(psc_dynarray_bsearch(&da, PTR_0, cmp) == 0);
	psc_assert(psc_dynarray_bsearch(&da, PTR_1, cmp) == 0);
	psc_assert(psc_dynarray_bsearch(&da, PTR_2, cmp) == 1);
	psc_assert(psc_dynarray_bsearch(&da, PTR_3, cmp) == 2);
	psc_assert(psc_dynarray_bsearch(&da, PTR_4, cmp) == 2);
	psc_assert(psc_dynarray_bsearch(&da, PTR_a, cmp) == 3);
	psc_assert(psc_dynarray_bsearch(&da, PTR_b, cmp) == 4);
	psc_assert(psc_dynarray_bsearch(&da, PTR_c, cmp) == 5);

	exit(0);
}
