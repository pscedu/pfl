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

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "psc_util/odtable.h"

/*
 * Keep in sync with SL_PATH_DATADIR and SL_FN_IONBMAPS_ODT
 */
#define DEF_FN	"/var/lib/slash/ion_bmaps.odt"

struct psc_dynarray myReceipts = DYNARRAY_INIT;
const char *progname;

char	*fn = DEF_FN;

int	crc_enabled = 1;
int	create_table;
int	num_free;
int	num_puts;
int	overwrite;
int	show;

size_t	table_size = 1024 * 128;
size_t	elem_size  = 128;
#define ODT_DEFAULT_ITEM_SIZE	128

void
my_odtcb(void *data, struct odtable_receipt *odtr)
{
	char *item = data;

	printf("slot=%zd odtr=%p: %s\n",
	    odtr->odtr_elem, odtr, item);

	psc_dynarray_add(&myReceipts, odtr);
}

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-Ccov] [-e elem_size] [-f #frees] [-n #puts]\n"
	    "\t[-z table_size] file\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct odtable *odt;
	int c, rc, i, verbose = 0;
	char *item;

	pfl_init();
	progname = argv[0];
	elem_size = ODT_DEFAULT_ITEM_SIZE;
	while ((c = getopt(argc, argv, "Cce:f:ln:oz:v")) != -1)
		switch (c) {
		case 'C':
			create_table = 1;
			break;
		case 'c':
			crc_enabled = 1;
			break;
		case 'e':
			elem_size = atoi(optarg);
			break;
		case 'f':
			num_free = atoi(optarg);
			break;
		case 'l':
			warnx("-l is deprecated");
			break;
		case 'n':
			num_puts = atoi(optarg);
			break;
		case 'o':
			overwrite = 1;
			break;
		case 's':
			show = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'z':
			table_size = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc == 1)
		fn = argv[0];
	else if (argc != 0)
		usage();

	if (create_table) {
		rc = odtable_create(fn, table_size, elem_size, overwrite);
		if (rc)
			errx(1, "create %s: %s", fn, strerror(-rc));
		if (verbose)
			warnx("created od-table %s (elem size = %ld, table size = %ld)",
				fn, elem_size, table_size);
	}

	rc = odtable_load(&odt, fn, "%s", fn);
	if (rc)
		errx(1, "load %s: %s", fn, strerror(-rc));
	odtable_scan(odt, my_odtcb);

	item = PSCALLOC(elem_size);
	for (i = 0; i < num_puts; i++) {
		snprintf(item, elem_size, "... put_number=%d ...", i);
		if (odtable_putitem(odt, item, elem_size) == NULL) {
			psc_error("odtable_putitem() failed: table full");
			break;
		}
	}
	PSCFREE(item);

	if (num_free) {
		struct odtable_receipt *odtr = NULL;

		while (psc_dynarray_len(&myReceipts) && num_free--) {
			odtr = psc_dynarray_getpos(&myReceipts, 0);
			psclog_debug("got odtr=%p key=%"PRIx64" slot=%zd",
			    odtr, odtr->odtr_key, odtr->odtr_elem);

			if (!odtable_freeitem(odt, odtr))
				psc_dynarray_remove(&myReceipts, odtr);
		}

		psclog_debug("# of items left is %d", psc_dynarray_len(&myReceipts));
	}

	exit(odtable_release(odt));
}
