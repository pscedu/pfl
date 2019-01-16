#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)setproctitle;
	exit(0);
}
