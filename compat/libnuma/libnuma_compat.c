#include <stdlib.h>
#include <numa.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)numa_bitmask_free;
	exit(0);
}
