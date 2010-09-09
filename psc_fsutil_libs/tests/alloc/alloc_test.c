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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	size_t sz;
	void *p;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	p = PSCALLOC(213);
	psc_assert(p);
	p = psc_realloc(p, 65536, 0);
	psc_assert(p);
	PSCFREE(p);

	p = PSCALLOC(128);
	PSCFREE(p);

	p = psc_alloc(24, PAF_PAGEALIGN);
	psc_free_aligned(p);

	p = PSCALLOC(24);
	psc_assert(p);
	p = psc_realloc(p, 128, 0);
	psc_assert(p);
	PSCFREE(p);

	p = psc_alloc(8, PAF_LOCK);
	*(uint64_t *)p = 0;
	psc_free_mlocked(p, 8);

	sz = 1024;
	p = psc_alloc(sz, PAF_LOCK | PAF_PAGEALIGN);
	memset(p, 0, sz);
	psc_free_mlocked_aligned(p, sz);

	exit(0);
}
