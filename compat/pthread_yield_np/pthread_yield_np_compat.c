#include <pthread.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)pthread_yield_np;
	exit(0);
}
