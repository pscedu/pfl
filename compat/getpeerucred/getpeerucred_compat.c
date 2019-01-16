#include <stdlib.h>
#include <ucred.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)getpeerucred;
	exit(0);
}
