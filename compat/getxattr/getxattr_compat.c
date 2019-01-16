#include <sys/types.h>
#include <sys/xattr.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)getxattr;
	exit(0);
}
