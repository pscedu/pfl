/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_ds/listcache.h"

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
	progname = argv[0];
	if (getopt(argc, argv, "") == -1)
		usage();
	argc -= optind;
	if (argc)
		usage();
	exit(0);
}
