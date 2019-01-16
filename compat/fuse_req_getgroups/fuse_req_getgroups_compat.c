#include <stdlib.h>

#include <fuse_lowlevel.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)fuse_req_getgroups; 
	exit(0);
}
