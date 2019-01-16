#include <sys/types.h>
#include <sys/disk.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)DKIOCEJECT; 
	exit(0);
}
