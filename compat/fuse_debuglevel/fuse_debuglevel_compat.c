#include <stdlib.h>

#include "fuse.h"

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)fuse_highlevel_setdebug;
	exit(0);
}
